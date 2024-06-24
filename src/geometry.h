#pragma once

#include "aabb.h"
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

struct geometry {
    virtual aabb bounding_box() const = 0;
    virtual bool hit(ray const &r, interval ray_t,
                     geometry_record &rec) const = 0;
    // NOTE: If I can put in the time, I can get the UVs off of the intersection for all things I think.
    virtual void getUVs(uvs &uv, point3 intersection, vec3 normal) const = 0;
};
