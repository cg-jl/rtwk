#include "transforms.h"

#include <tracy/Tracy.hpp>

namespace rotateY {

static point3 world2Local(point3 orig, double sin_theta, double cos_theta) {
    point3 p = orig;
    p[0] = cos_theta * orig[0] - sin_theta * orig[2];
    p[2] = sin_theta * orig[0] + cos_theta * orig[2];

    return p;
}

static point3 local2World(point3 local, double sin_theta, double cos_theta) {
    auto p = local;
    p[0] = cos_theta * local[0] + sin_theta * local[2];
    p[2] = -sin_theta * local[0] + cos_theta * local[2];
    return p;
}

}  // namespace rotateY
bool translate::hit(ray const &r, interval ray_t, geometry_record &rec) const {
    ZoneScopedN("translate hit");
    // Move the ray backwards by the offset
    ray offset_r(r.orig - offset, r.dir, r.time);

    // Determine whether an intersection exists along the offset ray (and if
    // so, where)
    if (!object->hit(offset_r, ray_t, rec)) return false;

    // Move the intersection point forwards by the offset
    rec.p += offset;

    return true;
}

bool rotate_y::hit(ray const &r, interval ray_t, geometry_record &rec) const {
    ZoneScopedN("rotate_y hit");
    // Change the ray from world space to object space
    auto origin = rotateY::world2Local(r.orig, sin_theta, cos_theta);
    auto direction = rotateY::world2Local(r.dir, sin_theta, cos_theta);

    ray rotated_r(origin, direction, r.time);

    // Determine whether an intersection exists in object space (and if so,
    // where)
    if (!object->hit(rotated_r, ray_t, rec)) return false;

    // Change the intersection point from object space to world space
    auto p = rotateY::local2World(rec.p, sin_theta, cos_theta);

    // Change the normal from object space to world space
    auto normal = rotateY::local2World(rec.normal, sin_theta, cos_theta);

    rec.p = p;
    rec.normal = normal;

    return true;
}

rotate_y::rotate_y(geometry const *object, double angle) : object(object) {
    auto radians = degrees_to_radians(angle);
    sin_theta = sin(radians);
    cos_theta = cos(radians);
}

aabb rotate_y::bounding_box() const {
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

// NOTE: Since the point and the normals have been already been changed
// (otherwise the material would have a wrong normal to process), transforms
// pass the baton to their wrapped object.
void translate::getUVs(uvs &uv, point3 p, vec3 normal) const {
    return object->getUVs(uv, p, normal);
}
void rotate_y::getUVs(uvs &uv, point3 p, vec3 normal) const {
    return object->getUVs(uv, p, normal);
}
