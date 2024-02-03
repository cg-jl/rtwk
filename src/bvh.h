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

struct bvh_node final : public hittable {
    // NOTE: what if I make an array where we 'binary search' bounding boxes
    // (starting from the middle, then going left and/or right depending on
    // which is hit), and compute via the (2n + b) & (node count) sequence the
    // object to hit?
    //
    // The space is currently fully partitioned. If it ends up not fully
    // partitioned, then I would have to know the range for the partition....
    // Knowing where each partition is made (either half on a std::partition
    // result), we can always have a valid range to ask for per leaf node.
    //
    // What does this solve? Hitting nodes results in a lot of cache misses.
    // Since discarding a node means that we don't continue hitting, then it is
    // reasonable to have cache misses at the beginning, and the more depth we
    // pursue, the more local the objects are, since we have to go back to check
    // the right side anyway.

   public:
    aabb bbox;
    shared_ptr<hittable> left;
    shared_ptr<hittable> right;

    bvh_node(shared_ptr<hittable> left, shared_ptr<hittable> right)
        : bbox(aabb(left->bounding_box(), right->bounding_box())),
          left(std::move(left)),
          right(std::move(right)) {}

    [[nodiscard]] aabb bounding_box() const& override { return bbox; }

    // NOTE: currently assuming that partitions are always symmetric, meaning
    // that we can always compute the child spans from a parent span.

    // Returns the index for the parent

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        if (!bbox.hit(r, ray_t)) return false;

        bool hit_left = left->hit(r, ray_t, rec);
        bool hit_right = right->hit(r, ray_t, rec);

        return hit_left || hit_right;
    }
};

namespace bvh {

static bool box_compare(shared_ptr<hittable> const& a,
                        shared_ptr<hittable> const& b, int axis_index) {
    return a->bounding_box().axis(axis_index).min <
           b->bounding_box().axis(axis_index).min;
}
static size_t partition(std::span<shared_ptr<hittable>> obs, int axis,
                        interval& left, interval& right) {
    std::sort(
        obs.begin(), obs.end(),
        [axis](shared_ptr<hittable> const& a, shared_ptr<hittable> const& b) {
            return box_compare(a, b, axis);
        });


    auto mid = obs.size() / 2;

    for (auto const &ob : obs.subspan(0, mid)) {
        left = interval(left, ob->bounding_box().axis(axis));
    }

    for (auto const &ob : obs.subspan(mid))  {
        right = interval(right, ob->bounding_box().axis(axis));
    }

    return mid;
}

// Cost of hit computation
static constexpr float hit_cost = 3;
[[nodiscard]] static float estimate_split_cost(
    std::span<std::shared_ptr<hittable> const> obs, size_t mid, interval left,
    interval right, interval parent) {
    // Probability that a ray hitting the parent hits the partition
    auto pleft = left.size() / parent.size();
    auto pright = right.size() / parent.size();

    // Number of hits to calculate per partition
    auto nhits_left = pleft * float(mid);
    auto nhits_right = pright * float(obs.size() - mid);
    auto nhits = nhits_left + nhits_right;

    // Cost of visiting, which accounts for miss of locality
    // Currently the visit cost is the same, since it has to traverse a node
    // each time. NOTE: if I adopt the above model and change the way these
    // are stored, then I'll have to use a more dynamic visit cost that
    // decrements per depth.
    static constexpr float visit_cost = 1;

    return nhits * hit_cost + visit_cost;
}
[[nodiscard]] static float estimate_linear_cost(
    std::span<shared_ptr<hittable> const> obs) {
    // TODO: account for miss of locality here, like how many cachelines
    // this spans, or even how many page faults
    return hit_cost * float(obs.size());
}

struct tree final : public hittable {
    struct node {
        aabb box;
        // Distance to left/right nodes. -1 if it has no child there.
        int left, right;

        node(aabb box, int left, int right)
            : box(box), left(left), right(right) {}
    };

    size_t root_node;
    std::vector<node> inorder_nodes;
    std::span<shared_ptr<hittable> const> objects;

    tree(size_t root_node, std::vector<node> inorder_nodes,
         std::span<shared_ptr<hittable> const> objects)
        : root_node(root_node),
          inorder_nodes(std::move(inorder_nodes)),
          objects(objects) {}

    [[nodiscard]] aabb bounding_box() const& override { return inorder_nodes[root_node].box; }

