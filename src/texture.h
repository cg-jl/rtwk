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

#include "color.h"
#include "geometry.h"
#include "perlin.h"
#include "rtw_stb_image.h"
#include "vec3.h"

struct texture {
    enum class tag {
        solid,
        checker,
        image,
        noise,
    } kind;

    struct noise_data {
        double scale;
    };

    struct checker_data {
        double inv_scale;
        texture const *even;
        texture const *odd;
    };

    union data {
        checker_data checker;
        rtw_image const *image;
        noise_data noise;
        color solid;

        constexpr data() {}
        ~data() {}

    } as;

    constexpr texture(tag kind, data &&d)
        : kind(kind), as(std::forward<data &&>(d)) {}

    static texture checker(double scale, texture const *even,
                           texture const *odd);

    static texture solid(color col);

    static texture image(char const *filename);

    static texture noise(double scale);

    color value(uvs uv, point3 const &p, perlin const &noise) const;
};

namespace detail {
static texture black = texture::solid(color(0, 0, 0));
static texture white = texture::solid(color(1, 1, 1));
}  // namespace detail
