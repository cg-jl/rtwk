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

#define assume(x) \
    if (!(x)) __builtin_unreachable()

// Usings

using std::make_shared;
using std::shared_ptr;
using std::sqrt;

// Constants

static float constexpr infinity = std::numeric_limits<float>::infinity();
static float constexpr pi = 3.1415926535897932385;

// NOTE: default-initializing mersenne-twister seems to always set the same
// seed, which is desirable for testing, but not for "real" runs.
static thread_local std::mt19937 s_mt;
static thread_local std::uniform_real_distribution<float> s_ds;

// Utility Functions

inline float degrees_to_radians(float degrees) { return degrees * pi / 180.0; }

// Returns a random floating point value in [0,1).
inline float random_float() { return s_ds(s_mt); }

// Returns a random floating point value in [min,max).
inline float random_float(float min, float max) {
    return min + (max - min) * random_float();
}

// Returns a random integer in [min,max].
inline int random_int(int min, int max) {
    return static_cast<int>(random_float(min, max + 1));
}
