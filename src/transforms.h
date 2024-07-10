#pragma once

#include "geometry.h"

struct transform final {
    vec3 offset;
    double sin_theta;
    double cos_theta;

    transform(double angleDegrees, vec3 offset) noexcept;
};

struct transformed final : public geometry {
    geometry const *object;
    transform tf;

    transformed(geometry const *object, transform tf);

    bool hit(ray const &r, double &closestHit) const;
    bool traverse(ray const &r, interval &intesect) const;
    void getUVs(uvs &uv, point3 p, double time) const final;

    vec3 getNormal(point3 const &intersection, double time) const final;
    aabb bounding_box() const final;
};
