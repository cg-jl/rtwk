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
#include "interval.h"
#include "list.h"
#include "rtweekend.h"
#include "view.h"

// NOTE: For layers, check if sorting the nodes via hit distance is better.
// The theory is that if I sort the nodes by hit distance, then I can exit the
// loop on the first hit, since I know it's not going to get better.
// To sort by highest, maybe it's time to move to an octotree?

// NOTE: what if I use the BVH to make object selection on a bit array? This way
// I may be able to process the hits as a masked line, and select the best one
// that way.

namespace bvh {

static bool box_compare(hittable const* a, hittable const* b, int axis_index) {
    return a->bounding_box().axis(axis_index).min <
           b->bounding_box().axis(axis_index).min;
}

// NOTE: what about using mean squares algorithm to fit a plane between the
// boxes?
// We could fit one plane for minimums and one plane for maximums. Check the
// distance of the plane to the closest box

template <is_hittable T>
static size_t partition(std::span<T> obs, int axis) {
    std::sort(obs.begin(), obs.end(), [axis](T const& a, T const& b) {
        return box_compare(&a, &b, axis);
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

template <is_hittable T>
static float eval_partition_cost(size_t split_index, std::span<T const> obs,
                                 int axis, int depth) {
    interval whole = interval::empty;
    interval left = interval::empty;
    interval right = interval::empty;

    for (auto const& ob : obs.subspan(0, split_index)) {
        left = interval(left, ob.bounding_box().axis(axis));
    }

    for (auto const& ob : obs.subspan(split_index)) {
        right = interval(right, ob.bounding_box().axis(axis));
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

template <is_hittable T>
static float eval_linear_cost(std::span<T const> obs) {
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

// NOTE: Could split nodes into the info needed before the check and info
// needed for recursive traversal.
struct node {
    aabb box;
    int left{};
    int right{};
    int axis{};
    float split_point{};
};
// NOTE: currently assuming that partitions are always symmetric, meaning
// that we can always compute the child spans from a parent span.
template <is_hittable T>
struct tree final : public collection {
    tree() = default;

    int root_node{};
    std::vector<node> inorder_nodes;
    view<T> objects;

    tree(int root_node, std::vector<node> inorder_nodes, view<T> objects)
        : root_node(root_node),
          inorder_nodes(std::move(inorder_nodes)),
          objects(objects) {}

    [[nodiscard]] aabb aggregate_box() const& override {
        return inorder_nodes[root_node].box;
    }

    void propagate(ray const& r, hit_status& status,
                   hit_record& rec) const& override {
        hit_tree(r, status, rec, inorder_nodes.data(), root_node, objects);
    }

    static void hit_tree(ray const& r, hit_status& status, hit_record& rec,
                         node const* nodes, int root, view<T> const& objects) {
        if (root == -1) {
            return objects.propagate(r, status, rec);
        } else {
            auto const& node = nodes[root];

            if (!node.box.hit(r, status.ray_t)) return ;

            // NOTE: space is partitioned along an axis, so we can know where
            // the ray may hit first. We also may know if the ray hits along
            // only one place or both.
            //
            // Remember that left means towards negative and right means towards
            // positive.

            // NOTE: With this, now we don't always visit left first, but the
            // best memory order is still in-order sorting.

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

            bool had_anything = status.hit_anything;
            status.hit_anything = false;
            hit_tree(r, status, rec, nodes, first_child, first_span);
            auto hit_first = status.hit_anything;
            status.hit_anything |= had_anything;

            if (hit_first) return;

            return hit_tree(r, status, rec, nodes, second_child, second_span);
        }
    }
};

// Returns the best axis to split, or none if no split can outperform the cost
// of going one by one.
template <is_hittable T>
[[nodiscard]] static std::optional<int> find_best_split(std::span<T> objects,
                                                        int depth) {
    if (objects.size() == 1) return {};
    std::optional<int> best_axis{};
    auto linear_cost = eval_linear_cost(std::span<T const>{objects});
    float best_cost = linear_cost;

    // NOTE: this relies on bvh::partition being idempotent!
    for (int axis = 0; axis < 3; ++axis) {
        auto mid = bvh::partition(objects, axis);
        auto partition_cost =
            eval_partition_cost(mid, std::span<T const>{objects}, axis, depth);
        // We only care about removing >70% of the cost
        if (best_cost / partition_cost > 1.5) {
            best_cost = partition_cost;
            best_axis = axis;
        }
    }

    return best_axis;
}

// Returns the index for the parent
template <is_hittable T>
[[nodiscard]] static int split_tree(std::span<T> objects,
                                    std::vector<node>& inorder_nodes,
                                    int best_axis, int depth = 0) {
    auto mid = bvh::partition(objects, best_axis);

    assert(mid == objects.size() / 2 &&
           "friendly reminder to add partition ranges to BVH nodes");

    interval left_part = interval::empty;
    aabb whole_bb = empty_bb;

    auto left_obs = objects.subspan(0, mid);
    auto right_obs = objects.subspan(mid);
    // NOTE: duplicate work from eval_partition_cost
    for (auto const& ob : left_obs) {
        left_part = interval(left_part, ob.bounding_box().axis(best_axis));
    }

    for (auto const& ob : objects) {
        whole_bb = aabb(whole_bb, ob.bounding_box());
    }

    auto left_best = find_best_split(left_obs, depth + 1);
    auto right_best = find_best_split(right_obs, depth + 1);

    int left = -1;
    if (left_best) {
        left = split_tree(left_obs, inorder_nodes, *left_best, depth + 1);
    }

    auto parent = inorder_nodes.size();
    inorder_nodes.emplace_back();

    int right = -1;
    if (right_best) {
        right = split_tree(right_obs, inorder_nodes, *right_best, depth + 1);
    }

    new (&inorder_nodes[parent])
        node{whole_bb, left, right, best_axis, left_part.max};

    return int(parent);
}

template <is_hittable T>
[[nodiscard]] static bvh::tree<T> split(list<T>& objects, int initial_split) {
    std::vector<node> inorder_nodes;
    auto root = split_tree(objects.span(), inorder_nodes, initial_split);

    return tree<T>(root, std::move(inorder_nodes), objects.finish());
}

template <is_hittable T>
[[nodiscard]] static bvh::tree<T> must_split(list<T>& objects) {
    auto initial_split = find_best_split(objects.span(), 0);
    assert(bool(initial_split) && "Should be able to split this");
    return split(objects, *initial_split);
}

template <is_hittable T>
[[nodiscard]] static collection const* split_or_view(
    list<T>& objects, poly_storage<collection>& coll_storage) {
    assert(objects.size() >= 2 && "There's no need to split this!");

    std::vector<node> inorder_nodes;

    auto initial_split = find_best_split(objects.span(), 0);

    if (!initial_split) return coll_storage.move<view<T>>(objects.finish());

    return coll_storage.move(split(objects, *initial_split));
}
}  // namespace bvh
