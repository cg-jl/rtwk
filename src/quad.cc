#include "quad.h"

#include <tracy/Tracy.hpp>

#include "trace_colors.h"

// @perf length(u) == length(v)?
uvs quad::getUVs(point3 intersection) const {
    // I have to make a base change, from [x y z] to [n u v], then extract the u
    // and the v

    vec3 pq = intersection - Q;
    auto v_squared = v.length_squared();
    auto u_squared = u.length_squared();
    auto dot_uq = dot(u, pq);
    auto dot_vq = dot(v, pq);
    // (a×b)⋅(c×d) = (a⋅c)(b⋅d) - (a⋅d)(b⋅c)
    uvs uv;
    uv.u = dot_uq / u_squared;
    uv.v = dot_vq / v_squared;
    return uv;
}

static bool is_interior(double a, double b) {
    static constexpr interval unit_interval = interval(0, 1);
    // Given the hit point in plane coordinates, return false if it is
    // outside the primitive, otherwise set the hit record UV coordinates
    // and return true.

    return unit_interval.contains(a) & unit_interval.contains(b);
}

// @perf length(u) == length(v)?
// @perf dot(u,v ) == 0.
double quad::hit(ray const r) const {
    ZoneNamedN(_tracy, "quad hit", filters::hit);
    auto n = cross(u, v);
    auto normal = unit_vector(n);
    // n.Q = (uxv).Q =(triple product expansion) = u.(vxQ) = v.(uxQ)
    // trying to use dot(u,v) = 0 here?
    auto D = dot(normal, Q);
    auto denom = dot(normal, r.dir);

    // No hit if the ray is parallel to the plane.
    if (fabs(denom) < 1e-8) return 0;

    // Return false if the hit point parameter t is outside the ray
    // interval.
    auto t = (D - dot(normal, r.orig)) / denom;
    // Determine the hit point lies within the planar shape using its plane
    // coordinates.
    auto intersection = r.at(t);
    // @perf dot(u,v) == 0. Try to find a relationship with `t`.
    auto uv = getUVs(intersection);

    if (!is_interior(uv.u, uv.v)) return {};

    // Ray hits the 2D shape; set the rest of the hit record and return
    // true.
    return t;
}

vec3 quad::getNormal() const { return unit_vector(cross(u, v)); }

// @perf length(u) == length(v)?
// @perf dot(u,v ) == 0.
quad quad::applyTransform(quad q, transform tf) noexcept {
    auto oldQ = q.Q;
    q.Q = tf.applyForward(q.Q);
    q.u = tf.applyForward(oldQ + q.u) - q.Q;
    q.v = tf.applyForward(oldQ + q.v) - q.Q;
    return q;
}
