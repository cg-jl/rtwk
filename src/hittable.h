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

#include <span>
#include <tracy/Tracy.hpp>

#include "aabb.h"
#include "geometry.h"
#include "rtweekend.h"

class material;

class hit_record {
   public:
    geometry_record geom;
    uvs uv;
    bool front_face;

    void set_face_normal(ray const &r, vec3 const &outward_normal) {
        // Sets the hit record normal vector.
        // NOTE: the parameter `outward_normal` is assumed to have unit length.

        front_face = dot(r.direction(), outward_normal) < 0;
        geom.normal = front_face ? outward_normal : -outward_normal;
    }
};

struct spatially_bounded {
    virtual aabb bounding_box() const = 0;
};

struct hittable : public spatially_bounded {
    material *mat;
    virtual ~hittable() = default;

    constexpr explicit hittable(material *mat) : mat(mat) {}

    virtual bool hit(ray const &r, interval ray_t,
                     geometry_record &rec) const = 0;
    virtual void getUVs(uvs &uv, point3 intersection, vec3 normal) const = 0;
};

// NOTE: maybe some sort of infra to have a hittable hit() and also restore()
// prepare(), end() as well to prepare a ray?
// We should end in a geometry anyway.

// TODO: this is not a hittable (no material) nor a selector. Transforms
// should be in the hittable class itself, and hit() separated into
// internalHit() (assumes transform) and hit()
class translate : public hittable {
   public:
    constexpr translate(hittable *object, vec3 offset)
        : hittable(object->mat), object(object), offset(offset) {}

    bool hit(ray const &r, interval ray_t, geometry_record &rec) const final {
        ZoneScopedN("translate hit");
        // Move the ray backwards by the offset
        ray offset_r(r.origin() - offset, r.direction(), r.time());

        // Determine whether an intersection exists along the offset ray (and if
        // so, where)
        if (!object->hit(offset_r, ray_t, rec)) return false;

        // Move the intersection point forwards by the offset
        rec.p += offset;

        return true;
    }

    // NOTE: Since the point and the normals have been already been changed
    // (otherwise the material would have a wrong normal to process), transforms
    // pass the baton to their wrapped object.
    void getUVs(uvs &uv, point3 p, vec3 normal) const final {
        return object->getUVs(uv, p, normal);
    }

    aabb bounding_box() const final { return object->bounding_box() + offset; }

   private:
    hittable *object;
    vec3 offset;
};

// TODO: this is not a hittable (no material) nor a selector. Transforms
// should be in the hittable class itself, and hit() separated into
// internalHit() (assumes transform) and hit()
class rotate_y : public hittable {
   public:
    rotate_y(hittable *object, double angle)
        : hittable(object->mat), object(object) {
        auto radians = degrees_to_radians(angle);
        sin_theta = sin(radians);
        cos_theta = cos(radians);
    }

    bool hit(ray const &r, interval ray_t, geometry_record &rec) const final {
        ZoneScopedN("rotate_y hit");
        // Change the ray from world space to object space
        auto origin = r.origin();
        auto direction = r.direction();

        origin[0] = cos_theta * r.origin()[0] - sin_theta * r.origin()[2];
        origin[2] = sin_theta * r.origin()[0] + cos_theta * r.origin()[2];

        direction[0] =
            cos_theta * r.direction()[0] - sin_theta * r.direction()[2];
        direction[2] =
            sin_theta * r.direction()[0] + cos_theta * r.direction()[2];

        ray rotated_r(origin, direction, r.time());

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
    // NOTE: Since the point and the normals have been already been changed
    // (otherwise the material would have a wrong normal to process), transforms
    // pass the baton to their wrapped object.
    void getUVs(uvs &uv, point3 p, vec3 normal) const final {
        return object->getUVs(uv, p, normal);
    }

    aabb bounding_box() const final {
        auto bbox = object->bounding_box();

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
        return bbox;
    }

   private:
    hittable *object;
    double sin_theta;
    double cos_theta;
};

static hittable const *hitSpan(std::span<hittable *const> objects, ray const &r,
                               interval ray_t, geometry_record &rec) {
    ZoneScoped;
    ZoneValue(objects.size());

    geometry_record temp_rec;
    hittable const *best = nullptr;
    auto closest_so_far = ray_t.max;

    for (auto const *object : objects) {
        if (object->hit(r, interval(ray_t.min, closest_so_far), rec)) {
            best = object;
            rec = temp_rec;
        }
    }

    return best;
}

#endif
