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
// Some "hittable"s are collections of objects, like BVH, hittable view and
// hittable list. Since BVH is the only 'weird' one, it will act as a 'filter'
// to select which objects are finally tested. BVHs are only used in geometry
// though.
//
// Last, I could separate storage for different types of objects, although then
// a BVH that goes over a disjoint set of objects might be more difficult to
// implement (other than having 3 BVHs). Specialization here might give this a
// nice boost, so it may be a good idea!

#include <cmath>
#include <utility>

#include "aabb.h"
#include "hit_record.h"
#include "rtweekend.h"
#include "texture.h"

struct material;

// NOTE: each shared_ptr occupies 16 bytes. What if we move that to 8 bytes by
// just having a const *?

struct light_info {
    texture const* tex{};
    point3 p;
    float u{}, v{};
};

// NOTE: material could be the next thing to make common on all hittables so it
// can be migrated to somewhere else! Reason is that we don't need to access the
// material pointer until we're sure that it's the correct one. By doing this
// we'll ensure a cache miss, but we have one cache miss per hit already
// guaranteed, so it may give more than it takes.

// NOTE: hittable occupies 8 bytes and adds always a pointer indirection, just
// to get a couple of functions. What about removing one of the indirections?

struct hittable {
    // NOTE: is it smart to force one cacheline per hittable?
    // Now that we've reduced numbers to half the size, there is more space
    // available.
    // It would be nice, since then we could modify storage to inline the
    // structs into cachelines.

    virtual ~hittable() = default;
    [[nodiscard]] virtual aabb bounding_box() const& = 0;

    virtual bool hit(ray const& r, interval& ray_t, hit_record& rec) const = 0;

    bool hit(ray const& r, hit_record& rec) const& {
        auto bb = bounding_box();
        auto absolute_max_dist = (point3(bb.x.max, bb.y.max, bb.z.max) -
                                  point3(bb.x.min, bb.y.min, bb.z.min))
                                     .length_squared();
        interval t(0.001, absolute_max_dist);
        return hit(r, t, rec);
    }
};
