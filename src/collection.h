#pragma once
#include <concepts>
#include <span>
#include <type_traits>

#include "hittable.h"

// NOTE: On making hittable_collection explicit on collection:
// It may force the compiler to produce multiple instantiations for each of the
// unused functions. Compile times could skyrocket.

// NOTE: Maybe it's wise to select the best one in layers when I have a layering
// builder. Perhaps the important thing next is to 'refactor' poly_list into
// a builder, and redirect all main.cc operations through it. We can already
// swap the storage, and the next thing is to swap the building method (which
// for geometry will be the storage, too!).

// NOTE: Should I make it a tagged union?
// Given that there is only BVH and hittable view (and hittable list while it's
// not a builder), it would remove one layer of indirection.

struct hit_status {
    bool hit_anything;
    interval ray_t;

    explicit hit_status(interval initial_intv)
        : hit_anything(false), ray_t(initial_intv) {}
};

// Abstraction over a collection. Will be used as I prototype different ways of
// fitting the final geometries into a separate structure, so that I con move
// material & texture information to other places.

// TODO: move out from base class
template <typename T>
concept is_collection =
    requires(T const &coll, ray const &r, hit_status &status, hit_record &rec) {
        { coll.propagate(r, status, rec) } -> std::same_as<void>;
    } && requires(T const &coll) {
        { coll.aggregate_box() } -> std::same_as<aabb>;
    };

namespace detail::collection::erased {
template <is_collection T>
static void propagate(void const *ptr, ray const &r, hit_status &status,
                      hit_record &rec) {
    T const &p = *reinterpret_cast<T const *>(ptr);
    return p.propagate(r, status, rec);
}

template <is_collection T>
static aabb aggregate_box(void const *ptr) {
    T const &p = *reinterpret_cast<T const *>(ptr);
    return p.aggregate_box();
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
template <is_collection T>
struct hittable_collection final {
    T wrapped;

    explicit constexpr hittable_collection(T wrapped)
        : wrapped(std::move(wrapped)) {}

    [[nodiscard]] aabb boundingBox() const & { return wrapped.aggregate_box(); }
    bool hit(ray const &r, interval &ray_t, hit_record &rec) const {
        hit_status status{ray_t};
        wrapped.propagate(r, status, rec);
        return status.hit_anything;
    }
};

// TODO: these wrappers should be named 'pointer collection" or sth like that.
struct dyn_collection final {
    void const *ptr;
    void (*propagate_pfn)(void const *, ray const &, hit_status &,
                          hit_record &);
    aabb (*box_pfn)(void const *);

    template <is_collection T>
    constexpr dyn_collection(T const *poly)
        : ptr(poly),
          propagate_pfn(detail::collection::erased::propagate<T>),
          box_pfn(detail::collection::erased::aggregate_box<T>) {}

    [[nodiscard]] aabb aggregate_box() const & noexcept { return box_pfn(ptr); }

    void propagate(ray const &r, hit_status &status, hit_record &rec) const & {
        return propagate_pfn(ptr, r, status, rec);
    }
};

// TODO: geometry-only collections?
// NOTE: BVH is somewhat a filter, but it has memory.
template <is_geometry T>
struct single_tex_wrapper {
    std::span<T const> objects;
};

using hittable_poly_collection = hittable_collection<dyn_collection>;
