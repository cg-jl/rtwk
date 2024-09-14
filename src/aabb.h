#ifndef AABB_H
#define AABB_H
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

#include "interval.h"
#include "ray.h"
#include "vec3.h"

struct aabb {
    vec3 min alignas(32), max alignas(32);

    // The default AABB is empty, since intervals are empty by default.
    constexpr aabb() = default;

    constexpr aabb(interval x, interval y, interval z)
        : min(x.min, y.min, z.min), max(x.max, y.max, z.max) {
        pad_to_minimums();
    }

    constexpr aabb(point3 const &a, point3 const &b) {
        // Treat the two points a and b as extrema for the bounding box, so we
        // don't require a particular minimum/maximum coordinate order.

        for (int axis = 0; axis < 3; ++axis) {
            min[axis] = a[axis];
            max[axis] = b[axis];
        }

        for (int axis = 0; axis < 3; ++axis) {
            if (min[axis] > max[axis]) std::swap(min[axis], max[axis]);
        }

        pad_to_minimums();
    }

    constexpr aabb(aabb const &box0, aabb const &box1) {
        for (int axis = 0; axis < 3; ++axis) {
            min[axis] = std::min(box0.min[axis], box1.min[axis]);
        }

        for (int axis = 0; axis < 3; ++axis) {
            max[axis] = std::max(box0.max[axis], box1.max[axis]);
        }
    }

    constexpr interval axis_interval(int n) const {
        return interval{min[n], max[n]};
    }

    bool hit(ray const &r, interval ray_t) const;
    // Helper method to traverse using an already existing `ray_t` and modifying
    // it. It clobbers `ray_t`.
    bool traverse(ray const &r, interval &ray_t) const;

    constexpr int longest_axis() const {
        // Returns the index of the longest axis of the bounding box.

        double xsizes[3];
        for (int axis = 0; axis < 3; ++axis) {
            xsizes[axis] = max[axis] - min[axis];
        }

        auto it = std::max_element(xsizes, xsizes + 3);

        return std::distance(xsizes, it);
    }

   private:
    constexpr void pad_to_minimums() {
        // Adjust the AABB so that no side is narrower than some delta, padding
        // if necessary.

        constexpr double delta = 0.0001;

        for (int axis = 0; axis < 3; ++axis) {
            auto size = max[axis] - min[axis];
            if (size < delta) {
                min[axis] -= delta / 2;
                max[axis] += delta / 2;
            }
        }
    }
};

static constexpr aabb empty_aabb =
    aabb{empty_interval, empty_interval, empty_interval};
static constexpr aabb universe_aabb =
    aabb{universe_interval, universe_interval, universe_interval};

constexpr aabb operator+(aabb bbox, vec3 offset) {
    return aabb(bbox.min + offset, bbox.max + offset);
}

constexpr aabb operator+(vec3 offset, aabb bbox) { return bbox + offset; }

#endif
