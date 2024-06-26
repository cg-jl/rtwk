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

#include "geometry.h"

// TODO: To instantiate spheres, I should separate instantiatable things
// and add a thread-private 'instantiate' buffer per worker thread where I can
// push any transformations. When separating list-of-ptrs into tagged arrays,
// these 'instantiate buffers' must also be separated.
class sphere : public geometry {
   public:
    // Stationary Sphere
    sphere(point3 const &center, double radius)
        : center1(center), radius(fmax(0, radius)) {}

    // Moving Sphere
    sphere(point3 const &center1, point3 const &center2, double radius)
        : center1(center1), radius(fmax(0, radius)) {
        center_vec = center2 - center1;
    }

    bool hit(ray const &r, interval ray_t, geometry_record &rec) const;
    // NOTE: right now sphere can't calculate its normal because it requires the
    // time instantiation parameter.
    void getUVs(uvs &uv, point3 p, vec3 normal) const final;

    aabb bounding_box() const final;

    point3 center1;
    double radius;
    vec3 center_vec;
};
