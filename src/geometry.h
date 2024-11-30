#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <iterator>
#include <ranges>
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

enum class geometry_kind : int { box,
    sphere,
    quad };
struct geometry {
    int relIndex;
    geometry_kind kind;

    union {
        sphere sphere;
        quad quad;
        aabb box;
    } data;

    geometry(sphere sph)
        : kind(geometry_kind::sphere)
        , data { .sphere = sph }
    {
    }
    geometry(quad q)
        : kind(geometry_kind::quad)
        , data { .quad = q }
    {
    }
    geometry(aabb b)
        : kind(geometry_kind::box)
        , data { .box = b }
    {
    }

    void applyTransform(transform tf)
    {
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

    aabb bounding_box() const
    {
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

        constexpr _ptrs(struct sphere const *sph)
            : sphere(sph)
        {
        }
        constexpr _ptrs(aabb const *box)
            : box(box)
        {
        }
        constexpr _ptrs(struct quad const *q)
            : quad(q)
        {
        }
        constexpr _ptrs() = default;

    } ptr;

    constexpr geometry_ptr() = default;

    constexpr geometry_ptr(sphere const *sph)
        : kind(geometry_kind::sphere)
        , ptr(sph)
    {
    }
    constexpr geometry_ptr(aabb const *box)
        : kind(geometry_kind::box)
        , ptr(box)
    {
    }
    constexpr geometry_ptr(quad const *q)
        : kind(geometry_kind::quad)
        , ptr(q)
    {
    }

    constexpr operator bool() const
    {
        return std::bit_cast<uint64_t>(ptr) != 0;
    }

    constexpr geometry_ptr(std::nullptr_t)
        : geometry_ptr()
    {
    }

    constexpr geometry_ptr(geometry const *gp)
        : kind(gp->kind)
        , relIndex(gp->relIndex)
    {
        switch (gp->kind) {
        case geometry_kind::box:
            new (&ptr) _ptrs(&gp->data.box);
        case geometry_kind::sphere:
            new (&ptr) _ptrs(&gp->data.sphere);
        case geometry_kind::quad:
            new (&ptr) _ptrs(&gp->data.quad);
        }
    }

    constexpr geometry_ptr(geometry const &gpref)
        : geometry_ptr(&gpref)
    {
    }

    vec3 getNormal(point3 const &__restrict intersection, float time) const
    {
        switch (kind) {
        case geometry_kind::box:
            return ptr.box->getNormal(intersection);
        case geometry_kind::quad:
            return ptr.quad->getNormal();
        case geometry_kind::sphere:
            return ptr.sphere->getNormal(intersection, time);
        }
    }
};

struct traversable_geometry {
    enum class kind : int { box,
        sphere } kind;
    union {
        sphere sphere;
        aabb box;
    } data;

    traversable_geometry(sphere sph)
        : kind(kind::sphere)
        , data { .sphere = sph }
    {
    }
    traversable_geometry(aabb box)
        : kind(kind::box)
        , data { .box = box }
    {
    }

    // End to end traversal of the geometry, just taking into account the
    // direction and the origin point. The intersection is geometric based
    // (distance), not relative to the ray's "speed" on each direction.

    void traverse(ray const *rays, float const *times, uint32 const len, interval *traversals) const
    {
        switch (kind) {

        case kind::box:
            // @perf bulk box traversal
            data.box.traverse(rays, len, traversals);
            break;
        case kind::sphere:
            data.sphere.traverse(rays, times, len, traversals);
            break;
        }
    }

    static traversable_geometry from_geometry(geometry g)
    {
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

struct hit_span_buf {
    geometry_ptr *ptr;
    float *dist;

    auto constexpr zip(auto end, decltype(end) start = 0) const noexcept
    {
        return std::ranges::views::zip(
            std::ranges::subrange(ptr + start, ptr + end),
            std::ranges::subrange(dist + start, dist + end));
    }

    void swap(auto const i, auto const k)
    {
        std::swap(ptr[i], ptr[k]);
        std::swap(dist[i], dist[k]);
    }

    static hit_span_buf request(uint32 const spp)
    {
        return {
            .ptr = new geometry_ptr[spp],
            .dist = new float[spp],
        };
    }
};

inline void hitSpan(std::span<geometry const> objects, ray_buffer rays, uint32 len, hit_span_buf acc, float *backbuf)
{
    for (auto const &obj : objects) {
        auto ptr = geometry_ptr(obj);
        // @perf bulk geometry hit :]
        switch (ptr.kind) {
        case geometry_kind::box:
            ptr.ptr.box->hit(rays.rays, len, backbuf);
            break;
        case geometry_kind::sphere: {
            ptr.ptr.sphere->hit(rays.rays, rays.times, len, backbuf);
            break;
        }
        case geometry_kind::quad:
            ptr.ptr.quad->hit(rays.rays, len, backbuf);
            break;
        }

        // @perf could be swapping things around ?
        for (decltype(len) i = 0; i < len; ++i) {
            auto const closestHit = acc.dist[i];
            auto const new_d = backbuf[i];
            if (interval { minRayDist, closestHit }.contains(new_d)) {
                acc.ptr[i] = ptr;
                acc.dist[i] = new_d;
            }
        }
    }
}
