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
    world.add(hittable(&detail::lambertian, checker,
                       new sphere(point3(0, -1000, 0), 1000)));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9 * random_double(), 0.2,
                          b + 0.9 * random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                material const *sphere_material;

                if (choose_mat < 0.8) {
                    // diffuse
                    auto albedo =
                        leak(texture::solid(random_vec() * random_vec()));
                    sphere_material = &detail::lambertian;
                    auto center2 = center + vec3(0, random_double(0, .5), 0);
                    world.add(hittable(sphere_material, albedo,
                                       new sphere(center, center2, 0.2)));
                } else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = leak(texture::solid(random_vec(0.5, 1)));
                    auto fuzz = random_double(0, 0.5);
                    sphere_material = leak(material::metal(fuzz));
                    world.add(hittable(sphere_material, albedo,
                                       new sphere(center, 0.2)));
                } else {
                    // glass
                    sphere_material = leak(material::dielectric(1.5));
                    world.add(hittable(sphere_material, &detail::white,
                                       new sphere(center, 0.2)));
                }
            }
        }
    }

    auto material1 = leak(material::dielectric(1.5));
    world.add(
        hittable(material1, &detail::white, new sphere(point3(0, 1, 0), 1.0)));

    auto color2 = leak(texture::solid(color(0.4, 0.2, 0.1)));
    auto material2 = &detail::lambertian;
    world.add(hittable(material2, color2, new sphere(point3(-4, 1, 0), 1.0)));

    auto color3 = leak(texture::solid(color(0.7, 0.6, 0.5)));
    auto material3 = leak(material::metal(0.0));
    world.add(hittable(material3, color3, new sphere(point3(4, 1, 0), 1.0)));

    hittable_list bvhd_world;
    bvhd_world.trees.emplace_back(world.objects);

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

    world.add(hittable(&detail::lambertian, checker,
                       new sphere(point3(0, -10, 0), 10)));
    world.add(hittable(&detail::lambertian, checker,
                       new sphere(point3(0, 10, 0), 10)));

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
    auto earth_surface = &detail::lambertian;
    auto globe =
        hittable(earth_surface, earth_texture, new sphere(point3(0, 0, 0), 2));

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

    cam.render(hittable_list(globe));
}

