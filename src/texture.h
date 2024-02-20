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
// 'texture' enum based, so that samplers don't have to deal with extra layers
// of indirection. The maximum size for each texture is 1 cacheline.The sampling
// loop could then move to sampling per texture and not per pixel, filtering
// through e.g texture kinds instead of just the pointers.

// NOTE: there is a tree structure in checker_texture.
// Sizes:
//  - solid_color: 32
//  - checker_texture: 48 (tree structure -> indirection + compute)
//  - noise: 48 (indirection through perlin)
//  - image: 40

struct texture;

namespace detail::texture {
static inline color value(struct texture const*, float u, float v,
                          point3 const& p) noexcept;
}

struct solid_color {
   public:
    template <typename... Args>
    explicit constexpr solid_color(Args&&... args)
        : color_value(std::forward<Args>(args)...) {}

    [[nodiscard]] color value(float u, float v, point3 const& p) const {
        return color_value;
    }

   private:
    color color_value;
};

struct checker_texture {
   public:
    constexpr checker_texture(float _scale, texture const* _even,
                              texture const* _odd)
        : inv_scale(1.0f / _scale), even(_even), odd(_odd) {}

    [[nodiscard]] color value(float u, float v, point3 const& p) const {
        auto xInteger = static_cast<int>(std::floor(inv_scale * p.x()));
        auto yInteger = static_cast<int>(std::floor(inv_scale * p.y()));
        auto zInteger = static_cast<int>(std::floor(inv_scale * p.z()));

        bool isEven = (xInteger + yInteger + zInteger) % 2 == 0;

        return isEven ? detail::texture::value(even, u, v, p)
                      : detail::texture::value(odd, u, v, p);
    }

   private:
    float inv_scale;
    texture const* even;
    texture const* odd;
};

// NOTE: ideally there should be only one perlin device.
// We don't lose anything for moving it out of here because it's readonly.
struct noise_texture {
   public:
    explicit noise_texture(float sc, perlin const* dev)
        : scale(sc), noise(dev) {}

    [[nodiscard]] color value(float u, float v, point3 const& p) const {
        auto s = scale * p;
        return color(1, 1, 1) * 0.5 * (1 + sin(s.z() + 10 * noise->turb(s)));
    }

   private:
    perlin const* noise;
    float scale{};
};

struct image_texture {
   public:
    explicit image_texture(char const* filename) : image(filename) {}

    [[nodiscard]] color value(float u, float v, point3 const& p) const {
        return image.sample(u, v);
    }

   private:
    rtw_image image;
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

    [[nodiscard]] color value(float u, float v, point3 const& p) const noexcept;

   private:
    constexpr texture() = default;
};
color texture::value(float u, float v, point3 const& p) const noexcept {
    switch (tag) {
        case kind::solid:
            return as.solid;
        case kind::checker:
            return as.checker.value(u, v, p);
        case kind::noise:
            return as.noise.value(u, v, p);
        case kind::image:
            return as.image.sample(u, v);
        default:
            __builtin_unreachable();
    }
}

namespace detail::texture {
static inline color value(struct texture const* tex, float u, float v,
                          point3 const& p) noexcept {
    return tex->value(u, v, p);
}
}  // namespace detail::texture
