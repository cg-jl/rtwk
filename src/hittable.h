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
#include "collection.h"
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
concept is_hittable =
    requires(T const& ob, ray const& r, interval& ray_t, hit_record& rec,
             transform_set& xforms, float time) {
        { ob.hit(r, ray_t, rec, xforms, time) } -> std::same_as<bool>;
    };

template <typename T>
concept time_invariant_hittable =
    requires(T const& ob, ray const& r, interval& ray_t, hit_record& rec) {
        { ob.hit(r, ray_t, rec) } -> std::same_as<bool>;
    };

namespace erase::hittable {
template <is_hittable H>
static bool hit(void const* ptr, ray const& r, interval& ray_t, hit_record& rec,
                transform_set& xforms, float time) {
    H const& h = *reinterpret_cast<H const*>(ptr);
    return h.hit(r, ray_t, rec, xforms, time);
}

}  // namespace erase::hittable

struct dyn_hittable final {
    void const* ptr;
    bool (*hit_pfn)(void const*, ray const&, interval&, hit_record&,
                    transform_set&, float);

    template <is_hittable H>
    explicit constexpr dyn_hittable(H const* ptr)
        : ptr(ptr), hit_pfn(erase::hittable::hit<H>) {}

    bool hit(ray const& r, interval& ray_t, hit_record& rec,
             transform_set& xforms, float time) const {
        return hit_pfn(ptr, r, ray_t, rec, xforms, time);
    }
};

template <typename T>
struct transformed final {
    std::span<transform const> transf;
    T object;

    transformed(std::span<transform const> transf, T object)
        : transf(transf), object(std::move(object)) {}

    [[nodiscard]] aabb boundingBox() const&
        requires(has_bb<T>)
    {
        aabb box = object.boundingBox();
        for (auto const& tf : transf) {
            tf.apply_to_bbox(box);
        }
        return box;
    }

    ray transform_ray(ray const& orig, float time) const& {
        ray r_copy = orig;

        for (auto const& tf : transf) {
            tf.apply(r_copy, time);
        }
        return r_copy;
    }

    void propagate(ray const& orig, hit_status& status, hit_record& rec,
                   transform_set& xforms, float time) const&
        requires(time_invariant_collection<T>)
    {
        auto r = transform_ray(orig, time);

        hit_status clean_status{status.ray_t};
        object.propagate(r, clean_status, rec);
        if (clean_status.hit_anything) xforms = transf;
        status.hit_anything |= clean_status.hit_anything;
        status.ray_t = clean_status.ray_t;
    }

    bool hit(ray const& orig, interval& ray_t, hit_record& rec,
             transform_set& xforms, float time) const&
        requires(time_invariant_hittable<T>)
    {
        auto r = transform_ray(orig, time);

        auto did_hit = object.hit(r, ray_t, rec);
        if (did_hit) xforms = transf;
        return did_hit;
    }
};
