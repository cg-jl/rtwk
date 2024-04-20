#ifndef BVH_H
#define BVH_H
//==============================================================================================
// Originally written in 2016 by Peter Shirley <ptrshrl@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public
// Domain Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include <algorithm>
#include <cassert>
#include <span>
#include <tracy/Tracy.hpp>

#include "aabb.h"
#include "hittable.h"
#include "rtweekend.h"

class bvh_node {
   public:
    bvh_node(std::span<hittable *> objects) {
        // Build the bounding box of the span of source objects.
        bbox = empty_aabb;
        for (auto ob : objects) bbox = aabb(bbox, ob->bounding_box());

        int axis = bbox.longest_axis();

        size_t object_span = objects.size();

        if (object_span == 1) {
            left = right = nullptr;
        } else if (object_span == 2) {
            left = nullptr;
            right = nullptr;
        } else {
            std::sort(objects.begin(), objects.end(),
                      [axis](hittable const *a, hittable const *b) {
                          auto a_axis_interval =
                              a->bounding_box().axis_interval(axis);
                          auto b_axis_interval =
                              b->bounding_box().axis_interval(axis);
                          return a_axis_interval.min < b_axis_interval.min;
                      });

            left = new bvh_node(objects.subspan(0, object_span / 2));
            right = new bvh_node(objects.subspan(object_span / 2));
        }
    }

    aabb bbox;
    bvh_node *left;
    bvh_node *right;
};

namespace bvh {
static hittable const *hitNode(ray const &r, interval ray_t, hit_record &rec,
                               bvh_node const *n,
                               std::span<hittable *> objects) {
    if (n == nullptr) {
        // TODO: With something like SAH (Surface Area Heuristic), we should see
        // improving times by hitting multiple in one go. Since I'm tracing each
        // kind of intersection, it will be interesting to bake statistics of
        // each object and use that as timing reference.
        assert(objects.size() == 1);
        return objects[0]->hit(r, ray_t, rec) ? objects[0] : nullptr;
    }
    if (!n->bbox.hit(r, ray_t)) return nullptr;

    auto hit_left =
        hitNode(r, ray_t, rec, n->left, objects.subspan(0, objects.size() / 2));
    auto hit_right =
        hitNode(r, interval(ray_t.min, hit_left ? rec.t : ray_t.max), rec,
                n->right, objects.subspan(objects.size() / 2));
    return hit_left ?: hit_right;
}
}  // namespace bvh

struct bvh_tree {
    bvh_node root;
    std::span<hittable *> objects;

    bvh_tree(std::span<hittable *> objects) : root(objects), objects(objects) {}

    aabb bounding_box() const { return root.bbox; }
    hittable const *hitSelect(ray const &r, interval ray_t,
                              hit_record &rec) const {
        ZoneScopedN("bvh_tree hit");
        return bvh::hitNode(r, ray_t, rec, &root, objects);
    }
};

#endif
