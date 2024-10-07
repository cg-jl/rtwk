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

// NOTE: When hitting constant mediums, we care about both the geometry and the
// density. The albedo can be linked through geom->refIndex.

struct constant_medium {
    traversable_geometry geom;
    double neg_inv_density;
    constant_medium(traversable_geometry boundary, double density)
        : geom(boundary), neg_inv_density(-1 / density) {}
};
