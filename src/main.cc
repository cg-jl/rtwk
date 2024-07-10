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

#include <print>

#include "box.h"
#include "camera.h"
#include "constant_medium.h"
#include "geometry.h"
#include "hittable.h"
#include "hittable_list.h"
#include "material.h"
#include "quad.h"
#include "rtweekend.h"
#include "sphere.h"
#include "texture.h"
#include "timer.h"
#include "transforms.h"

void bouncing_spheres() {
    hittable_list world;

    auto checker =
        leak(texture::checker(0.32, leak(texture::solid(color(.2, .3, .1))),
                              leak(texture::solid(color(.9, .9, .9)))));
    world.add(lightInfo(detail::lambertian, checker),
              new sphere(point3(0, -1000, 0), 1000));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9 * random_double(), 0.2,
                          b + 0.9 * random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                if (choose_mat < 0.8) {
                    // diffuse
                    auto albedo =
                        leak(texture::solid(random_vec() * random_vec()));
                    auto sphere_material = detail::lambertian;
                    auto center2 = center + vec3(0, random_double(0, .5), 0);
                    world.add(lightInfo(detail::lambertian, albedo),
                              new sphere(center, center2, 0.2));
                } else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = leak(texture::solid(random_vec(0.5, 1)));
                    auto fuzz = random_double(0, 0.5);
                    auto sphere_material = (material::metal(fuzz));
                    world.add(lightInfo(sphere_material, albedo),
                              new sphere(center, 0.2));
                } else {
                    // glass
                    auto sphere_material = (material::dielectric(1.5));
                    world.add(lightInfo(sphere_material, &detail::white),
                              new sphere(center, 0.2));
                }
            }
        }
    }

    auto material1 = (material::dielectric(1.5));
    world.add(lightInfo(material1, &detail::white),
              new sphere(point3(0, 1, 0), 1.0));

    auto color2 = leak(texture::solid(color(0.4, 0.2, 0.1)));
    auto material2 = detail::lambertian;
    world.add(lightInfo(material2, color2), new sphere(point3(-4, 1, 0), 1.0));

    auto color3 = leak(texture::solid(color(0.7, 0.6, 0.5)));
    auto material3 = (material::metal(0.0));
    world.add(lightInfo(material3, color3), new sphere(point3(4, 1, 0), 1.0));

    hittable_list bvhd_world;
    bvhd_world.trees.emplace_back(world.selectGeoms);
    // Make sure we retain the hittable data from the trees.
    bvhd_world.objects = std::move(world.objects);

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.70, 0.80, 1.00);

    cam.vfov = 20;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0.6;
    cam.focus_dist = 10.0;

    cam.render(bvhd_world);
}

void checkered_spheres() {
    hittable_list world;

    auto checker =
        leak(texture::checker(0.32, leak(texture::solid(color(.2, .3, .1))),
                              leak(texture::solid(color(.9, .9, .9)))));

    world.add(lightInfo(detail::lambertian, checker),
              new sphere(point3(0, -10, 0), 10));
    world.add(lightInfo(detail::lambertian, checker),
              new sphere(point3(0, 10, 0), 10));

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.70, 0.80, 1.00);

    cam.vfov = 20;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void earth() {
    auto earth_texture = leak(texture::image("earthmap.jpg"));
    auto earth_surface = detail::lambertian;
    auto globeLights = lightInfo(earth_surface, earth_texture);
    auto globe = new sphere(point3(0, 0, 0), 2);

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.70, 0.80, 1.00);

    cam.vfov = 20;
    cam.lookfrom = point3(0, 0, 12);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(hittable_list(globeLights, globe));
}

