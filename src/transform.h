#pragma once

#include <span>
#include <utility>

#include "aabb.h"
#include "geometry.h"
#include "ray.h"
#include "rtweekend.h"

// Transforms on an object prior to the ray intersection.
// These transforms are applied first to the ray, and then applied in reverse to
// the result.
struct y_rotation {
    float sin_theta = 0;
    float cos_theta = 1;
    constexpr y_rotation() = default;

    explicit y_rotation(float degrees) {
        sincosf(degrees_to_radians(degrees), &this->sin_theta,
                &this->cos_theta);
    }
    constexpr y_rotation(float sin_theta, float cos_theta)
        : sin_theta(sin_theta), cos_theta(cos_theta) {}

    y_rotation compose(y_rotation next) {
        // we want to do both rotations.
        // These are the angle sum trig formulae.
        // Use e^j to arrive at the result faster
        return {sin_theta * next.cos_theta + cos_theta * next.sin_theta,
                cos_theta * next.cos_theta - sin_theta * next.sin_theta};
    }

    void apply(ray& r) const&;
    void apply_reverse(point3& p, vec3& normal) const&;
    void apply_to_bbox(aabb& box) const&;
};

struct transform_set {
    vec3 translate_fixed;
    vec3 translate_time;
    y_rotation rotation;
    void apply_reverse_transforms(point3& p, float time, vec3& normal) const& {
        rotation.apply_reverse(p, normal);

        p += translate_fixed;
        p += translate_time * time;
    }

    void apply(ray& r, float time) const& {
        r.origin -= translate_fixed;
        r.origin -= translate_time * time;
        rotation.apply(r);
    }

    void apply_to_bbox(aabb& box) const& {
        box = box + translate_fixed;
        box = aabb(box, box + translate_time);
        rotation.apply_to_bbox(box);
    }
};

// IMPORTANT: ORDER OF APPLICATION:
// translate (fixed) -> translate (move) -> rotate_y

// TODO: integrate transforms into geometry_wrapper
inline void y_rotation::apply(ray& r) const& {
    ZoneScoped;
    ZoneValue(int(this->tag));
    vec3 origin = r.origin;
    vec3 direction = r.direction;

    origin[0] = cos_theta * r.origin[0] - sin_theta * r.origin[2];
    origin[2] = sin_theta * r.origin[0] + cos_theta * r.origin[2];

    direction[0] = cos_theta * r.direction[0] - sin_theta * r.direction[2];
    direction[2] = sin_theta * r.direction[0] + cos_theta * r.direction[2];

    r.origin = origin;
    r.direction = direction;
}
static inline point3 inv_y_rotation(float cos_theta, float sin_theta,
                                    point3 p) {
    auto x = p.x();
    auto y = p.y();
    auto z = p.z();
    return point3{x * cos_theta + sin_theta * z, y,
                  -sin_theta * x + cos_theta * z};
}
inline void y_rotation::apply_reverse(point3& p, vec3& normal_target) const& {
    p = inv_y_rotation(cos_theta, sin_theta, p);
    normal_target = inv_y_rotation(cos_theta, sin_theta, normal_target);
}
inline void y_rotation::apply_to_bbox(aabb& box) const& {
    point3 min(infinity, infinity, infinity);
    point3 max(-infinity, -infinity, -infinity);

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++) {
                auto x = float(i) * box.x.max + float(1 - i) * box.x.min;
                auto y = float(j) * box.y.max + float(1 - j) * box.y.min;
                auto z = float(k) * box.z.max + float(1 - k) * box.z.min;

                auto newx = cos_theta * x + sin_theta * z;
                auto newz = -sin_theta * x + cos_theta * z;

                vec3 tester(newx, y, newz);

                for (int c = 0; c < 3; c++) {
                    min[c] = std::fmin(min[c], tester[c]);
                    max[c] = std::fmax(max[c], tester[c]);
                }
            }
        }
    }

    box = aabb(min, max);
}

struct xform_builder {
    vec3 translate_fixed{};
    vec3 translate_time{};
    y_rotation rotation{};
    void move(vec3 max_displacement) { translate_time += max_displacement; }

    void translate(vec3 displacement) { translate_fixed += displacement; }

    void rotate_y(float degrees) {
        rotation = rotation.compose(y_rotation(degrees));
    }

    transform_set finish() {
        transform_set next = {translate_fixed, translate_time, rotation};
        translate_fixed = {};
        translate_time = {};
        rotation = {};
        return next;
    }
};
