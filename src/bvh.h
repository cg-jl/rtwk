#ifndef BVH_H
#define BVH_H
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

#include <algorithm>
#include <cassert>
#include <span>
#include <tracy/Tracy.hpp>

#include "aabb.h"
#include "geometry.h"
#include "hittable.h"
#include "rtweekend.h"

struct bvh_node {
    aabb bbox;

    // TODO: unify these four fields into two
    int objectsStart;
    int objectsEnd;

    bvh_node *left;
    bvh_node *right;
};

namespace bvh {
static hittable const *hitNode(ray const &r, interval ray_t,
                               geometry_record &rec, bvh_node const *n,
                               hittable **objects) {
    if (n->left == nullptr) {
        // TODO: With something like SAH (Surface Area Heuristic), we should see
        // improving times by hitting multiple in one go. Since I'm tracing each
        // kind of intersection, it will be interesting to bake statistics of
        // each object and use that as timing reference.
        assert(objects.size() == 1);
        return hitSpan(std::span{objects + n->objectsStart,
                                 size_t(n->objectsEnd - n->objectsStart)},
                       r, ray_t, rec);
    }
    if (!n->bbox.hit(r, ray_t)) return nullptr;

    auto hit_left = hitNode(r, ray_t, rec, n->left, objects);
    auto hit_right =
        hitNode(r, interval(ray_t.min, hit_left ? rec.t : ray_t.max), rec,
                n->right, objects);
    return hit_left ?: hit_right;
}
}  // namespace bvh

struct bvh_tree {
    bvh_node root;
    hittable **objects;

    bvh_tree(std::span<hittable *> objects);

    aabb bounding_box() const { return root.bbox; }
    hittable const *hitSelect(ray const &r, interval ray_t,
                              geometry_record &rec) const {
        ZoneScopedN("bvh_tree hit");
        return bvh::hitNode(r, ray_t, rec, &root, objects);
    }
};

#endif
