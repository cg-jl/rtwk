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

struct bvh_node final : public hittable {
   public:
    aabb bbox;
    shared_ptr<hittable> left;
    shared_ptr<hittable> right;

    bvh_node(shared_ptr<hittable> left, shared_ptr<hittable> right)
        : bbox(aabb(left->bounding_box(), right->bounding_box())),
          left(std::move(left)),
          right(std::move(right)) {}

    aabb bounding_box() const& override { return bbox; }

    static shared_ptr<hittable> split(std::span<shared_ptr<hittable>> objects) {
        assert(objects.size() != 0 && "not splitting empty objects!");
        // No need to split
        if (objects.size() == 1) {
            return objects[0];
        }

        // TODO: estimate area for split and avoid splitting if it's more than
        // traversing all.

        // NOTE: can we optimize intersection resources (i.e how much data
        // we need) by knowing what axis is each node split into?
        int axis = random_int(0, 2);
        auto comparator = [axis](shared_ptr<hittable> const& a,
                                 shared_ptr<hittable> const& b) {
            return box_compare(a, b, axis);
        };
        std::sort(objects.begin(), objects.end(), comparator);

        auto mid = objects.size() / 2;
        auto left = split(objects.subspan(0, mid));
        auto right = split(objects.subspan(mid));

        return make_shared<bvh_node>(std::move(left), std::move(right));
    }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        if (!bbox.hit(r, ray_t)) return false;

        bool hit_left = left->hit(r, ray_t, rec);
        bool hit_right = right->hit(r, ray_t, rec);

        return hit_left || hit_right;
    }

    static bool box_compare(shared_ptr<hittable> const a,
                            shared_ptr<hittable> const b, int axis_index) {
        return a->bounding_box().axis(axis_index).min <
               b->bounding_box().axis(axis_index).min;
    }
};
