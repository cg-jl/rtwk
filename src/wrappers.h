#pragma once

#include <tuple>

#include "aabb.h"
#include "collection.h"
#include "geometry.h"
#include "hittable.h"
#include "material.h"
#include "soa.h"
#include "texture.h"

template <typename T>
struct tex_wrapper final {
    T wrapped;
    material mat;
    texture tex;

    tex_wrapper(T wrapped, material mat, texture tex)
        : wrapped(std::move(wrapped)), mat(std::move(mat)), tex(tex) {}

    [[nodiscard]] aabb boundingBox() const &
        requires(has_bb<T>)
    {
        return wrapped.boundingBox();
    }

    bool hit(ray const &r, interval &ray_t, hit_record &rec) const &
        requires(is_geometry<T>)
    {
        auto did_hit = wrapped.hit(r, rec.geom, ray_t);
        if (did_hit) {
            wrapped.getUVs(rec.geom, rec.u, rec.v);
            rec.mat = mat;
            rec.tex = tex;
        }
        return did_hit;
    }

    void propagate(ray const &r, hit_status &status, hit_record &rec) const &
        requires(is_geometry_collection<T>)
    {
        typename T::Type const *ptr = wrapped.hit(r, rec.geom, status.ray_t);
        status.hit_anything |= ptr != nullptr;
        if (ptr != nullptr) {
            typename T::Type const &g = *ptr;
            g.getUVs(rec.geom, rec.u, rec.v);
            rec.tex = tex;
            rec.mat = mat;
        }
    }

    constexpr operator T const &() { return wrapped; }
};

namespace time_wrapper_impls {
template <time_invariant_hittable H>
static bool hit(H const &h, ray const &r, interval &ray_t, hit_record &rec,
                transform_set &xforms) {
    auto did_hit = h.hit(r, ray_t, rec);
    if (did_hit) xforms = {};
    return did_hit;
}

template <time_invariant_collection C>
static void propagate(C const &c, ray const &r, hit_status &status,
                      hit_record &rec, transform_set &xforms) {
    hit_status clean_status{status.ray_t};
    c.propagate(r, clean_status, rec);
    if (clean_status.hit_anything) xforms = {};
    status.hit_anything |= clean_status.hit_anything;
    status.ray_t = clean_status.ray_t;
}
}  // namespace time_wrapper_impls

template <typename T>
struct time_wrapper {
    T wrapped;

    explicit time_wrapper(T wrapped) : wrapped(std::move(wrapped)) {}

    constexpr operator T const &() { return wrapped; }

    void propagate(ray const &r, hit_status &status, hit_record &rec,
                   transform_set &xforms, float _time) const &
        requires(time_invariant_collection<T>)
    {
        time_wrapper_impls::propagate(wrapped, r, status, rec, xforms);
    }

    bool hit(ray const &r, interval &ray_t, hit_record &rec,
             transform_set &xforms, float _time) const &
        requires(time_invariant_hittable<T>)
    {
        return time_wrapper_impls::hit(wrapped, r, ray_t, rec, xforms);
    }
};

template <typename... Ts>
struct tuple_wrapper : soa::detail::type_indexable_tuple<Ts...> {
    constexpr tuple_wrapper(Ts &&...values)
        : soa::detail::type_indexable_tuple<Ts...>(
              std::forward<Ts &&>(values)...) {}

    // NOTE: it doesn't matter that we carry the `time` parameter here because
    // this call isn't being made at all. It is inlined.
    template <typename T>
    static void propagate_choose(T const &t, ray const &r, hit_status &status,
                                 hit_record &rec, transform_set &xforms,
                                 float time) {
        if constexpr (is_hittable<T>) {
            status.hit_anything |= t.hit(r, status.ray_t, rec, xforms, time);
        } else if constexpr (is_collection<T>) {
            return t.propagate(r, status, rec, xforms, time);
        } else if constexpr (time_invariant_hittable<T>) {
            status.hit_anything |=
                time_wrapper_impls::hit(t, r, status.ray_t, rec, xforms);
        } else {
            static_assert(time_invariant_collection<T>);

            return time_wrapper_impls::propagate(t, r, status, rec, xforms);
        }
    }

    void propagate(ray const &r, hit_status &status, hit_record &rec,
                   transform_set &xforms, float time) const &
        requires((is_hittable<Ts> || is_collection<Ts> ||
                  time_invariant_hittable<Ts> ||
                  time_invariant_collection<Ts>) &&
                 ...)
    {
        ((propagate_choose<Ts>(this->template get<Ts>(), r, status, rec, xforms,
                               time)),
         ...);
    }
};

template <time_invariant_collection T>
static void propagate_time(void const *ptr, ray const &r, hit_status &status,
                           hit_record &rec, transform_set &xforms,
                           float _time) {
    T const &p = *reinterpret_cast<T const *>(ptr);
    return time_wrapper_impls::propagate(p, r, status, rec, xforms);
}
