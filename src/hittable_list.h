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
    bvh::tree_builder treebld;
    std::vector<lightInfo> objects;
    std::vector<geometry> selectGeoms;
    std::vector<constant_medium> cms{};
    std::vector<color> cmAlbedos{};

    hittable_list() {}
    hittable_list(lightInfo object, geometry geom) {
        add(object, std::move(geom));
    }

    void clear() { objects.clear(); }

    // Links the geomery with the lighting information.
    void add(lightInfo object, geometry geom);
    void addTree(lightInfo object, geometry geom);
    void add(constant_medium medium, color albedo);

    void transformAll(transform tf);

    std::pair<geometry_ptr, double> hitSelect(timed_ray const &r) const;

    color const *sampleConstantMediums(timed_ray const &ray, double closestHit,
                                       double *hit) const noexcept;
};
