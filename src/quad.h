#ifndef QUAD_H
#define QUAD_H
//==============================================================================================
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public
// Domain Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include "hittable.h"
#include "hittable_list.h"
#include "material.h"
#include "rtweekend.h"
#include "vec3.h"

struct quad final : public hittable {
   public:
    aabb bbox;
    point3 Q;
    vec3 u, v;
    vec3 normal;
    double inv_sqrtn;
    shared_ptr<material> mat;
    aabb bounding_box() const& override { return bbox; }

    quad(point3 _Q, vec3 _u, vec3 _v, shared_ptr<material> m)
        : Q(_Q), u(_u), v(_v), mat(m) {
        auto n = cross(u, v);
        inv_sqrtn = 1 / n.length();
        normal = inv_sqrtn * n;

        set_bounding_box();
    }

    virtual void set_bounding_box() { bbox = aabb(Q, Q + u + v).pad(); }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        auto denom = dot(normal, r.direction);

        // No hit if the ray is parallel to the plane.
        if (fabs(denom) < 1e-8) return false;

        auto D = dot(normal, Q);
        // Return false if the hit point parameter t is outside the ray
        // interval.
        auto t = (D - dot(normal, r.origin)) / denom;
        if (!ray_t.contains(t)) return false;

        auto w = normal * inv_sqrtn;

        // Determine the hit point lies within the planar shape using its plane
        // coordinates.
        auto intersection = r.at(t);
        vec3 planar_hitpt_vector = intersection - Q;
        auto alpha = dot(w, cross(planar_hitpt_vector, v));
        auto beta = dot(w, cross(u, planar_hitpt_vector));

        if (!is_interior(alpha, beta, rec)) return false;

        // Ray hits the 2D shape; set the rest of the hit record and return
        // true.
        ray_t.max = t;
        rec.p = intersection;
        rec.mat = mat.get();
        rec.normal = normal;

        return true;
    }

    static bool is_interior(double a, double b, hit_record& rec) {
        // Given the hit point in plane coordinates, return false if it is
        // outside the primitive, otherwise set the hit record UV coordinates
        // and return true.

        if ((a < 0) || (1 < a) || (b < 0) || (1 < b)) return false;

        rec.u = a;
        rec.v = b;
        return true;
    }
};

static inline void box_into(point3 a, point3 b, shared_ptr<material> mat,
                            hittable_list& sides) {
    // Construct the two opposite vertices with the minimum and maximum
    // coordinates.
    auto min =
        point3(fmin(a.x(), b.x()), fmin(a.y(), b.y()), fmin(a.z(), b.z()));
    auto max =
        point3(fmax(a.x(), b.x()), fmax(a.y(), b.y()), fmax(a.z(), b.z()));

    auto dx = vec3(max.x() - min.x(), 0, 0);
    auto dy = vec3(0, max.y() - min.y(), 0);
    auto dz = vec3(0, 0, max.z() - min.z());

    sides.add(make_shared<quad>(point3(min.x(), min.y(), max.z()), dx, dy,
                                mat));  // front
    sides.add(make_shared<quad>(point3(max.x(), min.y(), max.z()), -dz, dy,
                                mat));  // right
    sides.add(make_shared<quad>(point3(max.x(), min.y(), min.z()), -dx, dy,
                                mat));  // back
    sides.add(make_shared<quad>(point3(min.x(), min.y(), min.z()), dz, dy,
                                mat));  // left
    sides.add(make_shared<quad>(point3(min.x(), max.y(), max.z()), dx, -dz,
                                mat));  // top
    sides.add(make_shared<quad>(point3(min.x(), min.y(), min.z()), dx, dz,
                                mat));  // bottom
}

static inline shared_ptr<hittable_list> box(point3 const& a, point3 const& b,
                                            shared_ptr<material> mat) {
    // Returns the 3D box (six sides) that contains the two opposite vertices a
    // & b.

    auto sides = make_shared<hittable_list>();

    box_into(a, b, std::move(mat), *sides);

    return sides;
}

#endif
