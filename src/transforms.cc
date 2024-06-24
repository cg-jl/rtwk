#include "transforms.h"

#include <array>
#include <tracy/Tracy.hpp>

#include "geometry.h"

namespace rotateY {

static point3 applyInverse(point3 orig, double sin_theta, double cos_theta) {
    point3 p = orig;
    p[0] = cos_theta * orig[0] - sin_theta * orig[2];
    p[2] = sin_theta * orig[0] + cos_theta * orig[2];

    return p;
}

static point3 applyForward(point3 local, double sin_theta, double cos_theta) {
    auto p = local;
    p[0] = cos_theta * local[0] + sin_theta * local[2];
    p[2] = -sin_theta * local[0] + cos_theta * local[2];
    return p;
}

}  // namespace rotateY

void translate::transformPoint(point3 &point) const noexcept {
    point += offset;
}

void translate::doTransform(point3 &point, vec3 &normal) const noexcept {
    point += offset;
}

void translate::transformRayOpposite(ray &r) const noexcept {
    r.orig = r.orig - offset;
}

void rotate_y::transformPoint(point3 &point) const noexcept {
    point = rotateY::applyForward(point, sin_theta, cos_theta);
}

void rotate_y::doTransform(point3 &hitPoint, vec3 &normal) const noexcept {
    hitPoint = rotateY::applyForward(hitPoint, sin_theta, cos_theta);
    normal = rotateY::applyForward(normal, sin_theta, cos_theta);
}

void rotate_y::transformRayOpposite(ray &r) const noexcept {
    r.orig = rotateY::applyInverse(r.orig, sin_theta, cos_theta);
    r.dir = rotateY::applyInverse(r.dir, sin_theta, cos_theta);
}

transformed::transformed(geometry const *object, rotate_y rotate,
                         struct translate translate)
    : object(object), rotate(rotate), translate(translate) {}

aabb transformed::bounding_box() const {
    aabb bbox = object->bounding_box();

    point3 min(infinity, infinity, infinity);
    point3 max(-infinity, -infinity, -infinity);

    for (auto p : std::array{
             point3{bbox.x.min, bbox.y.min, bbox.z.max},
             point3{bbox.x.min, bbox.y.min, bbox.z.min},
             point3{bbox.x.min, bbox.y.max, bbox.z.max},
             point3{bbox.x.min, bbox.y.max, bbox.z.min},
             point3{bbox.x.max, bbox.y.min, bbox.z.max},
             point3{bbox.x.max, bbox.y.min, bbox.z.min},
             point3{bbox.x.max, bbox.y.max, bbox.z.max},
             point3{bbox.x.max, bbox.y.max, bbox.z.min},
         }) {
        auto tester = p;

        rotate.transformPoint(tester);
        translate.transformPoint(tester);

        for (int c = 0; c < 3; c++) {
            min[c] = fmin(min[c], tester[c]);
            max[c] = fmax(max[c], tester[c]);
        }
    }

    return aabb(min, max);
}

bool transformed::hit(ray const &r, interval ray_t,
                      geometry_record &rec) const {
    ZoneScopedN("transformed hit");
    ray tfr{r};
    translate.transformRayOpposite(tfr);
    rotate.transformRayOpposite(tfr);

    if (!object->hit(tfr, ray_t, rec)) return false;

    {
        ZoneScopedN("de-transform");
        ZoneColor(tracy::Color::PowderBlue);
        rotate.doTransform(rec.p, rec.normal);
        translate.doTransform(rec.p, rec.normal);
    }
    return true;
}

void transformed::getUVs(uvs &uv, point3 p, vec3 normal) const {
    // NOTE: we already transform p/normal in the return. *maybe* transforming
    // them only here is required. normal may need to be a reference if so
    // (since it's used for material calcs)
    return object->getUVs(uv, p, normal);
}

// bool translate::hit(ray const &r, interval ray_t, geometry_record &rec) const
// {
//     ZoneScopedN("translate hit");
//     // Move the ray backwards by the offset
//     ray offset_r(r.orig - offset, r.dir, r.time);
//
//     // Determine whether an intersection exists along the offset ray (and if
//     // so, where)
//     if (!object->hit(offset_r, ray_t, rec)) return false;
//
//     // Move the intersection point forwards by the offset
//     rec.p += offset;
//
//     return true;
// }
//
// bool rotate_y::hit(ray const &r, interval ray_t, geometry_record &rec) const
// {
//     ZoneScopedN("rotate_y hit");
//     // Change the ray from world space to object space
//     auto origin = rotateY::applyInverse(r.orig, sin_theta, cos_theta);
//     auto direction = rotateY::applyInverse(r.dir, sin_theta, cos_theta);
//
//     ray rotated_r(origin, direction, r.time);
//
//     // Determine whether an intersection exists in object space (and if so,
//     // where)
//     if (!object->hit(rotated_r, ray_t, rec)) return false;
//
//     // Change the intersection point from object space to world space
//     auto p = rotateY::applyForward(rec.p, sin_theta, cos_theta);
//
//     // Change the normal from object space to world space
//     auto normal = rotateY::applyForward(rec.normal, sin_theta, cos_theta);
//
//     rec.p = p;
//     rec.normal = normal;
//
//     return true;
// }
//
rotate_y::rotate_y(double angle) {
    auto radians = degrees_to_radians(angle);
    sin_theta = sin(radians);
    cos_theta = cos(radians);
}
//
// aabb rotate_y::bounding_box() const {
//     auto bbox = object->bounding_box();
//
//     point3 min(infinity, infinity, infinity);
//     point3 max(-infinity, -infinity, -infinity);
//
//     for (auto p : std::array{
//              point3{bbox.x.min, bbox.y.min, bbox.z.max},
//              point3{bbox.x.min, bbox.y.min, bbox.z.min},
//              point3{bbox.x.min, bbox.y.max, bbox.z.max},
//              point3{bbox.x.min, bbox.y.max, bbox.z.min},
//              point3{bbox.x.max, bbox.y.min, bbox.z.max},
//              point3{bbox.x.max, bbox.y.min, bbox.z.min},
//              point3{bbox.x.max, bbox.y.max, bbox.z.max},
//              point3{bbox.x.max, bbox.y.max, bbox.z.min},
//          }) {
//         auto tester = rotateY::applyForward(p, sin_theta, cos_theta);
//
//         for (int c = 0; c < 3; c++) {
//             min[c] = fmin(min[c], tester[c]);
//             max[c] = fmax(max[c], tester[c]);
//         }
//     }
//
//     bbox = aabb(min, max);
//     return bbox;
// }
