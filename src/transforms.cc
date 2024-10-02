#include "transforms.h"

#include <array>
#include <tracy/Tracy.hpp>

#include "geometry.h"
#include "interval.h"
#include "rtweekend.h"
#include "trace_colors.h"

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

static void transformRayOpposite(transform const &tf, ray &r) noexcept {
    // translate back
    r.orig = r.orig - tf.offset;
    // rotate back
    r.orig = rotateY::applyInverse(r.orig, tf.sin_theta, tf.cos_theta);
    r.dir = rotateY::applyInverse(r.dir, tf.sin_theta, tf.cos_theta);
}

static void transformPoint(transform const &tf, point3 &point) noexcept {
    point = rotateY::applyForward(point, tf.sin_theta, tf.cos_theta);
    point += tf.offset;
}

transformed::transformed(geometry const *object, transform tf)
    : object(object), tf(tf) {}

aabb transformed::bounding_box() const {
    aabb bbox = object->bounding_box();

    point3 min(infinity, infinity, infinity);
    point3 max(-infinity, -infinity, -infinity);

    for (auto p : std::array{
             point3{bbox.min.x(), bbox.min.y(), bbox.max.z()},
             point3{bbox.min.x(), bbox.min.y(), bbox.min.z()},
             point3{bbox.min.x(), bbox.max.y(), bbox.max.z()},
             point3{bbox.min.x(), bbox.max.y(), bbox.min.z()},
             point3{bbox.max.x(), bbox.min.y(), bbox.max.z()},
             point3{bbox.max.x(), bbox.min.y(), bbox.min.z()},
             point3{bbox.max.x(), bbox.max.y(), bbox.max.z()},
             point3{bbox.max.x(), bbox.max.y(), bbox.min.z()},
         }) {
        auto tester = p;

        transformPoint(tf, tester);

        for (int c = 0; c < 3; c++) {
            min[c] = fmin(min[c], tester[c]);
            max[c] = fmax(max[c], tester[c]);
        }
    }

    return aabb(min, max);
}

bool transformed::hit(ray const &r, double &closestHit) const {
    ZoneNamedN(_tracy, "transformed hit", filters::hit);
    ray tfr{r};
    transformRayOpposite(tf, tfr);

    return object->hit(tfr, closestHit);
}

bool transformed::traverse(ray const &r, interval &intersect) const {
    // NOTE: @cutnpaste from transformed::hit
    ZoneNamedNC(_tracy, "transformed traverse", Ctp::Mantle, filters::hit);
    ray tfr{r};
    transformRayOpposite(tf, tfr);

    return object->traverse(tfr, intersect);
}

vec3 transformed::getNormal(point3 const &intersection, double time) const {
    // NOTE: Since `hit` transforms into the transformed space, we have to get
    // back to the local space.
    point3 p = intersection;
    p = p - tf.offset;
    p = rotateY::applyInverse(p, tf.sin_theta, tf.cos_theta);

    auto normal = object->getNormal(p, time);

    p = rotateY::applyForward(p, tf.sin_theta, tf.cos_theta);
    return normal;
}
transform::transform(double angleDegrees, vec3 offset) noexcept
    : offset(offset) {
    auto angleRad = degrees_to_radians(angleDegrees);
    sin_theta = std::sin(angleRad);
    cos_theta = std::cos(angleRad);
}
