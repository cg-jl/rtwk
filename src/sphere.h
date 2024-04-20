#ifndef SPHERE_H
#define SPHERE_H
//==============================================================================================
// Originally written in 2016 by Peter Shirley <ptrshrl@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public
// Domain Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include <tracy/Tracy.hpp>

#include "geometry.h"
#include "hittable.h"

// TODO: To instantiate spheres, I should separate instantiatable things
// and add a thread-private 'instantiate' buffer per worker thread where I can
// push any transformations. When separating list-of-ptrs into tagged arrays,
// these 'instantiate buffers' must also be separated.
class sphere : public hittable {
   public:
    // Stationary Sphere
    sphere(point3 const &center, double radius, material *mat)
        : hittable(mat), center1(center), radius(fmax(0, radius)) {}

    // Moving Sphere
    sphere(point3 const &center1, point3 const &center2, double radius,
           material *mat)
        : hittable(mat), center1(center1), radius(fmax(0, radius)) {
        center_vec = center2 - center1;
    }

    bool hit(ray const &r, interval ray_t, geometry_record &rec) const final {
        ZoneScopedN("sphere hit");
        point3 center = sphere_center(r.time);
        vec3 oc = center - r.orig;
        auto a = r.dir.length_squared();
        // Distance from ray origin to sphere center parallel to the ray
        // direction
        auto oc_alongside_ray = dot(r.dir, oc);
        auto c = oc.length_squared() - radius * radius;

        auto discriminant = oc_alongside_ray * oc_alongside_ray - a * c;
        if (discriminant < 0) return false;

        auto sqrtd = std::sqrt(discriminant);

        // Find the nearest root that lies in the acceptable range.
        auto root = (oc_alongside_ray - sqrtd) / a;
        if (!ray_t.surrounds(root)) {
            root = (oc_alongside_ray + sqrtd) / a;
            if (!ray_t.surrounds(root)) return false;
        }

        rec.t = root;
        rec.p = r.at(rec.t);
        vec3 outward_normal = (rec.p - center) / radius;
        rec.normal = outward_normal;

        return true;
    }
    void getUVs(uvs &uv, point3 _p, vec3 normal) const final {
        get_sphere_uv(normal, uv.u, uv.v);
    }

    aabb bounding_box() const final {
        auto rvec = vec3(radius, radius, radius);
        auto center2 = center1 + center_vec;
        aabb box1(center1 - rvec, center1 + rvec);
        aabb box2(center2 - rvec, center2 + rvec);
        return aabb(box1, box2);
    }

   private:
    point3 center1;
    double radius;
    vec3 center_vec;

    point3 sphere_center(double time) const {
        // Linearly interpolate from center1 to center2 according to time, where
        // t=0 yields center1, and t=1 yields center2.
        return center1 + time * center_vec;
    }

    static void get_sphere_uv(point3 const &p, double &u, double &v) {
        // p: a given point on the sphere of radius one, centered at the origin.
        // u: returned value [0,1] of angle around the Y axis from X=-1.
        // v: returned value [0,1] of angle from Y=-1 to Y=+1.
        //     <1 0 0> yields <0.50 0.50>       <-1  0  0> yields <0.00 0.50>
        //     <0 1 0> yields <0.50 1.00>       < 0 -1  0> yields <0.50 0.00>
        //     <0 0 1> yields <0.25 0.50>       < 0  0 -1> yields <0.75 0.50>

        auto theta = std::acos(-p.y());
        auto phi = std::atan2(-p.z(), p.x()) + pi;

        u = phi / (2 * pi);
        v = theta / pi;
    }
};

#endif