void perlin_spheres() {
    hittable_list world;

    auto pertext = leak(texture::noise(4));
    world.add(lightInfo(detail::lambertian, pertext),
              new sphere(point3(0, -1000, 0), 1000));
    world.add(lightInfo(detail::lambertian, pertext),
              new sphere(point3(0, 2, 0), 2));

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.70, 0.80, 1.00);

    cam.vfov = 20;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void quads() {
    hittable_list world;

    // Materials
    auto left_red = texture::solid(color(1.0, 0.2, 0.2));
    auto back_green = texture::solid(color(0.2, 1.0, 0.2));
    auto right_blue = texture::solid(color(0.2, 0.2, 1.0));
    auto upper_orange = texture::solid(color(1.0, 0.5, 0.0));
    auto lower_teal = texture::solid(color(0.2, 0.8, 0.8));

    auto lambert = detail::lambertian;

    // Quads
    world.add(lightInfo(lambert, &left_red),
              new quad(point3(-3, -2, 5), vec3(0, 0, -4), vec3(0, 4, 0)));
    world.add(lightInfo(lambert, &back_green),
              new quad(point3(-2, -2, 0), vec3(4, 0, 0), vec3(0, 4, 0)));
    world.add(lightInfo(lambert, &right_blue),
              new quad(point3(3, -2, 1), vec3(0, 0, 4), vec3(0, 4, 0)));
    world.add(lightInfo(lambert, &upper_orange),
              new quad(point3(-2, 3, 1), vec3(4, 0, 0), vec3(0, 0, 4)));
    world.add(lightInfo(lambert, &lower_teal),
              new quad(point3(-2, -3, 5), vec3(4, 0, 0), vec3(0, 0, -4)));

    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.70, 0.80, 1.00);

    cam.vfov = 80;
    cam.lookfrom = point3(0, 0, 9);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void simple_light() {
    hittable_list world;

    auto pertext = leak(texture::noise(4));
    world.add(lightInfo(detail::lambertian, pertext),
              new sphere(point3(0, -1000, 0), 1000));

    world.add(lightInfo(detail::lambertian, pertext),
              new sphere(point3(0, 2, 0), 2));

    auto difflight = detail::diffuse_light;
    auto light_tint = texture::solid(color(4, 4, 4));
    world.add(lightInfo(difflight, &light_tint),
              new sphere(point3(0, 7, 0), 2));
    world.add(lightInfo(difflight, &light_tint),
              new quad(point3(3, 1, -2), vec3(2, 0, 0), vec3(0, 2, 0)));

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0, 0, 0);

    cam.vfov = 20;
    cam.lookfrom = point3(26, 3, 6);
    cam.lookat = point3(0, 2, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void cornell_box() {
    hittable_list world;

    auto red = texture::solid(color(.65, .05, .05));
    auto white = texture::solid(color(.73, .73, .73));
    auto green = texture::solid(color(.12, .45, .15));
    auto light = detail::diffuse_light;
    auto light_tint = texture::solid(color(15, 15, 15));

    auto lambert = detail::lambertian;

    world.add(lightInfo(lambert, &green),
              new quad(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555)));
    world.add(lightInfo(lambert, &red),
              new quad(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555)));
    world.add(
        lightInfo(light, &light_tint),
        new quad(point3(343, 554, 332), vec3(-130, 0, 0), vec3(0, 0, -105)));
    world.add(lightInfo(lambert, &white),
              new quad(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555)));
    world.add(
        lightInfo(lambert, &white),
        new quad(point3(555, 555, 555), vec3(-555, 0, 0), vec3(0, 0, -555)));
    world.add(lightInfo(lambert, &white),
              new quad(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0)));

    world.add(lightInfo(lambert, &white),

              new transformed(new box(point3(0, 0, 0), point3(165, 330, 165)),
                              transform(15, vec3(265, 0, 295))));

    {
        geometry *b = new box(point3(0, 0, 0), point3(165, 165, 165));
        b = new transformed(b, transform(-18, vec3(130, 0, 65)));
        world.add(lightInfo(lambert, &white), b);
    }

    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 600;
    cam.samples_per_pixel = 200;
    cam.max_depth = 50;
    cam.background = color(0, 0, 0);

    cam.vfov = 40;
    cam.lookfrom = point3(278, 278, -800);
    cam.lookat = point3(278, 278, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void cornell_smoke() {
    hittable_list world;

    auto red = texture::solid(color(.65, .05, .05));
    auto white = texture::solid(color(.73, .73, .73));
    auto green = texture::solid(color(.12, .45, .15));
    auto light = detail::diffuse_light;
    auto light_tint = texture::solid(color(7, 7, 7));

    auto lambert = detail::lambertian;

    world.add(lightInfo(lambert, &green),
              new quad(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555)));
    world.add(lightInfo(lambert, &red),
              new quad(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555)));
    world.add(
        lightInfo(light, &light_tint),
        new quad(point3(113, 554, 127), vec3(330, 0, 0), vec3(0, 0, 305)));
    world.add(lightInfo(lambert, &white),
              new quad(point3(0, 555, 0), vec3(555, 0, 0), vec3(0, 0, 555)));
    world.add(lightInfo(lambert, &white),
              new quad(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555)));
    world.add(lightInfo(lambert, &white),
              new quad(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0)));

    {
        geometry *b = new box(point3(0, 0, 0), point3(165, 330, 165));
        b = new transformed(b, transform(15, vec3(265, 0, 295)));
        world.add(constant_medium(b, 0.01), color(0, 0, 0));
    }

    {
        geometry *b = new box(point3(0, 0, 0), point3(165, 165, 165));
        b = new transformed(b, transform(-18, vec3(130, 0, 65)));
        world.add(constant_medium(b, 0.01), color(1, 1, 1));
    };

    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 600;
    cam.samples_per_pixel = 200;
    cam.max_depth = 50;
    cam.background = color(0, 0, 0);

    cam.vfov = 40;
    cam.lookfrom = point3(278, 278, -800);
    cam.lookat = point3(278, 278, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void final_scene(int image_width, int samples_per_pixel, int max_depth) {
    auto build_timer = new rtwk::timer("Building scene");
    std::vector<geometry *> boxes1;
    auto lambert = detail::lambertian;
    auto ground_col = texture::solid(color(0.48, 0.83, 0.53));

    // NOTE: @waste could link all of these to the same light info.
    int boxes_per_side = 20;
    boxes1.reserve(boxes_per_side * boxes_per_side);
    for (int i = 0; i < boxes_per_side; i++) {
        for (int j = 0; j < boxes_per_side; j++) {
            auto w = 100.0;
            auto x0 = -1000.0 + i * w;
            auto z0 = -1000.0 + j * w;
            auto y0 = 0.0;
            auto x1 = x0 + w;
            auto y1 = random_double(1, 101);
            auto z1 = z0 + w;

            boxes1.emplace_back(
                new box(point3(x0, y0, z0), point3(x1, y1, z1)));
        }
    }

    hittable_list world;
    {
        int link = world.objects.size();
        for (auto box : boxes1) {
            box->relIndex = link;
        }
        world.objects.emplace_back(detail::lambertian, &ground_col);
    }

    world.trees.emplace_back(
        std::span(const_cast<geometry const **>(boxes1.data()), boxes1.size()));

    auto light = detail::diffuse_light;
    auto light_tint = texture::solid(color(7, 7, 7));
    world.add(
        lightInfo(light, &light_tint),
        new quad(point3(123, 554, 147), vec3(300, 0, 0), vec3(0, 0, 265)));

    auto center1 = point3(400, 400, 200);
    auto center2 = center1 + vec3(30, 0, 0);
    auto sphere_material = lambert;
    auto sphere_tint = texture::solid(color(0.7, 0.3, 0.1));
    world.add(lightInfo(sphere_material, &sphere_tint),
              new sphere(center1, center2, 50));

    world.add(lightInfo((material::dielectric(1.5)), &detail::white),
              new sphere(point3(260, 150, 45), 50));
    auto fuzzball_tint = texture::solid(color(0.8, 0.8, 0.9));
    world.add(lightInfo((material::metal(1)), &fuzzball_tint),
              new sphere(point3(0, 150, 145), 50));

    auto boundary = new sphere(point3(360, 150, 145), 70);
    world.add(lightInfo((material::dielectric(1.5)), &detail::white), boundary);
    world.add(constant_medium(boundary, 0.2), color(0.2, 0.4, 0.9));
    boundary = new sphere(point3(0, 0, 0), 5000);
    world.add(constant_medium(boundary, .0001), color(1, 1, 1));

    auto emat = lambert;
    auto eimg = leak(texture::image("earthmap.jpg"));
    world.add(lightInfo(emat, eimg), new sphere(point3(400, 200, 400), 100));
    auto pertext = leak(texture::noise(0.2));
    world.add(lightInfo(lambert, pertext),
              new sphere(point3(220, 280, 300), 80));

    auto white = texture::solid(color(.73, .73, .73));
    int ns = 1000;
    std::vector<geometry *> boxes2;
    boxes2.reserve(ns);
    for (int j = 0; j < ns; j++) {
        geometry *s = new sphere(random_vec(0, 165), 10);
        s = new transformed{
            s,

            transform(15, vec3(-100, 270, 395)),
        };

        boxes2.emplace_back(s);
    }

    // ahh I hate casting const through >1 depth.
    // "casts away qualifiers" but I'm marking them const!!!
    world.trees.emplace_back(
        std::span(const_cast<geometry const **>(boxes2.data()), ns));
    {
        // Link all of them to the same tex.
        int link = world.objects.size();
        for (auto sph : boxes2) {
            sph->relIndex = link;
        }
        world.objects.emplace_back(lightInfo{detail::lambertian, &white});
    }

    delete build_timer;

    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = image_width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = max_depth;
    cam.background = color(0, 0, 0);

    cam.vfov = 40;
    cam.lookfrom = point3(478, 278, -600);
    cam.lookat = point3(278, 278, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world);
}

int main() {
#if TRACY_ENABLE
    switch (10) {
#else
    switch (0) {
#endif
        case 1:
            bouncing_spheres();
            break;
        case 2:
            checkered_spheres();
            break;
        case 3:
            earth();
            break;
        case 4:
            perlin_spheres();
            break;
        case 5:
            quads();
            break;
        case 6:
            simple_light();
            break;
        case 7:
            cornell_box();
            break;
        case 8:
            cornell_smoke();
            break;
        case 9:
            final_scene(800, 10000, 40);
            break;
        case 10:
            // tracing scene.
            // 1spp at the testing capacity. Otherwise
            // I get so much data.
            final_scene(400, 1, 40);
            break;
        default:
            final_scene(400, 250, 40);
            break;
    }
}
