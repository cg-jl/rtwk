#ifndef RAY_H
#define RAY_H
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

struct ray {
   public:
    ray() {}

    ray(point3 origin, vec3 direction)
        : origin(origin), direction(direction), time(0) {}

    ray(point3 origin, vec3 direction, float time)
        : origin(origin), direction(direction), time(time) {}

    point3 at(float t) const { return origin + t * direction; }

    point3 origin;
    vec3 direction;
    float time;
};

#endif
