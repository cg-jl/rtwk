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

#include "vec3.h"

// NOTE: this one occupies 1 whole cacheline as of now.

struct quad final {
   public:
    //  u, v are accessed first, to calculate n
    vec3 u, v;
    // Q is accessed second, after a test
    point3 Q;

    quad() = default;

    [[nodiscard]] aabb boundingBox() const& { return aabb(Q, Q + u + v).pad(); }

    quad(point3 Q, vec3 u, vec3 v) : Q(Q), u(u), v(v) {}

    bool hit(ray const& r, hit_record::geometry& rec, interval& ray_t) const {
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

        // Determine the hit point lies within the planar shape using its plane
        // coordinates.
        auto intersection = r.at(t);
        float alpha, beta;
        calcUVs(normal, inv_sqrtn, intersection, alpha, beta);

        if ((alpha < 0) || (1 < alpha) || (beta < 0) || (1 < beta))
            return false;

        // Ray hits the 2D shape; set the rest of the hit record and return
        // true.
        ray_t.max = t;
        rec.p = intersection;
        rec.normal = normal;

        return true;
    }

    void calcUVs(vec3 normal, float inv_sqrtn, point3 hitp, float& u,
                 float& v) const noexcept {
        auto w = normal * inv_sqrtn;
        auto planar_hitpt_vector = hitp - Q;

        u = dot(w, cross(planar_hitpt_vector, this->v));
        v = dot(w, cross(this->u, planar_hitpt_vector));
    }

    void getUVs(hit_record::geometry const& res, float& u, float& v) const {
        auto n = cross(this->u, this->v);
        auto inv_sqrtn = 1 / n.length();
        calcUVs(res.normal, inv_sqrtn, res.p, u, v);
    }
};

static_assert(is_geometry<quad>);
