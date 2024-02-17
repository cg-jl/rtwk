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

#include <utility>

#include "color.h"
#include "perlin.h"
#include "rtw_stb_image.h"
#include "rtweekend.h"
#include "storage.h"

// NOTE: there is a tree structure in checker_texture.
// Sizes:
//  - solid_color: 32
//  - checker_texture: 48 (tree structure -> indirection + compute)
//  - noise: 48 (indirection through perlin)
//  - image: 40

struct texture {
   public:
    virtual ~texture() = default;

    [[nodiscard]] virtual color value(float u, float v,
                                      point3 const& p) const = 0;
};

struct solid_color : public texture {
   public:
    template <typename... Args>
    explicit solid_color(Args&&... args)
        : color_value(std::forward<Args>(args)...) {}

    [[nodiscard]] color value(float u, float v,
                              point3 const& p) const override {
        return color_value;
    }

   private:
    color color_value;
};

struct checker_texture : public texture {
   public:
    checker_texture(float _scale, texture const* _even, texture const* _odd)
        : inv_scale(1.0f / _scale), even(_even), odd(_odd) {}

    checker_texture(float _scale, color c1, color c2,
                    typed_storage<texture>& storage)
        : inv_scale(1.0f / _scale),
          even(storage.make<solid_color>(c1)),
          odd(storage.make<solid_color>(c2)) {}

    [[nodiscard]] color value(float u, float v,
                              point3 const& p) const override {
        auto xInteger = static_cast<int>(std::floor(inv_scale * p.x()));
        auto yInteger = static_cast<int>(std::floor(inv_scale * p.y()));
        auto zInteger = static_cast<int>(std::floor(inv_scale * p.z()));

        bool isEven = (xInteger + yInteger + zInteger) % 2 == 0;

        return isEven ? even->value(u, v, p) : odd->value(u, v, p);
    }

   private:
    float inv_scale;
    texture const* even;
    texture const* odd;
};

struct noise_texture : public texture {
   public:
    noise_texture() = default;

    explicit noise_texture(float sc) : scale(sc) {}

    [[nodiscard]] color value(float u, float v,
                              point3 const& p) const override {
        auto s = scale * p;
        return color(1, 1, 1) * 0.5 * (1 + sin(s.z() + 10 * noise.turb(s)));
    }

   private:
    perlin noise;
    float scale{};
};

struct image_texture : public texture {
   public:
    explicit image_texture(char const* filename) : image(filename) {}

    [[nodiscard]] color value(float u, float v,
                              point3 const& p) const override {
        // If we have no texture data, then return solid cyan as a debugging
        // aid.
        if (image.height() <= 0) return {0, 1, 1};

        // Clamp input texture coordinates to [0,1] x [1,0]
        u = interval(0, 1).clamp(u);
        v = 1.0f - interval(0, 1).clamp(v);  // Flip V to image coordinates

        auto i = static_cast<int>(u * float(image.width()));
        auto j = static_cast<int>(v * float(image.height()));
        auto pixel = image.pixel_data(i, j);

        auto color_scale = 1.0f / 255.0f;
        return {color_scale * float(pixel[0]), color_scale * float(pixel[1]),
                color_scale * float(pixel[2])};
    }

   private:
    rtw_image image;
};

#endif
