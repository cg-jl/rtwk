#pragma once
#include <tracy/Tracy.hpp>

#include "texture.h"

static color sample_image(rtw_image const &img, uvs uv) {
    ZoneScoped;

    static constexpr auto unit = interval(0, 1);

    uv.u = unit.clamp(uv.u);
    uv.v = unit.clamp(uv.v);

    auto i = int(uv.u * img.image_width);
    auto j = int((1 - uv.v) * img.image_height);
    auto px = img.pixel_data(i, j);

    auto col_scale = 1. / 255.;

    return color(col_scale * px[0], col_scale * px[1], col_scale * px[2]);
}

static color sample_noise(texture::noise_data const &data, point3 const &p,
                          perlin const &perlin) {
    ZoneScoped;

    return color(.5, .5, .5) *
           (1 + std::sin(data.scale * p.z() + 10 * perlin.turb(p, 7)));
}

// TODO: make checker have (non-)terminals so we can just have a tree of
// textures each leaf node is a texture pointer and each split node has the
// inv_scale.
static color sample_checker(texture::checker_data const &data, uvs uv,
                            point3 const &p, perlin const &noise) {
    ZoneScoped;

    auto xint = int(std::floor(data.inv_scale * p.x()));
    auto yint = int(std::floor(data.inv_scale * p.y()));
    auto zint = int(std::floor(data.inv_scale * p.z()));

    auto is_even = (xint + yint + zint) % 2 == 0;
    return is_even ? data.even->value(uv, p, noise)
                   : data.odd->value(uv, p, noise);
}
