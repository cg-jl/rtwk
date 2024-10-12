#include <bvh.h>
#include <hittable.h>

#include <cassert>
#include <tracy/Tracy.hpp>
#include <utility>

#include "trace_colors.h"

namespace bvh {

// @perf currently bounding boxes occupy a cacheline each (48 bytes). Aligning
// them to 32 bytes means they will occupy an entire cacheline, wanted or not.
// We want the pointers to be aligned to 32 bytes as well, so might need to use
// a vector that has the memory aligned to use the _mm256_stream_load trick.

// TODO: @perf std::vector uses `new`, which aligns the pointer to the required
// aligment, according to <https://stackoverflow.com/a/3658666>.

static int addNode(tree_builder &bld, aabb box, bvh_node node) {
    bld.boxes.emplace_back(std::forward<aabb &&>(box));
    bld.nodes.emplace_back(node);
    bld.node_ends.emplace_back(bld.nodes.size());
    return int(bld.nodes.size() - 1);
}

[[clang::noinline]] static int buildBVHNode(tree_builder &bld, int start,
                                            int end, int depth = 0) {
    static constexpr int minObjectsInTree = 6;
    static_assert(minObjectsInTree > 1,
                  "Min objects in tree must be at least 2, otherwise it will "
                  "stack overflow");

    assert(end > start);
    // Build the bounding box of the span of source objects.
    aabb bbox = empty_aabb;
    for (int i = start; i < end; ++i)
        bbox = aabb(bbox, bld.geoms[i].bounding_box());
    auto object_span = end - start;

    if (object_span == 1) {
        return addNode(bld, std::move(bbox), bvh_node{start, 1});
    }

    int axis = bbox.longest_axis();

    // split in half along longest axis.
    auto partitionPoint = bbox.axis_interval(axis).midPoint();

    auto it = std::partition(
        bld.geoms.data() + start, bld.geoms.data() + end,
        [axis, partitionPoint](geometry const &a) {
            auto a_axis_interval = a.bounding_box().axis_interval(axis);
            return a_axis_interval.midPoint() <= partitionPoint;
        });

    auto midIndex = std::distance(bld.geoms.data(), it);
    if (midIndex == start || midIndex == end) {
        // cannot split these objects
        return addNode(bld, std::move(bbox), bvh_node{start, (end - start)});
    }

    auto parent = addNode(bld, std::move(bbox), bvh_node{-1, 0});

    buildBVHNode(bld, start, midIndex, depth + 1);
    buildBVHNode(bld, midIndex, end, depth + 1);

    bld.nodes[parent].objectIndex = -1;
    bld.node_ends[parent] = bld.nodes.size();

    return parent;
}

}  // namespace bvh

void bvh::tree_builder::finish(size_t start) noexcept {
    bvh::buildBVHNode(*this, start, geoms.size());
}

std::pair<geometry const *, double> bvh::tree::hitBVH(
    timed_ray const &r, double closestHit) const noexcept {
    // deactivate this zone for now.
    ZoneNamedN(zone, "bvh_tree hit", filters::treeHit);
    geometry const *result = nullptr;

    // @perf This has a 'self time' of ~50%. Can I reduce it?
    // Median sample has ~300ns of latency, presumably due to memory fetches.
    // Perhaps inlining (or hinting) hitSpan is the answer?

    // test all relevant nodes against the ray.

    auto tree_end = boxes.size();
    int node_index = 0;
    while (node_index < tree_end) {
        if (!boxes[node_index].hit(r.r, interval{minRayDist, closestHit})) {
            if (node_ends[node_index] <= node_index) std::unreachable();
            node_index = node_ends[node_index];
            continue;
        }

        auto const n = nodes[node_index];

        if (n.objectIndex != -1) {
            auto span = std::span{geoms + n.objectIndex, size_t(n.objectCount)};
            std::tie(result, closestHit) = hitSpan(span, r, result, closestHit);
        }
        // the next node to process is adjacent to the current one:
        // Either it's the left node from this node, or the right subtree from
        // the parent of a leaf node.
        // If the current node is a parent node, then the directly
        // adjacent node is the root of the left subtree, since it's the first
        // node that we build when recurring in buildBVHNode().
        //
        // If the current node is a leaf node, then `node_index + 1` is the end
        // of the current subtree that we're visiting. Since nodes are stored in
        // pre-order, the right subtree is pushed directly after the left
        // subtree from a given parent. This means that `node_index + 1` in this
        // case is the right node from the previous parent.
        node_index += 1;
    }

    return {result, closestHit};
}
