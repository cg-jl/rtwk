#pragma once

#include <utility>

#include "aabb.h"
#include "hit_record.h"
#include "hittable.h"
#include "ray.h"
#include "rtweekend.h"

// NOTE: What if I make 'packs' of transforms via some sort of storage?
// I could then apply those rays differently to the shapes (and filters like
// BVH) that use those transforms, and then hit all of them.

// Transforms on an object prior to the ray intersection.
// These transforms are applied first to the ray, and then applied in reverse to
// the result.
struct transform {
    enum class kind {
        // NOTE: translate can be seen as a move with fixed time.
        translate,
        rotate_y,
        // NOTE: move can be seen as a partial translate
        move,
    } tag{};

    union data {
        vec3 displacement;
        struct rotate_y {
            float sin_theta;
            float cos_theta;
        } rotation{};

        constexpr data() {}
        ~data() {}
    } as;

    static transform move(vec3 displacement) {
        transform tf;
        tf.tag = kind::move;
        tf.as.displacement = displacement;
        return tf;
    }

    static transform translate(vec3 displacement) {
        transform tf;
        tf.tag = kind::translate;
        tf.as.displacement = displacement;
        return tf;
    }

    static transform rotate_y(float degrees) {
        transform tf;
        tf.tag = kind::rotate_y;
        sincosf(degrees_to_radians(degrees), &tf.as.rotation.sin_theta,
                &tf.as.rotation.cos_theta);
        return tf;
    }

    explicit constexpr transform(kind tag) : tag(tag) {}

    void apply(ray& r, float time) const&;
    void apply_reverse(float time, hit_record& rec) const&;
    void apply_to_bbox(aabb& box) const&;

   private:
    constexpr transform() = default;
};

static inline void apply_reverse_transforms(std::span<transform const> xforms,
                                            float time, hit_record& rec) {
    for (size_t i = xforms.size(); i != 0;) {
        --i;
        xforms[i].apply_reverse(time, rec);
    }
}

// TODO: integrate transforms into hittable geometry, and return transform when
// we get a hit. This will allow us to apply the reverse transform only once per
// hit check.
struct transformed_geometry final : public hittable {
    // TODO: make these not owned
    std::vector<transform> transf;
    hittable const* object;

    transformed_geometry(std::vector<transform> transf, hittable const* object)
        : transf(std::move(transf)), object(object) {}
    transformed_geometry(transform tf, hittable const* object)
        : transformed_geometry(std::vector{std::move(tf)}, object) {}

    [[nodiscard]] aabb bounding_box() const& override {
        aabb box = object->bounding_box();
        for (auto const& tf : transf) {
            tf.apply_to_bbox(box);
        }
        return box;
    }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        ray r_copy = r;

        for (auto const& tf : transf) {
            tf.apply(r_copy, r.time);
        }

        if (!object->hit(r_copy, ray_t, rec)) return false;

        rec.xforms = transf;
        return true;
    }
};
inline void transform::apply(ray& r, float time) const& {
    switch (tag) {
        case kind::translate:
            r.origin -= as.displacement;
            break;
        case kind::rotate_y: {
            vec3 origin = r.origin;
            vec3 direction = r.direction;

            auto cos_theta = as.rotation.cos_theta;
            auto sin_theta = as.rotation.sin_theta;

            origin[0] = cos_theta * r.origin[0] - sin_theta * r.origin[2];
            origin[2] = sin_theta * r.origin[0] + cos_theta * r.origin[2];

            direction[0] =
                cos_theta * r.direction[0] - sin_theta * r.direction[2];
            direction[2] =
                sin_theta * r.direction[0] + cos_theta * r.direction[2];

            r.origin = origin;
            r.direction = direction;
        } break;

            // NOTE: only 'move' requires the time sample. Maybe there's some
            // way to cache the sampled time value with a thread local? When
            // switching to a component-based approach, maybe the sample can be
            // done earlier. idea: Some kind of structure that allows me to
            // separate each move set by threads, and make each thread sample
            // the move at the time they want to. idea: Have threads sample a
            // new value each time, and share the value (cache coherency?)
        case kind::move: {
            auto displacement = as.displacement * time;

            r.origin -= displacement;
        } break;
    }
}
inline void transform::apply_reverse(float time, hit_record& rec) const& {
    switch (tag) {
        case kind::move: {
            auto displacement = as.displacement * time;
            rec.p += displacement;
        } break;
        case kind::rotate_y: {
            // Change the intersection point from object space to world space
            auto p = rec.p;

            auto cos_theta = as.rotation.cos_theta;
            auto sin_theta = as.rotation.sin_theta;

            p[0] = cos_theta * rec.p[0] + sin_theta * rec.p[2];
            p[2] = -sin_theta * rec.p[0] + cos_theta * rec.p[2];

            // Change the normal from object space to world space
            auto normal = rec.normal;
            normal[0] = cos_theta * rec.normal[0] + sin_theta * rec.normal[2];
            normal[2] = -sin_theta * rec.normal[0] + cos_theta * rec.normal[2];

            rec.p = p;
            rec.normal = normal;
        } break;

        case kind::translate:
            rec.p += as.displacement;
            break;
    }
}
inline void transform::apply_to_bbox(aabb& box) const& {
    switch (tag) {
        case kind::translate:
            box = box + as.displacement;
            break;
        case kind::rotate_y: {
            point3 min(infinity, infinity, infinity);
            point3 max(-infinity, -infinity, -infinity);

            auto cos_theta = as.rotation.cos_theta;
            auto sin_theta = as.rotation.sin_theta;

            for (int i = 0; i < 2; i++) {
                for (int j = 0; j < 2; j++) {
                    for (int k = 0; k < 2; k++) {
                        auto x =
                            float(i) * box.x.max + float(1 - i) * box.x.min;
                        auto y =
                            float(j) * box.y.max + float(1 - j) * box.y.min;
                        auto z =
                            float(k) * box.z.max + float(1 - k) * box.z.min;

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
        } break;
        case kind::move: {
            box = aabb(box, box + as.displacement);
        } break;
        default:
            __builtin_unreachable();
    }
}
