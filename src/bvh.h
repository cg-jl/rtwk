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
#include <span>

#include "aabb.h"
#include "hittable.h"
#include "hittable_list.h"
#include "rtweekend.h"

class bvh_node : public hittable {
   public:
    bvh_node(hittable_list &list) : bvh_node(list.objects) {}

    bvh_node(std::span<hittable *> objects) {
        // Build the bounding box of the span of source objects.
        bbox = empty_aabb;
        for (auto ob : objects) bbox = aabb(bbox, ob->bounding_box());

        int axis = bbox.longest_axis();

        size_t object_span = objects.size();

        if (object_span == 1) {
            left = right = objects[0];
        } else if (object_span == 2) {
            left = objects[0];
            right = objects[0 + 1];
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

    bool hit(ray const &r, interval ray_t, hit_record &rec) const override {
        if (!bbox.hit(r, ray_t)) return false;

        bool hit_left = left->hit(r, ray_t, rec);
        bool hit_right = right->hit(
            r, interval(ray_t.min, hit_left ? rec.t : ray_t.max), rec);

        return hit_left || hit_right;
    }

    aabb bounding_box() const override { return bbox; }

   private:
    aabb bbox;
    hittable *left;
    hittable *right;
};

#endif
