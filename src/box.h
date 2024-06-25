#pragma once

#include "geometry.h"

struct box final : public geometry {
    aabb bbox;

    aabb bounding_box() const { return bbox; }

    box(point3 a, point3 b) : bbox(a, b) {}

    void getUVs(uvs &uv, point3 intersection, double _time) const final;

    vec3 getNormal(point3 const& intersection, double time) const final;

    bool hit(ray const &r, interval ray_t, double &closestHiw) const;
};
