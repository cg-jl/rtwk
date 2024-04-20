#ifndef MATERIAL_H
#define MATERIAL_H
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

#include <tracy/Tracy.hpp>

#include "hittable.h"
#include "rtweekend.h"
#include "texture.h"

namespace detail {
static solid_color black(color(0, 0, 0));
static solid_color white(color(1, 1, 1));
}  // namespace detail

struct hit_record;

struct material {
    texture *tex;
    bool emits;

    constexpr material(texture *tex, bool emits) : tex(tex), emits(emits) {}

    virtual ~material() = default;

    virtual bool scatter(vec3 in_dir, hit_record const &rec,
                         vec3 &scattered) const {
        return false;
    }
};

struct lambertian : public material {
    lambertian(color const &albedo) : lambertian(new solid_color(albedo)) {}
    constexpr lambertian(texture *tex) : material(tex, false) {}

    bool scatter(vec3 in_dir, hit_record const &rec,
                 vec3 &scattered) const final {
        ZoneScopedN("lambertian scatter");
        auto scatter_direction = rec.geom.normal + random_unit_vector();

        // Catch degenerate scatter direction
        if (scatter_direction.near_zero()) scatter_direction = rec.geom.normal;

        scattered = scatter_direction;
        return true;
    }
};

struct metal : public material {
    metal(color const &albedo, double fuzz)
        : material(new solid_color(albedo), false), fuzz(fuzz < 1 ? fuzz : 1) {}

    bool scatter(vec3 in_dir, hit_record const &rec,
                 vec3 &scattered) const final {
        ZoneScopedN("metal scatter");
        vec3 reflected = reflect(in_dir, rec.geom.normal);
        reflected = unit_vector(reflected) + (fuzz * random_unit_vector());
        scattered = reflected;
        return (dot(scattered, rec.geom.normal) > 0);
    }

    double fuzz;
};

struct dielectric : public material {
    dielectric(double refraction_index)
        : material(&detail::white, false), refraction_index(refraction_index) {}

    bool scatter(vec3 in_dir, hit_record const &rec,
                 vec3 &scattered) const final {
        ZoneScopedN("metal scatter");
        double ri =
            rec.front_face ? (1.0 / refraction_index) : refraction_index;

        vec3 unit_direction = unit_vector(in_dir);
        double cos_theta = fmin(dot(-unit_direction, rec.geom.normal), 1.0);
        double sin_theta = sqrt(1.0 - cos_theta * cos_theta);

        bool cannot_refract = ri * sin_theta > 1.0;
        vec3 direction;

        if (cannot_refract || reflectance(cos_theta, ri) > random_double())
            direction = reflect(unit_direction, rec.geom.normal);
        else
            direction = refract(unit_direction, rec.geom.normal, ri);

        scattered = direction;
        return true;
    }

    // Refractive index in vacuum or air, or the ratio of the material's
    // refractive index over the refractive index of the enclosing media
    double refraction_index;

    static double reflectance(double cosine, double refraction_index) {
        // Use Schlick's approximation for reflectance.
        auto r0 = (1 - refraction_index) / (1 + refraction_index);
        r0 = r0 * r0;
        return r0 + (1 - r0) * pow((1 - cosine), 5);
    }
};

struct diffuse_light : public material {
    constexpr diffuse_light(texture *tex) : material(tex, true) {}
    constexpr diffuse_light(color const &emit)
        : diffuse_light(new solid_color(emit)) {}
};

struct isotropic : public material {
    isotropic(color const &albedo) : isotropic(new solid_color(albedo)) {}
    constexpr isotropic(texture *tex) : material(tex, false) {}

    bool scatter(vec3 in_dir, hit_record const &rec,
                 vec3 &scattered) const final {
        ZoneScopedN("metal scatter");
        scattered = random_unit_vector();
        return true;
    }
};

#endif
