#include "material.h"
#include <ranges>

#include <tracy/Tracy.hpp>

#include "random.h"

static float reflectance(float cosine, float refraction_index)
{
    // Use Schlick's approximation for reflectance.
    auto r0 = (1 - refraction_index) / (1 + refraction_index);
    r0 = r0 * r0;
    return r0 + (1 - r0) * pow((1 - cosine), 5);
}
static vec3 reflect(vec3 v, vec3 n) { return v - 2 * dot(v, n) * n; }

// `uv`, `n` are assumed to be unit vectors.
static vec3 refract(vec3 uv, vec3 n, float etai_over_etat)
{
    auto cos_theta = -dot(uv, n);
    vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    vec3 r_out_parallel = -std::sqrt(std::abs(1.0 - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_parallel;
}

static void randoms_in_unit_sphere(vec3 *start, vec3 *end)
{
    while (start != end) {
        // @perf generate random_vec with bulk :]
        std::generate(start, end, []() { return random_vec(-1, 1); });
        // keep trying only for the ones that haven't found their length squared.
        end = std::partition(start, end, [&](auto const &p) {
            return p.length_squared() >= 1;
        });
    }
}

static void random_unit_vectors(vec3 *start, vec3 *end)
{
    randoms_in_unit_sphere(start, end);
    std::transform(start, end, start, unit_vector);
}

void material::scatter_isotropic(vec3 *scattered, vec3 *end) noexcept
{
    ZoneScopedN("isotropic scatter");
    random_unit_vectors(scattered, end);
}

void material::scatter_lambertian(vec3 const *normals, uint32 const len, vec3 *scattered) noexcept
{
    ZoneScopedN("lambertian scatter");

    random_unit_vectors(scattered, scattered + len);
    std::transform(normals, normals + len, scattered, scattered, [&](auto const &normal, auto const &rng) {
        return normal + rng;
    });
}
void material::scatter_metal(float const fuzz, ray const *in_ray, vec3 const *normals, vec3 *scattered, vec3 *end) noexcept
{
    ZoneScopedN("metal scatter");
    // @perf might want a different buffer for random numbers and then add things to those.
    random_unit_vectors(scattered, end);
    auto const len = end - scattered;
    std::transform(scattered, scattered + len, scattered, [&](auto const &rng) { return fuzz * rng; });
    std::transform(in_ray, in_ray + len, std::views::iota(0).begin(), scattered, [&](auto const &r, auto const i) {
        return unit_vector(reflect(r.dir, normals[i])) + scattered[i];
    });
}
void material::scatter_dielectric(float const refraction_index, ray const *in_ray, bool const *front_faces, vec3 const *normals, vec3 *scattered, vec3 *end) noexcept
{
    ZoneScopedN("dielectric scatter");
    // @perf check out.
    auto const len = end - scattered;
    std::transform(
        in_ray, in_ray + len,
        std::views::iota(decltype(len)(0)).begin(),
        scattered, [&](auto const &hit_res, auto const i) -> vec3 {
            auto const &r = in_ray[i];
            auto const &in_dir = r.dir;
            auto const &normal = normals[i];
            auto const front_face = front_faces[i];
            // @perf if front face was partitioned, this would be branchless :]
            float ri = front_face ? (1.0 / refraction_index) : refraction_index;

            vec3 unit_direction = unit_vector(in_dir);
            float cos_theta = -dot(unit_direction, normal);

            bool cannot_refract = ri * ri * (1 - cos_theta * cos_theta) > 1.0;
            vec3 direction;

            if (cannot_refract || reflectance(cos_theta, ri) > random_float())
                direction = reflect(unit_direction, normal);
            else
                direction = refract(unit_direction, normal, ri);

            return direction;
        });
}
