#pragma once
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
#include <limits>
#include <memory>
#include <random>

// Usings

using std::make_shared;
using std::shared_ptr;
using std::sqrt;

// Constants

static double constexpr infinity = std::numeric_limits<double>::infinity();
static double constexpr pi = 3.1415926535897932385;

// NOTE: default-initializing mersenne-twister seems to always set the same
// seed, which is desirable for testing, but not for "real" runs.
static thread_local std::mt19937 s_mt;
static thread_local std::uniform_real_distribution<double> s_ds;

// Utility Functions

static inline double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

static inline double random_double() {
    // Returns a random real in [0,1).
    return s_ds(s_mt);
}

static inline double random_double(double min, double max) {
    // Returns a random real in [min,max).
    return min + (max - min) * random_double();
}

static inline int random_int(int min, int max) {
    // Returns a random integer in [min,max].
    return static_cast<int>(random_double(min, max + 1));
}
