#pragma once
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

#include <cmath>

#include "interval.h"
#include "ray.h"
#include "rtweekend.h"
#include "vec3.h"

struct aabb {
   public:
    interval x, y, z;

    aabb() = default;

    aabb(interval const& ix, interval const& iy, interval const& iz)
        : x(ix), y(iy), z(iz) {}

    aabb(point3 const& a, point3 const& b) {
        // Treat the two points a and b as extrema for the bounding box, so we
        // don't require a particular minimum/maximum coordinate order.
        x = interval(std::fmin(a[0], b[0]), std::fmax(a[0], b[0]));
        y = interval(std::fmin(a[1], b[1]), std::fmax(a[1], b[1]));
        z = interval(std::fmin(a[2], b[2]), std::fmax(a[2], b[2]));
    }

    aabb(aabb const& box0, aabb const& box1) {
        x = interval(box0.x, box1.x);
        y = interval(box0.y, box1.y);
        z = interval(box0.z, box1.z);
    }

    [[nodiscard]] aabb pad() const {
        // Return an AABB that has no side narrower than some delta, padding if
        // necessary.
        float delta = 0.0001;
        interval new_x = (x.size() >= delta) ? x : x.expand(delta);
        interval new_y = (y.size() >= delta) ? y : y.expand(delta);
        interval new_z = (z.size() >= delta) ? z : z.expand(delta);

        return {new_x, new_y, new_z};
    }

    [[nodiscard]] interval const& axis(int n) const {
        interval const* intv;
        switch (n) {
            case 0:
                intv = &x;
                break;
            case 1:
                intv = &y;
                break;
            case 2:
                intv = &z;
                break;
            default:
                __builtin_unreachable();
        }
        return *intv;
    }

    [[nodiscard]] bool hit(ray const& r, interval ray_t) const {
        for (int a = 0; a < 3; a++) {
            auto invD = 1 / r.direction[a];
            auto orig = r.origin[a];

            auto t0 = (axis(a).min - orig) * invD;
            auto t1 = (axis(a).max - orig) * invD;

            if (invD < 0) std::swap(t0, t1);

            if (t0 > ray_t.min) ray_t.min = t0;
            if (t1 < ray_t.max) ray_t.max = t1;

            if (ray_t.max <= ray_t.min) return false;
        }
        return true;
    }
};

inline aabb operator+(aabb const& bbox, vec3 const& offset) {
    return {bbox.x + offset.x(), bbox.y + offset.y(), bbox.z + offset.z()};
}

inline aabb operator+(vec3 const& offset, aabb const& bbox) {
    return bbox + offset;
}

static aabb const empty_bb =
    aabb(interval::empty, interval::empty, interval::empty);
