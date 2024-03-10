#pragma once

#include "hittable.h"
#include "soa.h"
#include "transform.h"

// Abstraction over a collection. It's used to pack multiple hittables,
// for caching & type homogeneity benefits.

struct hit_status {
    bool hit_anything;
    interval ray_t;

    explicit hit_status(interval initial_intv)
        : hit_anything(false), ray_t(initial_intv) {}
};

// TODO: move out from base class
template <typename T>
concept is_collection = requires(T const &coll, ray const &r,
                                 hit_status &status, hit_record &rec,
                                 float time) {
    { coll.propagate(r, status, rec, time) } -> std::same_as<void>;
} && has_bb<T>;

template <typename T>
concept time_invariant_collection =
    requires(T const &coll, ray const &r, hit_status &status, hit_record &rec) {
        { coll.propagate(r, status, rec) } -> std::same_as<void>;
    };

namespace detail::collection::erased {
template <is_collection T>
static void propagate(void const *ptr, ray const &r, hit_status &status,
                      hit_record &rec, float time) {
    T const &p = *reinterpret_cast<T const *>(ptr);
    return p.propagate(r, status, rec, time);
}

template <time_invariant_collection T>
static void propagate_time(void const *ptr, ray const &r, hit_status &status,
                           hit_record &rec, float _time) {
    T const &p = *reinterpret_cast<T const *>(ptr);
    return p.propagate(r, status, rec);
}

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
    bool hit(ray const &r, interval &ray_t, hit_record &rec, float time) const &
        requires(is_collection<T>)
    {
        hit_status status{ray_t};
        wrapped.propagate(r, status, rec, time);
        return status.hit_anything;
    }
};

// TODO: these wrappers should be named 'pointer collection" or sth like that.
struct dyn_collection final {
    void const *ptr;
    void (*propagate_pfn)(void const *, ray const &, hit_status &, hit_record &,
                          float);
    aabb (*box_pfn)(void const *);

    template <is_collection T>
    constexpr dyn_collection(T const *poly)
        : ptr(poly),
          propagate_pfn(detail::collection::erased::propagate<T>),
          box_pfn(detail::collection::erased::boundingBox<T>) {}

    template <time_invariant_collection T>
    constexpr dyn_collection(T const *ptr)
        : ptr(ptr),
          propagate_pfn(detail::collection::erased::propagate_time<T>),
          box_pfn(detail::collection::erased::boundingBox<T>) {}

    [[nodiscard]] aabb boundingBox() const & noexcept { return box_pfn(ptr); }

    void propagate(ray const &r, hit_status &status, hit_record &rec,
                   float time) const & {
        return propagate_pfn(ptr, r, status, rec, time);
    }
};

using hittable_poly_collection = hittable_collection<dyn_collection>;

template <is_hittable... Ts>
struct soa_collection {
    soa::span<Ts...> span;

    [[nodiscard]] aabb boundingBox() const & {
        aabb bb = empty_bb;

        ((bb = aabb(bb, view<Ts>(span.template get<Ts>()).boundingBox())), ...);

        return bb;
    }

    void propagate(ray const &r, hit_status &status, hit_record &rec,
                   float time) const & {
        (view<Ts>(span.template get<Ts>()).propagate(r, status, rec, time),
         ...);
    }
};
