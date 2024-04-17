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

#include <vector>

#include "aabb.h"
#include "hittable.h"
#include "rtweekend.h"

class hittable_list : public hittable {
   public:
    std::vector<hittable *> objects;

    hittable_list() {}
    hittable_list(hittable *object) { add(object); }

    void clear() { objects.clear(); }

    void add(hittable *object) {
        objects.push_back(object);
        bbox = aabb(bbox, object->bounding_box());
    }

    bool hit(ray const &r, interval ray_t, hit_record &rec) const final {
        std::span sp(objects);
        return hitSpan(sp, r, ray_t, rec);
    }

    aabb bounding_box() const final { return bbox; }

   private:
    aabb bbox;
};

#endif
