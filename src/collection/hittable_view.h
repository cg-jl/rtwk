#pragma once

#include <span>

#include "collection.h"
#include "hittable.h"
#include "rtweekend.h"

namespace explicit_geometry {
template <is_hittable T>
static inline void propagate(std::span<T const> objects, ray const& r,
                             collection::hit_status& status,
                             hit_record& rec) noexcept {
    for (auto const& ob : objects) {
        status.hit_anything |= ob.hit(r, status.ray_t, rec);
    }
}
}  // namespace explicit_geometry

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

template <is_hittable T>
struct explicit_view final : public collection {
    std::span<T const> objects;

    constexpr explicit explicit_view(std::span<T const> objects)
        : objects(objects) {}

    [[nodiscard]] aabb aggregate_box() const& override {
        aabb box = objects[0].bounding_box();
        for (auto const& ob : objects.subspan(1)) {
            box = aabb(box, ob.bounding_box());
        }
        return box;
    }

    void propagate(ray const& r, hit_status& status,
                   hit_record& rec) const& override {
        return explicit_geometry::propagate(objects, r, status, rec);
    }
};