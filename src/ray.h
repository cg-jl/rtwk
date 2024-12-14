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
#include <iterator>
#include <ranges>
#include <rtweekend.h>

// @perf the time portion is only fixed once per simulation.

// @perf to align the ray to a 32-byte boundary, I have to push the `time`
// variable to a different place. I don't need the alignment right now since I'm
// already using the cache to store the ray temporally.
struct ray {
    constexpr ray() = default;
    constexpr ray(point3 orig, vec3 dir)
        : orig(orig)
        , dir(dir)
    {
    }

    point3 at(float t) const { return orig + t * dir; }

    point3 orig;
    vec3 dir;
};

// @perf consider separating time from ray
struct timed_ray {
    ray r;
    float time;
};

struct transposed_vec_array {
    // @perf I know these should be separated by `len` so only one pointer is needed,
    // but this is going to be a helper struct.
    float *noalias x;
    float *noalias y;
    float *noalias z;

    static transposed_vec_array request(uint32 len)
    {

        auto *values = new float[3 * len];
        return {
            .x = values,
            .y = values + len,
            .z = values + 2 * len,
        };
    }

    void swap(auto i, auto k)
    {
        std::swap(x[i], x[k]);
        std::swap(y[i], y[k]);
        std::swap(z[i], z[k]);
    }

    auto constexpr read_scalars(auto const end, decltype(end) start = 0) const noexcept
    {
        return std::views::zip(
                   std::ranges::subrange(x + start, x + end),
                   std::ranges::subrange(y + start, y + end),
                   std::ranges::subrange(z + start, z + end))
            | std::views::transform(
                [](auto const t) {
                    return vec3 { std::get<0>(t), std::get<1>(t), std::get<2>(t) };
                });
    }

    struct assign_proxy {
        float *noalias x, *noalias y, *noalias z;

        operator vec3() const noexcept
        {
            return { *x, *y, *z };
        }

        assign_proxy &operator=(vec3 v)
        {
            *x = v.x();
            *y = v.y();
            *z = v.z();
            return *this;
        }
    };

    auto operator[](auto i) const noexcept
    {
        return assign_proxy {
            &x[i], &y[i], &z[i]
        };
    }

    transposed_vec_array constexpr offset(uint32 off) const noexcept
    {
        return {
            .x = x + off,
            .y = y + off,
            .z = z + off,
        };
    }

    constexpr auto coord(uint32 coord) const noexcept
    {
        return ((float **)this)[coord];
    }
};

struct transposed_ray_array {
    transposed_vec_array dirs, origs;

    static transposed_ray_array request(uint32 len)
    {
        return {
            .dirs = transposed_vec_array::request(len),
            .origs = transposed_vec_array::request(len),
        };
    }

    struct assign_proxy {
        transposed_vec_array::assign_proxy dir, orig;

        vec3 constexpr at(float t) const noexcept
        {
            return dir * t + orig;
        }

        operator ray() const noexcept
        {
            return ray(orig, dir);
        }

        assign_proxy &operator=(ray other)
        {
            dir = other.dir;
            orig = other.orig;
            return *this;
        }
    };

    auto constexpr operator[](auto i) const noexcept
    {
        return assign_proxy { dirs[i], origs[i] };
    }

    auto constexpr read_scalars(auto const end, decltype(end) start = 0) const noexcept
    {

        return std::views::zip(
                   dirs.read_scalars(end, start),
                   origs.read_scalars(end, start))
            | std::views::transform([](auto const &t) {
                  return ray(std::get<1>(t), std::get<0>(t));
              });
    }

    constexpr auto coord(uint32 ix) const noexcept
    {
        return transposed_ray_array {
            .dirs = { dirs.coord(ix) },
            .origs = { origs.coord(ix) }
        };
    }

    void swap(auto i, auto k)
    {
        dirs.swap(i, k);
        origs.swap(i, k);
    }

    void read_from(ray const *rays, uint32 const len)
    {

        for (auto i = 0u; i < len; ++i) {
            dirs.x[i] = rays[i].dir[0];
            dirs.y[i] = rays[i].dir[1];
            dirs.z[i] = rays[i].dir[2];
        }
    }
};

struct ray_buffer {
    transposed_ray_array rays;
    float *noalias times;

    struct assign_proxy {
        transposed_ray_array::assign_proxy r;
        float &noalias time;
    };

    auto constexpr operator[](uint32 const i)
    {
        return assign_proxy { rays[i], times[i] };
    }

    void swap(size_t i, size_t j) noexcept
    {
        rays.swap(i, j);
        std::swap(times[i], times[j]);
    }
};

#endif
