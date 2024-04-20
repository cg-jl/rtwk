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

#include "hittable.h"
#include "rtweekend.h"
#include "texture.h"

class hit_record;

class material {
   public:
    virtual ~material() = default;

    virtual color emitted(uvs uv, point3 const &p) const {
        return color(0, 0, 0);
    }

    virtual bool scatter(ray const &r_in, hit_record const &rec,
                         color &attenuation, ray &scattered) const {
        return false;
    }
};

class lambertian : public material {
   public:
    lambertian(color const &albedo) : tex(new solid_color(albedo)) {}
    lambertian(texture *tex) : tex(tex) {}

    bool scatter(ray const &r_in, hit_record const &rec, color &attenuation,
                 ray &scattered) const final {
        ZoneScopedN("lambertian scatter");
        auto scatter_direction = rec.normal + random_unit_vector();

        // Catch degenerate scatter direction
        if (scatter_direction.near_zero()) scatter_direction = rec.normal;

        scattered = ray(rec.p, scatter_direction, r_in.time());
        attenuation = tex->value(rec.uv, rec.p);
        return true;
    }

   private:
    texture *tex;
};

class metal : public material {
   public:
    metal(color const &albedo, double fuzz)
        : albedo(albedo), fuzz(fuzz < 1 ? fuzz : 1) {}

    bool scatter(ray const &r_in, hit_record const &rec, color &attenuation,
                 ray &scattered) const final {
        vec3 reflected = reflect(r_in.direction(), rec.normal);
        reflected = unit_vector(reflected) + (fuzz * random_unit_vector());
        scattered = ray(rec.p, reflected, r_in.time());
        attenuation = albedo;
        return (dot(scattered.direction(), rec.normal) > 0);
    }

   private:
    color albedo;
    double fuzz;
};

class dielectric : public material {
   public:
    dielectric(double refraction_index) : refraction_index(refraction_index) {}

    bool scatter(ray const &r_in, hit_record const &rec, color &attenuation,
                 ray &scattered) const final {
        attenuation = color(1.0, 1.0, 1.0);
        double ri =
            rec.front_face ? (1.0 / refraction_index) : refraction_index;

        vec3 unit_direction = unit_vector(r_in.direction());
        double cos_theta = fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = sqrt(1.0 - cos_theta * cos_theta);

        bool cannot_refract = ri * sin_theta > 1.0;
        vec3 direction;

        if (cannot_refract || reflectance(cos_theta, ri) > random_double())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, ri);

        scattered = ray(rec.p, direction, r_in.time());
        return true;
    }

   private:
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

class diffuse_light : public material {
   public:
    diffuse_light(texture *tex) : tex(tex) {}
    diffuse_light(color const &emit) : tex(new solid_color(emit)) {}

    color emitted(uvs uv, point3 const &p) const final {
        return tex->value(uv, p);
    }

   private:
    texture *tex;
};

class isotropic : public material {
   public:
    isotropic(color const &albedo) : tex(new solid_color(albedo)) {}
    isotropic(texture *tex) : tex(tex) {}

    bool scatter(ray const &r_in, hit_record const &rec, color &attenuation,
                 ray &scattered) const final {
        scattered = ray(rec.p, random_unit_vector(), r_in.time());
        attenuation = tex->value(rec.uv, rec.p);
        return true;
    }

   private:
    texture *tex;
};

#endif
