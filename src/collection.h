#pragma once

// Abstraction over a collection. It's used to pack multiple hittables,
// for caching & type homogeneity benefits.

#include "geometry.h"
#include "transform.h"
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