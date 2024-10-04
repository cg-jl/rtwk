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

    constexpr transform() = default;

    transform(double angleDegrees, vec3 offset) noexcept;


    point3 applyForward(point3 p) const noexcept;
    aabb applyForward(aabb) const noexcept;
};