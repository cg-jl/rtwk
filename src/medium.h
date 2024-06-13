#pragma once

// Physical medium where light travels.
// Useful to simulate "reflections" on different parts of the medium.
#include "geometry.h"
#include "ray.h"
#include "texture.h"
struct medium {
    double neg_inv_density;
    // NOTE: It doesn't make sense that a medium has a texture response.
    // A better way to model this is with color.
    // Might want to look at frequency dependent alphas or something,
    // that way we can have different responses at different levels.
    texture const *response;

    medium(double density, color const &albedo);
    medium(double density, texture const *response);

    // TODO: disperse the time from propagating too! Ensure that the time
    // we spent propagating the ray makes things move a little bit.

    // Disperse an incoming ray into another boundary.
    // Returns whether the ray was dispersed (modified).
    bool disperse(ray &r, double dist_inside_boundary) const noexcept;

    static medium const voidspace;
};
