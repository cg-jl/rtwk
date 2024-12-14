#include "material.h"
#include <ranges>

#include <tracy/Tracy.hpp>

#include "random.h"
#include "ray.h"

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

static void adjust_range(float *start, float *end, float min, float max)
{
    std::transform(start, end, start, [&](auto x) { return x * (max - min) + min; });
}

static void adjust_vec_range(transposed_vec_array dst, uint32 start, uint32 end, float min, float max)
{
    adjust_range(dst.x + start, dst.x + end, min, max);
    adjust_range(dst.y + start, dst.y + end, min, max);
    adjust_range(dst.z + start, dst.z + end, min, max);
}

static void random_vecs(transposed_vec_array dst, uint32 start, uint32 end)
{
    std::generate(dst.x + start, dst.x + end, [] { return random_float(); });
    std::generate(dst.y + start, dst.y + end, [] { return random_float(); });
    std::generate(dst.z + start, dst.z + end, [] { return random_float(); });
}
static void random_vecs(transposed_vec_array dst, uint32 start, uint32 end, float min, float max)
{
    random_vecs(dst, start, end);
    adjust_vec_range(dst, start, end, min, max);
}

static void randoms_in_unit_sphere(transposed_vec_array dst, uint32 start, uint32 end)
{
    while (start != end) {
        random_vecs(dst, start, end, -1, 1);
        // keep trying only for the ones that haven't found their length squared.
        end = partition(start, end, [&](auto i, auto k) { dst.swap(i, k); }, [&](auto const i) { return vec3(dst[i]).length_squared() >= 1; });
    }
}

static void random_unit_vectors(transposed_vec_array dst, uint32 start, uint32 end)
{
    randoms_in_unit_sphere(dst, start, end);
    for (auto i = start; i < end; ++i) {
        dst[i] = unit_vector(dst[i]);
    }
}

void material::scatter_isotropic(transposed_vec_array scattered, uint32 start, uint32 end) noexcept
{
    ZoneScopedN("isotropic scatter");
    random_unit_vectors(scattered, start, end);
}

void material::scatter_lambertian(transposed_vec_array normals, transposed_vec_array scattered, uint32 const start, uint32 const end) noexcept
{
    ZoneScopedN("lambertian scatter");

    random_unit_vectors(scattered, start, end);

    for (auto i = start; i < end; ++i) {
        scattered[i] = scattered[i] + normals[i];
    }
}

// @cleanup could make c++ iterators for this?
static void transform_transposed(transposed_vec_array src, transposed_vec_array dst, uint32 start, uint32 end, auto const &fnc)
{
    std::transform(src.x + start, src.x + end, dst.x, fnc);
    std::transform(src.y + start, src.y + end, dst.x, fnc);
    std::transform(src.z + start, src.z + end, dst.x, fnc);
}

void material::scatter_metal(float const fuzz, transposed_ray_array in_rays, transposed_vec_array normals, transposed_vec_array scattered, uint32 const start, uint32 const end) noexcept
{
    ZoneScopedN("metal scatter");
    // @perf might want a different buffer for random numbers and then add things to those.
    random_unit_vectors(scattered, start, end);
    transform_transposed(scattered, scattered, start, end, [&](auto const rng) { return fuzz * rng; });

    for (auto i = start; i < end; ++i) {
        scattered[i] = unit_vector(reflect(in_rays[i].dir, normals[i])) + scattered[i];
    }
}
void material::scatter_dielectric(float const refraction_index, transposed_ray_array in_rays, bool const *front_faces, transposed_vec_array normals, transposed_vec_array scattered, uint32 const start, uint32 const end) noexcept
{
    ZoneScopedN("dielectric scatter");
    // @perf check out.
    for (auto i = start; i < end; ++i) {

        auto const &r = in_rays[i];
        auto const &in_dir = r.dir;
        auto const normal = normals[i];
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

        scattered[i] = direction;
    }
}
