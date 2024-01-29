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

#include "rtweekend.h"
struct interval {
   public:
    double min, max;

    interval() : min(+infinity), max(-infinity) {}  // Default interval is empty

    interval(double _min, double _max) : min(_min), max(_max) {}

    interval(interval const& a, interval const& b)
        : min(fmin(a.min, b.min)), max(fmax(a.max, b.max)) {}

    double size() const { return max - min; }

    interval expand(double delta) const {
        auto padding = delta / 2;
        return interval(min - padding, max + padding);
    }

    bool contains(double x) const { return min <= x && x <= max; }

    bool surrounds(double x) const { return min < x && x < max; }

    double clamp(double x) const {
        if (x < min) return min;
        if (x > max) return max;
        return x;
    }

    static const interval empty, universe;
};

inline const interval interval::empty = interval(+infinity, -infinity);
inline const interval interval::universe = interval(-infinity, +infinity);

inline interval operator+(interval const& ival, double displacement) {
    return interval(ival.min + displacement, ival.max + displacement);
}

inline interval operator+(double displacement, interval const& ival) {
    return ival + displacement;
}

#endif
