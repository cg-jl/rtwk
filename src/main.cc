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

    typed_storage<texture> tex_storage;
    poly_storage<hittable> hit_storage;

    auto checker = texture::checker(
        0.32, tex_storage.make(texture::solid(color(.2, .3, .1))),
        tex_storage.make(texture::solid(color(.9, .9, .9))));

    world.add(hit_storage.make<sphere>(point3(0, -1000, 0), 1000,
                                       material::lambertian(), &checker));

    poly_storage<material> mat_storage;

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

                    world.add(hit_storage.make<transformed_geometry>(
                        transform::move(displacement),
                        hit_storage.make<sphere>(point3(center), 0.2,
                                                 sphere_material,
                                                 &sphere_texture)));
                } else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_float(0, 0.5);
                    auto sphere_material = material::metal(fuzz);
                    world.add(hit_storage.make<sphere>(
                        center, 0.2, sphere_material,
                        tex_storage.make(texture::solid(albedo))));
                } else {
                    // glass
                    world.add(hit_storage.make<sphere>(
                        center, 0.2, dielectric,
                        tex_storage.make(texture::solid(1, 1, 1))));
                }
            }
        }
    }

    auto material1 = material::dielectric(1.5);
    world.add(
        hit_storage.make<sphere>(point3(0, 1, 0), 1.0, material1,
                                 tex_storage.make(texture::solid(1, 1, 1))));

    auto material2 = material::lambertian();
    world.add(
        hit_storage.make<sphere>(point3(-4, 1, 0), 1.0, material2,
                                 tex_storage.make(texture::solid(.4, .2, .1))));

    auto material3 = material::metal(0.0);
    world.add(hit_storage.make<sphere>(
        point3(4, 1, 0), 1.0, material3,
        tex_storage.make(texture::solid(0.7, .6, .5))));

    poly_storage<collection> coll_storage;

    auto const *scene = bvh::split_or_view(world, coll_storage);

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

    cam.render(hittable_collection(scene), enable_progress);
}

static void two_spheres() {
    poly_list world;

    poly_storage<hittable> hit_storage;
    typed_storage<texture> tex_storage;

    auto checker =
        texture::checker(0.8, tex_storage.make(texture::solid(.2, .3, .1)),
                         tex_storage.make(texture::solid(.9, .9, .9)));

    world.add(hit_storage.make<sphere>(point3(0, -10, 0), 10,
                                       material::lambertian(), &checker));
    world.add(hit_storage.make<sphere>(point3(0, 10, 0), 10,
                                       material::lambertian(), &checker));

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

    poly_storage<collection> coll_storage;

    cam.render(hittable_collection(coll_storage.move(world.finish())),
               enable_progress);
}

static void earth() {
    poly_storage<hittable> hit_storage;

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

    cam.render(hittable_collection(&sphere_view), enable_progress);
}

static void two_perlin_spheres() {
    poly_list world;

    poly_storage<hittable> hit_storage;

    auto noise = std::make_unique<perlin>();

    auto pertext = texture::noise(4, noise.get());
    world.add(hit_storage.make<sphere>(point3(0, -1000, 0), 1000,
                                       material::lambertian(), &pertext));
    world.add(hit_storage.make<sphere>(point3(0, 2, 0), 2,
                                       material::lambertian(), &pertext));

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

    auto world_view = world.finish();

    cam.render(hittable_collection(&world_view), enable_progress);
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

    poly_storage<hittable> hit_storage;

    // Quads
    world.add(hit_storage.make<quad>(point3(-3, -2, 5), vec3(0, 0, -4),
                                     vec3(0, 4, 0), all_mat, &left_red));
    world.add(hit_storage.make<quad>(point3(-2, -2, 0), vec3(4, 0, 0),
                                     vec3(0, 4, 0), all_mat, &back_green));
    world.add(hit_storage.make<quad>(point3(3, -2, 1), vec3(0, 0, 4),
                                     vec3(0, 4, 0), all_mat, &right_blue));
    world.add(hit_storage.make<quad>(point3(-2, 3, 1), vec3(4, 0, 0),
                                     vec3(0, 0, 4), all_mat, &upper_orange));
    world.add(hit_storage.make<quad>(point3(-2, -3, 5), vec3(4, 0, 0),
                                     vec3(0, 0, -4), all_mat, &lower_teal));

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

    auto world_view = world.finish();
    cam.render(hittable_collection(&world_view), enable_progress);
}

