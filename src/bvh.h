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
#include <geometry.h>

#include <span>

struct bvh_node {
    aabb bbox;

    int objectIndex;

    int left;
};

struct bvh_tree {
    bvh_node root;
    geometry const *objects;

    bvh_tree(std::span<geometry> objects);

    geometry const *hitSelect(ray const &r, double &closestHit) const;
};
