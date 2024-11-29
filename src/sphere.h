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

#include <aabb.h>
#include <interval.h>
#include <ray.h>
#include <vec3.h>

#include "transforms.h"

// TODO: To instantiate spheres, I should separate instantiatable things
// and add a thread-private 'instantiate' buffer per worker thread where I can
// push any transformations. When separating list-of-ptrs into tagged arrays,
// these 'instantiate buffers' must also be separated.
struct sphere final {
    // Stationary Sphere
    sphere(point3 const &center, float radius)
        : center1(center)
        , radius(fmax(0, radius))
    {
    }

    // Moving Sphere
    sphere(point3 const &center1, point3 const &center2, float radius)
        : center1(center1)
        , radius(fmax(0, radius))
    {
        center_vec = center2 - center1;
    }

    // @perf move sphere center compute to caller. That way we can cache it :]
    void hit(ray const *rays, float const *times, uint32 const len, float *results) const noexcept;
    void traverse(ray const *rays, float const *times, uint32 const len, interval *results) const noexcept;
    static uvs getUVs(vec3 normal);

    vec3 getNormal(point3 const intersection, float time) const;

    aabb bounding_box() const;

    static sphere applyTransform(sphere a, transform tf) noexcept;

    point3 center1;
    float radius;
    vec3 center_vec;
};
