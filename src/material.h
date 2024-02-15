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

#include "hit_face.h"
#include "rtweekend.h"

struct material {
   public:
    enum class kind {
        metal,
        dielectric,
        // NOTE: these three don't need more data than their tag.
        // NOTE: diffuse light won't scatter, but it can signal that it's a
        // light source.
        diffuse_light,
        isotropic,
        lambertian,
    };

    kind tag{};

    union data {
        struct metal_mat {
            float fuzz;
        } metal{};
        struct dielectric_mat {
            float ir;
        } dielectric;

        // NOTE: It's ok to do this since all have the same layout.
        constexpr data(data const& other) noexcept : metal(other.metal) {}
        constexpr data(data&& other) noexcept : metal(other.metal) {}

        constexpr data& operator=(data const& other) noexcept {
            metal = other.metal;
            return *this;
        }

        constexpr data& operator=(data&& other) noexcept {
            metal = other.metal;
            return *this;
        }

        constexpr data() {}
        ~data() {}
    } data{};

    // NOTE: does *not* initialize things
    constexpr material() = default;

    static material lambertian() {
        material mat;
        mat.tag = kind::lambertian;
        return mat;
    }

    static material metal(float fuzz) {
        material mat;
        mat.tag = kind::metal;
        mat.data.metal.fuzz = fminf(fuzz, 1.0);
        return mat;
    }

    static material dielectric(float index_of_refraction) {
        material mat;
        mat.tag = kind::dielectric;
        mat.data.dielectric.ir = index_of_refraction;
        return mat;
    }

    static material diffuse_light() {
        material mat;
        mat.tag = kind::diffuse_light;
        return mat;
    }
    static material isotropic() {
        material mat;
        mat.tag = kind::isotropic;
        return mat;
    }

    void scatter(vec3 in_dir, face rec, vec3& scattered) const&;
};

static inline float reflectance(float cosine, float ref_idx) {
    // Use Schlick's approximation for reflectance.
    auto r0 = (1 - ref_idx) / (1 + ref_idx);
    auto one_minus_cosine = 1 - cosine;
    one_minus_cosine *= one_minus_cosine * one_minus_cosine * one_minus_cosine *
                        one_minus_cosine;
    r0 = r0 * r0;
    return r0 + (1 - r0) * one_minus_cosine;
}
inline void material::scatter(vec3 in_dir, face rec, vec3& scattered) const& {
    switch (tag) {
        case kind::lambertian: {
            // NOTE: we shouldn't need any checks if we generate the random
            // scatter according to lambert's law:
            // https://en.wikipedia.org/wiki/Lambert%27s_cosine_law

            auto scatter_direction = rec.normal + random_unit_vector();

            // Catch degenerate scatter direction
            if (scatter_direction.near_zero())
                scatter_direction = rec.normal;
            else
                scatter_direction = unit_vector(scatter_direction);

            scattered = scatter_direction;
        } break;
        case kind::metal: {
            vec3 reflected = reflect(in_dir, rec.normal);
            // We use fuzz to 'grow' the scatter angle.
            scattered = unit_vector(reflected +
                                    data.metal.fuzz * random_in_unit_sphere());

            // invert if it's in the other face
            // NOTE: This could be done with XORs with the sign bit of dot
            // product, if we want our compiler to be fancy with it.
            scattered *= std::copysignf(1, dot(scattered, rec.normal));
        } break;
        case kind::diffuse_light:
            // Do nothing
            break;
        case kind::isotropic: {
            scattered = random_unit_vector();
        } break;
        case kind::dielectric: {
            {
                auto ir = data.dielectric.ir;
                float refraction_ratio = rec.is_front ? (1.0f / ir) : ir;

                float cos_theta = -dot(in_dir, rec.normal);
                assume(cos_theta * cos_theta <= 1.0);
                float sin_theta_squared = 1.0f - cos_theta * cos_theta;

                bool cannot_refract =
                    refraction_ratio * refraction_ratio * sin_theta_squared >
                    1.0f;
                vec3 direction;

                if (cannot_refract ||
                    reflectance(cos_theta, refraction_ratio) > random_float())
                    direction = reflect(in_dir, rec.normal);
                else
                    direction = refract(in_dir, rec.normal, cos_theta,
                                        refraction_ratio);

                scattered = direction;
            }
            break;
        }
    }
}
