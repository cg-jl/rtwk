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

// NOTE: If ever going for raymarchin, consider a Voronoi LUT! This will tell
// the 'closest' object from a ray before considering it a hit. The layering is
// clear, too! Once the ray has got into a hit, then it can progress to the next
// 'layer' which is the objects that are inside of it.

#include <ctime>

#include "camera.h"
#include "collection/bvh.h"
#include "collection/list.h"
#include "geometry/box.h"
#include "geometry/constant_medium.h"
#include "geometry/quad.h"
#include "geometry/sphere.h"
#include "material.h"
#include "rtweekend.h"
#include "storage.h"
#include "texture.h"
#include "transform.h"

static bool enable_progress = true;

static void random_spheres() {
    poly_list world;

    auto checker =
        texture::checker(0.32, leak(texture::solid(color(.2, .3, .1))),
                         leak(texture::solid(color(.9, .9, .9))));

    world.add(leak(
        sphere(point3(0, -1000, 0), 1000, material::lambertian(), &checker)));

    auto dielectric = material::dielectric(1.5);

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_float();
            point3 center(float(a) + 0.9f * random_float(), 0.2f,
                          float(b) + 0.9f * random_float());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                if (choose_mat < 0.8) {
                    // diffuse
                    auto albedo = color::random() * color::random();
                    auto sphere_material = material::lambertian();
                    auto sphere_texture = texture::solid(albedo);
                    auto displacement = vec3(0, random_float(0, .5), 0);

                    world.add(leak(transformed_geometry(
                        transform::move(displacement),
                        leak(sphere(point3(center), 0.2, sphere_material,
                                    &sphere_texture)))));
                } else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_float(0, 0.5);
                    auto sphere_material = material::metal(fuzz);
                    world.add(leak(sphere(center, 0.2, sphere_material,
                                          leak(texture::solid(albedo)))));
                } else {
                    // glass
                    world.add(leak(sphere(center, 0.2, dielectric,
                                          leak(texture::solid(1, 1, 1)))));
                }
            }
        }
    }

    auto material1 = material::dielectric(1.5);
    world.add(leak(sphere(point3(0, 1, 0), 1.0, material1,
                          leak(texture::solid(1, 1, 1)))));

    auto material2 = material::lambertian();
    world.add(leak(sphere(point3(-4, 1, 0), 1.0, material2,
                          leak(texture::solid(.4, .2, .1)))));

    auto material3 = material::metal(0.0);
    world.add(leak(sphere(point3(4, 1, 0), 1.0, material3,
                          leak(texture::solid(0.7, .6, .5)))));

    auto const *scene = bvh::split_or_view(world);

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

    cam.render(*scene, enable_progress);
}

static void two_spheres() {
    poly_list world;

    auto checker = texture::checker(0.8, leak(texture::solid(.2, .3, .1)),
                                    leak(texture::solid(.9, .9, .9)));

    world.add(
        leak(sphere(point3(0, -10, 0), 10, material::lambertian(), &checker)));
    world.add(
        leak(sphere(point3(0, 10, 0), 10, material::lambertian(), &checker)));

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

    cam.render(world.finish(), enable_progress);
}

static void earth() {
    auto earth_texture = texture::image("earthmap.jpg");
    auto earth_surface = material::lambertian();
    auto globe = sphere(point3(0, 0, 0), 2, earth_surface, &earth_texture);

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

    auto sphere_view = view<sphere>(std::span{&globe, 1});

    cam.render(sphere_view, enable_progress);
}

static void two_perlin_spheres() {
    poly_list world;

    auto noise = std::make_unique<perlin>();

    auto pertext = texture::noise(4, noise.get());
    world.add(leak(
        sphere(point3(0, -1000, 0), 1000, material::lambertian(), &pertext)));
    world.add(
        leak(sphere(point3(0, 2, 0), 2, material::lambertian(), &pertext)));

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

    cam.render(world.finish(), enable_progress);
}

