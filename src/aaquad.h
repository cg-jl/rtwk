#pragma once

#include <tracy/Tracy.hpp>

#include "hittable.h"
#include "hittable_list.h"

struct aaquad final : public geometry {
    point3 Q;
    double u, v;
    int axis;

    constexpr aaquad() {}

    constexpr aaquad(point3 Q, int axis, double u, double v)
        : Q(Q), axis(axis), u(u), v(v) {}

    constexpr int uaxis() const noexcept { return (axis + 1) % 3; }
    constexpr int vaxis() const noexcept { return (axis + 2) % 3; }

    aabb bounding_box() const final {
        // Compute the bounding box of all four vertices.
        vec3 u, v;
        u[uaxis()] = this->u;
        v[vaxis()] = this->v;
        auto bbox_diagonal1 = aabb(Q, Q + u + v);
        auto bbox_diagonal2 = aabb(Q + u, Q + v);
        return aabb(bbox_diagonal1, bbox_diagonal2);
    }

    bool hit(ray const &r, interval ray_t, geometry_record &rec) const final {
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
        // NOTE: normal argument is not needed.
        getUVs(uv, intersection, vec3());

        if (!is_interior(uv.u, uv.v)) return false;

        rec.t = t;
        rec.p = intersection;
        vec3 n(0, 0, 0);
        n[axis] = 1;
        rec.normal = n;

        return true;
    }

    void getUVs(uvs &uv, point3 intersection, vec3 _normal) const final {
        vec3 pq = intersection - Q;
        uv.u = pq[uaxis()] / u;
        uv.v = pq[vaxis()] / v;
    }

    static bool is_interior(double a, double b) {
        static constexpr interval unit_interval = interval(0, 1);
        return unit_interval.contains(a) && unit_interval.contains(b);
    }
};
