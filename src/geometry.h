#pragma once

#include "ray.h"
#include "vec3.h"

struct geometry_record {
    point3 p;
    vec3 normal;
    double t;
};

struct uvs {
    double u, v;
};
