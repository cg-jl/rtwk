#pragma once
#include "hittable.h"

#include <span>

// IDEA: have hittable list contain all the juice?
// Maybe layer it up with accumulator as parameter?

// Abstraction over a collection. Will be used as I prototype different ways of
// fitting the final geometries into a separate structure, so that I con move
// material & texture information to other places.
struct collection {
    struct hit_status {
        bool hit_anything;
        interval ray_t;

        explicit hit_status(interval initial_intv)
            : hit_anything(false), ray_t(initial_intv) {}
    };

    // NOTE: Should I force having an object span or should I just let each
    // collection handle it how they want?
    virtual void propagate(ray const &r, hit_status &status,
                           hit_record &rec) const & = 0;
    // NOTE: I need a way to fiund the final box for layering.
    // Ideally this should go after I start specifying more on layers.
    [[nodiscard]] virtual aabb aggregate_box() const & = 0;
};

// Implementation of 'hittable' for any collection. Ideally this should be
// removed as I progress in dissecting the properties of hittables vs
// collections
struct hittable_collection final : public hittable {
    collection const *wrapped;

    explicit constexpr hittable_collection(collection const *wrapped)
        : wrapped(wrapped) {}

    [[nodiscard]] aabb bounding_box() const & override {
        return wrapped->aggregate_box();
    }
    bool hit(ray const &r, interval &ray_t, hit_record &rec) const override {
        collection::hit_status status{ray_t};
        wrapped->propagate(r, status, rec);
        return status.hit_anything;
    }
};