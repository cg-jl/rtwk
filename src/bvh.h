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
    int objectIndex;  // if == -1, then the node is a parent and we should not
                      // use this struct. otherwise, this is the range for the
                      // objects that the leaf node represents.
    int objectCount;
};
struct tree_builder {
    std::vector<int> node_ends;
    std::vector<aabb> boxes;
    std::vector<bvh_node> nodes;
    std::vector<geometry> geoms;

    // @perf I could remove the overhead from copying the objects if I just add
    // into the geoms vec directly, with start() and end() or something where a
    // call to end() executes the register
    void registerBVH(std::span<geometry> objects);
};
struct tree {
    std::span<aabb const> boxes;
    bvh_node const *nodes;
    int const *node_ends;
    geometry const *geoms;

    constexpr tree(tree_builder const &bld)
        : boxes(bld.boxes),
          nodes(bld.nodes.data()),
          node_ends(bld.node_ends.data()),
          geoms(bld.geoms.data()) {}

    // @perf Using __attribute__((const)) here makes the image black,
    // which means that the arguments here are taken into consideration as only
    // pointers instead of requiring the data behind them.
    geometry const *hitBVH(ray const &, double &) const noexcept
        __attribute__((pure));
};
};  // namespace bvh
