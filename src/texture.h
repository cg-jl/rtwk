#ifndef TEXTURE_H
#define TEXTURE_H
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

#include <tracy/Tracy.hpp>

#include "color.h"
#include "geometry.h"
#include "perlin.h"
#include "rtw_stb_image.h"
#include "rtweekend.h"
#include "vec3.h"

class texture {
   public:
    enum class tag {
        solid,
        checker,
        image,
        noise,
    } kind;

    struct noise_data {
        perlin noise;
        double scale;
    };

    struct checker_data {
        double inv_scale;
        texture const *even;
        texture const *odd;
    };

    union data {
        checker_data checker;
        rtw_image image;
        noise_data noise;
        color solid;

        constexpr data() {}
        ~data() {}

    } as;

    constexpr texture(tag kind, data &&d)
        : kind(kind), as(std::forward<data &&>(d)) {}

    static texture checker(double scale, texture const *even,
                           texture const *odd) {
        data d;
        new (&d.checker) checker_data{scale, even, odd};
        return texture(tag::checker, std::move(d));
    }

    template <typename... Ts>
    static texture solid(Ts &&...ts) {
        data d;
        new (&d.solid) color(ts...);
        return texture(tag::solid, std::move(d));
    }

    static texture image(char const *filename) {
        data d;
        new (&d.image) rtw_image(filename);
        return texture(tag::image, std::move(d));
    }

    static texture noise(double scale) {
        data d;
        new (&d.noise.noise) perlin();
        d.noise.scale = scale;
        return texture(tag::noise, std::move(d));
    }

    color value(uvs uv, point3 const &p) const;
};

namespace texture_samplers {
inline color sample_checker(texture::checker_data const &data, uvs uv,
                            point3 const &p) {
    ZoneScoped;

    auto xint = int(std::floor(data.inv_scale * p.x()));
    auto yint = int(std::floor(data.inv_scale * p.y()));
    auto zint = int(std::floor(data.inv_scale * p.z()));

    auto is_even = (xint + yint + zint) % 2 == 0;
    return is_even ? data.even->value(uv, p) : data.odd->value(uv, p);
}

inline color sample_image(rtw_image const &img, uvs uv) {
    ZoneScoped;

    static constexpr auto unit = interval(0, 1);

    uv.u = unit.clamp(uv.u);
    uv.v = unit.clamp(uv.v);

    auto i = int(uv.u * img.width());
    auto j = int((1 - uv.v) * img.height());
    auto px = img.pixel_data(i, j);

    auto col_scale = 1. / 255.;

    return color(col_scale * px[0], col_scale * px[1], col_scale * px[2]);
}

inline color sample_noise(texture::noise_data const &data, point3 const &p) {
    ZoneScoped;

    return color(.5, .5, .5) *
           (1 + std::sin(data.scale * p.z() + 10 * data.noise.turb(p, 7)));
}

}  // namespace texture_samplers

color texture::value(uvs uv, point3 const &p) const {
    switch (kind) {
        case tag::solid:
            return as.solid;
        case tag::checker:
            return texture_samplers::sample_checker(as.checker, uv, p);
        case tag::image:
            return texture_samplers::sample_image(as.image, uv);
        case tag::noise:
            return texture_samplers::sample_noise(as.noise, p);
    }
    __builtin_unreachable();
}

#endif
