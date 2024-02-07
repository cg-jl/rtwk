#pragma once

#include "vec3.h"

struct material;

struct hit_record {
   public:
    struct face {
        vec3 normal;
        bool is_front;

        face(vec3 in_dir, vec3 normal) {
            is_front = dot(in_dir, normal) < 0;
            this->normal = is_front ? normal : -normal;
        }
    };

    point3 p;
    vec3 normal;
    float u{};
    float v{};
    material const* mat{};
};
