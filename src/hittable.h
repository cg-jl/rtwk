#pragma once
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

#include "geometry.h"
#include "material.h"
#include "texture.h"

struct lightInfo {
    material mat;
    texture const *tex;

    constexpr explicit lightInfo(material mat, texture const *tex)
        : mat(mat), tex(tex) {}
};

// Minimum ray distance prepared to remove any zero rounding errors.
static constexpr double minRayDist = 0.001;

// NOTE: maybe some sort of infra to have a hittable hit() and also restore()
// prepare(), end() as well to prepare a ray?
// We should end in a geometry anyway.

geometry const *hitSpan(std::span<geometry const> objects, ray const &r,
                        double &closestHit);
