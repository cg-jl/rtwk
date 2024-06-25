#pragma once

#include <tracy/Tracy.hpp>

#include "aabb.h"
#include "geometry.h"
#include "vec3.h"

struct aaquad final : public geometry {
    point3 Q;
    double u, v;
    int axis;

    constexpr aaquad() {}

    constexpr aaquad(point3 Q, int axis, double u, double v)
        : Q(Q), axis(axis), u(u), v(v) {}

    aabb bounding_box() const final;

    bool hit(ray const &r, interval ray_t, geometry_record &rec) const final;

    void getUVs(uvs &uv, point3 intersection, double _time) const final;
    vec3 getNormal(point3 const &intersection, double time) const final;
};
