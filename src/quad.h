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

#include "geometry.h"

struct quad final : public geometry {
    constexpr quad(point3 Q, vec3 u, vec3 v) : Q(Q), u(u), v(v) {}

    aabb bounding_box() const final {
        // Compute the bounding box of all four vertices.
        auto bbox_diagonal1 = aabb(Q, Q + u + v);
        auto bbox_diagonal2 = aabb(Q + u, Q + v);
        return aabb(bbox_diagonal1, bbox_diagonal2);
    }

    bool hit(ray const &r, interval ray_t, double &closestHit) const final;

    void getUVs(uvs &uv, point3 intersection, double _time) const final;
    vec3 getNormal(point3 const& intersection, double time) const final;

    point3 Q;
    vec3 u, v;
};
