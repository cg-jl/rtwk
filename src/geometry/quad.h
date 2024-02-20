#pragma once
//==============================================================================================
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public
// Domain Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include <utility>

#include "collection/hittable_list.h"  // for box(). NOTE: maybe this should be in main.cc, or some other builder module
#include "hittable.h"
#include "material.h"
#include "rtweekend.h"
#include "vec3.h"

// NOTE: this one occupies 1 whole cacheline as of now.

struct quad final : public hittable {
   public:
    //  u, v are accessed first, to calculate n
    vec3 u, v;
    // Q is accessed second, after a test
    point3 Q;

    //  mat is accessed last, after all tests
    material mat;
    texture const* tex;

    quad() = default;

    [[nodiscard]] aabb bounding_box() const& override {
        return aabb(Q, Q + u + v).pad();
    }

    quad(point3 Q, vec3 u, vec3 v, material m, texture const* tex)
        : Q(Q), u(u), v(v), mat(std::move(m)), tex(tex) {}

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        auto n = cross(u, v);
        auto inv_sqrtn = 1 / n.length();
        auto normal = n * inv_sqrtn;

        auto denom = dot(normal, r.direction);

        // No hit if the ray is parallel to the plane.
        if (fabs(denom) < 1e-8) return false;

        auto D = dot(normal, Q);
        // Return false if the hit point parameter t is outside the ray
        // interval.
        auto t = (D - dot(normal, r.origin)) / denom;
        if (!ray_t.contains(t)) return false;

        auto w = normal * inv_sqrtn;

        // Determine the hit point lies within the planar shape using its plane
        // coordinates.
        auto intersection = r.at(t);
        vec3 planar_hitpt_vector = intersection - Q;
        auto alpha = dot(w, cross(planar_hitpt_vector, v));
        auto beta = dot(w, cross(u, planar_hitpt_vector));

        if (!is_interior(alpha, beta, rec)) return false;

        // Ray hits the 2D shape; set the rest of the hit record and return
        // true.
        ray_t.max = t;
        rec.p = intersection;
        rec.mat = mat;
        rec.tex = tex;
        rec.normal = normal;

        return true;
    }

    static bool is_interior(float a, float b, hit_record& rec) {
        // Given the hit point in plane coordinates, return false if it is
        // outside the primitive, otherwise set the hit record UV coordinates
        // and return true.

        if ((a < 0) || (1 < a) || (b < 0) || (1 < b)) return false;

        rec.u = a;
        rec.v = b;
        return true;
    }
};

