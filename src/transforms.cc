#include "transforms.h"

#include <array>
#include <tracy/Tracy.hpp>

#include "rtweekend.h"

namespace rotateY {


static point3 applyForward(point3 local, double sin_theta, double cos_theta) {
    auto p = local;
    p[0] = cos_theta * local[0] + sin_theta * local[2];
    p[2] = -sin_theta * local[0] + cos_theta * local[2];
    return p;
}

}  // namespace rotateY

// @perf could make these available in the header file :]

point3 transform::applyForward(point3 p) const noexcept {
    p = rotateY::applyForward(p, sin_theta, cos_theta);
    p += offset;
    return p;
}


aabb transform::applyForward(aabb bbox) const noexcept {
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
        auto tester = applyForward(p);

        for (int c = 0; c < 3; c++) {
            min[c] = fmin(min[c], tester[c]);
            max[c] = fmax(max[c], tester[c]);
        }
    }

    return aabb(min, max);
}


transform::transform(double angleDegrees, vec3 offset) noexcept
    : offset(offset) {
    auto angleRad = degrees_to_radians(angleDegrees);
    sin_theta = std::sin(angleRad);
    cos_theta = std::cos(angleRad);
}
