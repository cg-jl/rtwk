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

#include <time.h>

#include "camera.h"
#include "color.h"
#include "constant_medium.h"
#include "hittable_list.h"
#include "material.h"
#include "quad.h"
#include "rtweekend.h"
#include "sphere.h"
#include "texture.h"
#include "transform.h"

static void random_spheres() {
    hittable_list world;

    auto checker = make_shared<checker_texture>(0.32, color(.2, .3, .1),
                                                color(.9, .9, .9));
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000,
                                  make_shared<lambertian>(), checker));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_float();
            point3 center(float(a) + 0.9f * random_float(), 0.2f,
                          float(b) + 0.9f * random_float());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                shared_ptr<material> sphere_material;
                shared_ptr<texture> sphere_texture;

                if (choose_mat < 0.8) {
                    // diffuse
                    auto albedo = color::random() * color::random();
                    sphere_material = make_shared<lambertian>();
                    sphere_texture = make_shared<solid_color>(albedo);
                    auto displacement = vec3(0, random_float(0, .5), 0);

                    world.add(make_shared<transformed_geometry>(
                        make_shared<move>(displacement),
                        make_shared<sphere>(center, 0.2, sphere_material,
                                            sphere_texture)));
                } else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_float(0, 0.5);
                    sphere_material = make_shared<metal>(fuzz);
                    world.add(
                        make_shared<sphere>(center, 0.2, sphere_material,
                                            make_shared<solid_color>(albedo)));
                } else {
                    // glass
                    sphere_material = make_shared<dielectric>(1.5);
                    world.add(
                        make_shared<sphere>(center, 0.2, sphere_material,
                                            make_shared<solid_color>(1, 1, 1)));
                }
            }
        }
    }

    auto material1 = make_shared<dielectric>(1.5);
    world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, material1,
                                  make_shared<solid_color>(1, 1, 1)));

    auto material2 = make_shared<lambertian>();
    world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, material2,
                                  make_shared<solid_color>(.4, .2, .1)));

    auto material3 = make_shared<metal>(0.0);
    world.add(
        make_shared<sphere>(point3(4, 1, 0), 1.0, material3,
                            make_shared<solid_color>(color(0.7, .6, .5))));

    world = hittable_list(world.split());

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

    cam.defocus_angle = 0.02;
    cam.focus_dist = 10.0;

    cam.render(world);
}

static void two_spheres() {
    hittable_list world;

    auto checker =
        make_shared<checker_texture>(0.8, color(.2, .3, .1), color(.9, .9, .9));

    world.add(make_shared<sphere>(point3(0, -10, 0), 10,
                                  make_shared<lambertian>(), checker));
    world.add(make_shared<sphere>(point3(0, 10, 0), 10,
                                  make_shared<lambertian>(), checker));

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

static void earth() {
    auto earth_texture = make_shared<image_texture>("earthmap.jpg");
    auto earth_surface = make_shared<lambertian>();
    auto globe =
        make_shared<sphere>(point3(0, 0, 0), 2, earth_surface, earth_texture);

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

static void two_perlin_spheres() {
    hittable_list world;

    auto pertext = make_shared<noise_texture>(4);
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000,
                                  make_shared<lambertian>(), pertext));
    world.add(make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(),
                                  pertext));

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

static void quads() {
    hittable_list world;

    // Materials
    auto left_red = make_shared<solid_color>(color(1.0, 0.2, 0.2));
    auto back_green = make_shared<solid_color>(color(0.2, 1.0, 0.2));
    auto right_blue = make_shared<solid_color>(color(0.2, 0.2, 1.0));
    auto upper_orange = make_shared<solid_color>(color(1.0, 0.5, 0.0));
    auto lower_teal = make_shared<solid_color>(color(0.2, 0.8, 0.8));

    auto all_mat = make_shared<lambertian>();

    // Quads
    world.add(make_shared<quad>(point3(-3, -2, 5), vec3(0, 0, -4),
                                vec3(0, 4, 0), all_mat, left_red));
    world.add(make_shared<quad>(point3(-2, -2, 0), vec3(4, 0, 0), vec3(0, 4, 0),
                                all_mat, back_green));
    world.add(make_shared<quad>(point3(3, -2, 1), vec3(0, 0, 4), vec3(0, 4, 0),
                                all_mat, right_blue));
    world.add(make_shared<quad>(point3(-2, 3, 1), vec3(4, 0, 0), vec3(0, 0, 4),
                                all_mat, upper_orange));
    world.add(make_shared<quad>(point3(-2, -3, 5), vec3(4, 0, 0),
                                vec3(0, 0, -4), all_mat, lower_teal));

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

static void simple_light() {
    hittable_list world;

    auto pertext = make_shared<noise_texture>(4);
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000,
                                  make_shared<lambertian>(), pertext));
    world.add(make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(),
                                  pertext));

    auto difflight = make_shared<diffuse_light>();
    auto difflight_color = make_shared<solid_color>(color(4, 4, 4));
    world.add(
        make_shared<sphere>(point3(0, 7, 0), 2, difflight, difflight_color));
    world.add(make_shared<quad>(point3(3, 1, -2), vec3(2, 0, 0), vec3(0, 2, 0),
                                difflight, difflight_color));

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

