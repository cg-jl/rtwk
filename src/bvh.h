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

// NOTE: For layers, check if sorting the nodes via hit distance is better.
// The theory is that if I sort the nodes by hit distance, then I can exit the
// loop on the first hit, since I know it's not going to get better.
// To sort by highest, maybe it's time to move to an octotree?

// NOTE: what if I use the BVH to make object selection on a bit array? This way
// I may be able to process the hits as a masked line, and select the best one
// that way.

namespace bvh {

static bool box_compare(hittable const * const& a,
                        hittable const * const& b, int axis_index) {
    return a->bounding_box().axis(axis_index).min <
           b->bounding_box().axis(axis_index).min;
}

// NOTE: what about using mean squares algorithm to fit a plane between the
// boxes?
// We could fit one plane for minimums and one plane for maximums. Check the
// distance of the plane to the closest box

static size_t partition(std::span<hittable const *> obs, int axis) {
    std::sort(obs.begin(), obs.end(),
              [axis](hittable const * const& a,
                     hittable const * const& b) {
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

static float eval_partition_cost(
    size_t split_index, std::span<hittable const * const> obs,
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

static float eval_linear_cost(std::span<hittable const * const> obs) {
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
    // NOTE: Could split nodes into the info needed before the check and info
    // needed for recursive traversal.
    struct node {
        aabb box;
        int left;
        int right;
        int axis;
        float split_point;
    };

    tree() = default;

    int root_node;
    std::vector<node> inorder_nodes;
    std::span<hittable const * const> objects;

    tree(int root_node, std::vector<node> inorder_nodes,
         std::span<hittable const * const> objects)
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
                         std::span<hittable const * const> objects) {
        if (root == -1) {
            return hittable_view::hit(r, ray_t, rec, objects);
        } else {
            auto const& node = nodes[root];

            if (!node.box.hit(r, ray_t)) return false;

            // NOTE: space is partitioned along an axis, so we can know where
            // the ray may hit first. We also may know if the ray hits along
            // only one place or both.
            //
            // Remember that left means towards negative and right means towards
            // positive.

            // NOTE: With this, now we don't always visit left first, but the
            // best memory order is still in-order sorting.

            auto const& ax = node.box.axis(node.axis);
            auto space_mid = node.split_point;

            int first_child = node.left;
            int second_child = node.right;

            auto first_span = objects.subspan(0, objects.size() / 2);
            auto second_span = objects.subspan(objects.size() / 2);

            // If the origin is on the right part and it goes into the left
            // side, then we want to check right first

            if (r.origin[node.axis] > space_mid && r.direction[node.axis] < 0) {
                std::swap(first_child, second_child);
                std::swap(first_span, second_span);
            }

            auto hits_first =
                hit_tree(r, ray_t, rec, nodes, first_child, first_span);
            if (hits_first) return true;

            return hit_tree(r, ray_t, rec, nodes, second_child, second_span);
        }
    }
};

// Returns the index for the parent, or -1 if not splitting
[[nodiscard]] static int split_tree(
    std::span<hittable const *> objects,
    std::vector<tree::node>& inorder_nodes, int depth = 0) {
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

    interval left_part = interval::empty;
    aabb whole_bb = empty_bb;

    // NOTE: duplicate work from eval_partition_cost
    for (auto const& ob : objects.subspan(0, mid)) {
        left_part = interval(left_part, ob->bounding_box().axis(best_axis));
    }

    for (auto const& ob : objects) {
        whole_bb = aabb(whole_bb, ob->bounding_box());
    }

    auto left = split_tree(objects.subspan(0, mid), inorder_nodes, depth + 1);

    auto parent = inorder_nodes.size();
    inorder_nodes.emplace_back();

    auto right = split_tree(objects.subspan(mid), inorder_nodes, depth + 1);

    new (&inorder_nodes[parent])
        tree::node{whole_bb, left, right, best_axis, left_part.max};

    return int(parent);
}

[[nodiscard]] static hittable const * split_random(
    std::span<hittable const *> objects, shared_ptr_storage<hittable> &storage) {
    if (objects.size() == 1) return objects[0];

    std::vector<tree::node> inorder_nodes;

    auto root = split_tree(objects, inorder_nodes);

    if (root == -1) {
        aabb whole_bb = empty_bb;
        for (auto const& ob : objects) {
            whole_bb = aabb(whole_bb, ob->bounding_box());
        }
        return storage.make<hittable_view>(objects, whole_bb);
    } else {
        return storage.make<tree>(root, std::move(inorder_nodes), objects);
    }
}
}  // namespace bvh
