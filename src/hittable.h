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

#include <span>

#include "geometry.h"
#include "texture.h"
#include "vec3.h"

class material;

struct hit_record {
    uvs uv;
};

struct hittable {
    material const *mat;
    geometry const *geom;
    texture const *tex;

    constexpr explicit hittable(material const *mat, texture const *tex,
                                geometry const *geom)
        : mat(mat), geom(geom), tex(tex) {}

    bool hit(ray const &r, interval ray_t, double &closestHit) const {
        return geom->hit(r, ray_t, closestHit);
    }

    void getUVs(uvs &uv, point3 p, double time) const {
        return geom->getUVs(uv, p, time);
    }
};

// NOTE: maybe some sort of infra to have a hittable hit() and also restore()
// prepare(), end() as well to prepare a ray?
// We should end in a geometry anyway.

hittable const *hitSpan(std::span<hittable const> objects, ray const &r,
                        interval ray_t, double &closestHit);
