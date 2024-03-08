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
concept is_collection =
    requires(T const &coll, ray const &r, hit_status &status, hit_record &rec) {
        { coll.propagate(r, status, rec) } -> std::same_as<void>;
    } && has_bb<T>;

namespace detail::collection::erased {
template <is_collection T>
static void propagate(void const *ptr, ray const &r, hit_status &status,
                      hit_record &rec) {
    T const &p = *reinterpret_cast<T const *>(ptr);
    return p.propagate(r, status, rec);
}

template <is_collection T>
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
template <is_collection T>
struct hittable_collection final {
    T wrapped;

    explicit constexpr hittable_collection(T wrapped)
        : wrapped(std::move(wrapped)) {}

    [[nodiscard]] aabb boundingBox() const & { return wrapped.boundingBox(); }
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
          box_pfn(detail::collection::erased::boundingBox<T>) {}

    [[nodiscard]] aabb boundingBox() const & noexcept { return box_pfn(ptr); }

    void propagate(ray const &r, hit_status &status, hit_record &rec) const & {
        return propagate_pfn(ptr, r, status, rec);
    }
};

template <is_geometry_collection Coll>
struct geometry_collection_wrapper final {
    Coll coll;
    material mat;
    texture tex;

    geometry_collection_wrapper(Coll coll, material mat, texture tex)
        : coll(std::move(coll)), mat(std::move(mat)), tex(tex) {}

    [[nodiscard]] aabb boundingBox() const & { return coll.boundingBox(); }

    void propagate(ray const &r, hit_status &status, hit_record &rec) const & {
        typename Coll::Type const *ptr = coll.hit(r, rec.geom, status.ray_t);
        status.hit_anything |= ptr != nullptr;
        if (ptr != nullptr) {
            typename Coll::Type const &g = *ptr;
            g.getUVs(rec.geom, rec.u, rec.v);
            rec.xforms = g.getTransforms();
            rec.tex = tex;
            rec.mat = mat;
        }
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

    void propagate(ray const &r, hit_status &status, hit_record &rec) const & {
        (view<Ts>(span.template get<Ts>()).propagate(r, status, rec), ...);
    }
};

template <typename T>
struct transformed_collection final {
    std::span<transform const> transf;
    T coll;

    transformed_collection(std::span<transform const> transf, T coll)
        : transf(transf), coll(std::move(coll)) {}

    [[nodiscard]] aabb boundingBox() const & {
        aabb box = coll.boundingBox();
        for (auto const &tf : transf) {
            tf.apply_to_bbox(box);
        }
        return box;
    }

    void propagate(ray const &r, hit_status &status, hit_record &rec) const & {
        ray r_copy = r;

        for (auto const &tf : transf) {
            tf.apply(r_copy, r.time);
        }

        auto hit_before = status.hit_anything;
        status.hit_anything = false;
        coll.propagate(r_copy, status, rec);

        if (status.hit_anything) {
            rec.xforms = transf;
        }

        status.hit_anything |= hit_before;
    }
};
