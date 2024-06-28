#include "bvh.h"

#include <cassert>
#include <tracy/Tracy.hpp>

#include "trace_colors.h"

namespace bvh {

// TODO: @memory move all objects here? Last time it was good?

[[clang::noinline]] static bvh_node buildBVHNode(hittable *objects, int start,
                                                 int end, int depth = 0) {
    static constexpr int minObjectsInTree = 6;
    static_assert(minObjectsInTree > 1,
                  "Min objects in tree must be at least 2, otherwise it will "
                  "stack overflow");

    assert(end > start);
    // Build the bounding box of the span of source objects.
    aabb bbox = empty_aabb;
    for (int i = start; i < end; ++i)
        bbox = aabb(bbox, objects[i].geom->bounding_box());
    auto object_span = end - start;

    if (object_span == 1) {
        return bvh_node{bbox, start, end, nullptr, nullptr};
    }

    int axis = bbox.longest_axis();

    // split in half along longest axis.
    auto partitionPoint = bbox.axis_interval(axis).midPoint();

    auto it = std::partition(
        objects + start, objects + end,
        [axis, partitionPoint](hittable const &a) {
            auto a_axis_interval = a.geom->bounding_box().axis_interval(axis);
            return a_axis_interval.midPoint() < partitionPoint;
        });

    auto midIndex = std::distance(objects, it);

    auto left = buildBVHNode(objects, start, midIndex, depth + 1);
    auto right = buildBVHNode(objects, midIndex, end, depth + 1);

    return bvh_node{bbox, start, end, new bvh_node(left), new bvh_node(right)};
}

[[clang::noinline]] static hittable const *hitNode(ray const &r, interval ray_t,
                                                   double &closestHit,
                                                   bvh_node const &n,
                                                   hittable const *objects) {
    if (!n.bbox.hit(r, ray_t)) return nullptr;
    if (n.left == nullptr) {
        // TODO: With something like SAH (Surface Area Heuristic), we should see
        // improving times by hitting multiple in one go. Since I'm tracing each
        // kind of intersection, it will be interesting to bake statistics of
        // each object and use that as timing reference.
        return hitSpan(std::span{objects + n.objectsStart,
                                 size_t(n.objectsEnd - n.objectsStart)},
                       r, ray_t, closestHit);
    }

    auto hit_left = hitNode(r, ray_t, closestHit, *n.left, objects);
    auto hit_right =
        hitNode(r, interval(ray_t.min, hit_left ? closestHit : ray_t.max),
                closestHit, *n.right, objects);
    return hit_right ?: hit_left;
}

}  // namespace bvh

bvh_tree::bvh_tree(std::span<hittable> objects)
    : root(bvh::buildBVHNode(&objects[0], 0, objects.size())),
      objects(objects.data()) {}

hittable const *bvh_tree::hitSelect(ray const &r, interval ray_t,
                                    double &closestHit) const {
    // deactivate this zone for now.
    ZoneNamedN(zone, "bvh_tree hit", filters::hit);
    return bvh::hitNode(r, ray_t, closestHit, root, objects);
}
