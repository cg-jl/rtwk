#include "aabb.h"
#include <algorithm>
#include <utility>

#include <immintrin.h>
#include <x86intrin.h>

#include <tracy/Tracy.hpp>

#include "interval.h"
#include "trace_colors.h"

// @perf My L1 cache size (per CPU) is: 32kiB!
// L3 is 4MiB and L2 is 512kiB.

static std::pair<__m256d, __m256d> get_t0s_t1s(aabb const &bb, ray const &r)
{
    // NOTE: These load 4x double's, so the rightmost value (memory order) or
    // the leftmost value (register order) won't be used.

    // @perf for nontemporal loads we must have the ray aligned at a 32 byte
    // boundary.

    auto adinvs = _mm256_loadu_pd((double *)&r.dir.e);
    auto origs = _mm256_loadu_pd((double *)&r.orig.e);
    // @perf 'mins' has in its leftmost slot (register order) the first for
    // maxes. 'maxes' has in its leftmost slot (register order) garbage.
    auto mins = (__m256d)_mm256_load_pd((double *)&bb.min.e);
    auto maxes = (__m256d)_mm256_load_pd((double *)&bb.max.e);

    // <garbo> <tx[2]> <tx[1]> <tx[0]> (register order)
    auto t0s = (mins - origs) / adinvs;
    auto t1s = (maxes - origs) / adinvs;
    return { t0s, t1s };
}

static interval traverse(aabb const &bb, ray const &r) noexcept
{

    auto [t0s, t1s] = get_t0s_t1s(bb, r);
    auto tmins = _mm256_min_pd(t0s, t1s);
    auto tmaxs = _mm256_max_pd(t0s, t1s);

    // NOTE: @perf The compiler seems to be generating smarter code than I am
    // for this last comparison loop step (minsd, maxsd three times :P).

    auto tmin_array = (double *)&tmins;
    auto tmaxs_array = (double *)&tmaxs;
    interval ray_t { tmin_array[0], tmaxs_array[0] };
    for (int axis = 1; axis < 3; ++axis) {
        auto t0 = ((double *)&tmins)[axis];
        auto t1 = ((double *)&tmaxs)[axis];

        if (t0 > ray_t.min)
            ray_t.min = t0;
        if (t1 < ray_t.max)
            ray_t.max = t1;
    }

    return ray_t;
}

void aabb::traverse(ray const *rays, uint32 const len, interval *results) const noexcept
{
    // @perf think about splitting the transform
    std::transform(rays, rays + len, results, [&](auto const &r) {
        return ::traverse(*this, r);
    });
}

void aabb::hit(ray const *rays, uint32 const len, double *results) const noexcept
{
    // @perf think about splitting this transform.
    std::transform(rays, rays + len, results, [&](auto const &r) {
        auto [t0s, t1s] = get_t0s_t1s(*this, r);
        auto tmins = _mm256_min_pd(t0s, t1s);

        // NOTE: @perf The compiler seems to be generating smarter code than I am
        // for this last comparison loop step (minsd, maxsd three times :P).

        auto tmin_array = (double *)&tmins;
        double min = tmin_array[0];
        for (int axis = 1; axis < 3; ++axis) {
            auto t0 = ((double *)&tmins)[axis];

            if (t0 > min)
                min = t0;
        }

        return min;
    });
}

vec3 aabb::getNormal(point3 intersection) const
{
    // @perf could be simd'ized if required.
    for (int axis = 0; axis < 3; ++axis) {
        auto intv = axis_interval(axis);

        if (std::abs(intersection[axis] - intv.min) > 1e-8 && std::abs(intersection[axis] - intv.max) > 1e-8) {
            continue;
        }

        vec3 v { 0, 0, 0 };
        v[axis] = 1;
        return v;
    }
    std::unreachable();
}

// @perf could be optimized to use swizzled vectors.
uvs aabb::getUVs(point3 intersection) const
{
    // search for the "box" that borders the point interval, since we know that
    // the point is already within the bounds of the box.
    for (int axis = 0; axis < 3; ++axis) {
        auto uaxis = (axis + 2) % 3;
        auto vaxis = (axis + 1) % 3;

        auto intv = axis_interval(axis);
        auto uintv = axis_interval(uaxis);
        auto vintv = axis_interval(vaxis);

        double beta_distance;
        if (std::abs(intersection[axis] - intv.min) < 1e-8) {
            beta_distance = vintv.max;
        } else if (std::abs(intersection[axis] - intv.max) < 1e-8) {
            beta_distance = vintv.min;
        } else {
            continue;
        }
        auto inv_u_mag = 1 / uintv.size();
        auto inv_v_mag = 1 / vintv.size();
        uvs uv;
        uv.u = inv_u_mag * (intersection[uaxis] - uintv.min);
        uv.v = -inv_v_mag * (intersection[vaxis] - beta_distance);
        return uv;
    }
    std::unreachable();
}
