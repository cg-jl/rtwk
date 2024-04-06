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

#include <cmath>
#include <span>
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

    [[nodiscard]] float value(point3 const& p, perlin const* noise) const {
        auto s = scale * p;
        auto waves = std::sin(s.z() + 10 * noise->turb(s));
        auto grayscale = 0.5f * (1.f + waves);
        return grayscale;
    }

   private:
    float scale{};
};

struct texture {
    enum class kind { solid, checker, noise, image } tag{};
    uint32_t id;
};

struct tex_view {
    std::span<rtw_image const> images;
    std::span<checker_texture const> checkers;
    std::span<noise_texture const> noises;
    std::span<color const> solids;

   private:
    constexpr tex_view() = default;
    constexpr tex_view(std::span<rtw_image const> images,
                       std::span<checker_texture const> checkers,
                       std::span<noise_texture const> noises,
                       std::span<color const> solids)
        : images(images), checkers(checkers), noises(noises), solids(solids) {}

    friend class tex_storage;
};

struct tex_storage {
    id_storage<rtw_image> images;
    id_storage<checker_texture> checkers;
    id_storage<noise_texture> noises;
    id_storage<color> solids;

    template <typename... Args>
    texture solid(Args&&... args) {
        texture tex;
        tex.id = solids.add(std::forward<Args>(args)...);
        tex.tag = texture::kind::solid;
        return tex;
    }
    template <typename... Args>
    texture checker(Args&&... args) {
        texture tex;
        tex.id = checkers.add(std::forward<Args>(args)...);
        tex.tag = texture::kind::checker;
        return tex;
    }
    template <typename... Args>
    texture noise(Args&&... args) {
        texture tex;
        tex.id = noises.add(std::forward<Args>(args)...);
        tex.tag = texture::kind::noise;
        return tex;
    }
    template <typename... Args>
    texture image(Args&&... args) {
        texture tex;
        tex.id = images.add(std::forward<Args>(args)...);
        tex.tag = texture::kind::image;
        return tex;
    }

    constexpr tex_view view() const noexcept {
        return tex_view(images.view(), checkers.view(), noises.view(),
                        solids.view());
    }
};
