#pragma once

#include <span>

#include "hittable.h"
#include "rtweekend.h"
struct hittable_view final : public hittable {
    std::span<shared_ptr<hittable> const> objects;
    aabb bbox;

    hittable_view(std::span<shared_ptr<hittable> const> objects, aabb bb)
        : bbox(bb), objects(objects) {}

    aabb bounding_box() const& override { return bbox; }

    bool hit(ray const& r, interval& ray_t, hit_record& rec) const override {
        hit_record temp_rec;
        auto hit_anything = false;

        for (auto const& object : objects) {
            if (object->hit(r, ray_t, temp_rec)) {
                hit_anything = true;
                rec = temp_rec;
            }
        }

        return hit_anything;
    }
};
