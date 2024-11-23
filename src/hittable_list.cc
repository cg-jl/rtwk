#include "hittable_list.h"

#include <algorithm>
#include <print>
#include <ranges>
#include <span>
#include <tracy/Tracy.hpp>

#include "bvh.h"
#include "constant_medium.h"
#include "geometry.h"
#include "hittable.h"
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
    for (auto &box : treebld.boxes) {
        box = tf.applyForward(box);
    }
    for (auto &obj : selectGeoms) {
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

void hittable_list::sampleCMs(
    timed_ray *rays, uint32_t const len,
    std::pair<color const *, double> *results, SampleCM_Buffers buffers,
    std::function<void(uint32_t, uint32_t)> swap_rays) const noexcept {
    std::transform(rays, rays + len, buffers.rayLength,
                   [](auto const &ray) { return ray.r.dir.length(); });

    std::fill(buffers.selected, buffers.selected + len, std::nullopt);
    std::fill(buffers.currentHit, buffers.currentHit + len, infinity);

    // @perf specialize swaps to where they're needed
    auto swap = [&](auto i, decltype(i) j) {
        std::swap(buffers.currentHit[i], buffers.currentHit[j]);
        std::swap(buffers.selected[i], buffers.selected[j]);
        std::swap(buffers.rayLength[i], buffers.rayLength[j]);
        std::swap(buffers.traversals[i], buffers.traversals[j]);
        std::swap(buffers.thit[i], buffers.thit[j]);
        std::swap(results[i], results[j]);
        swap_rays(i, j);
    };

    using std::ranges::subrange;
    using std::views::iota;
    using std::views::zip;

    auto constexpr zero = decltype(len)(0);

    // @cleanup use webkit based format
    for (uint32_t cm_i = 0; cm_i < uint32_t(cms.size()); ++cm_i) {
        ZoneScopedN("ray|cm tick");
        auto const &cm = cms[cm_i];

        // @perf could separate cm.geom and cm.neg_inv_density.

        // @perf think about a traversal fn that walks the rays in bulk, for
        // each of the geometries. Later I could add partitioning to the mix so
        // I get less branch mispredicts.
        std::transform(rays, rays + len, buffers.traversals,
                       [&](auto const &ray) { return cm.geom.traverse(ray); });

        // Intersect with minimum distance that ray should travel.
        std::transform(buffers.rayLength, buffers.rayLength + len,
                       buffers.traversals, buffers.traversals,
                       [&](auto const rayLength, auto t) {
                           auto const minDist = rayLength * minRayDist;
                           t.min = std::max(t.min, minDist);
                           return t;
                       });

        auto const isempty_begin = partition(
            zero, len, swap,
            [&](auto const i) { return !buffers.traversals[i].isEmpty(); });

        // Discard the rays that have dispersed at an earlier point than the
        // current object hit.
        auto const dispersed_before_start_begin =
            partition(zero, isempty_begin, swap, [&](auto const i) {
                auto const tstart = buffers.traversals[i].min;
                auto const currentHit = buffers.currentHit[i];
                return !(tstart > currentHit);
            });

        // Hit distance calculation:
        // -1/alpha * log([rand]) / len = t
        // t * rlen * (-alpha) =  log([rand])
        // e^(-alpha * t * rlen) = [rand]
        // [rand] is the diminishing factor.

        // @perf random number generation could be taken a look for bulk
        // generation.
        std::generate(buffers.thit, buffers.thit + dispersed_before_start_begin,
                      [&]() { return random_double(); });

        std::transform(buffers.thit,
                       buffers.thit + dispersed_before_start_begin,
                       buffers.thit, log);
        std::transform(
            buffers.thit, buffers.thit + dispersed_before_start_begin,
            buffers.traversals, buffers.thit, [&](auto l, auto const &t) {
                auto const tstart = t.min;
                return cm.neg_inv_density * l + tstart;
            });

        // These rays do not disperse because the diminishing factor was not
        // big enough to absorb the ray.
        auto const hit_dist_outside_begin = partition(
            zero, dispersed_before_start_begin, swap, [&](auto const i) {
                auto const thit = buffers.thit[i];
                auto const tend = buffers.traversals[i].max;
                return !(thit > tend);
            });

        // These rays cannot disperse here because we calculated a different
        // object for them to be dispersed at.
        // @perf This forms a scan inside the algorithm. I should probably make
        // this routine just store the objects that dispersed and then find the
        // minimum later.
        auto const dispersed_before_hit_begin =
            partition(zero, hit_dist_outside_begin, swap, [&](auto const i) {
                auto const currentHit = buffers.currentHit[i];
                auto const thit = buffers.thit[i];
                return !(currentHit < thit);
            });

        // @perf this is just a copy! We can't ellide it using min() because
        // we also have to register the objects.
        // What if we first register all the results for all the objects and
        // then reduce them?

        std::copy(buffers.thit, buffers.thit + dispersed_before_hit_begin,
                  buffers.currentHit);

        std::fill(buffers.selected,
                  buffers.selected + dispersed_before_hit_begin, cm_i);
    }

    std::transform(buffers.currentHit, buffers.currentHit + len,
                   buffers.rayLength, buffers.currentHit,
                   [](auto hit, auto rlen) { return hit / rlen; });

    std::transform(buffers.currentHit, buffers.currentHit + len,
                   buffers.selected, results, [&](auto hit, auto sel) {
                       return std::pair{
                           sel.transform([&](auto i) { return &cmAlbedos[i]; })
                               .value_or(nullptr),
                           hit};
                   });
}
