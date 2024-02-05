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

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include "aabb.h"
#include "hittable.h"
#include "hittable_view.h"
#include "interval.h"
#include "rtweekend.h"

namespace bvh {

static bool box_compare(shared_ptr<hittable> const& a,
                        shared_ptr<hittable> const& b, int axis_index) {
    return a->bounding_box().axis(axis_index).min <
           b->bounding_box().axis(axis_index).min;
}

static size_t partition(std::span<shared_ptr<hittable>> obs, int axis) {
    std::sort(
        obs.begin(), obs.end(),
        [axis](shared_ptr<hittable> const& a, shared_ptr<hittable> const& b) {
            return box_compare(a, b, axis);
        });

    auto mid = obs.size() / 2;

    return mid;
}

// NOTE: currently assuming that partitions are always symmetric, meaning
// that we can always compute the child spans from a parent span.
struct tree final : public hittable {
    struct node {
        aabb box;
        int left;
        int right;
    };

    tree() = default;

    int root_node;
    std::vector<node> inorder_nodes;
    std::span<shared_ptr<hittable> const> objects;

    tree(int root_node, std::vector<node> inorder_nodes,
         std::span<shared_ptr<hittable> const> objects)
        : root_node(root_node),
          inorder_nodes(std::move(inorder_nodes)),
          objects(objects) {}

    aabb bounding_box() const& override { return inorder_nodes[root_node].box; }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        return hit_tree(r, ray_t, rec, inorder_nodes.data(), root_node,
                        objects);
    }

    static bool hit_tree(ray const& r, interval& ray_t, hit_record& rec,
                         node const* nodes, int root,
                         std::span<shared_ptr<hittable> const> objects) {
        if (root == -1) {
            return hittable_view::hit(r, ray_t, rec, objects);
        } else {
            auto const& node = nodes[root];

            if (!node.box.hit(r, ray_t)) return false;

            auto hits_left = hit_tree(r, ray_t, rec, nodes, node.left,
                                      objects.subspan(0, objects.size() / 2));

            auto hits_right = hit_tree(r, ray_t, rec, nodes, node.right,
                                       objects.subspan(objects.size() / 2));

            return hits_left | hits_right;
        }
    }
};

// Returns the index for the parent, or -1 if not splitting
[[nodiscard]] static int split_tree(std::span<shared_ptr<hittable>> objects,
                                    std::vector<tree::node>& inorder_nodes) {
    // Not required
    if (objects.size() == 1) return -1;

    // TODO: estimate which axis is the more cost effective split.

    // TODO: avoid splitting if our final partition cost is more than the
    // linear cost.

    // NOTE: can we optimize intersection resources (i.e how much data
    // we need) by knowing what axis is each node split into? This would
    // mean a reduction in node size.
    int axis = random_int(0, 2);

    auto mid = bvh::partition(objects, axis);

    assert(mid == objects.size() / 2 &&
           "friendly reminder to add partition ranges to BVH nodes");

    aabb whole_bb = empty_bb;
    for (auto const& ob : objects) {
        whole_bb = aabb(whole_bb, ob->bounding_box());
    }

    auto left = split_tree(objects.subspan(0, mid), inorder_nodes);

    auto parent = inorder_nodes.size();
    inorder_nodes.emplace_back();

    auto right = split_tree(objects.subspan(mid), inorder_nodes);

    new (&inorder_nodes[parent]) tree::node{whole_bb, left, right};

    return int(parent);
}

[[nodiscard]] static shared_ptr<hittable> split_random(
    std::span<shared_ptr<hittable>> objects) {
    if (objects.size() == 1) return objects[0];

    std::vector<tree::node> inorder_nodes;

    auto root = split_tree(objects, inorder_nodes);

    if (root == -1) {
        aabb whole_bb = empty_bb;
        for (auto const& ob : objects) {
            whole_bb = aabb(whole_bb, ob->bounding_box());
        }
        return make_shared<hittable_view>(objects, whole_bb);
    } else {
        return make_shared<tree>(root, std::move(inorder_nodes), objects);
    }
}
}  // namespace bvh
