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
// To do this, we'll do a second hit to the same geometry once we know it's a
// hit. We'll pass the ray through the object if the second hit doesn't check
// out.
template <is_geometry T>
struct constant_medium final {
   public:
    T boundary;
    float neg_inv_density;
    uint32_t color_id;

    constant_medium(T b, float d, color col, tex_storage& texes)
        : boundary(std::move(b)),
          neg_inv_density(-1 / d),
          color_id(texes.solid(col).id) {}

    [[nodiscard]] aabb boundingBox() const& { return boundary.boundingBox(); }

    bool hit(ray const& r, interval& ray_t, hit_record& rec,
             float _time) const {
        hit_record::geometry discarded_rec;

        interval hit1(interval::universe);
        if (!boundary.hit(r, discarded_rec, hit1)) return false;
        auto first_hit = std::max(hit1.max, ray_t.min);

        // hit into the other side
        interval hit2(first_hit + 0.001f, infinity);
        if (!boundary.hit(r, discarded_rec, hit2)) return false;
        auto second_hit = std::min(hit2.max, ray_t.max);

        if (first_hit >= second_hit) return false;

        auto distance_inside_boundary = second_hit - first_hit;
        // using a flat wave model of EM waves:
        // E(f) = E_0(f) exp(-alpha(f) * d) exp(-j beta(f) * d)
        // f is frequency
        // beta = 'dephase' factor (rad / m)
        // -alpha = 1 / neg_inv_density = 'dampen' factor (np / m)
        // - choose a dampen factor (0-1)
        // - get the distance by multiplying by -1 / alpha = d
        // NOTE: we're assuming it has a constant dephase factor at any
        //
        // if (hit_distance > distance_inside_boundary) return false;
        // frequency (color) which isn't exactly accurate :P
        // But, since we're path tracing in reverse, we don't know the 'set' of
        // frequencies that will pass through
        auto hit_distance = neg_inv_density * logf(random_float() + 0.01f);

        // ray came from outside the material and passed through.
        // Assuming it's a non-magnetic bad conductor ~ β = 1/c * j2πf√ϵr√(1 -
        // jtanδ) γ² = -ω²/c^2 * ϵᵣ(1 - jtanδ) = α^2 - β^2 + 2jαβ
        if (hit_distance > distance_inside_boundary) return false;

        ray_t.max = first_hit + hit_distance;
        rec.geom.p = r.at(ray_t.max);

        rec.mat = material::isotropic();
        rec.tex = texture{texture::kind::solid, color_id};

        return true;
    }
};
