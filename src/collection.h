#pragma once

// Abstraction over a collection. It's used to pack multiple hittables,
// for caching & type homogeneity benefits.

#include "geometry.h"
#include "hit_record.h"
#include "interval.h"
#include "ray.h"
struct hit_status {
    bool hit_anything;
    interval ray_t;

    explicit hit_status(interval initial_intv)
        : hit_anything(false), ray_t(initial_intv) {}
};

// TODO: split hit_record so that xforms are only required on time dependent
// hittables/collections
template <typename T>
concept is_collection =
    requires(T const &coll, ray const &r, hit_status &status, hit_record &rec,
             transform_set &xforms, float time) {
        { coll.propagate(r, status, rec, xforms, time) } -> std::same_as<void>;
    };

template <typename T>
concept time_invariant_collection =
    requires(T const &coll, ray const &r, hit_status &status, hit_record &rec) {
        { coll.propagate(r, status, rec) } -> std::same_as<void>;
    };

namespace detail::collection::erased {
template <is_collection T>
static void propagate(void const *ptr, ray const &r, hit_status &status,
                      hit_record &rec, transform_set &xforms, float time) {
    T const &p = *reinterpret_cast<T const *>(ptr);
    return p.propagate(r, status, rec, xforms, time);
}

template <time_invariant_collection T>
static void propagate_time(void const *ptr, ray const &r, hit_status &status,
                           hit_record &rec, transform_set &xforms, float _time);

template <has_bb T>
static aabb boundingBox(void const *ptr) {
    T const &p = *reinterpret_cast<T const *>(ptr);
    return p.boundingBox();
}
}  // namespace detail::collection::erased

// NOTE: Should I restrict sets of transforms to different collections?
// Maybe it's not a good idea if I want to separate differently rotated objects
// very near each other? Or is it actually better, since "editing" the ray
// doesn't take as much as the intersections?

// Implementation of 'hittable' for any collection. Ideally this should be
// removed as I progress in dissecting the properties of hittables vs
// collections.
// It serves as a way to layer collections
template <typename T>
struct hittable_collection final {
    T wrapped;

    explicit constexpr hittable_collection(T wrapped)
        : wrapped(std::move(wrapped)) {}

    [[nodiscard]] aabb boundingBox() const &
        requires(has_bb<T>)
    {
        return wrapped.boundingBox();
    }

    bool hit(ray const &r, interval &ray_t, hit_record &rec) const &
        requires(time_invariant_collection<T>)
    {
        hit_status status{ray_t};
        wrapped.propagate(r, status, rec);
        return status.hit_anything;
    }

    bool hit(ray const &r, interval &ray_t, hit_record &rec,
             transform_set &xforms, float time) const &
        requires(is_collection<T>)
    {
        hit_status status{ray_t};
        wrapped.propagate(r, status, rec, xforms, time);
        return status.hit_anything;
    }
};
