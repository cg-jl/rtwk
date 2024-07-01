#pragma once

#include "aabb.h"
#include "ray.h"
#include "vec3.h"


struct uvs {
    double u, v;
};

// NOTE: @maybe separating them in tags is interesting
// for hitSelect but not for constantMediums.


// NOTE: @maybe including an index for a geometry allows
// for easy sorting without a special interface while keeping
// the texture/material stuff out.

struct geometry {
    virtual aabb bounding_box() const = 0;
    virtual bool hit(ray const &r, interval ray_t,
                     double &closest_hit) const = 0;
    virtual void getUVs(uvs &uv, point3 intersection, double time) const = 0;
    // NOTE: intersection is only used by sphere & box, time is only used by sphere.
    virtual vec3 getNormal(point3 const& intersection, double time) const = 0;
};
