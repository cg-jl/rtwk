#pragma once

#include "aabb.h"
#include "ray.h"
#include "vec3.h"

struct uvs {
    double u, v;
};

// NOTE: @maybe separating them in tags is interesting
// for hitSelect but not for constantMediums.

// NOTE: @maybe just using traverse everywhere could be an interesting point.
// No geometry has significant cost to just do both, so perhaps intersecting
// with an infinite ray and then intersecting the intervals is more appropiate.

// NOTE: @maybe now that we don't do calculations on every one of the hits,
// we can drop the `closestHit` checks inside each hit and only check when we're
// aggregating.

struct geometry {
    int relIndex;

    virtual aabb bounding_box() const = 0;
    virtual bool hit(ray const &r, double &closest_hit) const = 0;
    virtual void getUVs(uvs &uv, point3 intersection, double time) const = 0;
    // NOTE: intersection is only used by sphere & box, time is only used by
    // sphere.
    virtual vec3 getNormal(point3 const &intersection, double time) const = 0;

    // NOTE: @waste `traverse` should be part of an interface that is queried
    // when building the constant mediums, and not clutter the geometry.

    // End to end traversal of the geometry, just taking into account the
    // direction and the origin point. The intersection is geometric based
    // (distance), not relative to the ray's "speed" on each direction.
    virtual bool traverse(ray const &r, interval &intersect) const = 0;
};
