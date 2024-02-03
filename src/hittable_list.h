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
#include "hittable_view.h"
#include "rtweekend.h"

// NOTE: could unify all pointers in just one storage.
// Each 'list' would have temporary ownership over the storage, so multiple
// lists still use the same local space.
// Then we could modify how things are allocated much easier.

struct hittable_list final : public hittable {
   public:
    aabb bbox;
    std::vector<shared_ptr<hittable>> objects;

    hittable_list() {}
    hittable_list(shared_ptr<hittable> object) { add(std::move(object)); }

    void clear() { objects.clear(); }

    void add(shared_ptr<hittable> object) {
        // NOTE: maybe we don't need incremental calculation and can have a
        // final pass for caching it, or another hittable thing that caches the
        // AABB and inlines your thing.
        bbox = aabb(bbox, object->bounding_box());
        objects.emplace_back(std::move(object));
    }

    shared_ptr<hittable> split() { return bvh::split_sah(objects, bbox); }

    aabb bounding_box() const& override { return bbox; }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        std::span obs = objects;
        return hittable_view(obs, bbox).hit(r, ray_t, rec);
    }
};

#endif
