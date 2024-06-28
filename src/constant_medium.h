#ifndef CONSTANT_MEDIUM_H
#define CONSTANT_MEDIUM_H
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

#include "geometry.h"
#include "rtweekend.h"
#include "texture.h"

// NOTE: This doesn't fit into the hittable model because it's a dispersion
// model! Hitting the model adds "color" to the ray as if it had hit a particle
// and deviates it with a different frequency response (hence the dispersion).
// Ideally we should first hit a geometry, tell it how far the ray travelled,
// and make it do the dispersion to re-check if the ray travelled far enough to
// make it deviate from the geometry. Separating it is relevant here because its
// API usage is forced and there are many things that could be improved if it
// were a separate component:
// - all hittables will be uniform types -> can be SoA'd if necessary.
// - dispersion check will happen once per ray, which means that we don't mix
// double hitting with BVH hits, which could be specialized into geometry-only,
// reducing their apparent complexity.
// - 'constant medium' or 'disperse medium' will have a tighter API -> easily
// understood by compiler, and easily transferrable into shaders

// TODO: This is another candidate to make a separate array of, since
// its behavior with materials is pretty specific.
class constant_medium final {
   public:
    geometry *geom;
    texture const *tex;
    constant_medium(geometry *boundary, double density, texture *tex)
        : geom(boundary), tex(tex), neg_inv_density(-1 / density) {}

    constant_medium(geometry *boundary, double density, color const &albedo)
        : constant_medium(boundary, density, leak(texture::solid(albedo))) {}

    double neg_inv_density;
};

#endif
