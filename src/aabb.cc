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
#include "simd.h"
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

void aabb::traverse(ray const *rays, uint32 const len, interval *results) const noexcept
{
    // @perf think about splitting the transform
    std::transform(rays, rays + len, results, [&](auto const &r) {
        auto t0s = (min - r.orig) / r.dir;
        auto t1s = (max - r.orig) / r.dir;

        auto tmins = vec3 {
            std::min(t0s[0], t1s[0]),
            std::min(t0s[1], t1s[1]),
            std::min(t0s[2], t1s[2]),
        };
        auto tmaxs = vec3 {
            std::max(t0s[0], t1s[0]),
            std::max(t0s[1], t1s[1]),
            std::max(t0s[2], t1s[2]),
        };

        interval ray_t { tmins[0], tmaxs[0] };
        for (int axis = 1; axis < 3; ++axis) {
            auto t0 = tmins[axis];
            auto t1 = tmaxs[axis];

            if (t0 > ray_t.min)
                ray_t.min = t0;
            if (t1 < ray_t.max)
                ray_t.max = t1;
        }

        return ray_t;
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

[[clang::always_inline]] static inline simd::floats extend_threef(float z0, float z1, float z2)
{
    return simd::setf(z1, z0, z2, z1, z0, z2, z1, z0);
}

[[clang::always_inline]] static inline simd::ints extend_threei(uint32 z0, uint32 z1, uint32 z2)
{
    return simd::seti(z1, z0, z2, z1, z0, z2, z1, z0);
}

void aabb::getNormals(ray const *rays, float const *dist, vec3 *results, uint32 start, uint32 end) const noexcept
{

    std::transform(dist + start, dist + end, rays + start, results + start, [&](auto const closestHit, auto const &r) {
        return r.at(closestHit);
    });

    // @cleanup move block/scalar versions to function, move layout distribution (alignment & blocks) to function as well.
    // A namespace to hide all of them wouldn't hurt :]
    auto const fresults = (float *)results;
    auto constexpr eps = 1e-8f;

    auto const find_normal_ones = [](simd::floats const fresults_ik, simd::floats const fresults_last_ik, simd::floats const min_axis_reg, simd::floats const max_axis_reg, simd::ints const axis_eq_0, simd::ints const axis_ne_2) {
        //  Note that only a shift (left in register, right in memory) is needed, but the smallest sequence
        // of instructions I could find includes the rotation result for free :].
        auto fresults_ikm1 = simd::insert_last_in_first(fresults_last_ik, simd::rotate_right(fresults_ik));

        // Mix in the last two values of fresults_last_ik and the first 6 of fresults_ik.
        auto fresults_ikm2 = simd::insert_last2_in_first2(fresults_last_ik, simd::rotate_right2(fresults_ik));

        auto min_intersect_reg = simd::absf(fresults_ik - min_axis_reg);

        auto max_intersect = simd::absf(fresults_ik - max_axis_reg);

        auto min_of_both = simd::minf(min_intersect_reg, max_intersect);
        auto const ones = simd::splatf(1.f);
        auto const eps_reg = simd::splatf(eps);
        auto has_this_reg = min_of_both < eps_reg;

        // @perf May want to use quiet instead of signaling!
        auto ikm1_is_1 = fresults_ikm1 == ones;

        // @perf May want to use quiet instead of signaling!
        auto ikm2_is_1 = fresults_ikm2 == ones;

        auto no_one_before = axis_eq_0 | ~ikm1_is_1;

        auto no_two_before = axis_ne_2 | ~ikm2_is_1;

        // We insert a 1 into the normal ifthe current value (intersection[axis]) is very close to min[axis] or max[axis].
        // We won't insert a 1, even if this happens, when we already have inserted a 1 before inside the current vec3.

        // @perf could reorganize these to AND has_two and has_one, AND has_one and ones, and then AND them together with has_this_reg. Would have a bit more throughput.
        has_this_reg = has_this_reg & no_two_before;
        has_this_reg = has_this_reg & no_one_before;
        has_this_reg = has_this_reg & ones;
        return has_this_reg;
    };

    auto const do_scalar = [&](auto start, auto end) {
        // We know that end - start < 8.
        // So we can load the previously aligned 32-byte boundary first, since we can't
        // be both across an 8 block boundary and a page boundary (4096). The reason for this
        // is that blocks that new() gives do not cross page boundaries.
        if (end == start)
            return;

        auto const misalignment = simd::find_misalignment(uintptr_t(fresults));
        auto const count = end - start;
        auto aligned_ptr = fresults - misalignment;

        // NOTE: I have to rotate BEFORE extending because the rotation has to be with a length
        // aligned to 8. I could make these fixed depending on start % 3.

        auto fresults_ik = simd::loadf_aligned(aligned_ptr);
        auto const axis_indices = extend_threei(start % 3, (start + 1) % 3, (start + 2) % 3);
        auto has_this = find_normal_ones(fresults_ik, simd::zero(),
            extend_threef(min[start % 3], min[(start + 1) % 3], min[(start + 2) % 3]),
            extend_threef(max[start % 3], max[(start + 1) % 3], max[(start + 2) % 3]),
            axis_indices == simd::zero(),
            axis_indices != simd::splati(2));

        simd::storef_overcommit(has_this, aligned_ptr, misalignment, count);
    };

    auto const do_8_aligned = [&](auto start, auto end) {
        // @perf make the rotations using simd
        std::array<simd::ints, 3> axis_eq_0 alignas(32);
        std::array<simd::ints, 3> axis_ne_2 alignas(32);
        std::array<simd::floats, 3> min_axis alignas(32);
        std::array<simd::floats, 3> max_axis alignas(32);

        auto const axis_ixs_0 = extend_threei((0 + start) % 3, (1 + start) % 3, (2 + start) % 3);
        auto const axis_ixs_1 = extend_threei((2 + start) % 3, (0 + start) % 3, (1 + start) % 3);
        auto const axis_ixs_2 = extend_threei((1 + start) % 3, (2 + start) % 3, (0 + start) % 3);

        axis_eq_0[0] = axis_ixs_0 == simd::zero();
        axis_eq_0[1] = axis_ixs_1 == simd::zero();
        axis_eq_0[2] = axis_ixs_2 == simd::zero();

        auto const twoi = simd::splati(2);

        axis_ne_2[0] = axis_ixs_0 != twoi;
        axis_ne_2[1] = axis_ixs_1 != twoi;
        axis_ne_2[2] = axis_ixs_2 != twoi;

        min_axis[0] = extend_threef(min[(0 + start) % 3], min[(1 + start) % 3], min[(2 + start) % 3]);
        min_axis[1] = extend_threef(min[(2 + start) % 3], min[(0 + start) % 3], min[(1 + start) % 3]);
        min_axis[2] = extend_threef(min[(1 + start) % 3], min[(2 + start) % 3], min[(0 + start) % 3]);

        max_axis[0] = extend_threef(max[(0 + start) % 3], max[(1 + start) % 3], max[(2 + start) % 3]);
        max_axis[1] = extend_threef(max[(2 + start) % 3], max[(0 + start) % 3], max[(1 + start) % 3]);
        max_axis[2] = extend_threef(max[(1 + start) % 3], max[(2 + start) % 3], max[(0 + start) % 3]);

        auto fresults_last_ik = _mm256_setzero_ps();

        auto rotate_three = [](auto &zero, auto &one, auto &two) {
            auto z = zero;
            zero = one;
            one = two;
            two = z;
        };

        // every 24 floats (or 3 blocks), the rotations cancel out, so there's no need to
        // rotate it between each of these blocks of 24.

        auto const x24_block_count = (end - start) / 24;
        // First index where we can't safely take a block of 24 floats
        auto const x24_block_end = start + x24_block_count * 24;
        for (auto x24_block_start = start; x24_block_start < x24_block_end; x24_block_start += 24) {
            auto fresults_ik_0 = simd::loadf_aligned(&fresults[x24_block_start]);
            auto fresults_ik_8 = simd::loadf_aligned(&fresults[x24_block_start + 8]);
            auto fresults_ik_16 = simd::loadf_aligned(&fresults[x24_block_start + 16]);

            simd::storef_aligned(find_normal_ones(fresults_ik_0, fresults_last_ik, min_axis[0], max_axis[0], axis_eq_0[0], axis_ne_2[0]), &fresults[x24_block_start]);
            simd::storef_aligned(find_normal_ones(fresults_ik_8, fresults_ik_0, min_axis[1], max_axis[1], axis_eq_0[1], axis_ne_2[1]), &fresults[x24_block_start + 8]);
            simd::storef_aligned(find_normal_ones(fresults_ik_16, fresults_ik_8, min_axis[2], max_axis[2], axis_eq_0[2], axis_ne_2[2]), &fresults[x24_block_start + 16]);

            fresults_last_ik = fresults_ik_16;
        }

        for (auto i = x24_block_end; i < end; i += 8) {
            auto axis_eq_2_reg = axis_ne_2[0];
            auto axis_ne_0_reg = axis_eq_0[0];
            auto max_axis_reg = max_axis[0];
            auto min_axis_reg = min_axis[0];

            auto fresults_ik = simd::loadf_aligned(&fresults[i]);
            auto has_this_reg = find_normal_ones(fresults_ik, fresults_last_ik, min_axis_reg, max_axis_reg, axis_ne_0_reg, axis_eq_2_reg);

            fresults_last_ik = fresults_ik;
            simd::storef_aligned(has_this_reg, &fresults[i]);

            rotate_three(min_axis[0], min_axis[1], min_axis[2]);
            rotate_three(max_axis[0], max_axis[1], max_axis[2]);
            rotate_three(axis_ne_2[0], axis_ne_2[1], axis_ne_2[2]);
            rotate_three(axis_eq_0[0], axis_eq_0[1], axis_eq_0[2]);
        }
    };

    auto const fstart = 3 * start;
    auto const fend = 3 * end;

    uint32 const aligned_32_offset = simd::offset_till_aligned(uintptr_t(fresults + fstart));
    auto const x8_block_start = std::min(aligned_32_offset + fstart, fend);
    // First index where we can't safely take a block of 8 floats
    auto const x8_block_end = x8_block_start + ((fend - x8_block_start) & ~7);
    do_scalar(fstart, x8_block_start);
    do_8_aligned(x8_block_start, x8_block_end);
    do_scalar(x8_block_end, fend);
}

using std::ranges::subrange;
using std::ranges::views::zip;
void aabb::getUVs(ray const *rays, float const *dist, uv_buffer results, uint32 start, uint32 end) const noexcept
{

    // @perf cache intersections, normals.

    // @perf cutnpaste transform
    std::ranges::transform(zip(
                               subrange(rays + start, rays + end),
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
                               subrange(rays + start, rays + end),
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
