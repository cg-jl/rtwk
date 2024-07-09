#include <geometry.h>

#include <span>
#include <tracy/Tracy.hpp>

#include "trace_colors.h"

geometry const *hitSpan(std::span<geometry const *const> objects, ray const &r,
                        double &closestHit) {
    ZoneScopedNC("hit span", Ctp::Green);
    ZoneValue(objects.size());

    geometry const *best = nullptr;

    for (auto const &object : objects) {
        if (object->hit(r, closestHit)) {
            best = object;
        }
    }

    return best;
}
