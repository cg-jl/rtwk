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

#include <utility>

#include "color.h"
#include "perlin.h"
#include "rtw_stb_image.h"
#include "rtweekend.h"
#include "storage.h"

// NOTE: Before moving checker_texture into a tree, I should first make
// 'texture' enum based, so that point_samplers don't have to deal with extra
// layers of indirection. The maximum size for each texture is 1 cacheline.The
// sampling loop could then move to sampling per texture and not per pixel,
// filtering through e.g texture kinds instead of just the pointers.

// Sizes:
//  - solid_color: 24
//  - checker_texture: 28
//  - noise: 16 (indirection through perlin)
//  - image: 16

struct texture;

struct checker_texture {
   public:
    constexpr checker_texture(float scale, color even, color odd)
        : inv_scale(1.0f / scale), even(even), odd(odd) {}

    [[nodiscard]] color value(point3 const& p) const {
        auto xInteger = static_cast<int>(std::floor(inv_scale * p.x()));
        auto yInteger = static_cast<int>(std::floor(inv_scale * p.y()));
        auto zInteger = static_cast<int>(std::floor(inv_scale * p.z()));

        bool isEven = (xInteger + yInteger + zInteger) % 2 == 0;

        return isEven ? even : odd;
    }

   private:
    float inv_scale;
    color even, odd;
};

struct noise_texture {
   public:
    constexpr noise_texture() = default;
    explicit noise_texture(float sc) : scale(sc) {}

    [[nodiscard]] color value(point3 const& p, perlin const* noise) const {
        auto s = scale * p;
        return color(1, 1, 1) * 0.5 * (1 + sin(s.z() + 10 * noise->turb(s)));
    }

   private:
    float scale{};
};

struct texture {
    enum class kind { solid, checker, noise, image } tag{};

    union data {
        color solid{};
        struct checker_texture checker;
        struct noise_texture noise;
        rtw_image image;
        constexpr data() {}
        ~data() {}
    } as;

    template <typename... Args>
    static texture solid(Args&&... args) {
        texture tex;
        tex.tag = kind::solid;
        new (&tex.as.solid) color(std::forward<Args&&>(args)...);
        return tex;
    }
    template <typename... Args>
    static texture checker(Args&&... args) {
        texture tex;
        tex.tag = kind::checker;
        new (&tex.as.checker) checker_texture(std::forward<Args&&>(args)...);
        return tex;
    }
    template <typename... Args>
    static texture noise(Args&&... args) {
        texture tex;
        tex.tag = kind::noise;
        new (&tex.as.noise) noise_texture(std::forward<Args&&>(args)...);
        return tex;
    }
    template <typename... Args>
    static texture image(Args&&... args) {
        texture tex;
        tex.tag = kind::image;
        new (&tex.as.image) rtw_image(std::forward<Args&&>(args)...);
        return tex;
    }

   private:
    constexpr texture() = default;
};
