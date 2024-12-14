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

#include "ray.h"
#include "rtweekend.h"
#include "vec3.h"

struct material {
    enum class kind {
        diffuse_light,
        isotropic,
        lambertian,
        metal,
        dielectric,
    } tag;

    union Data {
        float refraction_index;
        float fuzz;

        // NOTE: These constructors and destructors allow me to construct
        // everything easily.
        constexpr Data() { }
        constexpr ~Data() { }
    } data;

    constexpr material(kind tag, Data const &data)
        : tag(tag)
        , data(data)
    {
    }

    static void scatter_isotropic(transposed_vec_array scattered, uint32 start, uint32 end) noexcept;
    static void scatter_lambertian(transposed_vec_array normals, transposed_vec_array scattered, uint32 const start, uint32 const end) noexcept;
    static void scatter_metal(float const fuzz, transposed_ray_array in_rays, transposed_vec_array normals, transposed_vec_array scattered, uint32 const start, uint32 const end) noexcept;
    static void scatter_dielectric(float const refraction_index, transposed_ray_array in_rays, bool const *front_faces, transposed_vec_array normals, transposed_vec_array scattered, uint32 const start, uint32 const end) noexcept;

    static constexpr material metal(float fuzz)
    {
        Data d;
        d.fuzz = fuzz;
        return material(material::kind::metal, d);
    }

    static constexpr material dielectric(float ir)
    {
        Data d;
        d.refraction_index = ir;
        return material(material::kind::dielectric, d);
    }
};

namespace detail {
static material const lambertian(material::kind::lambertian, material::Data {});
static material const diffuse_light(material::kind::diffuse_light,
    material::Data {});
static material const isotropic(material::kind::isotropic, material::Data {});
}; // namespace detail
