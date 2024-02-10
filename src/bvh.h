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

// NOTE: what if I use the BVH to make object selection on a bit array? This way
// I may be able to process the hits as a masked line, and select the best one
// that way.

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

static constexpr float intersection_cost = 3;
static constexpr float visit_left_cost = 0.01;
// Visiting right means losing a cacheline potentially.
// NOTE: This is diminished each depth, so maybe depth has to be tracked?
static constexpr float visit_right_cost = 0.03;

static float cumulative_left_visit(int depth) {
    // visit_right_cost * sum(1, depth)
    return visit_left_cost * float(depth * (depth + 1) / 2);
}

static float cumulative_right_visit(int depth) {
    return visit_right_cost * float(depth * (depth + 1) / 2);
}

static float eval_partition_cost(size_t split_index,
                                 std::span<shared_ptr<hittable> const> obs,
                                 int axis, int depth) {
    interval whole = interval::empty;
    interval left = interval::empty;
    interval right = interval::empty;

    for (auto const& ob : obs.subspan(0, split_index)) {
        left = interval(left, ob->bounding_box().axis(axis));
    }

    for (auto const& ob : obs.subspan(split_index)) {
        right = interval(right, ob->bounding_box().axis(axis));
    }

    whole = interval(left, right);

    // Probability that ray hits in each subdivision given that it hit here.
    auto pa = left.size() / whole.size();
    auto pb = right.size() / whole.size();

    // Average number of hits to check on each partition, assuming it follows a
    // uniform distribution.
    auto hits_in_a = pa * float(split_index);
    auto hits_in_b = pb * float(obs.size() - split_index);

    auto a_cost = cumulative_left_visit(depth) + intersection_cost * hits_in_a;
    auto b_cost = cumulative_right_visit(depth) + intersection_cost * hits_in_b;

    return a_cost + b_cost;
}

static float eval_linear_cost(std::span<shared_ptr<hittable> const> obs) {
    // NOTE: Since everything is going through shared pointers right now, I'll
    // add another penalty to the linear one, since it isn't fully cached like
    // we're assuming here!
    static constexpr float traversal_penalty = 0.6;

    // NOTE: Could add more information like number of cache misses due to
    // visiting, but since it's linear that penalty might be very small
    return float(obs.size()) * (intersection_cost + traversal_penalty);
}

// NOTE: what if we start making an API for making multiple hits?
// Thinking about some way to do things layer by layer on the BVH, so that we
// do more work per cache miss.

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
                                    std::vector<tree::node>& inorder_nodes,
                                    int depth = 0) {
    // Not required
    if (objects.size() == 1) return -1;

    int best_axis = -1;
    auto linear_cost = eval_linear_cost(objects);
    float best_cost = linear_cost;

    // NOTE: this relies on bvh::partition being idempotent!
    for (int axis = 0; axis < 3; ++axis) {
        auto mid = bvh::partition(objects, axis);
        auto partition_cost = eval_partition_cost(mid, objects, axis, depth);
        // We only care about removing >70% of the cost
        if (best_cost / partition_cost > 1.5) {
            best_cost = partition_cost;
            best_axis = axis;
        }
    }

    if (best_axis == -1) return -1;

    auto mid = bvh::partition(objects, best_axis);

    assert(mid == objects.size() / 2 &&
           "friendly reminder to add partition ranges to BVH nodes");

    aabb whole_bb = empty_bb;
    for (auto const& ob : objects) {
        whole_bb = aabb(whole_bb, ob->bounding_box());
    }

    auto left = split_tree(objects.subspan(0, mid), inorder_nodes, depth + 1);

    auto parent = inorder_nodes.size();
    inorder_nodes.emplace_back();

    auto right = split_tree(objects.subspan(mid), inorder_nodes, depth + 1);

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
