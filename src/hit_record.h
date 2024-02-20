#pragma once

#include "material.h"
#include "texture.h"
#include "vec3.h"

struct transform;

struct hit_record {
   public:
    point3 p;
    vec3 normal;
    float u{};
    float v{};
    material mat{};
    // TODO: Inline texture struct into this?
    texture const* tex{};
    std::span<transform const> xforms{};
};
