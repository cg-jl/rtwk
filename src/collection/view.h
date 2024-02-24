#pragma once

#include <span>

#include "collection.h"
#include "hittable.h"
#include "rtweekend.h"

template <is_hittable T>
struct view final : public collection {
    std::span<T const> objects;

    constexpr explicit view(std::span<T const> objects)
        : objects(objects) {}

    [[nodiscard]] aabb aggregate_box() const& override {
        aabb box = empty_bb;
        for (auto const h : objects) {
            box = aabb(box, h.bounding_box());
        }
        return box;
    }

    static void propagate(ray const& r, hit_status& status, hit_record& rec,
                          std::span<T const> objects) {
        for (auto const& ob : objects) {
            status.hit_anything |= ob.hit(r, status.ray_t, rec);
        }
    }

    void propagate(ray const& r, hit_status& status,
                   hit_record& rec) const& override {
        return propagate(r, status, rec, objects);
    }
};

using poly_view = view<poly_hittable>;