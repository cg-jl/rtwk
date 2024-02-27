#pragma once

#include <span>

#include "material.h"
#include "vec3.h"

struct transform;
struct texture;

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
