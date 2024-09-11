#pragma once
#include <tracy/Tracy.hpp>

#include "perlin.h"
#include "texture.h"
#include "interval.h"
#include "trace_colors.h"

inline color sample_image(rtw_shared_image img, uvs uv) {
    ZoneScopedN("image");
    ZoneColor(Ctp::Teal);

    static constexpr auto unit = interval(0, 1);

    uv.u = unit.clamp(uv.u);
    uv.v = unit.clamp(uv.v);

    auto i = int(uv.u * img.image_width);
    auto j = int((1 - uv.v) * img.image_height);
    auto px = img.pixel_data(i, j);

    // NOTE: @coversion from f32 -> f64.
    // Maybe it's time to drop to floats?
    return {px[0], px[1], px[2]};
}

inline color sample_noise(texture::noise_data const &data, point3 const &p,
                          perlin const &perlin) {
    ZoneScopedN("noise");
    ZoneColor(Ctp::Blue);

    return color(.5, .5, .5) *
           (1 + std::sin(data.scale * p.z() + 10 * perlin.turb(p, 7)));
}

inline texture const *checkerSelect(texture::checker_data const &data,
                                    point3 const &p) {
    auto xint = int(std::floor(data.inv_scale * p.x()));
    auto yint = int(std::floor(data.inv_scale * p.y()));
    auto zint = int(std::floor(data.inv_scale * p.z()));
    auto is_even = (xint + yint + zint) % 2 == 0;
    return is_even ? data.even : data.odd;
}

// Traverses the checker tree and finds a texture that is not a checker.
inline texture const *traverseChecker(texture const *tex, point3 const &p) {
    ZoneScopedN("traverse checker");
    ZoneColor(Base);
    while (tex->kind == texture::tag::checker) {
        tex = checkerSelect(tex->as.checker, p);
    }
    return tex;
}
