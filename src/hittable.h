#pragma once
//==============================================================================================
// Originally written in 2016 by Peter Shirley <ptrshrl@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public
// Domain Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include <cmath>
#include <utility>

#include "aabb.h"
#include "rtweekend.h"

struct material;

// NOTE: each shared_ptr occupies 16 bytes. What if we move that to 8 bytes by
// just having a const *?


struct hit_record {
   public:
    struct face {
        vec3 normal;
        bool is_front;

        face(vec3 in_dir, vec3 normal) {
            is_front = dot(in_dir, normal) < 0;
            this->normal = is_front ? normal : -normal;
        }
    };

    point3 p;
    vec3 normal;
    float u{};
    float v{};
    material const* mat{};
};

// NOTE: material could be the next thing to make common on all hittables so it
// can be migrated to somewhere else! Reason is that we don't need to access the
// material pointer until we're sure that it's the correct one. By doing this
// we'll ensure a cache miss, but we have one cache miss per hit already
// guaranteed, so it may give more than it takes.

// NOTE: hittable occupies 8 bytes and adds always a pointer indirection, just
// to get a couple of functions. What about removing one of the indirections?

struct hittable {
    // NOTE: is it smart to force one cacheline per hittable?
    // Now that we've reduced numbers to half the size, there is more space
    // available.
    // It would be nice, since then we could modify storage to inline the
    // structs into cachelines.

    virtual ~hittable() = default;
    [[nodiscard]] virtual aabb bounding_box() const& = 0;

    virtual bool hit(ray const& r, interval& ray_t, hit_record& rec) const = 0;

    bool hit(ray const& r, hit_record& rec) const& {
        auto bb = bounding_box();
        auto absolute_max_dist = (point3(bb.x.max, bb.y.max, bb.z.max) -
                                  point3(bb.x.min, bb.y.min, bb.z.min))
                                     .length_squared();
        interval t(0.001, absolute_max_dist);
        return hit(r, t, rec);
    }
};
inline bool hit_displaced(ray const& r, interval& ray_t, hit_record& rec,
                          vec3 displacement, hittable const& obj) {
    // Move the ray backwards by the offset
    ray offset_r(r.origin - displacement, r.direction, r.time);

    // Determine whether an intersection exists along the offset ray (and if
    // so, where)
    if (!obj.hit(offset_r, ray_t, rec)) return false;

    // Move the intersection point forwards by the offset
    rec.p += displacement;

    return true;
}

struct translate final : public hittable {
   public:
    translate(shared_ptr<hittable> p, vec3 displacement)
        : object(std::move(p)), offset(displacement) {}

    [[nodiscard]] aabb bounding_box() const& override {
        return object->bounding_box() + offset;
    }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        return hit_displaced(r, ray_t, rec, offset, *object);
    }

   private:
    shared_ptr<hittable> object;
    vec3 offset;
};

struct rotate_y final : public hittable {
    float cos_theta;
    float sin_theta;
    shared_ptr<hittable> object;
    aabb bbox;

    [[nodiscard]] aabb bounding_box() const& override { return bbox; }

    rotate_y(shared_ptr<hittable> p, float angle) : object(std::move(p)) {
        auto radians = degrees_to_radians(angle);
        sin_theta = std::sin(radians);
        cos_theta = std::cos(radians);

        // TODO: bounding box of rotated object (on Y axis)?
        bbox = object->bounding_box();

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

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        // Change the ray from world space to object space
        auto origin = r.origin;
        auto direction = r.direction;

        origin[0] = cos_theta * r.origin[0] - sin_theta * r.origin[2];
        origin[2] = sin_theta * r.origin[0] + cos_theta * r.origin[2];

        direction[0] = cos_theta * r.direction[0] - sin_theta * r.direction[2];
        direction[2] = sin_theta * r.direction[0] + cos_theta * r.direction[2];

        ray rotated_r(origin, direction, r.time);

        // Determine whether an intersection exists in object space (and if so,
        // where)
        if (!object->hit(rotated_r, ray_t, rec)) return false;

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

        return true;
    }
};

// Move item along a line. It is a 'translate' that is dependent on time.
struct move final : public hittable {
    vec3 move_segment;
    shared_ptr<hittable> child;

    move(vec3 move_segment, shared_ptr<hittable> child)
        : move_segment(move_segment), child(std::move(child)) {}

    aabb bounding_box() const& override {
        aabb orig = child->bounding_box();
        return aabb(orig, orig + move_segment);
    }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        auto const displacement = move_segment * r.time;

        return hit_displaced(r, ray_t, rec, displacement, *child);
    }
};