static void quads() {
    poly_list world;

    // Materials
    auto left_red = texture::solid(color(1.0, 0.2, 0.2));
    auto back_green = texture::solid(color(0.2, 1.0, 0.2));
    auto right_blue = texture::solid(color(0.2, 0.2, 1.0));
    auto upper_orange = texture::solid(color(1.0, 0.5, 0.0));
    auto lower_teal = texture::solid(color(0.2, 0.8, 0.8));

    auto all_mat = material::lambertian();

    // Quads
    world.add(leak(quad(point3(-3, -2, 5), vec3(0, 0, -4), vec3(0, 4, 0),
                        all_mat, &left_red)));
    world.add(leak(quad(point3(-2, -2, 0), vec3(4, 0, 0), vec3(0, 4, 0),
                        all_mat, &back_green)));
    world.add(leak(quad(point3(3, -2, 1), vec3(0, 0, 4), vec3(0, 4, 0), all_mat,
                        &right_blue)));
    world.add(leak(quad(point3(-2, 3, 1), vec3(4, 0, 0), vec3(0, 0, 4), all_mat,
                        &upper_orange)));
    world.add(leak(quad(point3(-2, -3, 5), vec3(4, 0, 0), vec3(0, 0, -4),
                        all_mat, &lower_teal)));

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

    cam.render(world.finish(), enable_progress);
}

static void simple_light() {
    poly_list world;

    auto noise = std::make_unique<perlin>();

    auto pertext = texture::noise(4, noise.get());
    world.add(leak(
        sphere(point3(0, -1000, 0), 1000, material::lambertian(), &pertext)));
    world.add(
        leak(sphere(point3(0, 2, 0), 2, material::lambertian(), &pertext)));

    auto difflight = material::diffuse_light();
    auto difflight_color = texture::solid(4, 4, 4);
    world.add(leak(sphere(point3(0, 7, 0), 2, difflight, &difflight_color)));
    world.add(leak(quad(point3(3, 1, -2), vec3(2, 0, 0), vec3(0, 2, 0),
                        difflight, &difflight_color)));

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

    cam.render(world.finish(), enable_progress);
}

static void cornell_box() {
    poly_list world;

    auto red = texture::solid(.65, .05, .05);
    auto white = texture::solid(.73, .73, .73);
    auto green = texture::solid(.12, .45, .15);
    auto light = material::diffuse_light();
    auto light_color = texture::solid(15, 15, 15);

    world.add(leak(quad(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555),
                        material::lambertian(), &green)));
    world.add(leak(quad(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555),
                        material::lambertian(), &red)));
    world.add(leak(quad(point3(343, 554, 332), vec3(-130, 0, 0),
                        vec3(0, 0, -105), light, &light_color)));
    world.add(leak(quad(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555),
                        material::lambertian(), &white)));
    world.add(leak(quad(point3(555, 555, 555), vec3(-555, 0, 0),
                        vec3(0, 0, -555), material::lambertian(), &white)));
    world.add(leak(quad(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0),
                        material::lambertian(), &white)));

    auto box1 = leak(box(point3(0, 0, 0), point3(165, 330, 165),
                         material::lambertian(), &white));

    world.add(leak(transformed_geometry(
        std::vector<transform>{transform::translate(vec3(265, 0, 295)),
                               transform::rotate_y(15)},

        box1)));

    auto box2 = leak(box(point3(0, 0, 0), point3(165, 165, 165),
                         material::lambertian(), &white));

    world.add(leak(transformed_geometry(
        std::vector<transform>{transform::translate(vec3(130, 0, 65)),
                               transform::rotate_y(-18)},
        box2)));

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

    cam.render(world.finish(), enable_progress);
}

static void cornell_smoke() {
    poly_list world;

    auto red = texture::solid(.65, .05, .05);
    auto white = texture::solid(.73, .73, .73);
    auto green = texture::solid(.12, .45, .15);
    auto light = texture::solid(7, 7, 7);

    world.add(leak(quad(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555),
                        material::lambertian(), &green)));
    world.add(leak(quad(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555),
                        material::lambertian(), &red)));
    world.add(leak(quad(point3(113, 554, 127), vec3(330, 0, 0), vec3(0, 0, 305),
                        material::diffuse_light(), &light)));
    world.add(leak(quad(point3(0, 555, 0), vec3(555, 0, 0), vec3(0, 0, 555),
                        material::lambertian(), &white)));
    world.add(leak(quad(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555),
                        material::lambertian(), &white)));
    world.add(leak(quad(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0),
                        material::lambertian(), &white)));

    hittable const *box1 = leak(box(point3(0, 0, 0), point3(165, 330, 165),
                                    material::lambertian(), &white));

    box1 = leak(transformed_geometry(

        std::vector<transform>{transform::translate(vec3(265, 0, 295)),
                               transform::rotate_y(15)},
        box1));

    hittable const *box2 = leak(box(point3(0, 0, 0), point3(165, 165, 165),
                                    material::lambertian(), &white));

    box2 = leak(transformed_geometry(
        std::vector<transform>{transform::translate(vec3(130, 0, 65)),
                               transform::rotate_y(-18)},
        box2));

    world.add(leak(constant_medium(box1, 0.01, leak(texture::solid(0, 0, 0)))));
    world.add(leak(constant_medium(box2, 0.01, leak(texture::solid(1, 1, 1)))));

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

    cam.render(world.finish(), enable_progress);
}

