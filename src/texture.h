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

#include "color.h"
#include "geometry.h"
#include "perlin.h"
#include "rtw_stb_image.h"
#include "rtweekend.h"

class texture {
   public:
    virtual ~texture() = default;

    virtual color value(uvs uv, point3 const &p) const = 0;
};

class solid_color : public texture {
   public:
    solid_color(color const &albedo) : albedo(albedo) {}

    solid_color(double red, double green, double blue)
        : solid_color(color(red, green, blue)) {}

    color value(uvs uv, point3 const &p) const final { return albedo; }

   private:
    color albedo;
};

class checker_texture : public texture {
   public:
    checker_texture(double scale, texture *even, texture *odd)
        : inv_scale(1.0 / scale), even(even), odd(odd) {}

    checker_texture(double scale, color const &c1, color const &c2)
        : inv_scale(1.0 / scale),
          even(new solid_color(c1)),
          odd(new solid_color(c2)) {}

    color value(uvs uv, point3 const &p) const final {
        auto xInteger = int(std::floor(inv_scale * p.x()));
        auto yInteger = int(std::floor(inv_scale * p.y()));
        auto zInteger = int(std::floor(inv_scale * p.z()));

        bool isEven = (xInteger + yInteger + zInteger) % 2 == 0;

        return isEven ? even->value(uv, p) : odd->value(uv, p);
    }

   private:
    double inv_scale;
    texture *even;
    texture *odd;
};

class image_texture : public texture {
   public:
    image_texture(char const *filename) : image(filename) {}

    color value(uvs uv, point3 const &p) const final {
        // If we have no texture data, then return solid cyan as a debugging
        // aid.
        if (image.height() <= 0) return color(0, 1, 1);

        // Clamp input texture coordinates to [0,1] x [1,0]
        uv.u = interval(0, 1).clamp(uv.u);
        uv.v = 1.0 - interval(0, 1).clamp(uv.v);  // Flip V to image coordinates

        auto i = int(uv.u * image.width());
        auto j = int(uv.v * image.height());
        auto pixel = image.pixel_data(i, j);

        auto color_scale = 1.0 / 255.0;
        return color(color_scale * pixel[0], color_scale * pixel[1],
                     color_scale * pixel[2]);
    }

   private:
    rtw_image image;
};

class noise_texture : public texture {
   public:
    noise_texture() {}

    noise_texture(double scale) : scale(scale) {}

    color value(uvs _uv, point3 const &p) const final {
        return color(.5, .5, .5) *
               (1 + sin(scale * p.z() + 10 * noise.turb(p, 7)));
    }

   private:
    perlin noise;
    double scale;
};

#endif
