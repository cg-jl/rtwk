#pragma once

#include <aabb.h>
#include <rtweekend.h>

struct box final {
    aabb bbox;

    aabb bounding_box() const { return bbox; }

    box(point3 a, point3 b) : bbox(a, b) {}

    void getUVs(uvs &uv, point3 intersection, double _time) const;

    vec3 getNormal(point3 const &intersection, double time) const;

    bool hit(ray const &r, double &closestHit) const;
    bool traverse(ray const &r, interval &intesect) const;
};
