#include <algorithm>
#include <bvh.h>
#include <functional>
#include <hittable.h>

#include <cassert>
#include <memory>
#include <ranges>
#include <tracy/Tracy.hpp>
#include <unordered_set>
#include <utility>

#include "geometry.h"
#include "interval.h"
#include "rtweekend.h"
#include "trace_colors.h"

namespace bvh {

// @perf currently bounding boxes occupy a cacheline each (48 bytes). Aligning
// them to 32 bytes means they will occupy an entire cacheline, wanted or not.
// We want the pointers to be aligned to 32 bytes as well, so might need to use
// a vector that has the memory aligned to use the _mm256_stream_load trick.

// TODO: @perf std::vector uses `new`, which aligns the pointer to the required
// aligment, according to <https://stackoverflow.com/a/3658666>.

static int addNode(tree_builder &bld, aabb box, bvh_node node)
{
    bld.boxes.emplace_back(std::forward<aabb &&>(box));
    bld.nodes.emplace_back(node);
    bld.node_ends.emplace_back(bld.nodes.size());
    return int(bld.nodes.size() - 1);
}

[[clang::noinline]] static int buildBVHNode(tree_builder &bld, int start,
    int end, int depth = 0)
{
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
        return addNode(bld, std::move(bbox), bvh_node { start, 1 });
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
        return addNode(bld, std::move(bbox), bvh_node { start, (end - start) });
    }

    auto parent = addNode(bld, std::move(bbox), bvh_node { -1, 0 });

    buildBVHNode(bld, start, midIndex, depth + 1);
    buildBVHNode(bld, midIndex, end, depth + 1);

    bld.nodes[parent].objectIndex = -1;
    bld.node_ends[parent] = bld.nodes.size();

    return parent;
}

} // namespace bvh

void bvh::tree_builder::finish(size_t start) noexcept
{
    bvh::buildBVHNode(*this, start, geoms.size());
}

void bvh::tree_builder::prepareForRender() noexcept
{
    // leaf nodes point at tree root locations.
    // @perf could write the tree root locations elsewhere and then use it here :]
    std::unordered_set<uint32> root_locs;

    for (uint32 i = 0; i < boxes.size(); ++i) {
        if (nodes[i].objectIndex != -1) {
            root_locs.insert(node_ends[i]);
        }
    }

    std::vector<uint32> sorted_locs;

    std::copy(std::begin(root_locs), std::end(root_locs), std::back_inserter(sorted_locs));

    std::sort(sorted_locs.begin(), sorted_locs.end());

    using std::ranges::ref_view;
    using std::ranges::subrange;
    using std::views::zip;

    auto new_indices = std::make_unique<uint32[]>(boxes.size());
    std::generate(new_indices.get(), new_indices.get() + boxes.size(), [i = uint32(0)]() mutable { return i++; });

    auto start = 0uz;
    for (auto const end : sorted_locs) {

        auto leaf_nodes_begin = end;
        for (;;) {
            auto next_node_pos = std::distance(
                nodes.begin(),
                std::find_if(nodes.begin() + start, nodes.begin() + leaf_nodes_begin, [](auto const &n) {
                    return n.objectIndex != -1;
                }));

            if (next_node_pos == leaf_nodes_begin) {
                break;
            }

            std::rotate(nodes.begin() + start, nodes.begin() + next_node_pos, nodes.begin() + leaf_nodes_begin);
            std::rotate(boxes.begin() + start, boxes.begin() + next_node_pos, boxes.begin() + leaf_nodes_begin);
            std::rotate(node_ends.begin() + start, node_ends.begin() + next_node_pos, node_ends.begin() + leaf_nodes_begin);
            std::rotate(new_indices.get() + start, new_indices.get() + next_node_pos, new_indices.get() + leaf_nodes_begin);
            --leaf_nodes_begin;
        }

        std::reverse(nodes.begin() + leaf_nodes_begin, nodes.begin() + end);
        std::reverse(boxes.begin() + leaf_nodes_begin, boxes.begin() + end);
        std::reverse(node_ends.begin() + leaf_nodes_begin, node_ends.begin() + end);
        std::reverse(new_indices.get() + leaf_nodes_begin, new_indices.get() + end);

        start = end;
    }

    for (auto i = 0uz; i < boxes.size(); ++i) {
        node_ends[i] = new_indices[node_ends[i]];
    }
}

void bvh::tree::hit(ray_buffer rays, uint32 const len, hit_span_buf results, bvh::Hit_Buffer buffer, std::function<void(uint32, uint32)> const &swap_rays) const noexcept
{
    ZoneNamedN(zone, "bvh_tree hit", filters::treeHit);
    std::fill(results.zip(len).begin(), results.zip(len).end(), std::pair { nullptr, infinity });
    std::fill(buffer.node_indices, buffer.node_indices + len, 0);

    auto swap = [&](auto i, auto j) {
        std::swap(buffer.node_indices[i], buffer.node_indices[j]);
        swap_rays(i, j);
    };

    auto remaining = len;
    auto const tree_end = boxes.size();
    auto constexpr zero = uint32(0);
    while (remaining) {
        // @perf may want to select the node index differently
        auto const node_index = buffer.node_indices[0];
        auto const rays_for_node = partition(uint32(1), remaining, swap, [&](auto const i) { return buffer.node_indices[i] == node_index; });

        boxes[node_index].traverse(rays.rays, rays_for_node, buffer.bb_traverse, buffer.t);

        // @perf only distances are required here.
        std::transform(buffer.t.zip(rays_for_node).begin(), buffer.t.zip(rays_for_node).end(), results.zip(rays_for_node).begin(), buffer.t.zip(rays_for_node).begin(), [&](auto t, auto const &res) {
            auto const &closestHit = std::get<float &>(res);
            auto &[tmin, tmax] = t;
            tmax = std::min(tmax, closestHit);
            // @perf may be specialized to its own loop.
            tmin = std::max(tmin, minRayDist);
            return t;
        });

        auto swap_with_t = [&](auto i, auto j) {
            swap(i, j);
            buffer.t.swap(i, j);
        };

        // NOTE: Thanks to this partition, we know know that the index selection
        // will select node_index + 1 when there's at least one ray that is selected like that.
        auto const empty_begin = partition(zero, rays_for_node, swap_with_t, [&](auto const i) { return !buffer.t[i].isEmpty(); });

        std::fill(buffer.node_indices + empty_begin, buffer.node_indices + rays_for_node, node_ends[node_index]);

        // For rays that hit the node:
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
        std::fill(buffer.node_indices, buffer.node_indices + empty_begin, node_index + 1);
        auto const n = nodes[node_index];

        // @perf to evade this branch, I have to someway store which rays go for each node,
        // and then, for each node, go fetch the rays (partition), then fetch the node
        // and do the checks.
        // Or, the first thing I could do is prepare the BVH s.t the nodes that contain indices
        // are at the end of the array. For that I'd need to also update the indices for those nodes in the
        // tree references. But that would let me skip this branch altogether by dividing the loop in two.
        // Problem with this, is that leaf nodes point to the next tree as their next node.
        // What if I acknowledge again that I have multiple trees and restart from there?
        // After all, the spans are going to be different.
        if (n.objectIndex != -1) {

            auto span = std::span { geoms + n.objectIndex, size_t(n.objectCount) };

            hitSpan(span, buffer.bb_hit, rays, empty_begin, results, buffer.cmp_res);
        }
        remaining = partition(uint32(0), remaining, swap, [&](auto const i) {
            return buffer.node_indices[i] < tree_end;
        });
    }
}
