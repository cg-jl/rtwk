#pragma once

#include <span>

#include "collection.h"
#include "hittable.h"
#include "rtweekend.h"

template <typename T>
struct view final {
    std::span<T const> objects;

    constexpr view(std::span<T const> objects) : objects(objects) {}

    [[nodiscard]] aabb boundingBox() const& {
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
        size_t start, size_t count = std::dynamic_extent) const noexcept {
        return view(objects.subspan(start, count));
    }

    static void propagate(ray const& r, hit_status& status, hit_record& rec,
                          std::span<T const> objects, float time) {
        for (auto const& ob : objects) {
            status.hit_anything |= ob.hit(r, status.ray_t, rec, time);
        }
    }

    using Type = T;

    T const* hit(ray const& r, hit_record::geometry& res,
                 interval& ray_t) const&
        requires(is_geometry<T>)
    {
        T const* best = nullptr;
        for (auto const& ob : objects) {
            if (ob.hit(r, res, ray_t)) best = &ob;
        }
        return best;
    }

    void propagate(ray const& r, hit_status& status, hit_record& rec) const&
        requires(time_invariant_hittable<T>)
    {
        for (auto const& ob : objects) {
            status.hit_anything |= ob.hit(r, status.ray_t, rec);
        }
    }

    void propagate(ray const& r, hit_status& status, hit_record& rec,
                   float time) const&
        requires(is_hittable<T>)
    {
        return propagate(r, status, rec, objects, time);
    }
};

using poly_view = view<dyn_hittable>;
