#pragma once

#include <cstdint>
#include <iterator>
#include <bit>
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

enum class geometry_kind : int { box, sphere, quad };
struct geometry {
    int relIndex;
    geometry_kind kind;

    union {
        sphere sphere;
        quad quad;
        aabb box;
    } data;

    geometry(sphere sph) : kind(geometry_kind::sphere), data{.sphere = sph} {}
    geometry(quad q) : kind(geometry_kind::quad), data{.quad = q} {}
    geometry(aabb b) : kind(geometry_kind::box), data{.box = b} {}

    void applyTransform(transform tf) {
        switch (kind) {
            case geometry_kind::sphere:
                data.sphere = sphere::applyTransform(data.sphere, tf);
                break;
            case geometry_kind::quad:
                data.quad = quad::applyTransform(data.quad, tf);
                break;
            case geometry_kind::box:
                data.box = tf.applyForward(data.box);
                break;
        }
    }

    aabb bounding_box() const {
        switch (kind) {
            case geometry_kind::box:
                return data.box;
            case geometry_kind::quad:
                return data.quad.bounding_box();
            case geometry_kind::sphere:
                return data.sphere.bounding_box();
        }
        std::unreachable();
    }
};

// Pointer to a geometry object.
// Keeps the variant-like behavior without forcing memory layout
struct geometry_ptr {
    geometry_kind kind;
    int relIndex;
    union _ptrs {
        sphere const *sphere;
        aabb const *box;
        quad const *quad;

        constexpr _ptrs(struct sphere const *sph) : sphere(sph) {}
        constexpr _ptrs(aabb const *box) : box(box) {}
        constexpr _ptrs(struct quad const *q) : quad(q) {}
        constexpr _ptrs() = default;

    } ptr;

    constexpr geometry_ptr() = default;

    constexpr geometry_ptr(sphere const *sph)
        : kind(geometry_kind::sphere), ptr(sph) {}
    constexpr geometry_ptr(aabb const *box)
        : kind(geometry_kind::box), ptr(box) {}
    constexpr geometry_ptr(quad const *q) : kind(geometry_kind::quad), ptr(q) {}

    constexpr operator bool() const {
        return std::bit_cast<uint64_t>(ptr) != 0;
    }

    constexpr geometry_ptr(std::nullptr_t) : geometry_ptr() {}

    constexpr geometry_ptr(geometry const *gp)
        : kind(gp->kind), relIndex(gp->relIndex) {
        switch (gp->kind) {
            case geometry_kind::box:
                new (&ptr) _ptrs(&gp->data.box);
            case geometry_kind::sphere:
                new (&ptr) _ptrs(&gp->data.sphere);
            case geometry_kind::quad:
                new (&ptr) _ptrs(&gp->data.quad);
        }
    }

    constexpr geometry_ptr(geometry const &gpref) : geometry_ptr(&gpref) {}

    vec3 getNormal(point3 const &__restrict intersection, double time) const {
        switch (kind) {
            case geometry_kind::box:
                return ptr.box->getNormal(intersection);
            case geometry_kind::quad:
                return ptr.quad->getNormal();
            case geometry_kind::sphere:
                return ptr.sphere->getNormal(intersection, time);
        }
    }

    uvs getUVs(point3 const &__restrict intersection,
               point3 const &__restrict normal) const {
        switch (kind) {
            case geometry_kind::box:
                return ptr.box->getUVs(intersection);
            case geometry_kind::sphere:
                return ptr.sphere->getUVs(normal);
            case geometry_kind::quad:
                return ptr.quad->getUVs(intersection);
        }
    }
    // Returns something less than `minRayDist` when the ray does not hit.
    // TODO: write the result inconditionally everywhere.
    double hit(timed_ray const &r) const {
        // geometry is already transformed, so we can skip and set the actual
        // point.
        switch (kind) {
            case geometry_kind::box:
                return ptr.box->hit(r.r);
            case geometry_kind::sphere:
                return ptr.sphere->hit(r);
            case geometry_kind::quad:
                return ptr.quad->hit(r.r);
        }
    }
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
            case geometry_kind::box:
                return g.data.box;
            case geometry_kind::sphere:
                return g.data.sphere;
            case geometry_kind::quad:
                std::unreachable();
        }
    }
};

template <typename It>
concept geometry_iterator = std::input_iterator<It> && requires(It const &it) {
    // we want references because copying the geometries in the loop made it
    // ~1.8x slower.
    { *it } -> std::convertible_to<geometry_ptr>;
};

template <geometry_iterator It>
inline std::pair<geometry_ptr, double> hitSpan(It start, It end,
                                               timed_ray const &r,
                                               geometry_ptr best,
                                               double closestHit) {
    ZoneScopedNC("hit span", Ctp::Green);
    ZoneValue(objects.size());

    // @perf I need a perf report on this.
    // Tracy says that the loop overhead takes between 40-60% of the runtime.
    // I don't know more.

    for (auto it = start; it != end; ++it) {
        geometry_ptr const object = *it;
        auto res = object.hit(r);
        if (interval{minRayDist, closestHit}.contains(res)) {
            best = object;
            closestHit = res;
        }
    }

    return {best, closestHit};
}

// @perf get rid of this as I start providing better iteration options to the
// loop.
inline std::pair<geometry_ptr, double> hitSpan(
    std::span<geometry const> objects, timed_ray const &r, geometry_ptr best,
    double closestHit) {
    return hitSpan(std::begin(objects), std::end(objects), r, best, closestHit);
}
