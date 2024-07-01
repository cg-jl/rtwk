#include <geometry.h>

#include <span>
#include <tracy/Tracy.hpp>

#include "trace_colors.h"

geometry const *hitSpan(std::span<geometry const * const> objects, ray const &r,
                        interval ray_t, double &closestHit) {
    ZoneScopedNC("hit span", Ctp::Green);
    ZoneValue(objects.size());

    geometry const *best = nullptr;
    auto closest_so_far = ray_t.max;

    for (auto const &object : objects) {
        if (object->hit(r, interval(ray_t.min, closest_so_far), closestHit)) {
            best = object;
            closest_so_far = closestHit;
        }
    }

    return best;
}
