#ifndef CONSTANT_MEDIUM_H
#define CONSTANT_MEDIUM_H
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

#include "hittable.h"
#include "material.h"
#include "rtweekend.h"
#include "texture.h"

struct constant_medium final : public hittable {
   public:
    aabb bbox;
    shared_ptr<hittable> boundary;
    double neg_inv_density;
    shared_ptr<material> phase_function;

    constant_medium(shared_ptr<hittable> b, double d, shared_ptr<texture> a)
        : boundary(b),
          neg_inv_density(-1 / d),
          phase_function(make_shared<isotropic>(a)) {}

    constant_medium(shared_ptr<hittable> b, double d, color c)
        : boundary(b),
          neg_inv_density(-1 / d),
          phase_function(make_shared<isotropic>(c)) {}

    aabb bounding_box() const& override { return bbox; }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        hit_record rec1, rec2;

        interval hit1(interval::universe);
        if (!boundary->hit(r, hit1, rec1)) return false;
        auto first_hit = hit1.max;

        interval hit2(first_hit + 0.0001, infinity);
        if (!boundary->hit(r, hit2, rec2)) return false;
        auto second_hit = hit2.max;

        if (first_hit < ray_t.min) first_hit = ray_t.min;
        if (second_hit > ray_t.max) second_hit = ray_t.max;

        if (first_hit >= second_hit) return false;

        if (first_hit < 0) first_hit = 0;

        auto ray_length = r.direction.length();
        auto distance_inside_boundary = (second_hit - first_hit) * ray_length;
        auto hit_distance = neg_inv_density * log(random_double());

        if (hit_distance > distance_inside_boundary) return false;

        ray_t.max = first_hit + hit_distance / ray_length;
        rec.p = r.at(ray_t.max);

        rec.normal = vec3(1, 0, 0);  // arbitrary
        rec.mat = phase_function.get();

        return true;
    }
};

#endif
