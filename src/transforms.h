#pragma once

#include "geometry.h"

// TODO: this is not a hittable (no material) nor a selector. Transforms
// should be in the hittable class itself, and hit() separated into
// internalHit() (assumes transform) and hit()
struct translate final : public geometry {
    constexpr translate(geometry const *object, vec3 offset)
        : object(object), offset(offset) {}

    bool hit(ray const &r, interval ray_t, geometry_record &rec) const final;

    // NOTE: Since the point and the normals have been already been changed
    // (otherwise the material would have a wrong normal to process), transforms
    // pass the baton to their wrapped object.
    void getUVs(uvs &uv, point3 p, vec3 normal) const final;

    aabb bounding_box() const final { return object->bounding_box() + offset; }

    geometry const *object;
    vec3 offset;
};
// TODO: this is not a hittable (no material) nor a selector. Transforms
// should be in the hittable class itself, and hit() separated into
// internalHit() (assumes transform) and hit()
struct rotate_y final : public geometry {
    rotate_y(geometry const *object, double angle);

    bool hit(ray const &r, interval ray_t, geometry_record &rec) const final;
    // NOTE: Since the point and the normals have been already been changed
    // (otherwise the material would have a wrong normal to process), transforms
    // pass the baton to their wrapped object.
    void getUVs(uvs &uv, point3 p, vec3 normal) const final;

    aabb bounding_box() const final;

    geometry const *object;
    double sin_theta;
    double cos_theta;
};
