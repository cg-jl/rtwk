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

#include <vector>

#include "bvh.h"
#include "constant_medium.h"
#include "geometry.h"
#include "hittable.h"

struct hittable_list {
    std::vector<lightInfo> objects;
    std::vector<geometry const *> selectGeoms;
    // TODO: make BVH tree not own their spans.
    // This way we can force more objects to be in the same array vector.
    // We could also have two different vector<hittable*>: One for 'lone'
    // objects and one for the ones in trees.
    std::vector<bvh_tree> trees;
    std::vector<constant_medium> cms{};
    std::vector<color> cmAlbedos{};

    hittable_list() {}
    hittable_list(lightInfo object, geometry *geom) { add(object, geom); }

    void clear() { objects.clear(); }

    // Links the geomery with the lighting information.
    void add(lightInfo object, geometry *geom);
    void add(constant_medium medium, color albedo);

    geometry const *hitSelect(ray const &r, interval ray_t,
                              double &closestHit) const;

    color const *sampleConstantMediums(ray const &ray, interval ray_t,
                                       double *hit) const noexcept;
};
