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
    virtual ~material() = default;

    virtual color emitted(double u, double v, point3 const& p) const {
        return color(0, 0, 0);
    }

    virtual void attenuation(double u, double v, point3 p,
                             color& att) const = 0;

    virtual bool scatter(vec3 in_dir, hit_record const& rec,
                         vec3& scattered) const = 0;
};

struct lambertian : public material {
   public:
    lambertian(color const& a) : albedo(make_shared<solid_color>(a)) {}
    lambertian(shared_ptr<texture> a) : albedo(a) {}

    void attenuation(double u, double v, point3 p, color& att) const override {
        att = albedo->value(u, v, p);
    }

    bool scatter(vec3 in_dir, hit_record const& rec,
                 vec3& scattered) const override {
        auto scatter_direction = rec.normal + random_unit_vector();

        // Catch degenerate scatter direction
        if (scatter_direction.near_zero()) scatter_direction = rec.normal;

        scattered = scatter_direction;
        return true;
    }

   private:
    shared_ptr<texture> albedo;
};

struct metal : public material {
   public:
    metal(color const& a, double f) : albedo(a), fuzz(f < 1 ? f : 1) {}

    void attenuation(double u, double v, point3 p, color& att) const override {
        att = albedo;
    }

    bool scatter(vec3 in_dir, hit_record const& rec,
                 vec3& scattered) const override {
        vec3 reflected = reflect(unit_vector(in_dir), rec.normal);
        scattered = reflected + fuzz * random_in_unit_sphere();
        return (dot(scattered, rec.normal) > 0);
    }

   private:
    color albedo;
    double fuzz;
};

struct dielectric : public material {
   public:
    dielectric(double index_of_refraction) : ir(index_of_refraction) {}

    void attenuation(double u, double v, point3 p, color& att) const override {
        att = color(1, 1, 1);
    }

    bool scatter(vec3 in_dir, hit_record const& rec,
                 vec3& scattered) const override {
        double refraction_ratio = rec.front_face ? (1.0 / ir) : ir;

        vec3 unit_direction = unit_vector(in_dir);
        double cos_theta = fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = sqrt(1.0 - cos_theta * cos_theta);

        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        vec3 direction;

        if (cannot_refract ||
            reflectance(cos_theta, refraction_ratio) > random_double())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, refraction_ratio);

        scattered = direction;
        return true;
    }

   private:
    double ir;  // Index of Refraction

    static double reflectance(double cosine, double ref_idx) {
        // Use Schlick's approximation for reflectance.
        auto r0 = (1 - ref_idx) / (1 + ref_idx);
        r0 = r0 * r0;
        return r0 + (1 - r0) * pow((1 - cosine), 5);
    }
};

struct diffuse_light : public material {
   public:
    diffuse_light(shared_ptr<texture> a) : emit(a) {}
    diffuse_light(color c) : emit(make_shared<solid_color>(c)) {}

    bool scatter(vec3 in_dir, hit_record const& rec,
                 vec3& scattered) const override {
        return false;
    }

    void attenuation(double u, double v, point3 p, color& att) const override {
#if !NDEBUG
        assert(false && "unreachable!");
#endif
    }

    color emitted(double u, double v, point3 const& p) const override {
        return emit->value(u, v, p);
    }

   private:
    shared_ptr<texture> emit;
};

struct isotropic : public material {
   public:
    isotropic(color c) : albedo(make_shared<solid_color>(c)) {}
    isotropic(shared_ptr<texture> a) : albedo(a) {}

    void attenuation(double u, double v, point3 p, color& att) const override {
        att = albedo->value(u, v, p);
    }

    bool scatter(vec3 in_dir, hit_record const& rec,
                 vec3& scattered) const override {
        scattered =  random_unit_vector();
        return true;
    }

   private:
    shared_ptr<texture> albedo;
};
