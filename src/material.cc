#include "material.h"

#include "random.h"
#include "trace_colors.h"

#include <tracy/Tracy.hpp>

static double reflectance(double cosine, double refraction_index) {
    // Use Schlick's approximation for reflectance.
    auto r0 = (1 - refraction_index) / (1 + refraction_index);
    r0 = r0 * r0;
    return r0 + (1 - r0) * pow((1 - cosine), 5);
}
static vec3 reflect(vec3 v, vec3 n) { return v - 2 * dot(v, n) * n; }

// `uv`, `n` are assumed to be unit vectors.
static vec3 refract(vec3 uv, vec3 n, double etai_over_etat) {
    auto cos_theta = -dot(uv, n);
    vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    vec3 r_out_parallel =
        -std::sqrt(std::abs(1.0 - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_parallel;
}
static vec3 random_in_unit_sphere() {
    while (true) {
        auto p = random_vec(-1, 1);
        if (p.length_squared() < 1) return p;
    }
}

static vec3 random_unit_vector() {
    return unit_vector(random_in_unit_sphere());
}

bool material::scatter(vec3 in_dir, vec3 const &normal, bool front_face,
                       vec3 &scattered) const {
    ZoneScopedN("scatter");
    ZoneColor(Ctp::Pink);
    switch (tag) {
        case kind::diffuse_light:
            __builtin_unreachable();
            return false;
        case kind::isotropic: {
            ZoneScopedN("isotropic scatter");
            scattered = random_unit_vector();
            return true;
        }
        case kind::lambertian: {
            ZoneScopedN("lambertian scatter");
            auto scatter_direction = normal + random_unit_vector();

            // Catch degenerate scatter direction
            if (scatter_direction.near_zero()) scatter_direction = normal;

            scattered = scatter_direction;
            return true;
        }
        case kind::metal: {
            auto fuzz = data.fuzz;
            ZoneScopedN("metal scatter");
            vec3 reflected = reflect(in_dir, normal);
            auto fv = fuzz * random_unit_vector();
            reflected = unit_vector(reflected) + fv;
            scattered = reflected;
            return (dot(scattered, normal) > 0);
        }
        case kind::dielectric: {
            auto refraction_index = data.refraction_index;
            ZoneScopedN("dielectric scatter");
            double ri =
                front_face ? (1.0 / refraction_index) : refraction_index;

            vec3 unit_direction = unit_vector(in_dir);
            double cos_theta = -dot(unit_direction, normal);

            bool cannot_refract = ri * ri * (1 - cos_theta * cos_theta) > 1.0;
            vec3 direction;

            if (cannot_refract || reflectance(cos_theta, ri) > random_double())
                direction = reflect(unit_direction, normal);
            else
                direction = refract(unit_direction, normal, ri);

            scattered = direction;
            return true;
        }
    }
}
