#pragma once

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
