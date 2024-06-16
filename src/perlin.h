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

#include "vec3.h"

class perlin {
   public:
    perlin();

    ~perlin();

    double noise(point3 const &p) const;

    double turb(point3 const &p, int depth) const;

    vec3 *randvec;
    int *perm_x;
    int *perm_y;
    int *perm_z;
};
