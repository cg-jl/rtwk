#include "aabb.h"
#include <algorithm>
#include <ranges>
#include <utility>

#include <immintrin.h>
#include <x86intrin.h>

#include <tracy/Tracy.hpp>

#include "interval.h"
#include "trace_colors.h"

// @perf My L1 cache size (per CPU) is: 32kiB!
// L3 is 4MiB and L2 is 512kiB.

// @perf using __m128 because if I use m256 I use 3/8 slots. With m128 I use 3/4,
// which is significantly less drop rate. Still bad though.
static std::pair<__m128, __m128> get_t0s_t1s(aabb const &bb, ray const &r)
{
    // NOTE: These load 4x float's, so the rightmost value (memory order) or
    // the leftmost value (register order) won't be used.

    // @perf for nontemporal loads we must have the ray aligned at a 32 byte
    // boundary.

    auto adinvs = _mm_loadu_ps((float *)&r.dir.e);
    auto origs = _mm_loadu_ps((float *)&r.orig.e);
    // @perf 'mins' has in its leftmost slot (register order) the first for
    // maxes. 'maxes' has in its leftmost slot (register order) garbage.
    auto mins = _mm_load_ps((float *)&bb.min.e);
    auto maxes = _mm_load_ps((float *)&bb.max.e);

    // <garbo> <tx[2]> <tx[1]> <tx[0]> (register order)
    auto t0s = (mins - origs) / adinvs;
    auto t1s = (maxes - origs) / adinvs;
    return { t0s, t1s };
}

static interval traverse(aabb const &bb, ray const &r) noexcept
{

    auto [t0s, t1s] = get_t0s_t1s(bb, r);
    auto tmins = _mm_min_ps(t0s, t1s);
    auto tmaxs = _mm_max_ps(t0s, t1s);

    // NOTE: @perf The compiler seems to be generating smarter code than I am
    // for this last comparison loop step (minsd, maxsd three times :P).

    auto tmin_array = (float *)&tmins;
    auto tmaxs_array = (float *)&tmaxs;
    interval ray_t { tmin_array[0], tmaxs_array[0] };
    for (int axis = 1; axis < 3; ++axis) {
        auto t0 = ((float *)&tmins)[axis];
        auto t1 = ((float *)&tmaxs)[axis];

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

void aabb::hit(ray const *rays, uint32 const len, float *results) const noexcept
{
    // @perf think about splitting this transform.
    std::transform(rays, rays + len, results, [&](auto const &r) {
        auto [t0s, t1s] = get_t0s_t1s(*this, r);
        auto tmins = _mm_min_ps(t0s, t1s);

        // NOTE: @perf The compiler seems to be generating smarter code than I am
        // for this last comparison loop step (minsd, maxsd three times :P).

        auto tmin_array = (float *)&tmins;
        float min = tmin_array[0];
        for (int axis = 1; axis < 3; ++axis) {
            auto t0 = ((float *)&tmins)[axis];

            if (t0 > min)
                min = t0;
        }

        return min;
    });
}

static vec3 vabs(vec3 x)
{
    return { std::abs(x[0]), std::abs(x[1]), std::abs(x[2]) };
}

void aabb::getNormals(ray const *rays, float const *dist, vec3 *results, uint32 start, uint32 end) const noexcept
{

    std::transform(dist + start, dist + end, rays, results + start, [&](auto const closestHit, auto const &r) {
        auto intersection = r.at(closestHit);
        auto const min_intersect = vabs(intersection - min);
        auto const max_intersect = vabs(intersection - max);
        auto const min_of_both = vec3 {
            std::min(min_intersect[0], max_intersect[0]),
            std::min(min_intersect[1], max_intersect[1]),
            std::min(min_intersect[2], max_intersect[2]),
        };
        return min_of_both;
    });

    // @perf split transforms?
    std::transform(results + start, results + end, results + start, [&](auto const min_of_both) {
        auto const idx = std::distance(min_of_both.e, std::find_if(min_of_both.e, &min_of_both.e[3], [](auto const x) { return x <= 1e-8; }));

        vec3 v { 0, 0, 0 };
        v[idx] = 1;
        return v;
    });
}

using std::ranges::subrange;
using std::ranges::views::zip;
void aabb::getUVs(ray const *rays, float const *dist, uvs *results, uint32 start, uint32 end) const noexcept
{
    // @perf separate transform :]
    std::ranges::transform(zip(
                               subrange(rays + start, rays + end),
                               subrange(dist + start, dist + end)),
        results + start, [&](auto const &t) -> uvs {
            // @perf could be optimized to use swizzled vectors.
            auto const &[r, closestHit] = t;
            auto intersection = r.at(closestHit);
            auto const &bb = *this;
            // search for the "box" that borders the point interval, since we know that
            // the point is already within the bounds of the box.
            for (int axis = 0; axis < 3; ++axis) {
                auto uaxis = (axis + 2) % 3;
                auto vaxis = (axis + 1) % 3;

                auto intv = bb.axis_interval(axis);
                auto uintv = bb.axis_interval(uaxis);
                auto vintv = bb.axis_interval(vaxis);

                float beta_distance;
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
        });
}
