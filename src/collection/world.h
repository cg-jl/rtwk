#ifndef HITTABLE_LIST_H
#define HITTABLE_LIST_H
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

#include <memory>
#include <span>
#include <vector>

#include "aabb.h"
#include "bvh.h"
#include "hittable.h"
#include "rtweekend.h"
#include "view.h"

// NOTE: could unify all pointers in just one storage.
// Each 'list' would have temporary ownership over the storage, so multiple
// lists still use the same local space.
// Then we could modify how things are allocated much easier.

// NOTE: Maybe hittable list should be just a builder and not a final view.
// This way we may cache the AABB in it as a way to get the layering part for
// BVH easily, but get just the span for the final thing.
struct hittable_list final {
   public:
    std::vector<hittable const*> objects;

    hittable_list() = default;
    explicit hittable_list(hittable const* obj) { add(obj); }

    void clear() { objects.clear(); }

    void add(hittable const* ob) { objects.push_back(ob); }
    collection const* finish(typed_storage<collection>& storage) {
        return storage.make<hittable_view>(objects);
    }

    collection const* split(typed_storage<collection>& coll_storage) {
        return bvh::split_random(objects, coll_storage);
    }
};

#endif
