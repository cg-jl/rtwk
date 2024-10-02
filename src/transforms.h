#pragma once

#include <aabb.h>
#include <interval.h>
#include <ray.h>
#include <rtweekend.h>
#include <vec3.h>

struct transform final {
    vec3 offset;
    double sin_theta;
    double cos_theta;

    transform(double angleDegrees, vec3 offset) noexcept;
};

struct geometry;

struct transformed {
    geometry const *object;
    transform tf;

    transformed(geometry const *object, transform tf);

    bool hit(ray const &r, double &closestHit) const;
    bool traverse(ray const &r, interval &intesect) const;

    vec3 getNormal(point3 const &intersection, double time) const;
    aabb bounding_box() const;
};