static void simple_light() {
    poly_list world;

    poly_storage<hittable> hit_storage;

    auto noise = std::make_unique<perlin>();

    auto pertext = texture::noise(4, noise.get());
    world.add(hit_storage.make<sphere>(point3(0, -1000, 0), 1000,
                                       material::lambertian(), &pertext));
    world.add(hit_storage.make<sphere>(point3(0, 2, 0), 2,
                                       material::lambertian(), &pertext));

    auto difflight = material::diffuse_light();
    auto difflight_color = texture::solid(4, 4, 4);
    world.add(hit_storage.make<sphere>(point3(0, 7, 0), 2, difflight,
                                       &difflight_color));
    world.add(hit_storage.make<quad>(point3(3, 1, -2), vec3(2, 0, 0),
                                     vec3(0, 2, 0), difflight,
                                     &difflight_color));

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

    auto world_view = world.finish();

    cam.render(hittable_collection(&world_view), enable_progress);
}

static void cornell_box() {
    poly_list world;

    auto red = texture::solid(.65, .05, .05);
    auto white = texture::solid(.73, .73, .73);
    auto green = texture::solid(.12, .45, .15);
    auto light = material::diffuse_light();
    auto light_color = texture::solid(15, 15, 15);

    poly_storage<hittable> hit_storage;

    world.add(hit_storage.make<quad>(point3(555, 0, 0), vec3(0, 555, 0),
                                     vec3(0, 0, 555), material::lambertian(),
                                     &green));
    world.add(hit_storage.make<quad>(point3(0, 0, 0), vec3(0, 555, 0),
                                     vec3(0, 0, 555), material::lambertian(),
                                     &red));
    world.add(hit_storage.make<quad>(point3(343, 554, 332), vec3(-130, 0, 0),
                                     vec3(0, 0, -105), light, &light_color));
    world.add(hit_storage.make<quad>(point3(0, 0, 0), vec3(555, 0, 0),
                                     vec3(0, 0, 555), material::lambertian(),
                                     &white));
    world.add(hit_storage.make<quad>(point3(555, 555, 555), vec3(-555, 0, 0),
                                     vec3(0, 0, -555), material::lambertian(),
                                     &white));
    world.add(hit_storage.make<quad>(point3(0, 0, 555), vec3(555, 0, 0),
                                     vec3(0, 555, 0), material::lambertian(),
                                     &white));

    poly_storage<collection> coll_storage;

    auto box1 = hit_storage.make<box>(point3(0, 0, 0), point3(165, 330, 165),
                                      material::lambertian(), &white);

    world.add(hit_storage.make<transformed_geometry>(
        std::vector<transform>{transform::translate(vec3(265, 0, 295)),
                               transform::rotate_y(15)},

        box1));

    auto box2 = hit_storage.make<box>(point3(0, 0, 0), point3(165, 165, 165),
                                      material::lambertian(), &white);

    world.add(hit_storage.make<transformed_geometry>(
        std::vector<transform>{transform::translate(vec3(130, 0, 65)),
                               transform::rotate_y(-18)},
        box2));

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

    auto world_view = world.finish();

    cam.render(hittable_collection(&world_view), enable_progress);
}

static void cornell_smoke() {
    poly_list world;
    typed_storage<texture> tex_storage;

    auto red = texture::solid(.65, .05, .05);
    auto white = texture::solid(.73, .73, .73);
    auto green = texture::solid(.12, .45, .15);
    auto light = texture::solid(7, 7, 7);

    poly_storage<hittable> hit_storage;

    world.add(hit_storage.make<quad>(point3(555, 0, 0), vec3(0, 555, 0),
                                     vec3(0, 0, 555), material::lambertian(),
                                     &green));
    world.add(hit_storage.make<quad>(point3(0, 0, 0), vec3(0, 555, 0),
                                     vec3(0, 0, 555), material::lambertian(),
                                     &red));
    world.add(hit_storage.make<quad>(point3(113, 554, 127), vec3(330, 0, 0),
                                     vec3(0, 0, 305), material::diffuse_light(),
                                     &light));
    world.add(hit_storage.make<quad>(point3(0, 555, 0), vec3(555, 0, 0),
                                     vec3(0, 0, 555), material::lambertian(),
                                     &white));
    world.add(hit_storage.make<quad>(point3(0, 0, 0), vec3(555, 0, 0),
                                     vec3(0, 0, 555), material::lambertian(),
                                     &white));
    world.add(hit_storage.make<quad>(point3(0, 0, 555), vec3(555, 0, 0),
                                     vec3(0, 555, 0), material::lambertian(),
                                     &white));
    poly_storage<collection> coll_storage;

    auto box1 = hit_storage.make<box>(point3(0, 0, 0), point3(165, 330, 165),
                                      material::lambertian(), &white);

    box1 = hit_storage.make<transformed_geometry>(

        std::vector<transform>{transform::translate(vec3(265, 0, 295)),
                               transform::rotate_y(15)},
        box1);

    auto box2 = hit_storage.make<box>(point3(0, 0, 0), point3(165, 165, 165),
                                      material::lambertian(), &white);

    box2 = hit_storage.make<transformed_geometry>(
        std::vector<transform>{transform::translate(vec3(130, 0, 65)),
                               transform::rotate_y(-18)},
        box2);

    world.add(hit_storage.make<constant_medium>(
        box1, 0.01, tex_storage.make(texture::solid(0, 0, 0))));
    world.add(hit_storage.make<constant_medium>(
        box2, 0.01, tex_storage.make(texture::solid(1, 1, 1))));

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

    auto view = world.finish();

    cam.render(hittable_collection(&view), enable_progress);
}

