#include "sphere.h"

#include <tracy/Tracy.hpp>

point3 sphere_center(sphere const &sph, double time) {
    // Linearly interpolate from center1 to center2 according to time, where
    // t=0 yields center1, and t=1 yields center2.
    return sph.center1 + time * sph.center_vec;
}

bool sphere::hit(ray const &r, interval ray_t, double &closestHit) const {
    ZoneScopedN("sphere hit");
    point3 center = sphere_center(*this, r.time);
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

    closestHit = root;

    return true;
}

// p: a given point on the sphere of radius one, centered at the origin.
// u: returned value [0,1] of angle around the Y axis from X=-1.
// v: returned value [0,1] of angle from Y=-1 to Y=+1.
//     <1 0 0> yields <0.50 0.50>       <-1  0  0> yields <0.00 0.50>
//     <0 1 0> yields <0.50 1.00>       < 0 -1  0> yields <0.50 0.00>
//     <0 0 1> yields <0.25 0.50>       < 0  0 -1> yields <0.75 0.50>
void sphere::getUVs(uvs &uv, point3 p, double time) const {
    auto center = sphere_center(*this, time);

    auto normal = (p - center) / radius;

    auto theta = std::acos(-normal.y());
    auto phi = std::atan2(-normal.z(), normal.x()) + pi;

    uv.u = phi / (2 * pi);
    uv.v = theta / pi;
}

aabb sphere::bounding_box() const {
    auto rvec = vec3(radius, radius, radius);
    auto center2 = center1 + center_vec;
    aabb box1(center1 - rvec, center1 + rvec);
    aabb box2(center2 - rvec, center2 + rvec);
    return aabb(box1, box2);
}

vec3 sphere::getNormal(point3 const &intersection, double time) const {
    return (intersection - sphere_center(*this, time)) / radius;
}
