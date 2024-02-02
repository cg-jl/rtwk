#ifndef HITTABLE_H
#define HITTABLE_H
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

#include "aabb.h"
#include "rtweekend.h"

struct material;

// NOTE: each shared_ptr occupies 16 bytes. What if we move that to 8 bytes by
// just having a const ref?

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
    double u;
    double v;
    material const* mat;
};

struct hittable {
   public:
    aabb bbox = aabb{interval::empty, interval::empty, interval::empty};

    hittable() {}
    hittable(aabb bbox) : bbox(bbox) {}

    virtual ~hittable() = default;

    virtual bool hit(ray const& r, interval& ray_t, hit_record& rec) const = 0;

    bool hit(ray const& r, hit_record& rec) const& {
        auto bb = bbox;
        auto absolute_max_dist = (point3(bb.x.max, bb.y.max, bb.z.max) -
                                  point3(bb.x.min, bb.y.min, bb.z.min))
                                     .length_squared();
        interval t(0.001, absolute_max_dist);
        return hit(r, t, rec);
    }
};

struct translate : public hittable {
   public:
    translate(shared_ptr<hittable> p, vec3 const& displacement)
        : object(p), offset(displacement) {
        bbox = object->bbox + offset;
    }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        // Move the ray backwards by the offset
        ray offset_r(r.origin - offset, r.direction, r.time);

        // Determine whether an intersection exists along the offset ray (and if
        // so, where)
        if (!object->hit(offset_r, ray_t, rec)) return false;

        // Move the intersection point forwards by the offset
        rec.p += offset;

        return true;
    }

   private:
    shared_ptr<hittable> object;
    vec3 offset;
};

struct rotate_y : public hittable {
   public:
    rotate_y(shared_ptr<hittable> p, double angle) : object(p) {
        auto radians = degrees_to_radians(angle);
        sin_theta = sin(radians);
        cos_theta = cos(radians);
        bbox = object->bbox;

        point3 min(infinity, infinity, infinity);
        point3 max(-infinity, -infinity, -infinity);

        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 2; j++) {
                for (int k = 0; k < 2; k++) {
                    auto x = i * bbox.x.max + (1 - i) * bbox.x.min;
                    auto y = j * bbox.y.max + (1 - j) * bbox.y.min;
                    auto z = k * bbox.z.max + (1 - k) * bbox.z.min;

                    auto newx = cos_theta * x + sin_theta * z;
                    auto newz = -sin_theta * x + cos_theta * z;

                    vec3 tester(newx, y, newz);

                    for (int c = 0; c < 3; c++) {
                        min[c] = fmin(min[c], tester[c]);
                        max[c] = fmax(max[c], tester[c]);
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

   private:
    shared_ptr<hittable> object;
    double sin_theta;
    double cos_theta;
};

#endif
