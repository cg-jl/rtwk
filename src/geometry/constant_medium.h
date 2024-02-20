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

#include <utility>

#include "hittable.h"
#include "material.h"
#include "rtweekend.h"
#include "texture.h"

// NOTE: this is a material that may also decide texturing, not a geometry.
// To do this, we'll do a second hit to the same geometry once we know it's a hit.
// We'll pass the ray through the object if the second hit doesn't check out.
struct constant_medium final : public hittable {
   public:
    hittable const* boundary;
    float neg_inv_density;
    texture const* tex;

    constant_medium(hittable const* b, float d, texture const* tex)
        : boundary(b), neg_inv_density(-1 / d), tex(tex) {}

    constant_medium(hittable const* b, float d, color c,
                    poly_storage<texture>& tex_storage)
        : constant_medium(b, d, tex_storage.make<solid_color>(c)) {}

    [[nodiscard]] aabb bounding_box() const& override {
        return boundary->bounding_box();
    }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        hit_record rec1, rec2;

        interval hit1(interval::universe);
        if (!boundary->hit(r, hit1, rec1)) return false;
        auto first_hit = hit1.max;

        interval hit2(first_hit + 0.0001f, infinity);
        if (!boundary->hit(r, hit2, rec2)) return false;
        auto second_hit = hit2.max;

        if (first_hit < ray_t.min) first_hit = ray_t.min;
        if (second_hit > ray_t.max) second_hit = ray_t.max;

        if (first_hit >= second_hit) return false;

        if (first_hit < 0) first_hit = 0;

        auto ray_length = r.direction.length();
        auto distance_inside_boundary = (second_hit - first_hit) * ray_length;
        auto hit_distance = neg_inv_density * logf(random_float());

        if (hit_distance > distance_inside_boundary) return false;

        ray_t.max = first_hit + hit_distance / ray_length;
        rec.p = r.at(ray_t.max);

        rec.mat = material::isotropic();
        rec.tex = tex;

        return true;
    }
};
