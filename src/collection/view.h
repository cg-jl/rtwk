#pragma once

#include <span>

#include "collection.h"
#include "hittable.h"
#include "rtweekend.h"

template <is_hittable T>
struct view final {
    std::span<T const> objects;

    constexpr explicit view(std::span<T const> objects) : objects(objects) {}

    [[nodiscard]] aabb aggregate_box() const& {
        aabb box = empty_bb;
        for (auto const& h : objects) {
            box = aabb(box, h.boundingBox());
        }
        return box;
    }

    [[nodiscard]] constexpr explicit operator std::span<T const>()
        const noexcept {
        return objects;
    }

    [[nodiscard]] constexpr size_t size() const noexcept {
        return objects.size();
    }

    constexpr view<T> subspan(
        size_t start, size_t size = std::dynamic_extent) const noexcept {
        return view(objects.subspan(start, size));
    }

    static void propagate(ray const& r, hit_status& status, hit_record& rec,
                          std::span<T const> objects) {
        for (auto const& ob : objects) {
            status.hit_anything |= ob.hit(r, status.ray_t, rec);
        }
    }

    void propagate(ray const& r, hit_status& status, hit_record& rec) const& {
        return propagate(r, status, rec, objects);
    }
};

using poly_view = view<dyn_hittable>;
