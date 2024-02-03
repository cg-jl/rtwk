#pragma once
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
#include <memory>
#include <span>

#include "aabb.h"
#include "hittable.h"
#include "interval.h"
#include "rtweekend.h"

struct bvh_node : public hittable {
   public:
    bvh_node(shared_ptr<hittable> left, shared_ptr<hittable> right)
        : hittable(aabb(left->bbox, right->bbox)),
          left(std::move(left)),
          right(std::move(right)) {}

    static shared_ptr<hittable> split(std::span<shared_ptr<hittable>> objects) {
        int axis = random_int(0, 2);
        auto comparator = (axis == 0)   ? box_x_compare
                          : (axis == 1) ? box_y_compare
                                        : box_z_compare;

        size_t object_span = objects.size();

        if (object_span == 1) {
            return objects[0];
        } else if (object_span == 2) {
            if (comparator(objects[0], objects[1])) {
                return make_shared<bvh_node>(objects[0], objects[1]);
            } else {
                return make_shared<bvh_node>(objects[1], objects[0]);
            }
        } else {
            std::sort(objects.begin(), objects.begin() + objects.size(),
                      comparator);

            auto mid = object_span / 2;
            auto left = split(objects.subspan(0, mid));
            auto right = split(objects.subspan(mid));

            return make_shared<bvh_node>(std::move(left), std::move(right));
        }
    }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        if (!bbox.hit(r, ray_t)) return false;

        bool hit_left = left->hit(r, ray_t, rec);
        bool hit_right = right->hit(r, ray_t, rec);

        return hit_left || hit_right;
    }

   private:
    shared_ptr<hittable> left;
    shared_ptr<hittable> right;

    static bool box_compare(shared_ptr<hittable> const a,
                            shared_ptr<hittable> const b, int axis_index) {
        return a->bbox.axis(axis_index).min < b->bbox.axis(axis_index).min;
    }

    static bool box_x_compare(shared_ptr<hittable> const a,
                              shared_ptr<hittable> const b) {
        return box_compare(a, b, 0);
    }

    static bool box_y_compare(shared_ptr<hittable> const a,
                              shared_ptr<hittable> const b) {
        return box_compare(a, b, 1);
    }

    static bool box_z_compare(shared_ptr<hittable> const a,
                              shared_ptr<hittable> const b) {
        return box_compare(a, b, 2);
    }
};
