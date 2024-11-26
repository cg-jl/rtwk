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

#include <aabb.h>
#include <functional>
#include <geometry.h>

#include <span>
#include <vector>

namespace bvh {
// the node layout is <parent> <left> ... <right> ...
// (pre-order). This allows us to traverse the tree as if it were an array,
// skipping forwards to the end of a subtree when required.

// @perf the data structuers here need a bit of checking latencies.
// separating AABBs is definitely the right move, but separating bvh_node to be
// just a range and storing the ends elsewhere, when we know the node end index
// for a leaf node, is kind of a @waste. We still have to make the memory hit in
// any of the branches, so we may as well fetch the 8 bytes early.

struct bvh_node {
    int objectIndex; // if == -1, then the node is a parent and we should not
                     // use this struct. otherwise, this is the range for the
                     // objects that the leaf node represents.
    int objectCount;
};

// NOTE: @invariant node_ends[leaf node index] points to the next tree's root.
struct tree_builder {
    std::vector<int> node_ends;
    std::vector<aabb> boxes;
    std::vector<bvh_node> nodes;
    std::vector<geometry> geoms;

    constexpr size_t start() const { return geoms.size(); }
    void finish(size_t start) noexcept;
};

struct Hit_Buffer {
    uint32 *node_indices;
    interval *t;

    static Hit_Buffer request(uint32 const spp)
    {
        return {
            .node_indices = new uint32[spp],
            .t = new interval[spp],
        };
    }
};

// NOTE: @invariant node_ends[leaf node index] points to the next tree's root.
struct tree {
    std::span<aabb const> boxes;
    bvh_node const *nodes;
    int const *node_ends;
    geometry const *geoms;

    constexpr tree(tree_builder const &bld)
        : boxes(bld.boxes)
        , nodes(bld.nodes.data())
        , node_ends(bld.node_ends.data())
        , geoms(bld.geoms.data())
    {
    }

    void hit(timed_ray *rays, uint32 const len, std::pair<geometry_ptr, double> *results, bvh::Hit_Buffer buffer, std::function<void(uint32, uint32)> swap_rays) const noexcept;
};
}; // namespace bvh
