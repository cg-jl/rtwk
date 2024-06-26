
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
    // make sure we leak it so that we don't accidentally
    // free the memory.
    auto img = leak(rtw_image(filename));
    new (&d.image) rtw_shared_image(img->share());
    return texture(tag::image, std::move(d));
}

texture texture::noise(double scale) {
    data d;
    new (&d.noise) noise_data{scale};
    return texture(tag::noise, std::move(d));
}
