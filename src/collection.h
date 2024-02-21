#pragma once
#include <concepts>
#include <span>
#include <type_traits>

#include "hittable.h"

// NOTE: On making hittable_collection explicit on collection:
// It may force the compiler to produce multiple instantiations for each of the unused functions.
// Compile times could skyrocket.

// NOTE: Maybe it's wise to select the best one in layers when I have a layering
// builder. Perhaps the important thing next is to 'refactor' poly_list into
// a builder, and redirect all main.cc operations through it. We can already
// swap the storage, and the next thing is to swap the building method (which
// for geometry will be the storage, too!).

// NOTE: Should I make it a tagged union?
// Given that there is only BVH and hittable view (and hittable list while it's
// not a builder), it would remove one layer of indirection.

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

// NOTE: Should I restrict sets of transforms to different collections?
// Maybe it's not a good idea if I want to separate differently rotated objects
// very near each other? Or is it actually better, since "editing" the ray
// doesn't take as much as the intersections?

// Implementation of 'hittable' for any collection. Ideally this should be
// removed as I progress in dissecting the properties of hittables vs
// collections.
// It serves as a way to layer collections
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
