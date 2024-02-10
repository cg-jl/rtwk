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

#include <utility>

#include "hittable.h"
#include "rtweekend.h"
#include "texture.h"

struct material {
   public:
    bool is_light_source = false;

    virtual ~material() = default;

    explicit material(bool is_light_source)
        : is_light_source(is_light_source) {}

    virtual void scatter(vec3 in_dir, hit_record::face rec,
                         vec3& scattered) const = 0;
};

struct lambertian final : public material {
   public:
    explicit lambertian() : material(false) {}

    void scatter(vec3 in_dir, hit_record::face rec,
                 vec3& scattered) const override {
        // NOTE: we shouldn't need any checks if we generate the random scatter
        // according to lambert's law:
        // https://en.wikipedia.org/wiki/Lambert%27s_cosine_law

        auto scatter_direction = rec.normal + random_unit_vector();

        // Catch degenerate scatter direction
        if (scatter_direction.near_zero())
            scatter_direction = rec.normal;
        else
            scatter_direction = unit_vector(scatter_direction);

        scattered = scatter_direction;
    }
};

struct metal final : public material {
   public:
    explicit metal(float fuzz) : material(false), fuzz(fuzz < 1 ? fuzz : 1) {}

    void scatter(vec3 in_dir, hit_record::face rec,
                 vec3& scattered) const override {
        vec3 reflected = reflect(in_dir, rec.normal);
        // We use fuzz to 'grow' the scatter angle.
        scattered = unit_vector(reflected + fuzz * random_in_unit_sphere());

        // invert if it's in the other face
        // NOTE: This could be done with XORs with the sign bit of dot product,
        // if we want our compiler to be fancy with it.
        scattered *= std::copysignf(1, dot(scattered, rec.normal));
    }

   private:
    float fuzz;
};

struct dielectric final : public material {
   public:
    explicit dielectric(float index_of_refraction)
        : material(false), ir(index_of_refraction) {}

    void scatter(vec3 in_dir, hit_record::face rec,
                 vec3& scattered) const override {
        float refraction_ratio = rec.is_front ? (1.0f / ir) : ir;

        float cos_theta = -dot(in_dir, rec.normal);
        assume(cos_theta * cos_theta <= 1.0);
        float sin_theta_squared = 1.0f - cos_theta * cos_theta;

        bool cannot_refract =
            refraction_ratio * refraction_ratio * sin_theta_squared > 1.0f;
        vec3 direction;

        if (cannot_refract ||
            reflectance(cos_theta, refraction_ratio) > random_float())
            direction = reflect(in_dir, rec.normal);
        else
            direction =
                refract(in_dir, rec.normal, cos_theta, refraction_ratio);

        scattered = direction;
    }

   private:
    float ir;  // Index of Refraction

    static float reflectance(float cosine, float ref_idx) {
        // Use Schlick's approximation for reflectance.
        auto r0 = (1 - ref_idx) / (1 + ref_idx);
        auto one_minus_cosine = 1 - cosine;
        one_minus_cosine *= one_minus_cosine * one_minus_cosine *
                            one_minus_cosine * one_minus_cosine;
        r0 = r0 * r0;
        return r0 + (1 - r0) * one_minus_cosine;
    }
};

struct diffuse_light final : public material {
   public:
    explicit diffuse_light() : material(true) {}

    void scatter(vec3 in_dir, hit_record::face rec,
                 vec3& scattered) const override {}
};

struct isotropic final : public material {
   public:
    explicit isotropic() : material(false) {}

    void scatter(vec3 in_dir, hit_record::face rec,
                 vec3& scattered) const override {
        scattered = random_unit_vector();
    }
};
