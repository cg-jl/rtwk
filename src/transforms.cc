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
                      double &closestHit) const {
    ZoneScopedN("transformed hit");
    ray tfr{r};
    translate.transformRayOpposite(tfr);
    rotate.transformRayOpposite(tfr);

    return object->hit(tfr, ray_t, closestHit);
}

void transformed::getUVs(uvs &uv, point3 p, double time) const {
    return object->getUVs(uv, p, time);
}

vec3 transformed::getNormal(point3 const &intersection, double time) const {
    // NOTE: Since `hit` transforms into the transformed space, we have to get
    // back to the local space.
    point3 p = intersection;
    p = p - translate.offset;
    p = rotateY::applyInverse(p, rotate.sin_theta, rotate.cos_theta);

    auto normal = object->getNormal(p, time);

    rotate.transformPoint(normal);
    return normal;
}

rotate_y::rotate_y(double angle) {
    auto radians = degrees_to_radians(angle);
    sin_theta = sin(radians);
    cos_theta = cos(radians);
}
