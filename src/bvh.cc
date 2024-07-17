#include <bvh.h>
#include <hittable.h>

#include <cassert>
#include <tracy/Tracy.hpp>
#include <vector>

#include "trace_colors.h"

namespace bvh {

static std::vector<bvh_node> nodes;
static int addNode(bvh_node node) {
    nodes.emplace_back(node);
    return int(nodes.size() - 1);
}

[[clang::noinline]] static bvh_node buildBVHNode(geometry const **objects,
                                                 int start, int end,
                                                 int depth = 0) {
    static constexpr int minObjectsInTree = 6;
    static_assert(minObjectsInTree > 1,
                  "Min objects in tree must be at least 2, otherwise it will "
                  "stack overflow");

    assert(end > start);
    // Build the bounding box of the span of source objects.
    aabb bbox = empty_aabb;
    for (int i = start; i < end; ++i)
        bbox = aabb(bbox, objects[i]->bounding_box());
    auto object_span = end - start;

    if (object_span == 1) {
        return bvh_node{bbox, start, -1};
    }

    int axis = bbox.longest_axis();

    // split in half along longest axis.
    auto partitionPoint = bbox.axis_interval(axis).midPoint();

    auto it = std::partition(
        objects + start, objects + end,
        [axis, partitionPoint](geometry const *a) {
            auto a_axis_interval = a->bounding_box().axis_interval(axis);
            return a_axis_interval.midPoint() <= partitionPoint;
        });

    auto midIndex = std::distance(objects, it);
    if (midIndex == start || midIndex == end) {
        // cannot split these objects
        return bvh_node{bbox, start, -(end - start)};
    }

    auto left = buildBVHNode(objects, start, midIndex, depth + 1);
    auto right = buildBVHNode(objects, midIndex, end, depth + 1);
    auto leftIndex = addNode(left);
    [[maybe_unused]]  // NOTE: used in assert.
    auto rightIndex = addNode(right);
    assert(rightIndex == leftIndex + 1);

    return bvh_node{bbox, start, leftIndex};
}

[[clang::noinline]] static geometry const *hitNode(
    ray const &r, double &closestHit, bvh_node const &n,
    geometry const *const *objects) {
    if (!n.bbox.hit(r, interval{minRayDist, closestHit})) return nullptr;
    if (n.left < 0) {
        // TODO: With something like SAH (Surface Area Heuristic), we should see
        // improving times by hitting multiple in one go. Since I'm tracing each
        // kind of intersection, it will be interesting to bake statistics of
        // each object and use that as timing reference.
        auto span = std::span{objects + n.objectIndex, size_t(-n.left)};
        return hitSpan(span, r, closestHit);
    }

    auto hit_left = hitNode(r, closestHit, nodes[n.left], objects);
    auto hit_right = hitNode(r, closestHit, nodes[n.left + 1], objects);
    return hit_right ?: hit_left;
}

}  // namespace bvh

bvh_tree::bvh_tree(std::span<geometry const *> objects)
    : root(bvh::buildBVHNode(&objects[0], 0, objects.size())),
      objects(objects.data()) {}

geometry const *bvh_tree::hitSelect(ray const &r, double &closestHit) const {
    // deactivate this zone for now.
    ZoneNamedN(zone, "bvh_tree hit", filters::treeHit);
    return bvh::hitNode(r, closestHit, root, objects);
}
