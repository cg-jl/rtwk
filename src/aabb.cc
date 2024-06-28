#include "aabb.h"

#include <tracy/Tracy.hpp>
#include "trace_colors.h"

bool aabb::hit(ray const &r, interval ray_t) const {
    ZoneNamedN(zone, "AABB hit", filters::hit);
    point3 ray_orig = r.orig;
    vec3 ray_dir = r.dir;

    for (int axis = 0; axis < 3; axis++) {
        interval const &ax = axis_interval(axis);
        double const adinv = 1.0 / ray_dir[axis];

        auto t0 = (ax.min - ray_orig[axis]) * adinv;
        auto t1 = (ax.max - ray_orig[axis]) * adinv;

        if (adinv < 0) std::swap(t0, t1);

        if (t0 > ray_t.min) ray_t.min = t0;
        if (t1 < ray_t.max) ray_t.max = t1;

        if (ray_t.max <= ray_t.min) return false;
    }
    return true;
}
