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

#include "hittable.h"
#include "rtweekend.h"
#include "texture.h"

struct material {
   public:
    bool is_light_source = false;
    shared_ptr<texture> tex;
    virtual ~material() = default;

    material(bool is_light_source, shared_ptr<texture> tex)
        : is_light_source(is_light_source), tex(std::move(tex)) {}

    virtual void scatter(vec3 in_dir, hit_record::face rec,
                         vec3& scattered) const = 0;
};

struct lambertian : public material {
   public:
    lambertian(color const& a) : lambertian(make_shared<solid_color>(a)) {}
    lambertian(shared_ptr<texture> a) : material(false, a) {}

    void scatter(vec3 in_dir, hit_record::face rec,
                 vec3& scattered) const override {
        auto scatter_direction = rec.normal + random_unit_vector();

        // Catch degenerate scatter direction
        if (scatter_direction.near_zero()) scatter_direction = rec.normal;

        scattered = scatter_direction;
    }
};

struct metal : public material {
   public:
    metal(color const& a, double f)
        : material(false, make_shared<solid_color>(a)), fuzz(f < 1 ? f : 1) {}

    void scatter(vec3 in_dir, hit_record::face rec,
                 vec3& scattered) const override {
        vec3 reflected = reflect(unit_vector(in_dir), rec.normal);
        scattered = reflected + fuzz * random_in_unit_sphere();

        // invert if it's in the other face
        scattered *= std::copysign(1, dot(scattered, rec.normal));
    }

   private:
    double fuzz;
};

struct dielectric : public material {
   public:
    dielectric(double index_of_refraction)
        : material(false, make_shared<solid_color>(color(1, 1, 1))),
          ir(index_of_refraction) {}

    void scatter(vec3 in_dir, hit_record::face rec,
                 vec3& scattered) const override {
        double refraction_ratio = rec.is_front ? (1.0 / ir) : ir;

        vec3 unit_direction = unit_vector(in_dir);
        double cos_theta = dot(-unit_direction, rec.normal);
        assume(cos_theta * cos_theta <= 1.0);
        double sin_theta = sqrt(1.0 - cos_theta * cos_theta);

        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        vec3 direction;

        if (cannot_refract ||
            reflectance(cos_theta, refraction_ratio) > random_double())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, cos_theta,
                                refraction_ratio);

        scattered = direction;
    }

   private:
    double ir;  // Index of Refraction

    static double reflectance(double cosine, double ref_idx) {
        // Use Schlick's approximation for reflectance.
        auto r0 = (1 - ref_idx) / (1 + ref_idx);
        auto one_minus_cosine = 1 - cosine;
        one_minus_cosine *= one_minus_cosine * one_minus_cosine *
                            one_minus_cosine * one_minus_cosine;
        r0 = r0 * r0;
        return r0 + (1 - r0) * one_minus_cosine;
    }
};

struct diffuse_light : public material {
   public:
    diffuse_light(shared_ptr<texture> a) : material(true, a) {}
    diffuse_light(color c) : diffuse_light(make_shared<solid_color>(c)) {}

    void scatter(vec3 in_dir, hit_record::face rec,
                 vec3& scattered) const override {}
};

struct isotropic : public material {
   public:
    isotropic(color c) : isotropic(make_shared<solid_color>(c)) {}
    isotropic(shared_ptr<texture> a) : material(false, std::move(a)) {}

    void scatter(vec3 in_dir, hit_record::face rec,
                 vec3& scattered) const override {
        scattered = random_unit_vector();
    }
};
