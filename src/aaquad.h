#pragma once

#include <tracy/Tracy.hpp>

#include "hittable.h"
#include "hittable_list.h"

struct aaquad final : public hittable {
    point3 Q;
    double u, v;
    int axis;

    constexpr aaquad(point3 Q, int axis, double u, double v, material *mat)
        : hittable(mat), Q(Q), axis(axis), u(u), v(v) {}

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

    bool is_interior(double a, double b) const {
        static constexpr interval unit_interval = interval(0, 1);
        return unit_interval.contains(a) && unit_interval.contains(b);
    }
};

inline void box(point3 const &a, point3 const &b, material *mat,
                hittable_list &sides) {
    // Returns the 3D box (six sides) that contains the two opposite vertices a
    // & b.

    // Construct the two opposite vertices with the minimum and maximum
    // coordinates.
    auto min =
        point3(fmin(a.x(), b.x()), fmin(a.y(), b.y()), fmin(a.z(), b.z()));
    auto max =
        point3(fmax(a.x(), b.x()), fmax(a.y(), b.y()), fmax(a.z(), b.z()));

    auto dx = max.x() - min.x();
    auto dy = max.y() - min.y();
    auto dz = max.z() - min.z();

    // TODO: watch for u, v coordinates being correct!
    sides.add(new aaquad(point3(min.x(), min.y(), max.z()), 2, dx, dy,
                         mat));  // front
    sides.add(new aaquad(point3(max.x(), min.y(), max.z()), 0, dy, -dz,
                         mat));  // right
    sides.add(new aaquad(point3(min.x(), max.y(), max.z()), 1, -dz, dx,
                         mat));  // top
    sides.add(new aaquad(point3(max.x(), min.y(), min.z()), 2, -dx, dy,
                         mat));  // back
    sides.add(new aaquad(point3(min.x(), min.y(), min.z()), 0, dy, dz,
                         mat));  // left
    sides.add(new aaquad(point3(min.x(), min.y(), min.z()), 1, dz, dx,
                         mat));  // bottom
}
