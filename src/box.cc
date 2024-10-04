#include "box.h"

#include <tracy/Tracy.hpp>
#include <utility>

bool box::hit(ray const &r, double &closestHit) const {
    ZoneScopedN("box hit");

    auto intersect = universe_interval;

    auto ok = bbox.traverse(r, intersect);

    closestHit = intersect.min;
    return ok;
}

bool box::traverse(ray const &r, interval &intersect) const {
    intersect = universe_interval;

    return bbox.traverse(r, intersect);
}

uvs box::getUVs(point3 intersection) const {
    // search for the "box" that borders the point interval, since we know that
    // the point is already within the bounds of the box.
    for (int axis = 0; axis < 3; ++axis) {
        auto uaxis = (axis + 2) % 3;
        auto vaxis = (axis + 1) % 3;

        auto intv = bbox.axis_interval(axis);
        auto uintv = bbox.axis_interval(uaxis);
        auto vintv = bbox.axis_interval(vaxis);

        double beta_distance;
        if (std::abs(intersection[axis] - intv.min) < 1e-8) {
            beta_distance = vintv.max;
        } else if (std::abs(intersection[axis] - intv.max) < 1e-8) {
            beta_distance = vintv.min;
        } else {
            continue;
        }
        auto inv_u_mag = 1 / uintv.size();
        auto inv_v_mag = 1 / vintv.size();
        uvs uv;
        uv.u = inv_u_mag * (intersection[uaxis] - uintv.min);
        uv.v = -inv_v_mag * (intersection[vaxis] - beta_distance);
        return uv;
    }
    std::unreachable();
}

vec3 box::getNormal(point3 const intersection) const {
    for (int axis = 0; axis < 3; ++axis) {
        if (bbox.axis_interval(axis).atBorder(intersection[axis])) {
            vec3 n(0, 0, 0);
            n[axis] = 1;
            return n;
        }
    }
    std::unreachable();
}
