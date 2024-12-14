#include "aabb.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <ranges>
#include <utility>

#include <immintrin.h>
#include <x86intrin.h>

#include <tracy/Tracy.hpp>

#include "interval.h"
#include "ray.h"
#include "simd.h"

// @perf My L1 cache size (per CPU) is: 32kiB!
// L3 is 4MiB and L2 is 512kiB.

static void reduce_transposed(float const *__restrict__ src, uint32 vec3_len, float *__restrict__ dst, auto reduce_scalar)
{
    typedef float coord_array[vec3_len];

    auto tmp_coord = (coord_array *)src;
    auto const len = vec3_len;
    auto const results = dst;

    // @perf use 8-wide blocks
    for (auto k = 0uz; k < 3; ++k) {
        for (auto i = 0uz; i < len; i++) {
            tmp_coord[0][i] = reduce_scalar(tmp_coord[0][i], tmp_coord[k][i]);
        }
    }

    for (auto i = 0uz; i < len; ++i) {
        results[i] = tmp_coord[0][i];
    }
}

static void transpose_into(vec3 const *__restrict__ src, uint32 vec3_len, float *__restrict__ dst)
{
    typedef float coord_array[vec3_len];

    auto *dst_coord = (coord_array *)dst;

    // 1. transpose
    for (auto i = 0uz; i < vec3_len; ++i) {
        for (auto k = 0u; k < 3; ++k) {
            dst_coord[k][i] = src[i][k];
        }
    }
}

static void calc_transposed_ts(vec3 p, transposed_ray_array rays, uint32 const len, float *__restrict__ dst)
{
    // @perf undo transpose :]
    typedef float coord_array[len];

    for (auto k = 0u; k < 3; ++k) {
        for (auto i = 0u; i < len; ++i) {
            ((coord_array *)dst)[k][i] = (p[k] - vec3(rays[i].orig)[k]) / vec3(rays[i].dir)[k];
        }
    }
}

static void calc_t0s_t1s(vec3 min, vec3 max, transposed_ray_array rays, uint32 const len, float *__restrict__ t0s, float *__restrict__ t1s)
{
    calc_transposed_ts(min, rays, len, t0s);
    calc_transposed_ts(max, rays, len, t1s);
}

// FIXME: Plan of action:
// 1. Change *::traverse() to use transposed rays.
// 2. Change *::hit() to use transposed rays.

void aabb::traverse(transposed_ray_array rays, uint32 const len, Traverse_Buffers buffers, interval_buffer results) const noexcept
{
    // @perf soa'd vecs in rays

    calc_t0s_t1s(min, max, rays, len, (float *)buffers.t0s, (float *)buffers.t1s);

    // @perf pre-copying these and then reducing is much better for caching, since now there's
    // only two input streams to cache.
    std::copy(buffers.t0s, &buffers.t0s[len], buffers.mins);
    std::copy(buffers.t0s, &buffers.t0s[len], buffers.maxs);

    // @mem I can reuse one of the buffers as mins/maxes. Right now it would be `maxes` because it's the last thing that reads from t0 and t1.
    std::transform((float *)buffers.mins, (float *)&buffers.mins[len], (float *)buffers.t1s, (float *)buffers.mins, [](auto a, auto b) { return std::min(a, b); });

    std::transform((float *)buffers.maxs, (float *)&buffers.maxs[len], (float *)buffers.t1s, (float *)buffers.maxs, [](auto a, auto b) { return std::max(a, b); });

    // @mem I could do the reducing on only one at a time and reduce memory consumption.
    // Doing this without reusing arrays seems to make things slower though.
    reduce_transposed((float *)buffers.maxs, len, results.maxes, [](auto a, auto b) { return std::min(a, b); });
    reduce_transposed((float *)buffers.mins, len, results.mins, [](auto a, auto b) { return std::max(a, b); });
}

void aabb::hit(transposed_ray_array rays, uint32 const len, Hit_Buffers buffers, float *__restrict__ results) const noexcept
{

    // @perf soa'd vecs in rays
    calc_t0s_t1s(min, max, rays, len, (float *)buffers.t0s, (float *)buffers.t1s);

    std::copy(buffers.t0s, &buffers.t0s[len], buffers.mins);

    // @perf abstract n make it a zip_with with block transform
    std::transform((float *)buffers.mins, (float *)&buffers.mins[len], (float *)buffers.t1s, (float *)buffers.mins, [](auto a, auto b) { return std::min(a, b); });

    reduce_transposed((float *)buffers.mins, len, results, [](auto a, auto b) { return std::max(a, b); });
}