static void final_scene(int image_width, int samples_per_pixel, int max_depth) {
    struct timespec start;

    clock_gettime(CLOCK_MONOTONIC, &start);

    auto noise = std::make_unique<perlin>();

    list<box> boxes1;
    auto ground_col = texture::solid(0.48, 0.83, 0.53);
    auto ground = material::lambertian();

    poly_storage<hittable> hit_storage;
    poly_storage<collection> coll_storage;

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

    world.add(hit_storage.make<hittable_collection>(
        bvh::split_or_view(boxes1, coll_storage)));

    auto light = material::diffuse_light();
    auto light_color = texture::solid(7, 7, 7);

    world.add(hit_storage.make<quad>(point3(123, 554, 147), vec3(300, 0, 0),
                                     vec3(0, 0, 265), light, &light_color));

    auto center = point3(400, 400, 200);
    auto sphere_material = material::lambertian();
    auto sphere_color = texture::solid(0.7, 0.3, 0.1);
    world.add(hit_storage.make<transformed_geometry>(
        transform::move(vec3(30, 0, 0)),
        hit_storage.make<sphere>(center, 50, sphere_material, &sphere_color)));

    typed_storage<texture> tex_storage;

    auto dielectric = material::dielectric(1.5);

    // NOTE: Lookuout for duplication of materials/colors!

    world.add(
        hit_storage.make<sphere>(point3(260, 150, 45), 50, dielectric,
                                 tex_storage.make(texture::solid(1, 1, 1))));
    world.add(hit_storage.make<sphere>(
        point3(0, 150, 145), 50, material::metal(1.0),
        tex_storage.make(texture::solid(0.8, 0.8, 0.9))));

    auto full_white = texture::solid(1);
    auto boundary = hit_storage.make<sphere>(point3(360, 150, 145), 70,
                                             dielectric, &full_white);
    world.add(boundary);
    world.add(hit_storage.make<constant_medium>(
        boundary, 0.2, tex_storage.make(texture::solid(0.2, 0.4, 0.9))));
    boundary = hit_storage.make<sphere>(point3(0, 0, 0), 5000, dielectric,
                                        &full_white);
    world.add(hit_storage.make<constant_medium>(boundary, .0001, &full_white));

    auto emat = texture::image("earthmap.jpg");
    world.add(hit_storage.make<sphere>(point3(400, 200, 400), 100,
                                       material::lambertian(), &emat));
    auto pertext = texture::noise(0.1, noise.get());
    world.add(hit_storage.make<sphere>(point3(220, 280, 300), 80,
                                       material::lambertian(), &pertext));

    list<sphere> boxes2;
    auto white = texture::solid(.73, .73, .73);
    int ns = 1000;
    for (int j = 0; j < ns; j++) {
        boxes2.add(point3::random(0, 165), 10, material::lambertian(), &white);
    }

    world.add(hit_storage.make<hittable_collection>(
        coll_storage.make<transformed_collection>(
            std::vector<transform>{transform::translate(vec3(-100, 270, 395)),
                                   transform::rotate_y(15)},

            // NOTE: Maybe it's interesting to shift to a
            // `transformed_collection`, so we can have only one set of
            // transforms per collection. Maybe force it, like I said in
            // `collection.h`.
            bvh::split_or_view(boxes2, coll_storage))));

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

    auto view = world.finish();

    cam.render(hittable_collection(&view), enable_progress);
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

    switch (9) {
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
