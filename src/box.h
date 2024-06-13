#pragma once

#include <array>

#include "geometry.h"

struct box final : public geometry {
    aabb bbox;

    aabb bounding_box() const { return bbox; }

    box(point3 a, point3 b) : bbox(a, b) {}

    void getUVs(uvs &uv, point3 intersection, vec3 normal) const;

    bool hit(ray const &r, interval ray_t, geometry_record &rec) const noexcept;
};
