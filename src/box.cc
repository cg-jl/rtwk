#include "box.h"

#include <tracy/Tracy.hpp>
#include <utility>

#include "hittable.h"

static void calcUVs(double normal_dir, vec3 const &min, vec3 const &max,
                    int ax_u_idx, int ax_v_idx, point3 intersection, double &u,
                    double &v) {
    auto beta_distance = normal_dir > 0 ? max[ax_v_idx] : min[ax_v_idx];
    auto inv_u_mag = 1 / (max[ax_u_idx] - min[ax_u_idx]);
    auto inv_v_mag = 1 / (max[ax_v_idx] - min[ax_v_idx]);
    u = inv_u_mag * (intersection[ax_u_idx] - min[ax_u_idx]);
    v = -normal_dir * inv_v_mag * (intersection[ax_v_idx] - beta_distance);
}

static bool hit_side(point3 const &min, point3 const &max, int ax_i,
                     int ax_u_idx, int ax_v_idx, double dir, double orig,
                     ray const &r, double &closestHit) noexcept {
    // No hit if the ray is parallel to the plane.
    if (fabs(dir) < 1e-8) return false;
    double D;
    double normal_dir;
    // Only test the face that opposes the ray direction,
    // the other one is going to be missed.
    if (dir > 0) {
        D = min[ax_i];
        normal_dir = 1;
    } else {
        D = max[ax_i];
        normal_dir = -1;
    }

    auto t = (D - orig) / dir;

    // Determine the hit point lies within the planar shape
    // using its plane coordinates.
    auto intersection = r.at(t);
    double alpha, beta;
    calcUVs(normal_dir, min, max, ax_u_idx, ax_v_idx, intersection, alpha,
            beta);

    if ((alpha < 0) || (1 < alpha) || (beta < 0) || (1 < beta)) return false;

    closestHit = t;
    return true;
}

bool box::hit(ray const &r, double &closestHit) const {
    ZoneScopedN("box hit");

    // @cleanup could have hit side just accept the box (or min/max), since
    // the indices are the same.
    if (hit_side(bbox.min, bbox.max, 0, 2, 1, r.dir[0], r.orig[0], r,
                 closestHit)) {
        return true;
    }
    if (hit_side(bbox.min, bbox.max, 1, 0, 2, r.dir[1], r.orig[1], r,
                 closestHit)) {
        return true;
    }
    if (hit_side(bbox.min, bbox.max, 2, 1, 0, r.dir[2], r.orig[2], r,
                 closestHit)) {
        return true;
    }
    return false;

    // TODO: @waste @maybe I should clone the `traverse` method with an infinite
    // intersect as the begin point.
}

bool box::traverse(ray const &r, interval &intersect) const {
    intersect = universe_interval;

    return bbox.traverse(r, intersect);
}

uvs box::getUVs( point3 intersection) const {
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

vec3 box::getNormal(point3 const &intersection, double _time) const {
    for (int axis = 0; axis < 3; ++axis) {
        if (bbox.axis_interval(axis).atBorder(intersection[axis])) {
            vec3 n(0, 0, 0);
            n[axis] = 1;
            return n;
        }
    }
    std::unreachable();
}
