#include "hittable_list.h"

#include <algorithm>
#include <print>
#include <tracy/Tracy.hpp>

#include "constant_medium.h"
#include "geometry.h"
#include "hittable.h"
#include "interval.h"
#include "rtweekend.h"
#include "trace_colors.h"

geometry const *hittable_list::hitSelect(ray const &r, interval ray_t,
                                         double &closestHit) const {
    ZoneNamedN(_tracy, "hittable_list hit", filters::surfaceHit);

    geometry const *best = nullptr;

    {
        ZoneNamedN(_tracy, "hit trees", filters::hit);
        for (auto const &tree : trees) {
            if (auto const next = tree.hitSelect(r, ray_t, closestHit); next) {
                ray_t.max = closestHit;
                best = next;
            }
        }
    }

    {
        ZoneNamedN(_tracy, "hit individuals", filters::hit);
        auto const hit_individual = hitSpan(selectGeoms, r, ray_t, closestHit);
        best = hit_individual ?: best;
    }

    return best;
}

void hittable_list::add(lightInfo object, geometry *geom) {
    geom->relIndex = objects.size();  // Make sure we link the texture/mat data.
    selectGeoms.emplace_back(geom);
    objects.emplace_back(object);
}

void hittable_list::add(constant_medium medium, color albedo) {
    cms.emplace_back(medium);
    cmAlbedos.emplace_back(albedo);
}

// TODO: @waste Consider giving just an (optional) index to cmAlbedos.
// The compiler may be optimizing for the wrong case (not having a null pointer)
// here, as well as the hitSelect result.
color const *hittable_list::sampleConstantMediums(ray const &ray,
                                                  interval ray_t,
                                                  double *hit) const noexcept {
    ZoneScoped;
    auto rayLength = ray.dir.length();
    color const *selected = nullptr;

    double currentHit = infinity;

    // NOTE: cmAlbedos and colors are linked not by refIndex, but are just
    // sorted. @maybe if I get to BVH'ing this (although currently it would be
    // detrimental), then I would need to store their indices here.

    // TODO: @waste @maybe The indices stored in the geometries are wasted here,
    // since we use a different (better) mechanism.


    for (size_t i = 0; i < cms.size(); ++i) {
        auto const &cm = cms[i];
        double tstart, tend;

        if (!cm.geom->hit(ray, universe_interval, tstart)) continue;
        if (!cm.geom->hit(ray, interval{tstart + 0.0001, infinity}, tend))
            continue;

        // intertse
        tstart = std::max(tstart, ray_t.min);
        tend = std::min(tend, ray_t.max);

        if (tstart >= tend) continue;

        // It doesn't matter how much we can disperse if we already dispersed
        // after this.
        if (tstart > currentHit) continue;

        auto hitDistance =
            cm.neg_inv_density * log(random_double()) / rayLength;

        auto thit = hitDistance + tstart;

        // Not dispersing anyway.
        if (thit > tend) continue;

        // The ray already got dispersed here
        if (currentHit < thit) continue;

        currentHit = thit;
        // NOTE: @waste We don't need to read the cmAlbedos pointer until we've selected
        // the index.
        selected = &cmAlbedos[i];
    }

    *hit = currentHit;
    return selected;
}
