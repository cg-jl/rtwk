#include "hittable.h"

#include <geometry.h>

#include <print>
#include <span>
#include <tracy/Tracy.hpp>

#include "trace_colors.h"

geometry const *hitSpan(std::span<geometry const *const> objects, ray const &r,
                        double &closestHit) {
    ZoneScopedNC("hit span", Ctp::Green);
    ZoneValue(objects.size());

    geometry const *best = nullptr;

    for (auto const &object : objects) {
        double t;
        if (object->hit(r, t) && interval{minRayDist, closestHit}.contains(t)) {
            best = object;
            closestHit = t;
        }
    }

    return best;
}