void perlin_spheres() {
    hittable_list world;

    auto pertext = leak(texture::noise(4));
    world.add(hittable(&detail::lambertian, pertext,
                       new sphere(point3(0, -1000, 0), 1000)));
    world.add(
        hittable(&detail::lambertian, pertext, new sphere(point3(0, 2, 0), 2)));

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

    auto lambert = &detail::lambertian;

    // Quads
    world.add(
        hittable(lambert, &left_red,
                 new quad(point3(-3, -2, 5), vec3(0, 0, -4), vec3(0, 4, 0))));
    world.add(
        hittable(lambert, &back_green,
                 new quad(point3(-2, -2, 0), vec3(4, 0, 0), vec3(0, 4, 0))));
    world.add(
        hittable(lambert, &right_blue,
                 new quad(point3(3, -2, 1), vec3(0, 0, 4), vec3(0, 4, 0))));
    world.add(
        hittable(lambert, &upper_orange,
                 new quad(point3(-2, 3, 1), vec3(4, 0, 0), vec3(0, 0, 4))));
    world.add(
        hittable(lambert, &lower_teal,
                 new quad(point3(-2, -3, 5), vec3(4, 0, 0), vec3(0, 0, -4))));

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
    world.add(hittable(&detail::lambertian, pertext,
                       new sphere(point3(0, -1000, 0), 1000)));

    world.add(
        hittable(&detail::lambertian, pertext, new sphere(point3(0, 2, 0), 2)));

    auto difflight = &detail::diffuse_light;
    auto light_tint = texture::solid(color(4, 4, 4));
    world.add(hittable(difflight, &light_tint, new sphere(point3(0, 7, 0), 2)));
    world.add(
        hittable(difflight, &light_tint,
                 new quad(point3(3, 1, -2), vec3(2, 0, 0), vec3(0, 2, 0))));

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
    auto light = &detail::diffuse_light;
    auto light_tint = texture::solid(color(15, 15, 15));

    auto lambert = &detail::lambertian;

    world.add(hittable(
        lambert, &green,
        new quad(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555))));
    world.add(
        hittable(lambert, &red,
                 new quad(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555))));
    world.add(hittable(
        light, &light_tint,
        new quad(point3(343, 554, 332), vec3(-130, 0, 0), vec3(0, 0, -105))));
    world.add(
        hittable(lambert, &white,
                 new quad(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555))));
    world.add(hittable(
        lambert, &white,
        new quad(point3(555, 555, 555), vec3(-555, 0, 0), vec3(0, 0, -555))));
    world.add(hittable(
        lambert, &white,
        new quad(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0))));

    auto box1 = world.with([white, lambert](auto &world) {
        world.add(hittable(lambert, &white,
                           new box(point3(0, 0, 0), point3(165, 330, 165))));
    });
    for (auto &b : box1) {
        b.geom = new rotate_y(b.geom, 15);
        b.geom = new translate(b.geom, vec3(265, 0, 295));
    }

    auto box2 = world.with([white, lambert](auto &world) {
        world.add(hittable(lambert, &white,
                           new box(point3(0, 0, 0), point3(165, 165, 165))));
    });
    for (auto &b : box2) {
        b.geom = new rotate_y(b.geom, -18);
        b.geom = new translate(b.geom, vec3(130, 0, 65));
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
    auto light = &detail::diffuse_light;
    auto light_tint = texture::solid(color(7, 7, 7));

    auto lambert = &detail::lambertian;

    world.add(hittable(
        lambert, &green,
        new quad(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555))));
    world.add(
        hittable(lambert, &red,
                 new quad(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555))));
    world.add(hittable(
        light, &light_tint,
        new quad(point3(113, 554, 127), vec3(330, 0, 0), vec3(0, 0, 305))));
    world.add(hittable(
        lambert, &white,
        new quad(point3(0, 555, 0), vec3(555, 0, 0), vec3(0, 0, 555))));
    world.add(
        hittable(lambert, &white,
                 new quad(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555))));
    world.add(hittable(
        lambert, &white,
        new quad(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0))));

    {
        geometry *b = new box(point3(0, 0, 0), point3(165, 330, 165));
        b = new rotate_y(b, 15);
        b = new translate(b, vec3(265, 0, 295));
        world.add(constant_medium(b, 0.01, color(0, 0, 0)));
    }

    {
        geometry *b = new box(point3(0, 0, 0), point3(165, 165, 165));
        b = new rotate_y(b, -18);
        b = new translate(b, vec3(130, 0, 65));
        world.add(constant_medium(b, 0.01, color(1, 1, 1)));
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
    hittable_list boxes1;
    auto lambert = &detail::lambertian;
    auto ground_col = texture::solid(color(0.48, 0.83, 0.53));

    int boxes_per_side = 20;
    for (int i = 0; i < boxes_per_side; i++) {
        for (int j = 0; j < boxes_per_side; j++) {
            auto w = 100.0;
            auto x0 = -1000.0 + i * w;
            auto z0 = -1000.0 + j * w;
            auto y0 = 0.0;
            auto x1 = x0 + w;
            auto y1 = random_double(1, 101);
            auto z1 = z0 + w;

            boxes1.add(
                hittable(lambert, &ground_col,
                         new box(point3(x0, y0, z0), point3(x1, y1, z1))));
        }
    }

    hittable_list world;

    world.trees.emplace_back(boxes1.objects);

    auto light = &detail::diffuse_light;
    auto light_tint = texture::solid(color(7, 7, 7));
    world.add(hittable(
        light, &light_tint,
        new quad(point3(123, 554, 147), vec3(300, 0, 0), vec3(0, 0, 265))));

    auto center1 = point3(400, 400, 200);
    auto center2 = center1 + vec3(30, 0, 0);
    auto sphere_material = lambert;
    auto sphere_tint = texture::solid(color(0.7, 0.3, 0.1));
    world.add(hittable(sphere_material, &sphere_tint,
                       new sphere(center1, center2, 50)));

    world.add(hittable(leak(material::dielectric(1.5)), &detail::white,
                       new sphere(point3(260, 150, 45), 50)));
    auto fuzzball_tint = texture::solid(color(0.8, 0.8, 0.9));
    world.add(hittable(leak(material::metal(1)), &fuzzball_tint,
                       new sphere(point3(0, 150, 145), 50)));

    auto boundary = new sphere(point3(360, 150, 145), 70);
    world.add(
        hittable(leak(material::dielectric(1.5)), &detail::white, boundary));
    world.add(constant_medium(boundary, 0.2, color(0.2, 0.4, 0.9)));
    boundary = new sphere(point3(0, 0, 0), 5000);
    world.add(constant_medium(boundary, .0001, color(1, 1, 1)));

    auto emat = lambert;
    auto eimg = leak(texture::image("earthmap.jpg"));
    world.add(hittable(emat, eimg, new sphere(point3(400, 200, 400), 100)));
    auto pertext = leak(texture::noise(0.2));
    world.add(
        hittable(lambert, pertext, new sphere(point3(220, 280, 300), 80)));

    hittable_list boxes2;
    auto white = texture::solid(color(.73, .73, .73));
    int ns = 1000;
    for (int j = 0; j < ns; j++) {
        geometry *s = new sphere(random_vec(0, 165), 10);

        s = new rotate_y(s, 15);
        s = new translate(s, vec3(-100, 270, 395));

        boxes2.add(hittable(lambert, &white, s));
    }

    world.trees.emplace_back(boxes2.objects);

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
    switch (0) {
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
        default:
            final_scene(400, 250, 40);
            break;
    }
}
