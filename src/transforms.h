#pragma once

#include "geometry.h"

struct translate final {
    constexpr translate(vec3 offset) : offset(offset) {}
    constexpr translate() : offset{} {}

    void transformRayOpposite(ray &r) const noexcept;
    void doTransform(point3 &hitPoint, point3 &normal) const noexcept;
    void transformPoint(point3 &point) const noexcept;

    vec3 offset;
};

struct rotate_y final {
    constexpr rotate_y() : sin_theta{0}, cos_theta{1} {}
    rotate_y(double angle);

    void transformRayOpposite(ray &r) const noexcept;
    void doTransform(point3 &hitPoint, point3 &normal) const noexcept;
    void transformPoint(point3 &point) const noexcept;

    double sin_theta;
    double cos_theta;
};

struct transformed final : public geometry {
    geometry const *object;
    rotate_y const rotate;
    translate const translate;

    transformed(geometry const *object, rotate_y rotate,
                struct translate translate);

    bool hit(ray const &r, double &closestHit) const;
    bool traverse(ray const &r, interval &intesect) const;
    void getUVs(uvs &uv, point3 p, double time) const final;

    vec3 getNormal(point3 const &intersection, double time) const final;
    aabb bounding_box() const final;
};
