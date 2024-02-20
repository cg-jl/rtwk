#pragma once

#include "geometry/quad.h"

// NOTE: ideally the box just has three normals and three intervals (aabb +
// normals)
struct box : public hittable {
    struct aaquad {
        point3 Q;
        vec3 u, v;
    };
    struct axis {
        std::array<aaquad, 2> sides{};
    };
    std::array<axis, 3> axes{};
    material mat;
    texture const *tex;

    box(point3 a, point3 b, material mat, texture const *tex)
        : mat(std::move(mat)), tex(tex) {
        // Construct the two opposite vertices with the minimum and maximum
        // coordinates.
        auto minmax = aabb(a, b);

        auto dx = vec3(minmax.x.size(), 0, 0);
        auto dy = vec3(0, minmax.y.size(), 0);
        auto dz = vec3(0, 0, minmax.z.size());

        // YZ plane (X axis)
        axes[0].sides[0] =
            aaquad(point3(minmax.x.max, minmax.y.min, minmax.z.max), -dz,
                   dy);  // right
        axes[0].sides[1] = aaquad(
            point3(minmax.x.min, minmax.y.min, minmax.z.min), dz, dy);  // left
        // XZ plane (Y axis)
        axes[1].sides[0] = aaquad(
            point3(minmax.x.min, minmax.y.max, minmax.z.max), dx, -dz);  // top
        axes[1].sides[1] =
            aaquad(point3(minmax.x.min, minmax.y.min, minmax.z.min), dx,
                   dz);  // bottom
        // XY plane (Z axis)
        axes[2].sides[0] =
            aaquad(point3(minmax.x.min, minmax.y.min, minmax.z.max), dx, dy);
        axes[2].sides[1] = aaquad(
            point3(minmax.x.max, minmax.y.min, minmax.z.min), -dx, dy);  // back
    }

    [[nodiscard]] aabb bounding_box() const & override {
        aabb box = empty_bb;
        for (auto const &ax : axes) {
            for (auto const &side : ax.sides) {
                auto Q = side.Q;
                auto u = side.u;
                auto v = side.v;
                auto side_box = aabb(Q, Q + u + v).pad();
                box = aabb(box, side_box);
            }
        }
        return box;
    }

    bool hit(ray const &r, interval &ray_t, hit_record &rec) const override {
        bool did_hit = false;
        for (auto const &ax : axes) {
            for (auto const &side : ax.sides) {
                auto u = side.u;
                auto v = side.v;
                auto Q = side.Q;
                auto n = cross(u, v);
                auto inv_sqrtn = 1 / n.length();
                auto normal = n * inv_sqrtn;

                auto denom = dot(normal, r.direction);

                // No hit if the ray is parallel to the plane.
                if (fabs(denom) < 1e-8) continue;

                auto D = dot(normal, Q);
                // Return false if the hit point parameter t is outside the ray
                // interval.
                auto t = (D - dot(normal, r.origin)) / denom;
                if (!ray_t.contains(t)) continue;

                auto w = normal * inv_sqrtn;

                // Determine the hit point lies within the planar shape using
                // its plane coordinates.
                auto intersection = r.at(t);
                vec3 planar_hitpt_vector = intersection - Q;
                auto alpha = dot(w, cross(planar_hitpt_vector, v));
                auto beta = dot(w, cross(u, planar_hitpt_vector));

                if (!quad::is_interior(alpha, beta, rec)) continue;

                // Ray hits the 2D shape; set the rest of the hit record and
                // return true.
                ray_t.max = t;
                rec.p = intersection;
                rec.mat = mat;
                rec.tex = tex;
                rec.normal = normal;

                did_hit = true;
            }
        }
        return did_hit;
    }
};