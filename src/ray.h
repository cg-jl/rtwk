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

// @perf the time portion is only fixed once per simulation.

// @perf to align the ray to a 32-byte boundary, I have to push the `time`
// variable to a different place. I don't need the alignment right now since I'm
// already using the cache to store the ray temporally.
struct ray {
    constexpr ray() = default;
    constexpr ray(point3 orig, vec3 dir) : orig(orig), dir(dir) {}

    point3 at(double t) const { return orig + t * dir; }

    point3 orig;
    vec3 dir;
};

struct timed_ray {
    ray r;
    double time;
};

#endif
