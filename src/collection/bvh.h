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
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "aabb.h"
#include "fixedvec.h"
#include "geometry.h"
#include "geometry/box.h"
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

// NOTE: what about using mean squares algorithm to fit a plane between the
// boxes?
// We could fit one plane for minimums and one plane for maximums. Check the
// distance of the plane to the closest box

template <has_bb T>
static size_t partition(std::span<T> obs, int axis) {
    std::sort(obs.begin(), obs.end(), [axis](T const& a, T const& b) {
        return a.boundingBox().axis(axis).min < b.boundingBox().axis(axis).min;
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

template <has_bb T>
static float eval_partition_cost(size_t split_index, std::span<T const> obs,
                                 int axis, int depth) {
    interval whole = interval::empty;
    interval left = interval::empty;
    interval right = interval::empty;

    for (auto const& ob : obs.subspan(0, split_index)) {
        left = interval(left, ob.boundingBox().axis(axis));
    }

    for (auto const& ob : obs.subspan(split_index)) {
        right = interval(right, ob.boundingBox().axis(axis));
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

template <typename T>
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
// needed for recursive traversal, but that showed no improvement.
struct node {
    aabb box;
    int left{};
    int right{};
    int axis{};
    float split_point{};
};

// TODO: can I figure out box index from layer?
struct state {
    range span;
    int box_index;
    float dist_to_split_plane;
};

namespace {
static thread_local std::span<node const> s_all_nodes;
static thread_local state* s_second_sides_ptr = nullptr;
}  // namespace

static void setThreadLocals(std::span<node const> all_nodes, int max_depth) {
    s_all_nodes = all_nodes;
    s_second_sides_ptr = new state[2ull * max_depth];
}

struct builder {
    std::vector<node> nodes;
    int max_depth = 0;

    struct result {
        std::span<node const> nodes{};
        int max_depth{};

        void initWorker() {
            if (max_depth > 0) {
                setThreadLocals(nodes, max_depth);
            }
        }
    };

    [[nodiscard]] result lock() const& { return result{nodes, max_depth}; }
};

// NOTE: currently assuming that partitions are always symmetric, meaning
// that we can always compute the child spans from a parent span.
struct tree final {
    constexpr tree() = default;

    int root_node;

    explicit constexpr tree(int root_node) : root_node(root_node) {}

    [[nodiscard]] aabb boundingBox() const& {
        return s_all_nodes[root_node].box;
    }

    std::optional<range> nextRange(
        ray const& r, interval ray_t,
        uncapped_vecview<state>& second_sides) const {
        while (!second_sides.empty()) {
            auto curr = second_sides.pop();

            // Go left while we can
            while (curr.box_index != -1) {
                auto const& node = s_all_nodes[curr.box_index];

                // We don't need to process the hit if the recorded distance
                // can't be made better.
                if (curr.dist_to_split_plane >= ray_t.max ||
                    !node.box.hit(r, ray_t)) {
                    goto pop_next;
                }

                auto space_mid = node.split_point;
                auto axis = node.axis;

                auto first_child = node.left;
                auto second_child = node.right;

                auto [first_span, second_span] =
                    curr.span.split(curr.span.size() / 2);

                auto ray_origins_in_right = r.origin[axis] > space_mid;
                auto dist_to_split_plane =
                    std::fabs(r.origin[axis] - space_mid);

                // If the origin is on the right part then we want to check
                // right first
                if (ray_origins_in_right) {
                    std::swap(first_child, second_child);
                    std::swap(first_span, second_span);
                }

                second_sides.emplace_back(second_span, second_child,
                                          dist_to_split_plane);

                curr = {first_span, first_child, dist_to_split_plane};
            }

            return curr.span;

        pop_next:;
        }
        return {};
    }

    // NOTE: maybe it's better to change this to an iterator, so it doesn't
    // have to be re-instanced.
    void filter(range initial, ray const& r, interval& ray_t,
                auto func) const& {
        // TODO: Some passable structure like 'thread buffers' where I can tell
        // the threads how much to allocate and pass it to this structure could
        // be beneficial, and I can stick it to everyone and make it trivially
        // removable via templates.
        // TODO: some pmr resource usage?

        uncapped_vecview<state> second_sides(s_second_sides_ptr);

        second_sides.emplace_back(initial, root_node, ray_t.min);

        for (auto next = nextRange(r, ray_t, second_sides); next;
             next = nextRange(r, ray_t, second_sides)) {
            func(ray_t, *next);
        }
    }
};

template <typename T>
struct over final {
    constexpr over(tree bvh, view<T> view)
        : bvh(std::move(bvh)), objects(std::move(view)) {}
    constexpr over(tree bvh, std::span<T const> view)
        : bvh(std::move(bvh)), objects(view) {}
    tree bvh;
    view<T> objects;
    using Type = T;

    [[nodiscard]] aabb boundingBox() const& { return bvh.boundingBox(); }

    T const* hit(ray const& r, hit_record::geometry& res,
                 interval& ray_t) const&
        requires(is_geometry<T>)
    {
        T const* best = nullptr;
        bvh.filter(
            range{0, uint32_t(objects.size())}, r, ray_t,
            [&r, &res, &best, &v = objects](interval& ray_t, range span) {
                T const* next =
                    v.subspan(span.start, span.size()).hit(r, res, ray_t);
                if (next != nullptr) best = next;
            });
        return best;
    }

    void propagate(ray const& r, hit_status& status, hit_record& rec) const&
        requires(time_invariant_hittable<T>)
    {
        bvh.filter(
            range{0, uint32_t(objects.size())}, r, status.ray_t,
            [&r, &status, &rec, v = objects](interval& ray_t, range span) {
                v.subspan(span.start, span.size()).propagate(r, status, rec);
            });
    }

    void propagate(ray const& r, hit_status& status, hit_record& rec,
                   transform_set& xforms, float time) const&
        requires(is_hittable<T>)
    {
        bvh.filter(range{0, uint32_t(objects.size())}, r, status.ray_t,
                   [&r, &status, &rec, v = objects, time, &xforms](
                       interval& ray_t, range span) {
                       v.subspan(span.start, span.size())
                           .propagate(r, status, rec, xforms, time);
                   });
    }
};

// Returns the best axis to split, or none if no split can outperform the cost
// of going one by one.
template <has_bb T>
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

// TODO: make splitting independent of span type (random + multiple access
// iterator)

// Returns the index for the parent
template <has_bb T>
[[nodiscard]] static int split_tree(std::span<T> objects, int& max_depth,
                                    std::vector<node>& inorder_nodes,
                                    int best_axis, int depth = 0) {
    max_depth = std::max(max_depth, depth);
    auto mid = bvh::partition(objects, best_axis);

    assert(mid == objects.size() / 2 &&
           "friendly reminder to add partition ranges to BVH nodes");

    interval left_part = interval::empty;
    aabb whole_bb = empty_bb;

    auto left_obs = objects.subspan(0, mid);
    auto right_obs = objects.subspan(mid);
    // NOTE: duplicate work from eval_partition_cost
    for (auto const& ob : left_obs) {
        left_part = interval(left_part, ob.boundingBox().axis(best_axis));
    }

    for (auto const& ob : objects) {
        whole_bb = aabb(whole_bb, ob.boundingBox());
    }

    auto left_best = find_best_split(left_obs, depth + 1);
    auto right_best = find_best_split(right_obs, depth + 1);

    int left = -1;
    if (left_best) {
        left = split_tree(left_obs, max_depth, inorder_nodes, *left_best,
                          depth + 1);
    }

    auto parent = inorder_nodes.size();
    inorder_nodes.emplace_back();

    int right = -1;
    if (right_best) {
        right = split_tree(right_obs, max_depth, inorder_nodes, *right_best,
                           depth + 1);
    }

    new (&inorder_nodes[parent])
        node{whole_bb, left, right, best_axis, left_part.max};

    return int(parent);
}

template <has_bb T>
[[nodiscard]] static bvh::tree split(std::span<T> objects, int initial_split,
                                     bvh::builder& builder) {
    auto root =
        split_tree(objects, builder.max_depth, builder.nodes, initial_split);

    return tree(root);
}

template <has_bb T>
[[nodiscard]] static bvh::tree must_split(std::span<T> objects,
                                          builder& builder) {
    auto initial_split = find_best_split(objects, 0);
    assert(bool(initial_split) && "Should be able to split this");
    return split(objects, *initial_split, builder);
}

}  // namespace bvh
