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

struct hittable_list : public hittable {
   public:
    std::vector<shared_ptr<hittable>> objects;

    hittable_list() {}
    hittable_list(shared_ptr<hittable> object) { add(object); }

    void clear() { objects.clear(); }

    void add(shared_ptr<hittable> object) {
        objects.push_back(object);
        bbox = aabb(bbox, object->bbox);
    }

    shared_ptr<hittable> split() {
        return make_shared<bvh_node>(objects, 0, objects.size());
    }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        std::span obs = objects;
        return hittable_view(obs, bbox).hit(r, ray_t, rec);
    }
};

#endif
