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
#include <memory>
#include <ranges>

#include "random.h"

#define noalias __restrict__ 

// C++ Std Usings

using std::fabs;
using std::make_shared;
using std::shared_ptr;
using std::sqrt;
using uint32 = uint32_t;

// Constants

static constexpr float infinity = 1e11;
static constexpr float pi = 3.1415926535897932385;

// Utility Functions

inline float degrees_to_radians(float degrees)
{
    return degrees * pi / 180.0;
}

inline float random_float(float min, float max)
{
    // Returns a random real in [min,max).
    return min + (max - min) * random_float();
}

inline int random_int(int min, int max)
{
    // Returns a random integer in [min,max].
    return int(random_float(min, max + 1));
}

struct range {
    uint32_t start, end;
};

struct single_uvs {
    float u, v;
};

struct uv_buffer {
    float *u;
    float *v;

    static uv_buffer request(uint32 spp)
    {
        return {
            .u = new float[spp],
            .v = new float[spp],
        };
    }

    void swap(uint32 const i, uint32 const k) noexcept
    {
        std::swap(u[i], u[k]);
        std::swap(v[i], v[k]);
    }

    auto constexpr zip_view(uint32 const start, uint32 const end) const noexcept
    {
        return std::views::zip(
            std::ranges::subrange(u + start, u + end),
            std::ranges::subrange(v + start, v + end));
    }

    // @perf remove.
    single_uvs operator[](uint32 i) const noexcept
    {
        return single_uvs { u[i], v[i] };
    }
};

static auto partition(auto start, decltype(start) end, auto swap, auto pred)
{
    if (start >= end)
        goto r;
    --end;
    while (start < end) {
        if (!pred(start)) {
            while (!pred(end)) {
                --end;
                if (end == start)
                    goto r;
            }
            swap(start, end);
        }
        ++start;
    }
r:
    return start;
}

#endif
