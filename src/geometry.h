#pragma once

#include "aabb.h"
#include "ray.h"
#include "vec3.h"

struct geometry_record {
    double t;
};

struct uvs {
    double u, v;
};

struct geometry {
    virtual aabb bounding_box() const = 0;
    virtual bool hit(ray const &r, interval ray_t,
                     geometry_record &rec) const = 0;
    virtual void getUVs(uvs &uv, point3 intersection, double time) const = 0;
    // NOTE: intersection is only used by sphere & box, time is only used by sphere.
    virtual vec3 getNormal(point3 const& intersection, double time) const = 0;
};
