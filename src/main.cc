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
#include "wrappers.h"

static bool enable_progress = true;

static void random_spheres() {
    poly_list world;
    tex_storage texes;

    auto checker = texes.checker(0.32, color(.2, .3, .1), color(.9, .9, .9));

    world.add(leak(tex_wrapper(sphere(point3(0, -1000, 0), 1000),
                               material::lambertian(), checker)));

    auto dielectric = material::dielectric(1.5);

    xform_builder xforms;

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
                    auto sphere_texture = texes.solid(albedo);
                    auto displacement = vec3(0, random_float(0, .5), 0);

                    xforms.move(displacement);
                    world.add(leak(transformed_hittable(
                        xforms.finish(),
                        tex_wrapper(sphere(point3(center), 0.2),
                                    sphere_material, sphere_texture))));
                } else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_float(0, 0.5);
                    auto sphere_material = material::metal(fuzz);
                    world.add(
                        leak(tex_wrapper(sphere(center, 0.2), sphere_material,
                                         texes.solid(albedo))));
                } else {
                    // glass
                    world.add(leak(tex_wrapper(sphere(center, 0.2), dielectric,
                                               texes.solid(1, 1, 1))));
                }
            }
        }
    }

    auto material1 = material::dielectric(1.5);
    world.add(leak(tex_wrapper(sphere(point3(0, 1, 0), 1.0), material1,
                               texes.solid(1, 1, 1))));

    auto material2 = material::lambertian();
    world.add(leak(tex_wrapper(sphere(point3(-4, 1, 0), 1.0), material2,
                               texes.solid(.4, .2, .1))));

    auto material3 = material::metal(0.0);
    world.add(leak(tex_wrapper(sphere(point3(4, 1, 0), 1.0), material3,
                               texes.solid(0.7, .6, .5))));

    auto const scene = bvh::split_or_view(world);

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

    cam.render(scene, enable_progress, texes.view());
}

static void two_spheres() {
    poly_list world;

    tex_storage texes;

    auto checker = texes.checker(0.8, color(.2, .3, .1), color(.9, .9, .9));

    world.add(leak(tex_wrapper(sphere(point3(0, -10, 0), 10),
                               material::lambertian(), checker)));
    world.add(leak(tex_wrapper(sphere(point3(0, 10, 0), 10),
                               material::lambertian(), checker)));

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

    cam.render(world.finish(), enable_progress, texes.view());
}

static void earth() {
    tex_storage texes;
    auto earth_texture = texes.image("earthmap.jpg");
    auto earth_surface = material::lambertian();

    list<tex_wrapper<sphere>> world;

    // globe
    world.add(sphere(point3(0, 0, 0), 2), earth_surface, earth_texture);

    // light
    world.add(sphere(point3(0, 0, 0), -100), material::diffuse_light(),
              texes.solid(.8, .5, .1));

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

    cam.render(world.finish(), enable_progress, texes.view());
}

static void two_perlin_spheres() {
    poly_list world;
    tex_storage texes;

    auto pertext = texes.noise(4);
    world.add(leak(tex_wrapper(sphere(point3(0, -1000, 0), 1000),
                               material::lambertian(), pertext)));
    world.add(leak(tex_wrapper(sphere(point3(0, 2, 0), 2),
                               material::lambertian(), pertext)));

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

    cam.render(world.finish(), enable_progress, texes.view());
}

static void quads() {
    poly_list world;
    tex_storage texes;

    // Materials
    auto left_red = texes.solid(color(1.0, 0.2, 0.2));
    auto back_green = texes.solid(color(0.2, 1.0, 0.2));
    auto right_blue = texes.solid(color(0.2, 0.2, 1.0));
    auto upper_orange = texes.solid(color(1.0, 0.5, 0.0));
    auto lower_teal = texes.solid(color(0.2, 0.8, 0.8));

    auto all_mat = material::lambertian();

    // Quads
    world.add(
        leak(tex_wrapper(quad(point3(-3, -2, 5), vec3(0, 0, -4), vec3(0, 4, 0)),
                         all_mat, left_red)));
    world.add(
        leak(tex_wrapper(quad(point3(-2, -2, 0), vec3(4, 0, 0), vec3(0, 4, 0)),
                         all_mat, back_green)));
    world.add(
        leak(tex_wrapper(quad(point3(3, -2, 1), vec3(0, 0, 4), vec3(0, 4, 0)),
                         all_mat, right_blue)));
    world.add(
        leak(tex_wrapper(quad(point3(-2, 3, 1), vec3(4, 0, 0), vec3(0, 0, 4)),
                         all_mat, upper_orange)));
    world.add(
        leak(tex_wrapper(quad(point3(-2, -3, 5), vec3(4, 0, 0), vec3(0, 0, -4)),
                         all_mat, lower_teal)));

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

    cam.render(world.finish(), enable_progress, texes.view());
}

