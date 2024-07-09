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
                                                  double const maxT,
                                                  double *hit) const noexcept {
    ZoneScoped;
    auto rayLength = ray.dir.length();
    color const *selected = nullptr;

    double currentHit = infinity;

    auto const minDist = 0.001 * rayLength;
    auto const maxDist = maxT * rayLength;

    // NOTE: cmAlbedos and colors are linked not by refIndex, but are just
    // sorted. @maybe if I get to BVH'ing this (although currently it would be
    // detrimental), then I would need to store their indices here.

    // TODO: @waste @maybe The indices stored in the geometries are wasted here,
    // since we use a different (better) mechanism.

    for (size_t i = 0; i < cms.size(); ++i) {
        auto const &cm = cms[i];
        interval t;

        if (!cm.geom->traverse(ray, t)) continue;

        auto tstart = t.min;
        auto tend = t.max;

        // intertse
        tstart = std::max(tstart, minDist);
        tend = std::min(tend, maxDist);

        if (tstart >= tend) continue;

        // It doesn't matter how much we can disperse if we already dispersed
        // after this.
        if (tstart > currentHit) continue;

        // -1/alpha * log([rand]) / len = t
        // t * rlen * (-alpha) =  log([rand])
        // e^(-alpha * t * rlen) = [rand]
        // [rand] is the diminishing factor.
        auto hitDistance = cm.neg_inv_density * log(random_double());

        auto thit = hitDistance + tstart;

        // Not dispersing anyway.
        if (thit > tend) continue;

        // The ray already got dispersed here
        if (currentHit < thit) continue;

        currentHit = thit;
        // NOTE: @waste We don't need to read the cmAlbedos pointer until we've
        // selected the index.
        selected = &cmAlbedos[i];
    }

    *hit = currentHit / rayLength;
    return selected;
}
