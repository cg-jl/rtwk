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

#include "geometry.h"
#include "hittable.h"
#include "hittable_list.h"
#include "rtweekend.h"

class quad : public hittable {
   public:
    constexpr quad(point3 Q, vec3 u, vec3 v, material *mat)
        : hittable(mat), Q(Q), u(u), v(v) {}

    aabb bounding_box() const final {
        // Compute the bounding box of all four vertices.
        auto bbox_diagonal1 = aabb(Q, Q + u + v);
        auto bbox_diagonal2 = aabb(Q + u, Q + v);
        return aabb(bbox_diagonal1, bbox_diagonal2);
    }

    bool hit(ray const &r, interval ray_t, geometry_record &rec) const final {
        ZoneScopedN("quad hit");
        auto n = cross(u, v);
        auto normal = unit_vector(n);
        auto D = dot(normal, Q);
        auto denom = dot(normal, r.dir);

        // No hit if the ray is parallel to the plane.
        if (fabs(denom) < 1e-8) return false;

        // Return false if the hit point parameter t is outside the ray
        // interval.
        auto t = (D - dot(normal, r.orig)) / denom;
        if (!ray_t.contains(t)) return false;

        // Determine the hit point lies within the planar shape using its plane
        // coordinates.
        auto intersection = r.at(t);
        uvs uv;
        // NOTE: normal argument is not needed.
        getUVs(uv, intersection, vec3());

        if (!is_interior(uv.u, uv.v)) return false;

        // Ray hits the 2D shape; set the rest of the hit record and return
        // true.
        rec.t = t;
        rec.p = intersection;
        rec.normal = normal;

        return true;
    }

    void getUVs(uvs &uv, point3 intersection, vec3 _normal) const final {
        vec3 pq = intersection - Q;
        auto v_squared = v.length_squared();
        auto u_squared = u.length_squared();
        auto dot_uv = dot(u, v);
        auto cross_uv_lensq = u_squared * v_squared - dot_uv * dot_uv;
        auto dot_uq = dot(u, pq);
        auto dot_vq = dot(v, pq);
        // (a×b)⋅(c×d) = (a⋅c)(b⋅d) - (a⋅d)(b⋅c)
        uv.u = (dot_uq * v_squared - dot_uv * dot_vq) / cross_uv_lensq;
        uv.v = (u_squared * dot_vq - dot_uq * dot_uv) / cross_uv_lensq;
    }

    bool is_interior(double a, double b) const {
        static constexpr interval unit_interval = interval(0, 1);
        // Given the hit point in plane coordinates, return false if it is
        // outside the primitive, otherwise set the hit record UV coordinates
        // and return true.

        return unit_interval.contains(a) && unit_interval.contains(b);
    }

   private:
    point3 Q;
    vec3 u, v;
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
