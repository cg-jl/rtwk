#pragma once

#include <span>

#include "material.h"
#include "texture.h"
#include "vec3.h"

struct transform;

using transform_set = std::span<transform const>;

struct hit_record {
    struct geometry {
        point3 p;
        vec3 normal;
    };

   public:
    geometry geom;
    float u{};
    float v{};
    material mat{};
    // TODO: Inline texture struct into this?
    texture tex{};
};