static void simple_light() {
    poly_list world;
    tex_storage texes;

    auto pertext = texes.noise(4);
    world.add(leak(tex_wrapper(sphere(point3(0, -1000, 0), 1000),
                               material::lambertian(), pertext)));
    world.add(leak(tex_wrapper(sphere(point3(0, 2, 0), 2),
                               material::lambertian(), pertext)));

    auto difflight = material::diffuse_light();
    auto difflight_color = texes.solid(4, 4, 4);
    world.add(leak(
        tex_wrapper(sphere(point3(0, 7, 0), 2), difflight, difflight_color)));
    world.add(
        leak(tex_wrapper(quad(point3(3, 1, -2), vec3(2, 0, 0), vec3(0, 2, 0)),
                         difflight, difflight_color)));

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

    cam.render(world.finish(), enable_progress, texes.view());
}

static void cornell_box() {
    poly_list world;
    tex_storage texes;

    auto red = texes.solid(.65, .05, .05);
    auto white = texes.solid(.73, .73, .73);
    auto green = texes.solid(.12, .45, .15);
    auto light = material::diffuse_light();
    auto light_color = texes.solid(15, 15, 15);

    world.add(leak(
        tex_wrapper(quad(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555)),
                    material::lambertian(), green)));
    world.add(leak(
        tex_wrapper(quad(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555)),
                    material::lambertian(), red)));
    world.add(leak(tex_wrapper(
        quad(point3(343, 554, 332), vec3(-130, 0, 0), vec3(0, 0, -105)), light,
        light_color)));
    world.add(leak(
        tex_wrapper(quad(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555)),
                    material::lambertian(), white)));
    world.add(leak(tex_wrapper(
        quad(point3(555, 555, 555), vec3(-555, 0, 0), vec3(0, 0, -555)),
        material::lambertian(), white)));
    world.add(leak(
        tex_wrapper(quad(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0)),
                    material::lambertian(), white)));

    xform_builder xforms;
    xforms.translate(vec3(265, 0, 295)), xforms.rotate_y(15);
    world.add(leak(transformed_hittable(
        xforms.finish(), tex_wrapper(

                             box(point3(0, 0, 0), point3(165, 330, 165)),
                             material::lambertian(), white))));

    xforms.translate(vec3(130, 0, 65)), xforms.rotate_y(-18);
    world.add(leak(

        transformed_hittable(
            xforms.finish(),
            tex_wrapper((box(point3(0, 0, 0), point3(165, 165, 165))),
                        material::lambertian(), white))));

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

    cam.render(world.finish(), enable_progress, texes.view());
}

static void cornell_smoke() {
    poly_list world;
    tex_storage texes;

    auto red = texes.solid(.65, .05, .05);
    auto white = texes.solid(.73, .73, .73);
    auto green = texes.solid(.12, .45, .15);
    auto light = texes.solid(7, 7, 7);

    world.add(leak(
        tex_wrapper(quad(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555)),
                    material::lambertian(), green)));
    world.add(leak(
        tex_wrapper(quad(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555)),
                    material::lambertian(), red)));
    world.add(leak(tex_wrapper(
        quad(point3(113, 554, 127), vec3(330, 0, 0), vec3(0, 0, 305)),
        material::diffuse_light(), light)));
    world.add(leak(
        tex_wrapper(quad(point3(0, 555, 0), vec3(555, 0, 0), vec3(0, 0, 555)),
                    material::lambertian(), white)));
    world.add(leak(
        tex_wrapper(quad(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555)),
                    material::lambertian(), white)));
    world.add(leak(
        tex_wrapper(quad(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0)),
                    material::lambertian(), white)));

    xform_builder xforms;
    xforms.translate(vec3(265, 0, 295)), xforms.rotate_y(15);
    world.add(leak(

        transformed_hittable(
            xforms.finish(),
            constant_medium(box(point3(0, 0, 0), point3(165, 330, 165)), 0.01,
                            color(0, 0, 0), texes))));
    xforms.translate(vec3(130, 0, 65)), xforms.rotate_y(-18);
    world.add(leak(transformed_hittable(
        xforms.finish(),
        constant_medium(box(point3(0, 0, 0), point3(165, 165, 165)), 0.01,
                        color(1, 1, 1), texes))));

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

    cam.render(world.finish(), enable_progress, texes.view());
}

