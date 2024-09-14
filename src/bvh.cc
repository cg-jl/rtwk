#include <bvh.h>
#include <hittable.h>

#include <cassert>
#include <tracy/Tracy.hpp>
#include <vector>

#include "trace_colors.h"

namespace bvh {

// the node layout is <parent> <left> ... <right> ...
// (pre-order). This allows us to traverse the tree as if it were an array,
// skipping forwards to the end of a subtree when required.

// @perf the data structuers here need a bit of checking latencies.
// separating AABBs is definitely the right move, but separating bvh_node to be
// just a range and storing the ends elsewhere, when we know the node end index
// for a leaf node, is kind of a @waste. We still have to make the memory hit in
// any of the branches, so we may as well fetch the 8 bytes early.

struct bvh_node {
    int objectIndex;  // if == -1, then the node is a parent and we should not
                      // use this struct. otherwise, this is the range for the
                      // objects that the leaf node represents.
    int objectCount;
};

static std::vector<int> node_ends;

// @perf currently bounding boxes occupy a cacheline each (48 bytes). Aligning
// them to 32 bytes means they will occupy an entire cacheline, wanted or not.
// We want the pointers to be aligned to 32 bytes as well, so might need to use
// a vector that has the memory aligned to use the _mm256_stream_load trick.

// TODO: @perf std::vector uses `new`, which aligns the pointer to the required
// aligment, according to <https://stackoverflow.com/a/3658666>.

static std::vector<aabb> boxes;
static std::vector<bvh_node> nodes;
static int addNode(aabb box, bvh_node node) {
    boxes.emplace_back(std::forward<aabb &&>(box));
    nodes.emplace_back(node);
    node_ends.emplace_back(nodes.size());
    return int(nodes.size() - 1);
}

static int maxDepth = 0;

[[clang::noinline]] static int buildBVHNode(geometry *objects, int start,
                                            int end, int depth = 0) {
    maxDepth = std::max(depth, maxDepth);
    static constexpr int minObjectsInTree = 6;
    static_assert(minObjectsInTree > 1,
                  "Min objects in tree must be at least 2, otherwise it will "
                  "stack overflow");

    assert(end > start);
    // Build the bounding box of the span of source objects.
    aabb bbox = empty_aabb;
    for (int i = start; i < end; ++i)
        bbox = aabb(bbox, objects[i].bounding_box());
    auto object_span = end - start;

    if (object_span == 1) {
        return addNode(std::move(bbox), bvh_node{start, 1});
    }

    int axis = bbox.longest_axis();

    // split in half along longest axis.
    auto partitionPoint = bbox.axis_interval(axis).midPoint();

    auto it = std::partition(
        objects + start, objects + end,
        [axis, partitionPoint](geometry const &a) {
            auto a_axis_interval = a.bounding_box().axis_interval(axis);
            return a_axis_interval.midPoint() <= partitionPoint;
        });

    auto midIndex = std::distance(objects, it);
    if (midIndex == start || midIndex == end) {
        // cannot split these objects
        return addNode(std::move(bbox), bvh_node{start, (end - start)});
    }

    auto parent = addNode(std::move(bbox), bvh_node{-1, 0});

    buildBVHNode(objects, start, midIndex, depth + 1);
    buildBVHNode(objects, midIndex, end, depth + 1);

    nodes[parent].objectIndex = -1;
    node_ends[parent] = nodes.size();

    return parent;
}

static geometry const *hitTree(ray const &r, double &closestHit, int node_index,
                               int node_count, geometry const *objects) {
    geometry const *result = nullptr;

    // test all relevant nodes against the ray.

    auto tree_end = node_ends[node_index];
    while (node_index < tree_end) {
        interval i{minRayDist, closestHit};
        if (!boxes[node_index].traverse(r, i)) {
            node_index = node_ends[node_index];
            continue;
        }

        auto const n = nodes[node_index];

        if (n.objectIndex != -1) {
            auto span =
                std::span{objects + n.objectIndex, size_t(n.objectCount)};
            auto hit = hitSpan(span, r, closestHit);
            result = hit ?: result;
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

    return result;
}

}  // namespace bvh

bvh_tree::bvh_tree(std::span<geometry> objects)
    : root(bvh::buildBVHNode(&objects[0], 0, objects.size())),
      objects(objects.data()) {
    node_count = bvh::nodes.size() - root;
}

geometry const *bvh_tree::hitSelect(ray const &r, double &closestHit) const {
    // deactivate this zone for now.
    ZoneNamedN(zone, "bvh_tree hit", filters::treeHit);
    return bvh::hitTree(r, closestHit, root, node_count, objects);
}