    static bool hit_node(ray const& r, interval& ray_t, hit_record& rec,
                         std::span<node const> nodes, size_t root,
                         std::span<shared_ptr<hittable> const> objects_in_box) {
        if (nodes.empty()) {
            return hittable_view::hit(r, ray_t, rec, objects_in_box);
        } else {
            auto const& node = nodes[root];

            if (!node.box.hit(r, ray_t)) return false;

            auto mid = objects_in_box.size() / 2;
            auto hits_left =
                hit_node(r, ray_t, rec, nodes.subspan(0, root),
                         root - node.left, objects_in_box.subspan(0, mid));
            auto hits_right =
                hit_node(r, ray_t, rec, nodes.subspan(root), root + node.right,
                         objects_in_box.subspan(mid));

            return hits_left | hits_right;
        }
    }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        return hit_node(r, ray_t, rec, inorder_nodes, root_node, objects);
    }
};

// TODO: make this non-recursive?
// hint: go layer by layer.
static int split_sah_incremental(std::span<shared_ptr<hittable>> objects,
                                 int depth,
                                 std::vector<tree::node>& inorder_nodes) {
    assert(objects.size() != 0 && "not splitting empty objects!");

    // There's nothing to partition here.
    // We don't add anything because there's no check to do.
    if (objects.size() == 1) return -1;

    auto axis = random_int(0, 2);

    auto left_intv = interval::empty;
    auto right_intv = interval::empty;

    auto mid = partition(objects, axis, left_intv, right_intv);
    assert(mid == objects.size() &&
           "friendly reminder to add span information to hit tree");

    aabb whole_bb = empty_bb;
    for (auto const& ob : objects) {
        whole_bb = aabb(whole_bb, ob->bounding_box());
    }

    auto split_cost = estimate_split_cost(objects, mid, left_intv, right_intv,
                                          whole_bb.axis(axis));
    auto no_split_cost = estimate_linear_cost(objects);

    // We may as well just visit the array as-is.
    if (split_cost >= no_split_cost) return -1;

    auto left_index = split_sah_incremental(objects.subspan(0, mid), depth + 1,
                                            inorder_nodes);
    auto parent_index = inorder_nodes.size();
    inorder_nodes.emplace_back(whole_bb, -1, -1);

    auto right_index =
        split_sah_incremental(objects.subspan(mid), depth + 1, inorder_nodes);

    inorder_nodes[parent_index].left = int(size_t(parent_index) - left_index);
    inorder_nodes[parent_index].right = int(size_t(right_index) - parent_index);

    return int(parent_index);
}

[[nodiscard]] static shared_ptr<hittable> split_sah(
    std::span<shared_ptr<hittable>> objects, aabb full_box) {
    assert(objects.size() != 0 && "not splitting empty objects!");
    if (objects.size() == 1) return objects[0];

    std::vector<tree::node> inorder_nodes;

    auto root = split_sah_incremental(objects, 0, inorder_nodes);

    if (root == -1) {
        return make_shared<hittable_view>(objects, full_box);
    }

    return make_shared<tree>(root, std::move(inorder_nodes), objects);
}

[[nodiscard]] static shared_ptr<hittable> split_random(
    std::span<shared_ptr<hittable>> objects) {
    assert(objects.size() != 0 && "not splitting empty objects!");
    // No need to split
    if (objects.size() == 1) {
        return objects[0];
    }

    // TODO: estimate which axis is the more cost effective split.

    // TODO: avoid splitting if our final partition cost is more than the
    // linear cost.

    // NOTE: can we optimize intersection resources (i.e how much data
    // we need) by knowing what axis is each node split into? This would
    // mean a reduction in node size.
    int axis = random_int(0, 2);

    interval left_intv = interval::empty, right_intv = interval::empty;
    auto mid = bvh::partition(objects, axis, left_intv, right_intv);

    aabb whole_bb = empty_bb;
    for (auto const& ob : objects) {
        whole_bb = aabb(whole_bb, ob->bounding_box());
    }

    auto split_cost = estimate_split_cost(objects, mid, left_intv, right_intv,
                                          whole_bb.axis(axis));
    auto no_split_cost = estimate_linear_cost(objects);
    if (split_cost >= no_split_cost) {
        return make_shared<hittable_view>(objects, whole_bb);
    }

    auto left = split_random(objects.subspan(0, mid));
    auto right = split_random(objects.subspan(mid));

    return make_shared<bvh_node>(std::move(left), std::move(right));
}
}  // namespace bvh
