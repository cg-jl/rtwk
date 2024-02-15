#pragma once

#include <span>

#include "collection.h"
#include "hittable.h"
#include "rtweekend.h"

struct hittable_view final : public collection {
    std::span<hittable const* const> objects;

    constexpr explicit hittable_view(std::span<hittable const* const> objects)
        : objects(objects) {}

    [[nodiscard]] aabb aggregate_box() const& override {
        aabb box = empty_bb;
        for (auto const h : objects) {
            box = aabb(box, h->bounding_box());
        }
        return box;
    }

    static void propagate(ray const& r, hit_status& status, hit_record& rec,
                          std::span<hittable const* const> objects) {
        for (auto const& object : objects) {
            status.hit_anything |= object->hit(r, status.ray_t, rec);
        }
    }

    void propagate(ray const& r, hit_status& status,
                   hit_record& rec) const& override {
        return propagate(r, status, rec, objects);
    }
};