static void final_scene(int image_width, int samples_per_pixel, int max_depth) {
    struct timespec start;

    clock_gettime(CLOCK_MONOTONIC, &start);

    auto noise = std::make_unique<perlin>();

    list<box> boxes1;
    auto ground_col = texture::solid(0.48, 0.83, 0.53);
    auto ground = material::lambertian();

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

            boxes1.add(point3(x0, y0, z0), point3(x1, y1, z1), ground,
                       &ground_col);
        }
    }

    poly_list world;

    world.add(
        leak(hittable_collection<bvh::tree<box>>(bvh::must_split(boxes1))));

    auto light = material::diffuse_light();
    auto light_color = texture::solid(7, 7, 7);

    world.add(leak(quad(point3(123, 554, 147), vec3(300, 0, 0), vec3(0, 0, 265),
                        light, &light_color)));

    auto center = point3(400, 400, 200);
    auto sphere_material = material::lambertian();
    auto sphere_color = texture::solid(0.7, 0.3, 0.1);
    world.add(leak(transformed_geometry(
        transform::move(vec3(30, 0, 0)),
        leak(sphere(center, 50, sphere_material, &sphere_color)))));

    auto dielectric = material::dielectric(1.5);

    // NOTE: Lookuout for duplication of materials/colors!

    world.add(leak(sphere(point3(260, 150, 45), 50, dielectric,
                          leak(texture::solid(1, 1, 1)))));
    world.add(leak(sphere(point3(0, 150, 145), 50, material::metal(1.0),
                          leak(texture::solid(0.8, 0.8, 0.9)))));

    auto full_white = texture::solid(1);
    auto boundary =
        leak(sphere(point3(360, 150, 145), 70, dielectric, &full_white));
    // NOTE: This addition is necessary so that the medium is contained within
    // the boundary.
    // So these two are one (constant_medium) inside another (dielectric).
    world.add(boundary);
    world.add(leak(
        constant_medium(boundary, 0.2, leak(texture::solid(0.2, 0.4, 0.9)))));
    boundary = leak(sphere(point3(0, 0, 0), 5000, dielectric, &full_white));
    world.add(leak(constant_medium(boundary, .0001, &full_white)));

    auto emat = texture::image("earthmap.jpg");
    world.add(leak(
        sphere(point3(400, 200, 400), 100, material::lambertian(), &emat)));
    auto pertext = texture::noise(0.1, noise.get());
    world.add(leak(
        sphere(point3(220, 280, 300), 80, material::lambertian(), &pertext)));

    list<sphere> boxes2;
    auto white = texture::solid(.73, .73, .73);
    int ns = 1000;
    for (int j = 0; j < ns; j++) {
        boxes2.add(point3::random(0, 165), 10, material::lambertian(), &white);
    }

    world.add(
        leak(hittable_collection(transformed_collection<bvh::tree<sphere>>(
            std::vector<transform>{transform::translate(vec3(-100, 270, 395)),
                                   transform::rotate_y(15)},

            bvh::must_split(boxes2)))));

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

    cam.render(world.finish(), enable_progress);
}

int main(int argc, char const *argv[]) {
    for (int i = 0; i < argc; ++i) {
        if (strncmp(argv[i], "--disable-progress",
                    sizeof("--disable-progress") - 1) == 0) {
            fputs(
                "WARN: Progress reporting is disabled. Expect no output "
                "from here until the end\n",
                stderr);
            enable_progress = false;
        }
    }

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
            // NOTE: 4875 spp gives the same quality as 10k for 800x800, so why
            // bother adding more?
            final_scene(800, 4875, 40);
            break;
        default:
            /* final_scene(400, 250, 4); */
            final_scene(400, 250, 40);
            break;
    }
}
