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

#include <cmath>
#include <utility>

#include "aabb.h"
#include "geometry.h"
#include "hit_record.h"
#include "rtweekend.h"
#include "texture.h"

// NOTE: material could be the next thing to make common on all hittables so it
// can be migrated to somewhere else! Reason is that we don't need to access the
// material pointer until we're sure that it's the correct one. By doing this
// we'll ensure a cache miss, but we have one cache miss per hit already
// guaranteed, so it may give more than it takes.

// NOTE: hittable occupies 8 bytes and adds always a pointer indirection, just
// to get a couple of functions. What about removing one of the indirections?

template <typename T>
concept is_hittable =
    requires(T const& ob, ray const& r, interval& ray_t, hit_record& rec) {
        { ob.hit(r, ray_t, rec) } -> std::same_as<bool>;
    } && has_bb<T>;

namespace erase::hittable {
template <is_hittable H>
static bool hit(void const* ptr, ray const& r, interval& ray_t,
                hit_record& rec) {
    H const& h = *reinterpret_cast<H const*>(ptr);
    return h.hit(r, ray_t, rec);
}

template <is_hittable H>
static aabb boundingBox(void const* ptr) {
    H const& h = *reinterpret_cast<H const*>(ptr);
    return h.boundingBox();
}

}  // namespace erase::hittable

struct dyn_hittable final {
    void const* ptr;
    bool (*hit_pfn)(void const*, ray const&, interval&, hit_record&);
    aabb (*box_pfn)(void const*);

    template <is_hittable H>
    explicit constexpr dyn_hittable(H const* ptr)
        : ptr(ptr),
          hit_pfn(erase::hittable::hit<H>),
          box_pfn(erase::hittable::boundingBox<H>) {}

    [[nodiscard]] aabb boundingBox() const& { return box_pfn(ptr); }
    bool hit(ray const& r, interval& ray_t, hit_record& rec) const {
        return hit_pfn(ptr, r, ray_t, rec);
    }
};

template <is_geometry T>
struct geometry_wrapper final {
    T geom;
    material mat;
    texture const* tex;

    geometry_wrapper(T geom, material mat, texture* tex)
        : geom(std::move(geom)), mat(std::move(mat)), tex(tex) {}

    [[nodiscard]] aabb boundingBox() const& { return geom.boundingBox(); }
    bool hit(ray const& r, interval& ray_t, hit_record& rec) const {
        auto did_hit = geom.hit(r, rec.geom, ray_t);
        if (did_hit) {
            geom.getUVs(rec.geom, rec.u, rec.v);
            rec.xforms = geom.getTransforms();
            rec.mat = mat;
            rec.tex = tex;
        }
        return did_hit;
    }
};
