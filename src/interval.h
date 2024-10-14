#pragma once
//==============================================================================================
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public
// Domain Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include <algorithm>
#include <cmath>

#include "rtweekend.h"
struct interval {
    double min, max;

    constexpr interval()
        : min(+infinity), max(-infinity) {}  // Default interval is empty

    constexpr interval(double min, double max) : min(min), max(max) {}

    // Create the interval tightly enclosing the two input intervals.
    constexpr interval(interval const &a, interval const &b)
        : min(std::min(a.min, b.min)), max(std::max(a.max, b.max)) {}

    constexpr double size() const { return max - min; }
    constexpr bool isEmpty() const { return min >= max; }

    constexpr bool contains(double x) const { return min <= x && x <= max; }

    constexpr bool surrounds(double x) const { return min < x && x < max; }
    constexpr bool atBorder(double x) const {
        return std::abs(min - x) <= 1e-8 || std::abs(x - max) <= 1e-8;
    }

    constexpr double clamp(double x) const { return std::clamp(x, min, max); }

    constexpr double midPoint() const { return min + (max - min) / 2.; }

    constexpr interval expand(double delta) const {
        auto padding = delta / 2;
        return interval(min - padding, max + padding);
    }
};

static constexpr interval empty_interval = interval(+infinity, -infinity);
static constexpr interval universe_interval = interval(-infinity, +infinity);

constexpr interval operator+(interval ival, double displacement) {
    return interval(ival.min + displacement, ival.max + displacement);
}

constexpr interval operator+(double displacement, interval ival) {
    return ival + displacement;
}
