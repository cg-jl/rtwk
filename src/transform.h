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
    virtual void apply(ray& r, float time) const& = 0;
    virtual void apply_reverse(float time, hit_record& rec) const& = 0;
    virtual void apply_to_bbox(aabb& box) const& = 0;
};

struct translate final : public transform {
   public:
    translate(vec3 displacement) : offset(displacement) {}

    void apply(ray& r, float time) const& override { r.origin -= offset; }
    void apply_reverse(float time, hit_record& rec) const& override {
        rec.p += offset;
    }

    void apply_to_bbox(aabb& box) const& override { box = box + offset; }

   private:
    vec3 offset;
};

struct rotate_y final : public transform {
    float cos_theta, sin_theta;

    void apply_to_bbox(aabb& bbox) const& override {
        point3 min(infinity, infinity, infinity);
        point3 max(-infinity, -infinity, -infinity);

        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 2; j++) {
                for (int k = 0; k < 2; k++) {
                    auto x = float(i) * bbox.x.max + float(1 - i) * bbox.x.min;
                    auto y = float(j) * bbox.y.max + float(1 - j) * bbox.y.min;
                    auto z = float(k) * bbox.z.max + float(1 - k) * bbox.z.min;

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

        bbox = aabb(min, max);
    }

    void apply(ray& r, float time) const& override {
        vec3 origin = r.origin;
        vec3 direction = r.direction;
        origin[0] = cos_theta * r.origin[0] - sin_theta * r.origin[2];
        origin[2] = sin_theta * r.origin[0] + cos_theta * r.origin[2];

        direction[0] = cos_theta * r.direction[0] - sin_theta * r.direction[2];
        direction[2] = sin_theta * r.direction[0] + cos_theta * r.direction[2];

        r.origin = origin;
        r.direction = direction;
    }

    void apply_reverse(float time, hit_record& rec) const& override {
        // Change the intersection point from object space to world space
        auto p = rec.p;
        p[0] = cos_theta * rec.p[0] + sin_theta * rec.p[2];
        p[2] = -sin_theta * rec.p[0] + cos_theta * rec.p[2];

        // Change the normal from object space to world space
        auto normal = rec.normal;
        normal[0] = cos_theta * rec.normal[0] + sin_theta * rec.normal[2];
        normal[2] = -sin_theta * rec.normal[0] + cos_theta * rec.normal[2];

        rec.p = p;
        rec.normal = normal;
    }

    rotate_y(float angle) {
        auto radians = degrees_to_radians(angle);
        sincosf(radians, &sin_theta, &cos_theta);
    }
};

struct move final : public transform {
    vec3 move_segment;

    move(vec3 move_segment) : move_segment(move_segment) {}

    void apply_to_bbox(aabb& bbox) const& override {
        bbox = aabb(bbox, bbox + move_segment);
    }

    void apply(ray& r, float time) const& override {
        auto displacement = move_segment * time;

        r.origin -= displacement;
    }

    void apply_reverse(float time, hit_record& rec) const& override {
        auto displacement = move_segment * time;
        rec.p += displacement;
    }
};

// TODO: integrate transforms into hittable geometry, and return transform when
// we get a hit. This will allow us to apply the reverse transform only once per
// hit check.
struct transformed_geometry final : public hittable {
    std::vector<transform const*> transf;
    hittable const* object;

    transformed_geometry(std::vector<transform const*> transf,
                         hittable const* object)
        : transf(std::move(transf)), object(object) {}
    transformed_geometry(transform const* tf, hittable const* object)
        : transformed_geometry(std::vector{tf}, object) {}

    [[nodiscard]] aabb bounding_box() const& override {
        aabb box = object->bounding_box();
        for (auto const tf : transf) {
            tf->apply_to_bbox(box);
        }
        return box;
    }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        ray r_copy = r;

        for (auto const tf : transf) {
            tf->apply(r_copy, r.time);
        }

        if (!object->hit(r_copy, ray_t, rec)) return false;

        for (size_t i = transf.size(); i > 0;) {
            --i;
            auto const tf = transf[i];
            tf->apply_reverse(r.time, rec);
        }
        return true;
    }
};
