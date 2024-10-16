#include "aabb.h"
#include <utility>

#include <immintrin.h>
#include <x86intrin.h>

#include <tracy/Tracy.hpp>

#include "interval.h"
#include "trace_colors.h"

// @perf My L1 cache size (per CPU) is: 32kiB!
// L3 is 4MiB and L2 is 512kiB.

interval aabb::traverse(ray const &r) const {
    // NOTE: These load 4x double's, so the rightmost value (memory order) or
    // the leftmost value (register order) won't be used.

    // @perf for nontemporal loads we must have the ray aligned at a 32 byte
    // boundary.

    auto adinvs = _mm256_loadu_pd((double *)&r.dir.e);
    auto origs = _mm256_loadu_pd((double *)&r.orig.e);
    // @perf 'mins' has in its leftmost slot (register order) the first for
    // maxes. 'maxes' has in its leftmost slot (register order) garbage.
    auto mins = (__m256d)_mm256_stream_load_si256((double *)&min.e);
    auto maxes = (__m256d)_mm256_stream_load_si256((double *)&max.e);

    // <garbo> <tx[2]> <tx[1]> <tx[0]> (register order)
    auto t0s = (mins - origs) / adinvs;
    auto t1s = (maxes - origs) / adinvs;

    auto tmins = _mm256_min_pd(t0s, t1s);
    auto tmaxs = _mm256_max_pd(t0s, t1s);

    // NOTE: @perf The compiler seems to be generating smarter code than I am
    // for this last comparison loop step (minsd, maxsd three times :P).

    // @perf This is just one shuffle and one min/max instruction, don't need to use scalars.
    // Since it's just three way:
    // <garbo> <tx[2]> <tx[1]> <tx[0]>
    // <garbo> <tx[0]> <tx[2]> <tx[1]> <- ideal (I don't know if I can have it)
    // <garbo> <0/2>   <1/2>   <0/1>
    // minsd, maxsd twice?
    auto tmin_array = (double *)&tmins;
    auto tmaxs_array = (double *)&tmaxs;
    interval ray_t{tmin_array[0], tmaxs_array[0]};
    for (int axis = 1; axis < 3; ++axis) {
        auto t0 = ((double *)&tmins)[axis];
        auto t1 = ((double *)&tmaxs)[axis];

        if (t0 > ray_t.min) ray_t.min = t0;
        if (t1 < ray_t.max) ray_t.max = t1;
    }

    return ray_t;
}

double aabb::hit(ray const &r) const {
    auto intv = traverse(r);
    auto ok = !intv.isEmpty();
    // @perf ok is just `intv.min < intv.max`, so -sign(intv.min - intv.max)
    // should be good. -sign because we want intv.min == intv.max to yield -1,
    // sign(0) = 0 -> -sign(0) = 1. Since we swap the sign we also have to swap
    // the subtraction.
    // If `traverse` returned bool64 we could negate the bool64 to get 11111...
    // and then use `copysign` to copy the sign over to the result. We can also
    // get "bool64" by multiplying/shifting.
    auto res = intv.min;
    res = ok ? res : -res;
    return res;
}

vec3 aabb::getNormal(point3 intersection) const {
    // @perf could be simd'ized if required.
    for (int axis = 0; axis < 3; ++axis) {
        auto intv = axis_interval(axis);

        if (std::abs(intersection[axis] - intv.min) > 1e-8 &&
            std::abs(intersection[axis] - intv.max) > 1e-8) {
            continue;
        }

        vec3 v{0, 0, 0};
        v[axis] = 1;
        return v;
    }
    std::unreachable();
}

// @perf could be optimized to use swizzled vectors.
uvs aabb::getUVs(point3 intersection) const {
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
