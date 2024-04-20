#ifndef CONSTANT_MEDIUM_H
#define CONSTANT_MEDIUM_H
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

#include "geometry.h"
#include "hittable.h"
#include "material.h"
#include "rtweekend.h"
#include "texture.h"

// TODO: This is another candidate to make a separate array of, since
// its behavior with materials is pretty specific.
class constant_medium : public hittable {
   public:
    constant_medium(hittable *boundary, double density, texture *tex)
        : hittable(new isotropic(tex)),
          boundary(boundary),
          neg_inv_density(-1 / density) {}

    constant_medium(hittable *boundary, double density, color const &albedo)
        : hittable(new isotropic(albedo)),
          boundary(boundary),
          neg_inv_density(-1 / density) {}

    bool hit(ray const &r, interval ray_t, geometry_record &rec) const final {
        ZoneScopedN("constant_medium hit");
        // Print occasional samples when debugging. To enable, set enableDebug
        // true.
        bool const enableDebug = false;
        bool const debugging = enableDebug && random_double() < 0.00001;

        geometry_record rec1, rec2;

        if (!boundary->hit(r, universe_interval, rec1)) return false;

        if (!boundary->hit(r, interval(rec1.t + 0.0001, infinity), rec2))
            return false;

        if (rec1.t < ray_t.min) rec1.t = ray_t.min;
        if (rec2.t > ray_t.max) rec2.t = ray_t.max;

        if (rec1.t >= rec2.t) return false;

        if (rec1.t < 0) rec1.t = 0;

        auto ray_length = r.direction().length();
        auto distance_inside_boundary = (rec2.t - rec1.t) * ray_length;
        auto hit_distance = neg_inv_density * log(random_double());

        if (hit_distance > distance_inside_boundary) return false;

        rec.t = rec1.t + hit_distance / ray_length;
        rec.p = r.at(rec.t);

        rec.normal = vec3(1, 0, 0);  // arbitrary

        return true;
    }

    // The normal is assigned arbitrarily, and the material is isotropic.
    // We don't care about its value.
    void getUVs(uvs &_uv, point3 _p, vec3 _normal) const final {}

    aabb bounding_box() const final { return boundary->bounding_box(); }

   private:
    hittable *boundary;
    double neg_inv_density;
};

#endif