[[clang::always_inline]] static inline simd::floats extend_threef(float z0, float z1, float z2)
{
    return simd::setf(z1, z0, z2, z1, z0, z2, z1, z0);
}

[[clang::always_inline]] static inline simd::ints extend_threei(uint32 z0, uint32 z1, uint32 z2)
{
    return simd::seti(z1, z0, z2, z1, z0, z2, z1, z0);
}

static inline vec3 vabs(vec3 v) { return { std::abs(v[0]), std::abs(v[1]), std::abs(v[2]) }; }

void aabb::getNormals(transposed_ray_array rays, float const *dist, transposed_vec_array results, uint32 const start, uint32 const end) const noexcept
{

    for (auto i = start; i < end; ++i) {
        results[i] = rays[i].at(dist[i]);
    }

    auto constexpr eps = 1e-8f;
    for (auto i = start; i < end; ++i) {
        // FIXME: @perf this should be:
        // - first check on x axis
        // - then check on y axis while making sure it's not on x axis
        // - then check on z axis while making sure it's not on x/y axis.
        auto intersection = results[i];
        auto min_intersect = vabs(intersection - min);
        auto max_intersect = vabs(intersection - max);

        auto min_of_both = vec3 {
            std::min(min_intersect[0], max_intersect[0]),
            std::min(min_intersect[1], max_intersect[1]),
            std::min(min_intersect[2], max_intersect[2]),
        };

        if (min_of_both[0] < eps) {
            results[i] = { 1, 0, 0 };
        } else if (min_of_both[1] < eps) {
            results[i] = { 0, 1, 0 };
        } else {
            results[i] = { 0, 0, 1 };
        }
    }
}

using std::ranges::subrange;
using std::ranges::views::zip;
void aabb::getUVs(transposed_ray_array rays, float const *dist, uv_buffer results, uint32 start, uint32 end) const noexcept
{

    // @perf cache intersections, normals.

    // @perf cutnpaste transform
    std::ranges::transform(zip(
                               rays.read_scalars(end, start),
                               subrange(dist + start, dist + end)),
        results.u + start, [&](auto const &t) -> float {
            // @perf could be optimized to use swizzled vectors.
            auto const &[r, closestHit] = t;
            auto intersection = r.at(closestHit);
            auto const &bb = *this;

            // search for the "box" that borders the point interval, since we know that
            // the point is already within the bounds of the box.
            for (int axis = 0; axis < 3; ++axis) {
                auto uaxis = (axis + 2) % 3;

                auto intv = bb.axis_interval(axis);
                auto uintv = bb.axis_interval(uaxis);

                if (std::abs(intersection[axis] - intv.min) < 1e-8) {
                } else if (std::abs(intersection[axis] - intv.max) < 1e-8) {
                } else {
                    continue;
                }
                auto inv_u_mag = 1 / uintv.size();
                return inv_u_mag * (intersection[uaxis] - uintv.min);
            }
            // FIXME: There is some bug here that makes the loop not find any appropiate interval for the hit.
            // Running under debug makes it trigger stack smashing
            return 0;
            std::unreachable();
        });

    // @perf separate transform :]
    std::ranges::transform(zip(
                               rays.read_scalars(end, start),
                               subrange(dist + start, dist + end)),
        results.v + start, [&](auto const &t) -> float {
            // @perf could be optimized to use swizzled vectors.
            auto const &[r, closestHit] = t;
            auto intersection = r.at(closestHit);
            auto const &bb = *this;

            // search for the "box" that borders the point interval, since we know that
            // the point is already within the bounds of the box.
            for (int axis = 0; axis < 3; ++axis) {
                auto vaxis = (axis + 1) % 3;

                auto intv = bb.axis_interval(axis);
                auto vintv = bb.axis_interval(vaxis);

                float beta_distance;
                if (std::abs(intersection[axis] - intv.min) < 1e-8) {
                    beta_distance = vintv.max;
                } else if (std::abs(intersection[axis] - intv.max) < 1e-8) {
                    beta_distance = vintv.min;
                } else {
                    continue;
                }
                auto inv_v_mag = 1 / vintv.size();
                return -inv_v_mag * (intersection[vaxis] - beta_distance);
            }
            // FIXME: There is some bug here that makes the loop not find any appropiate interval for the hit.
            // Running under debug makes it trigger stack smashing
            return 0;
            std::unreachable();
        });
}
