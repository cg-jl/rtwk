#pragma once

#include "vec3.h"
struct face {
    vec3 normal;
    bool is_front;

    face(vec3 in_dir, vec3 normal) {
        is_front = dot(in_dir, normal) < 0;
        this->normal = is_front ? normal : -normal;
    }
};
