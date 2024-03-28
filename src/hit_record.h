#pragma once

#include <span>

#include "material.h"
#include "texture.h"
#include "vec3.h"



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
