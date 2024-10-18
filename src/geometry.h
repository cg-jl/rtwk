#pragma once

#include <iterator>
#include <span>
#include <tracy/Tracy.hpp>
#include <utility>

#include "aabb.h"
#include "hittable.h"
#include "quad.h"
#include "ray.h"
#include "sphere.h"
#include "trace_colors.h"
#include "transforms.h"
#include "vec3.h"

//  @maybe separating them in tags is interesting
// for hitSelect but not for constantMediums.

//  @maybe just using traverse everywhere could be an interesting point.
// No geometry has significant cost to just do both, so perhaps intersecting
// with an infinite ray and then intersecting the intervals is more appropiate.

//  @maybe now that we don't do calculations on every one of the hits,
// we can drop the `closestHit` checks inside each hit and only check when we're
// aggregating.

struct geometry {
    int relIndex;
    enum class kind : int { box, sphere, quad } kind;

    union {
        sphere sphere;
        quad quad;
        aabb box;
    } data;

    geometry(sphere sph) : kind(kind::sphere), data{.sphere = sph} {}
    geometry(quad q) : kind(kind::quad), data{.quad = q} {}
    geometry(aabb b) : kind(kind::box), data{.box = b} {}

    void applyTransform(transform tf) {
        switch (kind) {
            case kind::sphere:
                data.sphere = sphere::applyTransform(data.sphere, tf);
                break;
            case kind::quad:
                data.quad = quad::applyTransform(data.quad, tf);
                break;
            case kind::box:
                data.box = tf.applyForward(data.box);
                break;
        }
    }

    aabb bounding_box() const {
        switch (kind) {
            case kind::box:
                return data.box;
            case kind::quad:
                return data.quad.bounding_box();
            case kind::sphere:
                return data.sphere.bounding_box();
        }
        std::unreachable();
    }

    // Returns something less than `minRayDist` when the ray does not hit.
    // TODO: write the result inconditionally everywhere.
    double hit(timed_ray const &r) const {
        // geometry is already transformed, so we can skip and set the actual
        // point.
        switch (kind) {
            case kind::box:
                return data.box.hit(r.r);
            case kind::sphere:
                return data.sphere.hit(r);
            case kind::quad:
                return data.quad.hit(r.r);
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

    vec3 getNormal(point3 intersection, double time) const {
        switch (kind) {
            case kind::box:
                return data.box.getNormal(intersection);
            case kind::sphere:
                return data.sphere.getNormal(intersection, time);
            case kind::quad:
                return data.quad.getNormal();
        }
    };
};

struct traversable_geometry {
    enum class kind : int { box, sphere } kind;
    union {
        sphere sphere;
        aabb box;
    } data;

    traversable_geometry(sphere sph)
        : kind(kind::sphere), data{.sphere = sph} {}
    traversable_geometry(aabb box) : kind(kind::box), data{.box = box} {}

    // End to end traversal of the geometry, just taking into account the
    // direction and the origin point. The intersection is geometric based
    // (distance), not relative to the ray's "speed" on each direction.
    interval traverse(timed_ray const &r) const {
        switch (kind) {
            case kind::box:
                return data.box.traverse(r.r);
            case kind::sphere:
                return data.sphere.traverse(r);
        }
    };

    static traversable_geometry from_geometry(geometry g) {
        switch (g.kind) {
            case geometry::kind::box:
                return g.data.box;
            case geometry::kind::sphere:
                return g.data.sphere;
            case geometry::kind::quad:
                std::unreachable();
        }
    }
};

template <typename It>
concept geometry_iterator = std::input_iterator<It> && requires(It const &it) {
    // we want references because copying the geometries in the loop made it ~1.8x slower.
    { *it } -> std::same_as<geometry const&>;
};

template <geometry_iterator It>
inline std::pair<geometry const *, double> hitSpan(It start, It end,
                                                   timed_ray const &r,
                                                   geometry const *best,
                                                   double closestHit) {
    ZoneScopedNC("hit span", Ctp::Green);
    ZoneValue(objects.size());

    // @perf I need a perf report on this.
    // Tracy says that the loop overhead takes between 40-60% of the runtime.
    // I don't know more.

    for (auto it = start; it != end; ++it) {
        auto const &object = *it;
        auto res = object.hit(r);
        if (interval{minRayDist, closestHit}.contains(res)) {
            best = &object;
            closestHit = res;
        }
    }

    return {best, closestHit};
}

// @perf get rid of this as I start providing better iteration options to the loop.
inline std::pair<geometry const *, double> hitSpan(std::span<geometry const> objects, timed_ray const &r, geometry const *best, double closestHit) {
    return hitSpan(std::begin(objects), std::end(objects), r, best, closestHit);
}
