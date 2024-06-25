#include "aaquad.h"

constexpr int uaxis(aaquad const &q) { return (q.axis + 1) % 3; }
constexpr int vaxis(aaquad const &q) { return (q.axis + 2) % 3; }

static bool is_interior(double a, double b) {
    static constexpr interval unit_interval = interval(0, 1);
    return unit_interval.contains(a) && unit_interval.contains(b);
}

bool aaquad::hit(ray const &r, interval ray_t, geometry_record &rec) const {
    ZoneScopedN("aaquad hit");

    // No hit if the ray is parallel to the plane.
    if (fabs(r.dir[axis]) < 1e-8) return false;

    // Return false if the hit point parameter t is outside the ray
    // interval.
    auto t = (Q[axis] - r.orig[axis]) / (r.dir[axis]);
    if (!ray_t.contains(t)) return false;

    // Determine the hit point lies within the planar shape using its plane
    // coordinates.
    auto intersection = r.at(t);
    uvs uv;
    // NOTE: time argument is not needed.
    getUVs(uv, intersection, 0);

    if (!is_interior(uv.u, uv.v)) return false;

    rec.t = t;
    rec.p = intersection;

    return true;
}

void aaquad::getUVs(uvs &uv, point3 intersection, double _time) const {
    vec3 pq = intersection - Q;
    uv.u = pq[uaxis(*this)] / u;
    uv.v = pq[vaxis(*this)] / v;
}
aabb aaquad::bounding_box() const {
    // Compute the bounding box of all four vertices.
    vec3 u, v;
    u[uaxis(*this)] = this->u;
    v[vaxis(*this)] = this->v;
    auto bbox_diagonal1 = aabb(Q, Q + u + v);
    auto bbox_diagonal2 = aabb(Q + u, Q + v);
    return aabb(bbox_diagonal1, bbox_diagonal2);
}
