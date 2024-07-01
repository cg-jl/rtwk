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
#include "hittable.h"

struct hittable_list {
    std::vector<hittable> objects;
    // TODO: make BVH tree not own their spans.
    // This way we can force more objects to be in the same array vector.
    // We could also have two different vector<hittable*>: One for 'lone'
    // objects and one for the ones in trees.
    std::vector<bvh_tree> trees;
    std::vector<constant_medium> cms{};

    hittable_list() {}
    hittable_list(hittable object) { add(object); }

    void clear() { objects.clear(); }

    void add(hittable object);
    void add(constant_medium medium);

    std::span<hittable> from(size_t start) {
        std::span sp(objects);
        return sp.subspan(start);
    }

    std::span<hittable> with(auto add_lam) {
        auto start = objects.size();
        add_lam(*this);
        return from(start);
    }

    hittable const *hitSelect(ray const &r, interval ray_t,
                              double &closestHit) const;

    color const *sampleConstantMediums(ray const &ray, interval ray_t,
                                       double *hit) const noexcept;
};
