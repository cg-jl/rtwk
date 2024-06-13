#include "constant_medium.h"

#include <tracy/Tracy.hpp>

bool constant_medium::hit(ray const &r, interval ray_t,
                          geometry_record &rec) const {
    ZoneScopedN("constant_medium hit");
    // Print occasional samples when debugging. To enable, set enableDebug
    // true.
    bool const enableDebug = false;
    bool const debugging = enableDebug && random_double() < 0.00001;

    geometry_record rec1, rec2;

    auto boundary = geom;

    // Find some entry point
    if (!boundary->hit(r, universe_interval, rec1)) return false;

    // Find exit point
    if (!boundary->hit(r, interval(rec1.t + 0.0001, infinity), rec2))
        return false;

    // Intersect the entry & exit with the given interval
    if (rec1.t < ray_t.min) rec1.t = ray_t.min;
    if (rec2.t > ray_t.max) rec2.t = ray_t.max;

    if (rec1.t >= rec2.t) return false;

    // entry should be at least 0 (should already be the case since `min`
    // doesn't ever change)
    if (rec1.t < 0) rec1.t = 0;

    auto ray_length = r.dir.length();
    auto distance_inside_boundary = (rec2.t - rec1.t) * ray_length;
    auto hit_distance = neg_inv_density * log(random_double());

    if (hit_distance > distance_inside_boundary) return false;

    rec.t = rec1.t + hit_distance / ray_length;
    rec.p = r.at(rec.t);

    rec.normal = vec3(1, 0, 0);  // arbitrary

    return true;
}
