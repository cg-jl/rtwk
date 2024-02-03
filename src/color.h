#pragma once
//==============================================================================================
// Originally written in 2020 by Peter Shirley <ptrshrl@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public
// Domain Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include <cstdint>
#include <iostream>

#include "interval.h"
#include "vec3.h"

using color = vec3;
struct icolor {
    uint8_t r, g, b;
} __attribute__((packed));

inline float linear_to_gamma(float linear_component) {
    return sqrtf(linear_component);
}

inline void discretize(color pixel_color, icolor &output) {
    auto r = pixel_color.x();
    auto g = pixel_color.y();
    auto b = pixel_color.z();
    // Apply the linear to gamma transform.
    r = linear_to_gamma(r);
    g = linear_to_gamma(g);
    b = linear_to_gamma(b);
    // Write the translated [0,255] value of each color component.
    static const interval intensity(0.000, 0.999);
    output.r = static_cast<uint8_t>(256 * intensity.clamp(r));
    output.g = static_cast<uint8_t>(256 * intensity.clamp(g));
    output.b = static_cast<uint8_t>(256 * intensity.clamp(b));
}
