#include "quad.h"

#include <tracy/Tracy.hpp>

#include "hittable.h"
#include "trace_colors.h"

uvs quad::getUVs(point3 intersection) const {

	// I know normal, u, v make an orthonormal basis
    // I have to make a base change, from [x y z] to [n u v], then extract the u and the v

    vec3 pq = intersection - Q;
    auto v_squared = v.length_squared();
    auto u_squared = u.length_squared();
    auto dot_uv = dot(u, v);
    auto cross_uv_lensq = u_squared * v_squared - dot_uv * dot_uv;
    auto dot_uq = dot(u, pq);
    auto dot_vq = dot(v, pq);
    // (a×b)⋅(c×d) = (a⋅c)(b⋅d) - (a⋅d)(b⋅c)
    uvs uv;
    uv.u = (dot_uq * v_squared - dot_uv * dot_vq) / cross_uv_lensq;
    uv.v = (u_squared * dot_vq - dot_uq * dot_uv) / cross_uv_lensq;
    return uv;
}

static bool is_interior(double a, double b) {
    static constexpr interval unit_interval = interval(0, 1);
    // Given the hit point in plane coordinates, return false if it is
    // outside the primitive, otherwise set the hit record UV coordinates
    // and return true.

    return unit_interval.contains(a) && unit_interval.contains(b);
}

bool quad::hit(ray const &r, double &closestHit) const {
    ZoneNamedN(_tracy, "quad hit", filters::hit);
    auto n = cross(u, v);
    auto normal = unit_vector(n);
    auto D = dot(normal, Q);
    auto denom = dot(normal, r.dir);

    // No hit if the ray is parallel to the plane.
    if (fabs(denom) < 1e-8) return false;

    // Return false if the hit point parameter t is outside the ray
    // interval.
    auto t = (D - dot(normal, r.orig)) / denom;
    // Determine the hit point lies within the planar shape using its plane
    // coordinates.
    auto intersection = r.at(t);
    // NOTE: time argument is not needed.
    auto uv = getUVs(intersection);

    if (!is_interior(uv.u, uv.v)) return false;

    // Ray hits the 2D shape; set the rest of the hit record and return
    // true.
    closestHit = t;

    return true;
}

vec3 quad::getNormal(point3 const &_intersection, double _time) const {
    return unit_vector(cross(u, v));
}

// NOTE: @maybe If I end up moving hit to always do both traverses, then this
// should compute the hit and duplicate it in both fields of `intersect`.
bool quad::traverse(ray const &r, interval &intersect) const { return false; }
