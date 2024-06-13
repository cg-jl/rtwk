#include "medium.h"

#include "random.h"

medium::medium(double density, texture const *response)
    : neg_inv_density(-1. / density), response(response) {}

medium::medium(double density, color const &albedo)
    : medium(density, new texture(texture::solid(albedo))) {}

bool medium::disperse(ray &r, double dist_inside_boundary) const noexcept {
    auto hit_distance = neg_inv_density * log(random_double());
    // No dispersion
    if (hit_distance > dist_inside_boundary) return false;
    // Yes dispersion
    r.orig += hit_distance * r.dir;
    // isotropic
    r.dir = unit_vector(random_vec());

    return true;
}

// will never disperse, since -inf (neg_inv_density * log...)
// is always < dist_inside_boundary
medium const medium::voidspace(0., color(1., 1., 1.));
