#ifndef RTWEEKEND_H
#define RTWEEKEND_H
//==============================================================================================
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public
// Domain Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <random>

// C++ Std Usings

using std::fabs;
using std::make_shared;
using std::shared_ptr;
using std::sqrt;

// Constants

static constexpr double infinity = 1e11;
static constexpr double pi = 3.1415926535897932385;

// Utility Functions

inline double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

// Adapted from glibc/glibc/stdlib/rand_r.c
/* This algorithm is mentioned in the ISO C standard, here extended
   for 32 bits.  */
inline int rand_r(unsigned int *seed) {
    unsigned int next = *seed;
    int result;
    next *= 1103515245;
    next += 12345;
    result = (unsigned int)(next / 65536) % 2048;
    next *= 1103515245;
    next += 12345;
    result <<= 10;
    result ^= (unsigned int)(next / 65536) % 1024;
    next *= 1103515245;
    next += 12345;
    result <<= 10;
    result ^= (unsigned int)(next / 65536) % 1024;
    *seed = next;
    return result;
}

inline double random_double() {
    static thread_local unsigned int seed;
    return double(rand_r(&seed) & RAND_MAX) / (double(RAND_MAX) + 1);
}

inline double random_double(double min, double max) {
    // Returns a random real in [min,max).
    return min + (max - min) * random_double();
}

inline int random_int(int min, int max) {
    // Returns a random integer in [min,max].
    return int(random_double(min, max + 1));
}

struct range {
    uint32_t start, end;
};

template <typename T>
T *leak(T val) {
    return new T(std::move(val));
}

#endif
