#pragma once

#include <array>

#include "aaquad.h"
#include "geometry.h"
#include "hittable.h"

struct box final : public geometry {
    aabb bbox;

    aabb bounding_box() const { return bbox; }

    box(point3 a, point3 b) : bbox(a, b) {}

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

        if ((alpha < 0) || (1 < alpha) || (beta < 0) || (1 < beta))
            return false;

        rec.t = t;
        rec.p = intersection;
        rec.normal = vec3();
        rec.normal[ax_i] = normal_dir;
        return true;
    }

    static void calcUVs(double normal_dir, interval ax_u, interval ax_v,
                        int ax_u_idx, int ax_v_idx, point3 intersection,
                        double &u, double &v) {
        auto beta_distance = normal_dir > 0 ? ax_v.max : ax_v.min;
        auto inv_u_mag = 1 / ax_u.size();
        auto inv_v_mag = 1 / ax_v.size();
        u = inv_u_mag * (intersection[ax_u_idx] - ax_u.min);
        v = -normal_dir * inv_v_mag * (intersection[ax_v_idx] - beta_distance);
    }

    void getUVs(uvs &uv, point3 intersection, vec3 normal) const {
        if (normal.x() != 0) {
            return calcUVs(normal.x(), bbox.z, bbox.y, 2, 1, intersection, uv.u,
                           uv.v);
        } else if (normal.y() != 0) {
            return calcUVs(normal.y(), bbox.x, bbox.z, 0, 2, intersection, uv.u,
                           uv.v);
        } else {
            return calcUVs(normal.z(), bbox.y, bbox.x, 1, 0, intersection, uv.u,
                           uv.v);
        }
    }

    bool hit(ray const &r, interval ray_t,
             geometry_record &rec) const noexcept {
        bool did_hit = false;
        if (hit_side(bbox.x, bbox.z, bbox.y, 0, 2, 1, r.dir[0], r.orig[0], r,
                     ray_t, rec)) {
            did_hit = true;
        }
        if (hit_side(bbox.y, bbox.x, bbox.z, 1, 0, 2, r.dir[1], r.orig[1], r,
                     ray_t, rec)) {
            did_hit = true;
        }
        if (hit_side(bbox.z, bbox.y, bbox.x, 2, 1, 0, r.dir[2], r.orig[2], r,
                     ray_t, rec)) {
            did_hit = true;
        }

        return did_hit;
    }
};
