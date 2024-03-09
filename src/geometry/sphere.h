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

#include <cmath>
#include <utility>

#include "hittable.h"
#include "rtweekend.h"

// NOTE: With a different model for hittable collections, I could make every
// entity have a:
// - geometry component
// - light model
// - color model

// NOTE: how about saving non-moving spheres differently in memory?
// These moving spheres are just 'instanced' spheres with a translation that is
// time-dpendent.
// NOTE: This instancing could be done before each 'sample', as in *really*
// taking a shot of the full scene and averaging it?
struct sphere final {
   public:
    point3 center;
    // NOTE: padding is being applied here.
    // Of 4 bytes, for some reason.
    float radius;

    [[nodiscard]] aabb boundingBox() const& {
        auto rvec = vec3(radius, radius, radius);
        return {center + rvec, center - rvec};
    }

    // Stationary Sphere
    sphere(point3 center, float radius) : center(center), radius(radius) {}

    bool hit(ray const& r, hit_record::geometry& rec,
             interval& ray_t) const noexcept {
        vec3 oc = r.origin - center;
        auto a = r.direction.length_squared();
        auto half_b = dot(oc, r.direction);
        auto c = oc.length_squared() - radius * radius;

        auto discriminant = half_b * half_b - a * c;
        if (discriminant < 0) return false;

        // Find the nearest root that lies in the acceptable range.
        auto sqrtd = sqrtf(discriminant);
        auto root = (-half_b - sqrtd) / a;
        if (!ray_t.surrounds(root)) {
            root = (-half_b + sqrtd) / a;
            if (!ray_t.surrounds(root)) return false;
        }

        ray_t.max = root;
        rec.p = r.at(ray_t.max);
        vec3 outward_normal = (rec.p - center) / radius;
        rec.normal = outward_normal;

        return true;
    }


    static void getUVs(hit_record::geometry const& res, float& u, float& v) {
        // p: a given point on the sphere of radius one, centered at the origin.
        // u: returned value [0,1] of angle around the Y axis from X=-1.
        // v: returned value [0,1] of angle from Y=-1 to Y=+1.
        //     <1 0 0> yields <0.50 0.50>       <-1  0  0> yields <0.00 0.50>
        //     <0 1 0> yields <0.50 1.00>       < 0 -1  0> yields <0.50 0.00>
        //     <0 0 1> yields <0.25 0.50>       < 0  0 -1> yields <0.75 0.50>

        auto theta = std::acos(-res.normal.y());
        auto phi = std::atan2(-res.normal.z(), res.normal.x()) + pi;

        u = phi / (2 * pi);
        v = theta / pi;
    }
};

static_assert(is_geometry<sphere>);
