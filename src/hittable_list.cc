#include "hittable_list.h"

#include <algorithm>
#include <print>
#include <tracy/Tracy.hpp>

#include "bvh.h"
#include "constant_medium.h"
#include "geometry.h"
#include "hittable.h"
#include "interval.h"
#include "ray.h"
#include "rtweekend.h"
#include "trace_colors.h"

std::pair<geometry_ptr, double> hittable_list::hitSelect(
    timed_ray const &r) const {
    ZoneNamedN(_tracy, "hittable_list hit", filters::surfaceHit);

    geometry_ptr best;
    double closestHit;

    std::tie(best, closestHit) = bvh::tree(treebld).hitBVH(r, infinity);

    {
        ZoneNamedN(_tracy, "hit individuals", filters::hit);
        std::tie(best, closestHit) = hitSpan(selectGeoms, r, best, closestHit);
    }

    return {best, closestHit};
}

void hittable_list::transformAll(transform tf) {
    for (auto &obj : treebld.geoms) {
        obj.applyTransform(tf);
    }
    for (auto& box : treebld.boxes) {
        box = tf.applyForward(box);
    }
    for (auto & obj : selectGeoms) {
        obj.applyTransform(tf);
    }
    for (auto &obj : cms) {
        switch (obj.geom.kind) {
            case traversable_geometry::kind::box:
                obj.geom.data.box = tf.applyForward(obj.geom.data.box);
                break;
            case traversable_geometry::kind::sphere:
                obj.geom.data.sphere =
                    sphere::applyTransform(obj.geom.data.sphere, tf);
                break;
        }
    }
}

void hittable_list::add(lightInfo object, geometry geom) {
    geom.relIndex = objects.size();  // Make sure we link the texture/mat data.
    selectGeoms.emplace_back(geom);
    objects.emplace_back(object);
}
void hittable_list::addTree(lightInfo object, geometry geom) {
    geom.relIndex = objects.size();  // Make sure we link the texture/mat data.
    treebld.geoms.emplace_back(geom);
    objects.emplace_back(object);
}

void hittable_list::add(constant_medium medium, color albedo) {
    cms.emplace_back(medium);
    cmAlbedos.emplace_back(albedo);
}

enum bool32 : uint32_t { True = 0xFFFFFFFFul, False = 0x0ul };

// TODO: @waste Consider giving just an (optional) index to cmAlbedos.
// The compiler may be optimizing for the wrong case (not having a null pointer)
// here, as well as the hitSelect result.
color const *hittable_list::sampleConstantMediums(timed_ray const &ray,
                                                  double const maxT,
                                                  double *hit) const noexcept {
    ZoneScoped;
    auto rayLength = ray.r.dir.length();
    color const *selected = nullptr;

    double currentHit = infinity;

    auto const minDist = minRayDist * rayLength;
    auto const maxDist = maxT * rayLength;

    for (size_t i = 0; i < cms.size(); ++i) {
        auto const &cm = cms[i];
        auto t = cm.geom.traverse(ray);

        if (t.isEmpty()) continue;

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
