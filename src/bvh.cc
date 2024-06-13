#include "bvh.h"

namespace bvh {

bvh_node buildBVHNode(hittable **objects, int start, int end, int depth) {
    static constexpr int maxTreeDepth = 10;

    assert(end > start);
    // Build the bounding box of the span of source objects.
    aabb bbox = empty_aabb;
    for (int i = start; i < end; ++i)
        bbox = aabb(bbox, objects[i]->geom->bounding_box());
    auto object_span = end - start;


    int axis = bbox.longest_axis();

    if (object_span == 1) {
        auto node = new bvh_node{};
        node->objectsStart = start;
        node->objectsEnd = end;
        return node;
    } else {
        // split in half along longest axis.
        auto partitionPoint = bbox.axis_interval(axis).midPoint();

        auto it = std::partition(
            objects + start, objects + end,
            [axis, partitionPoint](hittable const *a) {
                auto a_axis_interval =
                    a->geom->bounding_box().axis_interval(axis);
                return a_axis_interval.midPoint() < partitionPoint;
            });

        auto midIndex = std::distance(objects, it);

        auto left = buildBVHNode(objects, start, midIndex, depth + 1);
        auto right = buildBVHNode(objects, midIndex, end, depth + 1);

    }
    return bvh_node{bbox, start, end, new bvh_node(left), new bvh_node(right)};
}
}  // namespace bvh

bvh_tree::bvh_tree(std::span<hittable *> objects)
    : root(bvh::buildBVHNode(&objects[0], 0, objects.size())),
      objects(objects.data()) {}
