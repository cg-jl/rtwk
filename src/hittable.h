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

#include "material.h"
#include "texture.h"

struct lightInfo {
    material mat;
    texture const *tex;

    constexpr explicit lightInfo(material mat, texture const *tex)
        : mat(mat), tex(tex) {}
};

struct geometry;
struct geometryFound {
    geometry const *ptr;
    double hit;
};

// Minimum ray distance prepared to remove any zero rounding errors.
static constexpr double minRayDist = 0.001;
