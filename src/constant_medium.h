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

#include <color.h>
#include <geometry.h>

struct constant_medium {
    geometry const *geom;
    double neg_inv_density;
    color albedo;
    constant_medium(geometry const *boundary, double density, color albedo)
        : geom(boundary), neg_inv_density(-1 / density), albedo(albedo) {}
};

