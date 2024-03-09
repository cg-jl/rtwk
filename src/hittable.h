#pragma once
//==============================================================================================
// Originally written in 2016 by Peter Shirley <ptrshrl@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public
// Domain Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

// NOTE: Had an idea for a system closer to ECS:
// - each object has a set of properties, including:
//      - geometry. This determines if and where in the surface the object got
//      hit by a ray.
//      - geometry instances. They are transformations to the ray. They will be
//      first run forward and then reversed when the point, UVs and normal are
//      determined. Since the minimum distance isn't modified by any of those,
//      we can just run the reverse direction once!
//      - geometric material. This determines how the ray will bounce off of the
//      object.
//      - texture sampler. This determines the color that the ray is multiplied
//      by. Some joint materials like dielectric or metal don't accept any
//      texturer.
//
// Last, I could separate storage for different types of objects, although then
// a BVH that goes over a disjoint set of objects might be more difficult to
// implement (other than having 3 BVHs). Specialization here might give this a
// nice boost, so it may be a good idea!

#include <utility>

#include "aabb.h"
#include "geometry.h"
#include "hit_record.h"
#include "interval.h"
#include "texture.h"
#include "transform.h"

// NOTE: There are two kinds of hittables:
// those that are wrapped by a transform layer
// those that aren't
// Is it worth it to separate the concept into two?

template <typename T>
concept is_hittable = requires(T const& ob, ray const& r, interval& ray_t,
                               hit_record& rec, float time) {
    { ob.hit(r, ray_t, rec, time) } -> std::same_as<bool>;
} && has_bb<T>;

namespace erase::hittable {
template <is_hittable H>
static bool hit(void const* ptr, ray const& r, interval& ray_t, hit_record& rec,
                float time) {
    H const& h = *reinterpret_cast<H const*>(ptr);
    return h.hit(r, ray_t, rec, time);
}

template <is_hittable H>
static aabb boundingBox(void const* ptr) {
    H const& h = *reinterpret_cast<H const*>(ptr);
    return h.boundingBox();
}

}  // namespace erase::hittable

struct dyn_hittable final {
    void const* ptr;
    bool (*hit_pfn)(void const*, ray const&, interval&, hit_record&, float);
    aabb (*box_pfn)(void const*);

    template <is_hittable H>
    explicit constexpr dyn_hittable(H const* ptr)
        : ptr(ptr),
          hit_pfn(erase::hittable::hit<H>),
          box_pfn(erase::hittable::boundingBox<H>) {}

    [[nodiscard]] aabb boundingBox() const& { return box_pfn(ptr); }
    bool hit(ray const& r, interval& ray_t, hit_record& rec, float time) const {
        return hit_pfn(ptr, r, ray_t, rec, time);
    }
};

template <is_geometry T>
struct geometry_wrapper final {
    T geom;
    material mat;
    texture tex;

    geometry_wrapper(T geom, material mat, texture tex)
        : geom(std::move(geom)), mat(std::move(mat)), tex(tex) {}

    [[nodiscard]] aabb boundingBox() const& { return geom.boundingBox(); }
    bool hit(ray const& r, interval& ray_t, hit_record& rec,
             float _time) const {
        auto did_hit = geom.hit(r, rec.geom, ray_t);
        if (did_hit) {
            geom.getUVs(rec.geom, rec.u, rec.v);
            rec.mat = mat;
            rec.tex = tex;
        }
        return did_hit;
    }
};

template <is_hittable T>
struct transformed_hittable final {
    std::span<transform const> transf;
    T object;

    transformed_hittable(std::span<transform const> transf, T object)
        : transf(transf), object(std::move(object)) {}

    [[nodiscard]] aabb boundingBox() const& {
        aabb box = object.boundingBox();
        for (auto const& tf : transf) {
            tf.apply_to_bbox(box);
        }
        return box;
    }

    bool hit(ray const& r, interval& ray_t, hit_record& rec, float time) const {
        ray r_copy = r;

        for (auto const& tf : transf) {
            tf.apply(r_copy, time);
        }

        // NOTE: ideally there is only one transform layer here so time shouldn't be used.
        auto did_hit = object.hit(r_copy, ray_t, rec, time);
        if (did_hit) rec.xforms = transf;
        return did_hit;
    }
};
