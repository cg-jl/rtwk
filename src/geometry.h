#pragma once

#include <span>

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
    enum class kind : int { box, sphere, quad } kind;

    std::span<transform const> tfs;
    union {
        sphere sphere;
        quad quad;
        box box;
    } data;

    geometry(sphere sph) : kind(kind::sphere), data{.sphere = sph} {}
    geometry(quad q) : kind(kind::quad), data{.quad = q} {}
    geometry(box b) : kind(kind::box), data{.box = b} {}

    aabb bounding_box() const {
        aabb bbox;

        switch (kind) {
            case kind::box:
                bbox = data.box.bounding_box();
                break;
            case kind::quad:
                bbox = data.quad.bounding_box();
                break;
            case kind::sphere:
                bbox = data.sphere.bounding_box();
                break;
        }

        for (size_t i = tfs.size(); i > 0;) {
            --i;
            bbox = tfs[i].applyForward(bbox);
        }

        return bbox;
    }

    // TODO: write the result inconditionally everywhere.
    bool hit(ray r, double &closestHit) const {
        for (auto const &tf : tfs) {
            r = tf.applyInverse(r);
        }
        switch (kind) {
            case kind::box:
                return data.box.hit(r, closestHit);
            case kind::sphere:
                return data.sphere.hit(r, closestHit);
            case kind::quad:
                return data.quad.hit(r, closestHit);
        }
    }

    // Calculates the UVs given an intersection and a normal.  They are passed
    // as restrict references because we want to maintain their uniqueness
    // semantics, but we know only one of them is going to be accessed.
    uvs getUVs(point3 const &__restrict__ intersection,
               point3 const &__restrict__ normal) const {
        switch (kind) {
            case kind::box:
                return data.box.getUVs(intersection);
            case kind::sphere:
                return sphere::getUVs(normal);
            case kind::quad:
                return data.quad.getUVs(intersection);
        }
    }

    // NOTE: intersection is only used by sphere & box, time is only used by
    // sphere.
    vec3 getNormal(point3 intersection, double time) const {
        for (auto const &tf : tfs) {
            // NOTE: Since `hit` transforms into the transformed space, we have
            // to get back to the local space.
            intersection = tf.applyInverse(intersection);
        }

        switch (kind) {
            case kind::box:
                return data.box.getNormal(intersection);
            case kind::sphere:
                return data.sphere.getNormal(intersection, time);
            case kind::quad:
                return data.quad.getNormal();
        }
    };

    // NOTE: @waste `traverse` should be part of an interface that is queried
    // when building the constant mediums, and not clutter the geometry.

    // End to end traversal of the geometry, just taking into account the
    // direction and the origin point. The intersection is geometric based
    // (distance), not relative to the ray's "speed" on each direction.
    bool traverse(ray r, interval &intersect) const {

        for (auto const &tf : tfs) {
            r = tf.applyInverse(r);
        }

        switch (kind) {
            case kind::box:
                return data.box.traverse(r, intersect);
            case kind::sphere:
                return data.sphere.traverse(r, intersect);
            case kind::quad:
                return data.quad.traverse(r, intersect);
        }
    };
};
