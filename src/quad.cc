#include "quad.h"

#include <tracy/Tracy.hpp>

void quad::getUVs(uvs &uv, point3 intersection, double _time) const {
    vec3 pq = intersection - Q;
    auto v_squared = v.length_squared();
    auto u_squared = u.length_squared();
    auto dot_uv = dot(u, v);
    auto cross_uv_lensq = u_squared * v_squared - dot_uv * dot_uv;
    auto dot_uq = dot(u, pq);
    auto dot_vq = dot(v, pq);
    // (a×b)⋅(c×d) = (a⋅c)(b⋅d) - (a⋅d)(b⋅c)
    uv.u = (dot_uq * v_squared - dot_uv * dot_vq) / cross_uv_lensq;
    uv.v = (u_squared * dot_vq - dot_uq * dot_uv) / cross_uv_lensq;
}

static bool is_interior(double a, double b) {
    static constexpr interval unit_interval = interval(0, 1);
    // Given the hit point in plane coordinates, return false if it is
    // outside the primitive, otherwise set the hit record UV coordinates
    // and return true.

    return unit_interval.contains(a) && unit_interval.contains(b);
}

bool quad::hit(ray const &r, interval ray_t, geometry_record &rec) const {
    ZoneScopedN("quad hit");
    auto n = cross(u, v);
    auto normal = unit_vector(n);
    auto D = dot(normal, Q);
    auto denom = dot(normal, r.dir);

    // No hit if the ray is parallel to the plane.
    if (fabs(denom) < 1e-8) return false;

    // Return false if the hit point parameter t is outside the ray
    // interval.
    auto t = (D - dot(normal, r.orig)) / denom;
    if (!ray_t.contains(t)) return false;

    // Determine the hit point lies within the planar shape using its plane
    // coordinates.
    auto intersection = r.at(t);
    uvs uv;
    // NOTE: time argument is not needed.
    getUVs(uv, intersection, 0);

    if (!is_interior(uv.u, uv.v)) return false;

    // Ray hits the 2D shape; set the rest of the hit record and return
    // true.
    rec.t = t;

    return true;
}

vec3 quad::getNormal(point3 const &_intersection, double _time) const {
    return unit_vector(cross(u, v));
}
