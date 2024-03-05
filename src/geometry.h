#pragma once

#include <span>
#include <type_traits>

#include "aabb.h"
#include "hit_record.h"
#include "interval.h"
#include "ray.h"

struct transform;

template <typename T>
concept has_bb = requires(T const &t) {
    { t.boundingBox() } -> std::same_as<aabb>;
};

template <typename T>
concept is_geometry =
    requires(T const &g, hit_record::geometry const &p, float &u, float &v) {
        { g.getUVs(p, u, v) } -> std::same_as<void>;
    } &&
    requires(T const &g, ray const &r, hit_record::geometry &res,
             interval &ray_t) {
        { g.hit(r, res, ray_t) } -> std::same_as<bool>;
    } &&
    requires(T const &g) {
        { g.getTransforms() } -> std::same_as<std::span<transform const>>;
    } && has_bb<T>;

template <typename Coll>
concept is_geometry_collection =
    requires(Coll const &gc, ray const &r, hit_record::geometry &res,
             interval &ray_t) {
        { gc.hit(r, res, ray_t) } -> std::same_as<typename Coll::Type const *>;
    } &&
    is_geometry<typename Coll::Type> && has_bb<Coll>;

template <is_geometry T>
struct geometry_view {
    std::span<T const> objects;
    using Type = T;

    [[nodiscard]] constexpr size_t size() const { return objects.size(); }
    [[nodiscard]] constexpr geometry_view subspan(
        size_t offset, size_t count = std::dynamic_extent) const noexcept {
        return geometry_view{objects.subspan(offset, count)};
    };

    [[nodiscard]] aabb boundingBox() const {
        aabb box = empty_bb;
        for (auto const &ob : objects) {
            box = aabb(box, ob.boundingBox());
        }
        return box;
    }

    [[nodiscard]] T const *hit(ray const &r, hit_record::geometry &res,
                               interval &ray_t) const & {
        T const *best = nullptr;
        for (auto const &ob : objects) {
            if (ob.hit(r, res, ray_t)) best = &ob;
        }
        return best;
    }
};
