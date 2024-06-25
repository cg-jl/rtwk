
#include "texture.h"

#include <tracy/Tracy.hpp>

#include "texture_impls.h"

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

color texture::value(uvs uv, point3 const &p, perlin const &noise) const {
    switch (kind) {
        case tag::solid:
            return as.solid;
        case tag::checker:
            return sample_checker(as.checker, uv, p, noise);
        case tag::image:
            return sample_image(*as.image, uv);
        case tag::noise:
            return sample_noise(as.noise, p, noise);
    }
    __builtin_unreachable();
}
