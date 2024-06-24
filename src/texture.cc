
#include "texture.h"

#include <tracy/Tracy.hpp>

texture texture::checker(double scale, texture const *even,
                         texture const *odd) {
    data d;
    new (&d.checker) checker_data{scale, even, odd};
    return texture(tag::checker, std::move(d));
}
texture texture::solid(color col) {
    data d;
    d.solid = col;
    return texture(tag::solid, std::move(d));
}

texture texture::image(char const *filename) {
    data d;
    d.image = new rtw_image(filename);
    return texture(tag::image, std::move(d));
}

texture texture::noise(double scale) {
    data d;
    new (&d.noise) noise_data{scale};
    return texture(tag::noise, std::move(d));
}

namespace texture_samplers {
// TODO: make checker have (non-)terminals so we can just have a tree of textures
// each leaf node is a texture pointer and each split node has the inv_scale.
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

}  // namespace texture_samplers
color texture::value(uvs uv, point3 const &p, perlin const& noise) const {
    switch (kind) {
        case tag::solid:
            return as.solid;
        case tag::checker:
            return texture_samplers::sample_checker(as.checker, uv, p, noise);
        case tag::image:
            return texture_samplers::sample_image(*as.image, uv);
        case tag::noise:
            return texture_samplers::sample_noise(as.noise, p, noise);
    }
    __builtin_unreachable();
}
