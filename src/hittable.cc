#include "hittable.h"

#include <tracy/Tracy.hpp>

hittable const *hitSpan(std::span<hittable const> objects, ray const &r,
                        interval ray_t, geometry_record &rec) {
    ZoneScoped;
    ZoneValue(objects.size());

    hittable const *best = nullptr;
    auto closest_so_far = ray_t.max;

    for (auto const &object : objects) {
        if (object.hit(r, interval(ray_t.min, closest_so_far), rec)) {
            best = &object;
            closest_so_far = rec.t;
        }
    }

    return best;
}
