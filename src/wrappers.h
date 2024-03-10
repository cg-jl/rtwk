#pragma once

#include "aabb.h"
#include "collection.h"
#include "geometry.h"
#include "material.h"
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

template <typename T>
struct time_wrapper {
    T wrapped;

    explicit time_wrapper(T wrapped) : wrapped(std::move(wrapped)) {}

    constexpr operator T const &() { return wrapped; }

    [[nodiscard]] aabb boundingBox() const &
        requires(has_bb<T>)
    {
        return wrapped.boundingBox();
    }

    void propagate(ray const &r, hit_status &status, hit_record &rec,
                   float _time) const &
        requires(time_invariant_collection<T>)
    {
        return wrapped.propagate(r, status, rec);
    }

    bool hit(ray const &r, interval &ray_t, hit_record &rec,
             float _time) const &
        requires(time_invariant_hittable<T>)
    {
        return wrapped.hit(r, ray_t, rec);
    }
};
