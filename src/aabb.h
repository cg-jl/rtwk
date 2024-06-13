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

class aabb {
   public:
    interval x, y, z;

    // The default AABB is empty, since intervals are empty by default.
    constexpr aabb() = default;

    constexpr aabb(interval x, interval y, interval z) : x(x), y(y), z(z) {
        pad_to_minimums();
    }

    constexpr aabb(point3 const &a, point3 const &b) {
        // Treat the two points a and b as extrema for the bounding box, so we
        // don't require a particular minimum/maximum coordinate order.

        x = (a[0] <= b[0]) ? interval(a[0], b[0]) : interval(b[0], a[0]);
        y = (a[1] <= b[1]) ? interval(a[1], b[1]) : interval(b[1], a[1]);
        z = (a[2] <= b[2]) ? interval(a[2], b[2]) : interval(b[2], a[2]);

        pad_to_minimums();
    }

    constexpr aabb(aabb const &box0, aabb const &box1)
        : x(box0.x, box1.x), y(box0.y, box1.y), z(box0.z, box1.z) {}

    constexpr interval const &axis_interval(int n) const {
        if (n == 1) return y;
        if (n == 2) return z;
        return x;
    }

    bool hit(ray const &r, interval ray_t) const;

    constexpr int longest_axis() const {
        // Returns the index of the longest axis of the bounding box.

        if (x.size() > y.size())
            return x.size() > z.size() ? 0 : 2;
        else
            return y.size() > z.size() ? 1 : 2;
    }

   private:
    constexpr void pad_to_minimums() {
        // Adjust the AABB so that no side is narrower than some delta, padding
        // if necessary.

        double delta = 0.0001;
        if (x.size() < delta) x = x.expand(delta);
        if (y.size() < delta) y = y.expand(delta);
        if (z.size() < delta) z = z.expand(delta);
    }
};

static constexpr aabb empty_aabb =
    aabb{empty_interval, empty_interval, empty_interval};
static constexpr aabb universe_aabb =
    aabb{universe_interval, universe_interval, universe_interval};

constexpr aabb operator+(aabb bbox, vec3 offset) {
    return aabb(bbox.x + offset.x(), bbox.y + offset.y(), bbox.z + offset.z());
}

constexpr aabb operator+(vec3 offset, aabb bbox) { return bbox + offset; }

#endif
