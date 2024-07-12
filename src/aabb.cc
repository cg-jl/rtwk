#include "aabb.h"

#include <x86intrin.h>

#include <tracy/Tracy.hpp>

#include "trace_colors.h"

bool aabb::traverse(ray const &r, interval &ray_t) const {
    // NOTE: These load 4x double's, so the rightmost value (memory order) or
    // the leftmost value (register order) won't be used.
    auto adinvs = _mm256_loadu_pd((double *)&r.dir.e);
    auto origs = _mm256_loadu_pd((double *)&r.orig.e);
    auto mins = _mm256_set_pd(0 /* unused */, z.min, y.min, x.min);
    auto maxes = _mm256_set_pd(0 /* unused */, z.max, y.max, x.max);

    auto t0s = (mins - origs) / adinvs;
    auto t1s = (maxes - origs) / adinvs;

    auto tmins = _mm256_min_pd(t0s, t1s);
    auto tmaxs = _mm256_max_pd(t0s, t1s);

    // NOTE: @perf The compiler seems to be generating smarter code than I am for this last comparison loop step.

    for (int axis = 0; axis < 3; ++axis) {
        auto t0 = ((double *)&tmins)[axis];
        auto t1 = ((double *)&tmaxs)[axis];

        if (t0 > ray_t.min) ray_t.min = t0;
        if (t1 < ray_t.max) ray_t.max = t1;
    }

    return ray_t.min < ray_t.max;
}

bool aabb::hit(ray const &r, interval ray_t) const {
    ZoneNamedN(zone, "AABB hit", filters::hit);
    // Makes a copy of `ray_t` and then makes the comparison.
    return traverse(r, ray_t);
}
