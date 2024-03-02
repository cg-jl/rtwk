#pragma once

#include "aabb.h"
#include "geometry.h"
#include "hit_record.h"

struct box final {
    aabb bbox;

    box(point3 a, point3 b) : bbox(a, b) {}

    [[nodiscard]] aabb boundingBox() const & { return bbox; }

    static bool hit_side(interval ax, interval ax_u, interval ax_v, int ax_i,
                         int ax_u_idx, int ax_v_idx, float dir, float orig,
                         ray const &r, interval &ray_t,
                         hit_record::geometry &rec) noexcept {
        // No hit if the ray is parallel to the plane.
        if (fabs(dir) < 1e-8) return false;
        float D;
        float normal_dir;
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
        float alpha, beta;
        calcUVs(normal_dir, ax_u, ax_v, ax_u_idx, ax_v_idx, intersection, alpha,
                beta);

        if ((alpha < 0) || (1 < alpha) || (beta < 0) || (1 < beta))
            return false;

        ray_t.max = t;
        rec.p = intersection;
        rec.normal = vec3(0);
        rec.normal[ax_i] = normal_dir;
        return true;
    }

    static void calcUVs(float normal_dir, interval ax_u, interval ax_v,
                        int ax_u_idx, int ax_v_idx, point3 intersection,
                        float &u, float &v) {
        auto beta_distance = normal_dir > 0 ? ax_v.max : ax_v.min;
        auto inv_u_mag = 1 / ax_u.size();
        auto inv_v_mag = 1 / ax_v.size();
        u = inv_u_mag * (intersection[ax_u_idx] - ax_u.min);
        v = -normal_dir * inv_v_mag * (intersection[ax_v_idx] - beta_distance);
    }

    static std::span<transform const> getTransforms() { return {}; }

    void getUVs(hit_record::geometry const &res, float &u, float &v) const {
        if (res.normal.x() != 0) {
            return calcUVs(res.normal.x(), bbox.z, bbox.y, 2, 1, res.p, u, v);
        } else if (res.normal.y() != 0) {
            return calcUVs(res.normal.y(), bbox.x, bbox.z, 0, 2, res.p, u, v);
        } else {
            return calcUVs(res.normal.z(), bbox.y, bbox.x, 1, 0, res.p, u, v);
        }
    }

    bool hit(ray const &r, hit_record::geometry &rec,
             interval &ray_t) const noexcept {
        bool did_hit = false;
        if (hit_side(bbox.x, bbox.z, bbox.y, 0, 2, 1, r.direction[0],
                     r.origin[0], r, ray_t, rec)) {
            did_hit = true;
        }
        if (hit_side(bbox.y, bbox.x, bbox.z, 1, 0, 2, r.direction[1],
                     r.origin[1], r, ray_t, rec)) {
            did_hit = true;
        }
        if (hit_side(bbox.z, bbox.y, bbox.x, 2, 1, 0, r.direction[2],
                     r.origin[2], r, ray_t, rec)) {
            did_hit = true;
        }

        return did_hit;
    }
};

static_assert(is_geometry<box>);
