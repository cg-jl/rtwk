#include "hittable_list.h"

#include <algorithm>
#include <print>
#include <tracy/Tracy.hpp>

#include "constant_medium.h"
#include "geometry.h"
#include "hittable.h"
#include "interval.h"
#include "rtweekend.h"

hittable const *hittable_list::hitSelect(ray const &r, interval ray_t,
                                         double &closestHit) const {
    ZoneScopedN("hittable_list hit");

    hittable const *best = nullptr;

    {
        ZoneScopedN("hit trees");
        for (auto const &tree : trees) {
            if (auto const next = tree.hitSelect(r, ray_t, closestHit); next) {
                ray_t.max = closestHit;
                best = next;
            }
        }
    }

    {
        ZoneScopedN("hit individuals");
        auto const hit_individual = hitSpan(objects, r, ray_t, closestHit);
        best = hit_individual ?: best;
    }

    return best;
}

void hittable_list::add(hittable object) { objects.emplace_back(object); }

void hittable_list::add(constant_medium medium) { cms.emplace_back(medium); }

constant_medium const *hittable_list::sampleConstantMediums(
    ray const &ray, interval ray_t, double *hit) const noexcept {
    ZoneScoped;
    auto rayLength = ray.dir.length();
    constant_medium const *selected = nullptr;

    double currentHit = infinity;

    for (auto const &cm : cms) {
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
        selected = &cm;
    }

    *hit = currentHit;
    return selected;
}
