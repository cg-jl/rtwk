#ifndef INTERVAL_H
#define INTERVAL_H
//==============================================================================================
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public
// Domain Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include <cmath>

#include "rtweekend.h"
struct interval {
   public:
    float min, max;

    interval() : min(+infinity), max(-infinity) {}  // Default interval is empty

    interval(float _min, float _max) : min(_min), max(_max) {}

    interval(interval const& a, interval const& b)
        : min(std::fmin(a.min, b.min)), max(std::fmax(a.max, b.max)) {}

    [[nodiscard]] float size() const { return max - min; }

    [[nodiscard]] interval expand(float delta) const {
        auto padding = delta / 2;
        return {min - padding, max + padding};
    }

    [[nodiscard]] bool contains(float x) const { return min <= x && x <= max; }

    [[nodiscard]] bool surrounds(float x) const { return min < x && x < max; }

    [[nodiscard]] float clamp(float x) const {
        if (x < min) return min;
        if (x > max) return max;
        return x;
    }

    static interval const empty, universe;
};

inline interval const interval::empty = interval(+infinity, -infinity);
inline interval const interval::universe = interval(-infinity, +infinity);

inline interval operator+(interval const& ival, float displacement) {
    return {ival.min + displacement, ival.max + displacement};
}

inline interval operator+(float displacement, interval const& ival) {
    return ival + displacement;
}

#endif
