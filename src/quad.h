#pragma once
//==============================================================================================
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public
// Domain Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include <aabb.h>
#include <vec3.h>

#include <cassert>

#include "transforms.h"

// @perf length(u) == length(v)?
// @perf dot(u, v) == 0.
struct quad {
    quad(point3 Q, vec3 u, vec3 v) : Q(Q), u(u), v(v) {
        assert(dot(v, u) == 0.);
    }

    aabb bounding_box() const {
        // Compute the bounding box of all four vertices.
        auto bbox_diagonal1 = aabb(Q, Q + u + v);
        auto bbox_diagonal2 = aabb(Q + u, Q + v);
        return aabb(bbox_diagonal1, bbox_diagonal2);
    }

    double hit(ray r) const;

    uvs getUVs(point3 intersection) const;
    vec3 getNormal() const;

    static quad applyTransform(quad q, transform tf) noexcept;

    // @perf length(u) == length(v)?
    // @perf dot(u,v ) == 0.
    point3 Q;
    vec3 u, v;
};
