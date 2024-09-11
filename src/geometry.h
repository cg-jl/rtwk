#pragma once

#include "aabb.h"
#include "box.h"
#include "quad.h"
#include "ray.h"
#include "sphere.h"
#include "transforms.h"
#include "vec3.h"

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
    enum class kind : int { transform, box, sphere, quad } kind;

    union {
        sphere sphere;
        quad quad;
        box box;
        transformed transform;
    } data;

    geometry(sphere sph) : kind(kind::sphere), data{.sphere = sph} {}
    geometry(quad q) : kind(kind::quad), data{.quad = q} {}
    geometry(box b) : kind(kind::box), data{.box = b} {}
    geometry(transformed t) : kind(kind::transform), data{.transform = t} {}

    aabb bounding_box() const {
        switch (kind) {
            case kind::transform:
                return data.transform.bounding_box();
            case kind::box:
                return data.box.bounding_box();
            case kind::quad:
                return data.quad.bounding_box();
            case kind::sphere:
                return data.sphere.bounding_box();
        }
    }

    // TODO: write the result inconditionally everywhere.
    bool hit(ray const &r, double &closestHit) const {
        switch (kind) {
            case kind::transform:
                return data.transform.hit(r, closestHit);
            case kind::box:
                return data.box.hit(r, closestHit);
            case kind::sphere:
                return data.sphere.hit(r, closestHit);
            case kind::quad:
                return data.quad.hit(r, closestHit);
        }
    }

    void getUVs(uvs &uv, point3 intersection, double time) const {
        switch (kind) {
            case kind::transform:
                return data.transform.getUVs(uv, intersection, time);
            case kind::box:
                return data.box.getUVs(uv, intersection, time);
            case kind::sphere:
                return data.sphere.getUVs(uv, intersection, time);
            case kind::quad:
                return data.quad.getUVs(uv, intersection, time);
        }
    }

    // NOTE: intersection is only used by sphere & box, time is only used by
    // sphere.
    vec3 getNormal(point3 const &intersection, double time) const {
        switch (kind) {
            case kind::transform:
                return data.transform.getNormal(intersection, time);
            case kind::box:
                return data.box.getNormal(intersection, time);
            case kind::sphere:
                return data.sphere.getNormal(intersection, time);
            case kind::quad:
                return data.quad.getNormal(intersection, time);
        }
    };

    // NOTE: @waste `traverse` should be part of an interface that is queried
    // when building the constant mediums, and not clutter the geometry.

    // End to end traversal of the geometry, just taking into account the
    // direction and the origin point. The intersection is geometric based
    // (distance), not relative to the ray's "speed" on each direction.
    bool traverse(ray const &r, interval &intersect) const {
        switch (kind) {
            case kind::transform:
                return data.transform.traverse(r, intersect);
            case kind::box:
                return data.box.traverse(r, intersect);
            case kind::sphere:
                return data.sphere.traverse(r, intersect);
            case kind::quad:
                return data.quad.traverse(r, intersect);
        }
    };
};
