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
#include "rtweekend.h"

class quad : public hittable {
   public:
    constexpr quad(point3 Q, vec3 u, vec3 v, material *mat)
        : hittable(mat), Q(Q), u(u), v(v) {
        auto n = cross(u, v);
        normal = unit_vector(n);
        D = dot(normal, Q);
        w = n / dot(n, n);
    }

    aabb bounding_box() const final {
        // Compute the bounding box of all four vertices.
        auto bbox_diagonal1 = aabb(Q, Q + u + v);
        auto bbox_diagonal2 = aabb(Q + u, Q + v);
        return aabb(bbox_diagonal1, bbox_diagonal2);
    }

    bool hit(ray const &r, interval ray_t, hit_record &rec) const final {
        ZoneScopedN("quad hit");
        auto denom = dot(normal, r.direction());

        // No hit if the ray is parallel to the plane.
        if (fabs(denom) < 1e-8) return false;

        // Return false if the hit point parameter t is outside the ray
        // interval.
        auto t = (D - dot(normal, r.origin())) / denom;
        if (!ray_t.contains(t)) return false;

        // Determine the hit point lies within the planar shape using its plane
        // coordinates.
        auto intersection = r.at(t);
        vec3 planar_hitpt_vector = intersection - Q;
        auto alpha = dot(w, cross(planar_hitpt_vector, v));
        auto beta = dot(w, cross(u, planar_hitpt_vector));

        if (!is_interior(alpha, beta, rec)) return false;

        // Ray hits the 2D shape; set the rest of the hit record and return
        // true.
        rec.t = t;
        rec.p = intersection;
        rec.set_face_normal(r, normal);

        return true;
    }

    bool is_interior(double a, double b, hit_record &rec) const {
        interval unit_interval = interval(0, 1);
        // Given the hit point in plane coordinates, return false if it is
        // outside the primitive, otherwise set the hit record UV coordinates
        // and return true.

        if (!unit_interval.contains(a) || !unit_interval.contains(b))
            return false;

        rec.u = a;
        rec.v = b;
        return true;
    }

   private:
    point3 Q;
    vec3 u, v;
    vec3 w;
    vec3 normal;
    double D;
    material *mat;
};

inline void box(point3 const &a, point3 const &b, material *mat,
                hittable_list &sides) {
    // Returns the 3D box (six sides) that contains the two opposite vertices a
    // & b.

    // Construct the two opposite vertices with the minimum and maximum
    // coordinates.
    auto min =
        point3(fmin(a.x(), b.x()), fmin(a.y(), b.y()), fmin(a.z(), b.z()));
    auto max =
        point3(fmax(a.x(), b.x()), fmax(a.y(), b.y()), fmax(a.z(), b.z()));

    auto dx = vec3(max.x() - min.x(), 0, 0);
    auto dy = vec3(0, max.y() - min.y(), 0);
    auto dz = vec3(0, 0, max.z() - min.z());

    sides.add(new quad(point3(min.x(), min.y(), max.z()), dx, dy,
                       mat));  // front
    sides.add(new quad(point3(max.x(), min.y(), max.z()), -dz, dy,
                       mat));  // right
    sides.add(new quad(point3(max.x(), min.y(), min.z()), -dx, dy,
                       mat));  // back
    sides.add(new quad(point3(min.x(), min.y(), min.z()), dz, dy,
                       mat));  // left
    sides.add(new quad(point3(min.x(), max.y(), max.z()), dx, -dz,
                       mat));  // top
    sides.add(new quad(point3(min.x(), min.y(), min.z()), dx, dz,
                       mat));  // bottom
}

#endif
