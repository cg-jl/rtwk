#include "box.h"
#include <utility>

static void calcUVs(double normal_dir, interval ax_u, interval ax_v,
                    int ax_u_idx, int ax_v_idx, point3 intersection, double &u,
                    double &v) {
    auto beta_distance = normal_dir > 0 ? ax_v.max : ax_v.min;
    auto inv_u_mag = 1 / ax_u.size();
    auto inv_v_mag = 1 / ax_v.size();
    u = inv_u_mag * (intersection[ax_u_idx] - ax_u.min);
    v = -normal_dir * inv_v_mag * (intersection[ax_v_idx] - beta_distance);
}

static bool hit_side(interval ax, interval ax_u, interval ax_v, int ax_i,
                     int ax_u_idx, int ax_v_idx, double dir, double orig,
                     ray const &r, interval ray_t,
                     geometry_record &rec) noexcept {
    // No hit if the ray is parallel to the plane.
    if (fabs(dir) < 1e-8) return false;
    double D;
    double normal_dir;
    // Only test the face that opposes the ray direction,
    // the other one is going to be missed.
    if (dir > 0) {
        D = ax.min;
        normal_dir = 1;
    } else {
        D = ax.max;
        normal_dir = -1;
    }

    auto t = (D - orig) / dir;
    if (!ray_t.contains(t)) return false;

    // Determine the hit point lies within the planar shape
    // using its plane coordinates.
    auto intersection = r.at(t);
    double alpha, beta;
    calcUVs(normal_dir, ax_u, ax_v, ax_u_idx, ax_v_idx, intersection, alpha,
            beta);

    if ((alpha < 0) || (1 < alpha) || (beta < 0) || (1 < beta)) return false;

    rec.t = t;
    return true;
}

bool box::hit(ray const &r, interval ray_t,
              geometry_record &rec) const noexcept {
    bool did_hit = false;
    if (hit_side(bbox.x, bbox.z, bbox.y, 0, 2, 1, r.dir[0], r.orig[0], r, ray_t,
                 rec)) {
        did_hit = true;
    }
    if (hit_side(bbox.y, bbox.x, bbox.z, 1, 0, 2, r.dir[1], r.orig[1], r, ray_t,
                 rec)) {
        did_hit = true;
    }
    if (hit_side(bbox.z, bbox.y, bbox.x, 2, 1, 0, r.dir[2], r.orig[2], r, ray_t,
                 rec)) {
        did_hit = true;
    }

    return did_hit;
}

void box::getUVs(uvs &uv, point3 intersection, double _time) const {
    // search for the "box" that borders the point interval, since we know that
    // the point is already within the bounds of the box.
    for (int axis = 0; axis < 3; ++axis) {
        auto uaxis = (axis + 2) % 3;
        auto vaxis = (axis + 1) % 3;

        auto intv = bbox.axis_interval(axis);
        auto uintv = bbox.axis_interval(uaxis);
        auto vintv = bbox.axis_interval(vaxis);

        double beta_distance;
        if (intersection[axis] == intv.min) {
            beta_distance = vintv.max;
        } else if (intersection[axis] == intv.max) {
            beta_distance = vintv.min;
        } else {
            continue;
        }
        auto inv_u_mag = 1 / uintv.size();
        auto inv_v_mag = 1 / vintv.size();
        uv.u = inv_u_mag * (intersection[uaxis] - uintv.min);
        uv.v = -inv_v_mag * (intersection[vaxis] - beta_distance);
        return;
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
