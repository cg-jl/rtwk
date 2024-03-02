#pragma once

#include "geometry/quad.h"

struct box : public hittable {
    aabb bbox;
    material mat;
    texture const *tex;

    box(point3 a, point3 b, material mat, texture const *tex)
        : bbox(a, b), mat(std::move(mat)), tex(tex) {}

    [[nodiscard]] aabb bounding_box() const & override { return bbox; }

    static bool hit_side(interval ax, interval ax_u, interval ax_v, int ax_i,
                         int ax_u_idx, int ax_v_idx, float dir, float orig,
                         ray const &r, interval &ray_t,
                         hit_record &rec) noexcept {
        // No hit if the ray is parallel to the plane.
        if (fabs(dir) < 1e-8) return false;
        float D;
        float normal_dir;
        float beta_distance;
        // Only test the face that opposes the ray direction,
        // the other one is going to be missed.
        if (dir > 0) {
            D = ax.min;
            normal_dir = 1;
            beta_distance = ax_v.max;
        } else {
            D = ax.max;
            normal_dir = -1;
            beta_distance = ax_v.min;
        }

        auto t = (D - orig) / dir;
        if (!ray_t.contains(t)) return false;

        auto inv_u_mag = 1 / ax_u.size();
        auto inv_v_mag = 1 / ax_v.size();
        // Determine the hit point lies within the planar shape
        // using its plane coordinates.
        auto intersection = r.at(t);
        auto alpha = inv_u_mag * (intersection[ax_u_idx] - ax_u.min);
        auto beta =
            -normal_dir * inv_v_mag * (intersection[ax_v_idx] - beta_distance);

        if ((alpha < 0) || (1 < alpha) || (beta < 0) || (1 < beta))
            return false;

        ray_t.max = t;
        rec.u = alpha;
        rec.v = beta;
        rec.geom.p = intersection;
        rec.geom.normal = vec3(0);
        rec.geom.normal[ax_i] = normal_dir;
        return true;
    }

    bool hit(ray const &r, interval &ray_t, hit_record &rec) const override {
        bool did_hit = false;
        if (hit_side(bbox.x, bbox.z, bbox.y, 0, 2, 1, r.direction[0],
                     r.origin[0], r, ray_t, rec)) {
            rec.mat = mat;
            rec.tex = tex;
            did_hit = true;
        }
        if (hit_side(bbox.y, bbox.x, bbox.z, 1, 0, 2, r.direction[1],
                     r.origin[1], r, ray_t, rec)) {
            rec.mat = mat;
            rec.tex = tex;
            did_hit = true;
        }
        if (hit_side(bbox.z, bbox.y, bbox.x, 2, 1, 0, r.direction[2],
                     r.origin[2], r, ray_t, rec)) {
            rec.mat = mat;
            rec.tex = tex;
            did_hit = true;
        }

        return did_hit;
    }
};