static void final_scene(int image_width, int samples_per_pixel, int max_depth) {
    struct timespec start;
    tex_storage texes;

    clock_gettime(CLOCK_MONOTONIC, &start);

    std::vector<box> boxes1;
    auto ground_col = texes.solid(0.48, 0.83, 0.53);
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

            boxes1.emplace_back(point3(x0, y0, z0), point3(x1, y1, z1));
        }
    }

    poly_list world;

    world.add(leak(hittable_collection(
        tex_wrapper(bvh::over(bvh::must_split(std::span<box>(boxes1)),
                              std::span<box const>(boxes1)),
                    ground, ground_col))));

    auto light = material::diffuse_light();
    auto light_color = texes.solid(7, 7, 7);

    world.add(leak(tex_wrapper(
        quad(point3(123, 554, 147), vec3(300, 0, 0), vec3(0, 0, 265)), light,
        light_color)));

    auto center = point3(400, 400, 200);
    auto sphere_material = material::lambertian();
    auto sphere_color = texes.solid(0.7, 0.3, 0.1);
    xform_builder xforms;
    xforms.move(vec3(30, 0, 0));
    world.add(leak(transformed_hittable(
        xforms.finish(),
        tex_wrapper(sphere(center, 50), sphere_material, sphere_color))));

    auto dielectric = material::dielectric(1.5);

    // NOTE: Lookuout for duplication of materials/colors!
    list<tex_wrapper<sphere>> wrapped_spheres;

    wrapped_spheres.add(sphere(point3(260, 150, 45), 50), dielectric,
                        texes.solid(1, 1, 1));
    wrapped_spheres.add(sphere(point3(0, 150, 145), 50), material::metal(1.0),
                        texes.solid(0.8, 0.8, 0.9));

    auto full_white = texes.solid(1);
    sphere boundary_geom(point3(360, 150, 145), 70);
    // NOTE: This addition is necessary so that the medium is contained within
    // the boundary.
    // So these two are one (constant_medium) inside another (dielectric).
    wrapped_spheres.add(boundary_geom, dielectric, full_white);

    world.add(
        leak(constant_medium(boundary_geom, 0.2, color(0.2, 0.4, 0.9), texes)));
    world.add(leak(constant_medium(sphere(point3(0, 0, 0), 5000), .0001,
                                   color(1), texes)));

    id_storage<rtw_image> images;
    auto emat = texes.image("earthmap.jpg");
    wrapped_spheres.add(sphere(point3(400, 200, 400), 100),
                        material::lambertian(), emat);
    auto pertext = texes.noise(0.1);
    wrapped_spheres.add(sphere(point3(220, 280, 300), 80),
                        material::lambertian(), pertext);

    std::vector<sphere> box_of_spheres;
    auto white = texes.solid(.73, .73, .73);
    int ns = 1000;
    box_of_spheres.reserve(ns);
    for (int j = 0; j < ns; j++) {
        box_of_spheres.emplace_back(point3::random(0, 165), 10);
    }

    xforms.translate(vec3(-100, 270, 395)), xforms.rotate_y(15);

    world.add(leak(transformed_hittable(
        xforms.finish(),
        hittable_collection(tex_wrapper(
            bvh::over(bvh::must_split(std::span<sphere>(box_of_spheres)),
                      std::span<sphere const>(box_of_spheres)),
            material::lambertian(), white)))));

    // FIXME: There's some bug in propagation that makes the spheres up there ^
    // appear before the perlin-textured sphere. Maybe it's something related to
    // the BVH, or maybe it's something that makes propagation not
    // order-independent. In any case, it's a bug.
    world.add(leak(hittable_collection(wrapped_spheres.finish())));

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

    cam.render(world.finish(), enable_progress, texes.view());
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
