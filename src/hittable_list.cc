#include "hittable_list.h"

#include <array>
#include <iostream>
#include <print>
#include <tracy/Tracy.hpp>


hittable const *hittable_list::hitSelect(ray const &r, interval ray_t,
                                         geometry_record &rec) const {
    ZoneScopedN("hittable_list hit");

    hittable const *best = nullptr;

    {
        ZoneScopedN("hit trees");
        for (auto const &tree : trees) {
            if (auto const next = tree.hitSelect(r, ray_t, rec); next) {
                ray_t.max = rec.t;
                best = next;
            }
        }
    }

    {
        ZoneScopedN("hit individuals");
        auto const hit_individual = hitSpan(objects, r, ray_t, rec);
        best = hit_individual ?: best;
    }

    return best;
}