static void cornell_box() {
    hittable_list world;

    auto red = make_shared<solid_color>(color(.65, .05, .05));
    auto white = make_shared<solid_color>(color(.73, .73, .73));
    auto green = make_shared<solid_color>(color(.12, .45, .15));
    auto light = make_shared<diffuse_light>();
    auto light_color = make_shared<solid_color>(15, 15, 15);

    world.add(make_shared<quad>(point3(555, 0, 0), vec3(0, 555, 0),
                                vec3(0, 0, 555), make_shared<lambertian>(),
                                green));
    world.add(make_shared<quad>(point3(0, 0, 0), vec3(0, 555, 0),
                                vec3(0, 0, 555), make_shared<lambertian>(),
                                red));
    world.add(make_shared<quad>(point3(343, 554, 332), vec3(-130, 0, 0),
                                vec3(0, 0, -105), light, light_color));
    world.add(make_shared<quad>(point3(0, 0, 0), vec3(555, 0, 0),
                                vec3(0, 0, 555), make_shared<lambertian>(),
                                white));
    world.add(make_shared<quad>(point3(555, 555, 555), vec3(-555, 0, 0),
                                vec3(0, 0, -555), make_shared<lambertian>(),
                                white));
    world.add(make_shared<quad>(point3(0, 0, 555), vec3(555, 0, 0),
                                vec3(0, 555, 0), make_shared<lambertian>(),
                                white));

    shared_ptr<hittable> box1 = box(point3(0, 0, 0), point3(165, 330, 165),
                                    make_shared<lambertian>(), white);
    ordered_transforms transforms;
    transforms.add<translate>(vec3(265, 0, 295));
    transforms.add<rotate_y>(15);
    world.add(make_shared<transformed_geometry>(
        make_shared<ordered_transforms>(std::move(transforms)),
        std::move(box1)));

    shared_ptr<hittable> box2 = box(point3(0, 0, 0), point3(165, 165, 165),
                                    make_shared<lambertian>(), white);
    // NOTE: This is safe because we moved it
    transforms.tfs.clear();

    transforms.add<translate>(vec3(130, 0, 65));
    transforms.add<rotate_y>(-18);

    world.add(make_shared<transformed_geometry>(
        make_shared<ordered_transforms>(std::move(transforms)),
        std::move(box2)));

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

static void cornell_smoke() {
    hittable_list world;

    auto red = make_shared<solid_color>(color(.65, .05, .05));
    auto white = make_shared<solid_color>(color(.73, .73, .73));
    auto green = make_shared<solid_color>(color(.12, .45, .15));
    auto light = make_shared<solid_color>(color(7, 7, 7));

    world.add(make_shared<quad>(point3(555, 0, 0), vec3(0, 555, 0),
                                vec3(0, 0, 555), make_shared<lambertian>(),
                                green));
    world.add(make_shared<quad>(point3(0, 0, 0), vec3(0, 555, 0),
                                vec3(0, 0, 555), make_shared<lambertian>(),
                                red));
    world.add(make_shared<quad>(point3(113, 554, 127), vec3(330, 0, 0),
                                vec3(0, 0, 305), make_shared<diffuse_light>(),
                                light));
    world.add(make_shared<quad>(point3(0, 555, 0), vec3(555, 0, 0),
                                vec3(0, 0, 555), make_shared<lambertian>(),
                                white));
    world.add(make_shared<quad>(point3(0, 0, 0), vec3(555, 0, 0),
                                vec3(0, 0, 555), make_shared<lambertian>(),
                                white));
    world.add(make_shared<quad>(point3(0, 0, 555), vec3(555, 0, 0),
                                vec3(0, 555, 0), make_shared<lambertian>(),
                                white));

    shared_ptr<hittable> box1 = box(point3(0, 0, 0), point3(165, 330, 165),
                                    make_shared<lambertian>(), white);

    ordered_transforms transforms;
    transforms.add<translate>(vec3(265, 0, 295));
    transforms.add<rotate_y>(15);

    box1 = make_shared<transformed_geometry>(
        make_shared<ordered_transforms>(std::move(transforms)),
        std::move(box1));

    shared_ptr<hittable> box2 = box(point3(0, 0, 0), point3(165, 165, 165),
                                    make_shared<lambertian>(), white);

    // NOTE: We can do this because we moved earlier
    transforms.tfs.clear();

    transforms.add<translate>(vec3(130, 0, 65));
    transforms.add<rotate_y>(-18);

    box2 = make_shared<transformed_geometry>(
        make_shared<ordered_transforms>(std::move(transforms)),
        std::move(box2));

    world.add(make_shared<constant_medium>(box1, 0.01, color(0, 0, 0)));
    world.add(make_shared<constant_medium>(box2, 0.01, color(1, 1, 1)));

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

static void final_scene(int image_width, int samples_per_pixel, int max_depth) {
    struct timespec start;

    clock_gettime(CLOCK_MONOTONIC, &start);

    hittable_list boxes1;
    auto ground_col = make_shared<solid_color>(0.48, 0.83, 0.53);
    auto ground = make_shared<lambertian>();

    int boxes_per_side = 20;
    for (int i = 0; i < boxes_per_side; i++) {
        for (int j = 0; j < boxes_per_side; j++) {
            auto w = 100.0f;
            auto x0 = -1000.0f + float(i) * w;
            auto z0 = -1000.0f + float(j) * w;
            auto y0 = 0.0f;
            auto x1 = x0 + w;
            auto y1 = random_float(1, 101);
            auto z1 = z0 + w;

            boxes1.add(box(point3(x0, y0, z0), point3(x1, y1, z1), ground,
                           ground_col));
        }
    }

    hittable_list world;

    world.add(boxes1.split());

    auto light = make_shared<diffuse_light>();
    auto light_color = make_shared<solid_color>(7, 7, 7);

    world.add(make_shared<quad>(point3(123, 554, 147), vec3(300, 0, 0),
                                vec3(0, 0, 265), light, light_color));

    auto center = point3(400, 400, 200);
    auto sphere_material = make_shared<lambertian>();
    auto sphere_color = make_shared<solid_color>(0.7, 0.3, 0.1);
    world.add(make_shared<transformed_geometry>(
        make_shared<move>(vec3(30, 0, 0)),
        make_shared<sphere>(center, 50, sphere_material, sphere_color)));

    world.add(make_shared<sphere>(point3(260, 150, 45), 50,
                                  make_shared<dielectric>(1.5),
                                  make_shared<solid_color>(1, 1, 1)));
    world.add(
        make_shared<sphere>(point3(0, 150, 145), 50, make_shared<metal>(1.0),
                            make_shared<solid_color>(color(0.8, 0.8, 0.9))));

    auto boundary = make_shared<sphere>(point3(360, 150, 145), 70,
                                        make_shared<dielectric>(1.5),
                                        make_shared<solid_color>(1, 1, 1));
    world.add(boundary);
    world.add(
        make_shared<constant_medium>(boundary, 0.2, color(0.2, 0.4, 0.9)));
    boundary =
        make_shared<sphere>(point3(0, 0, 0), 5000, make_shared<dielectric>(1.5),
                            make_shared<solid_color>(1, 1, 1));
    world.add(make_shared<constant_medium>(boundary, .0001, color(1, 1, 1)));

    auto emat = (make_shared<image_texture>("earthmap.jpg"));
    world.add(make_shared<sphere>(point3(400, 200, 400), 100,
                                  make_shared<lambertian>(), emat));
    auto pertext = make_shared<noise_texture>(0.1);
    world.add(make_shared<sphere>(point3(220, 280, 300), 80,
                                  make_shared<lambertian>(), pertext));

    hittable_list boxes2;
    auto white = make_shared<solid_color>(color(.73, .73, .73));
    int ns = 1000;
    for (int j = 0; j < ns; j++) {
        boxes2.add(make_shared<sphere>(point3::random(0, 165), 10,
                                       make_shared<lambertian>(), white));
    }

    ordered_transforms transforms;
    transforms.with<translate>(vec3(-100, 270, 395));
    transforms.with<rotate_y>(15);
    world.add(make_shared<transformed_geometry>(
        make_shared<ordered_transforms>(std::move(transforms)),
        boxes2.split()));

    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);

    auto start_ns = start.tv_sec * 1000000000ll + start.tv_nsec;
    auto end_ns = end.tv_sec * 1000000000ll + end.tv_nsec;
    auto total_ns = end_ns - start_ns;

    printf("Loading the scene took %lf us\n", double(total_ns) / 1000.0);

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
            random_spheres();
            break;
        case 2:
            two_spheres();
            break;
        case 3:
            earth();
            break;
        case 4:
            two_perlin_spheres();
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
            final_scene(400, 250, 4);
            break;
    }
}
