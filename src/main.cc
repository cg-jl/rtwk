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

#include "bvh.h"
#include "camera.h"
#include "constant_medium.h"
#include "hittable.h"
#include "hittable_list.h"
#include "material.h"
#include "quad.h"
#include "rtweekend.h"
#include "sphere.h"
#include "texture.h"

void bouncing_spheres() {
    hittable_list world;

    auto checker =
        new checker_texture(0.32, color(.2, .3, .1), color(.9, .9, .9));
    world.add(new sphere(point3(0, -1000, 0), 1000, new lambertian(checker)));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9 * random_double(), 0.2,
                          b + 0.9 * random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                material *sphere_material;

                if (choose_mat < 0.8) {
                    // diffuse
                    auto albedo = color::random() * color::random();
                    sphere_material = new lambertian(albedo);
                    auto center2 = center + vec3(0, random_double(0, .5), 0);
                    world.add(
                        new sphere(center, center2, 0.2, sphere_material));
                } else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    sphere_material = new metal(albedo, fuzz);
                    world.add(new sphere(center, 0.2, sphere_material));
                } else {
                    // glass
                    sphere_material = new dielectric(1.5);
                    world.add(new sphere(center, 0.2, sphere_material));
                }
            }
        }
    }

    auto material1 = new dielectric(1.5);
    world.add(new sphere(point3(0, 1, 0), 1.0, material1));

    auto material2 = new lambertian(color(0.4, 0.2, 0.1));
    world.add(new sphere(point3(-4, 1, 0), 1.0, material2));

    auto material3 = new metal(color(0.7, 0.6, 0.5), 0.0);
    world.add(new sphere(point3(4, 1, 0), 1.0, material3));

    world = hittable_list(new bvh_node(world));

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

    cam.render(world);
}

void checkered_spheres() {
    hittable_list world;

    auto checker =
        new checker_texture(0.32, color(.2, .3, .1), color(.9, .9, .9));

    world.add(new sphere(point3(0, -10, 0), 10, new lambertian(checker)));
    world.add(new sphere(point3(0, 10, 0), 10, new lambertian(checker)));

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
    auto earth_texture = new image_texture("earthmap.jpg");
    auto earth_surface = new lambertian(earth_texture);
    auto globe = new sphere(point3(0, 0, 0), 2, earth_surface);

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

    auto pertext = new noise_texture(4);
    world.add(new sphere(point3(0, -1000, 0), 1000, new lambertian(pertext)));
    world.add(new sphere(point3(0, 2, 0), 2, new lambertian(pertext)));

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
    auto left_red = new lambertian(color(1.0, 0.2, 0.2));
    auto back_green = new lambertian(color(0.2, 1.0, 0.2));
    auto right_blue = new lambertian(color(0.2, 0.2, 1.0));
    auto upper_orange = new lambertian(color(1.0, 0.5, 0.0));
    auto lower_teal = new lambertian(color(0.2, 0.8, 0.8));

    // Quads
    world.add(
        new quad(point3(-3, -2, 5), vec3(0, 0, -4), vec3(0, 4, 0), left_red));
    world.add(
        new quad(point3(-2, -2, 0), vec3(4, 0, 0), vec3(0, 4, 0), back_green));
    world.add(
        new quad(point3(3, -2, 1), vec3(0, 0, 4), vec3(0, 4, 0), right_blue));
    world.add(
        new quad(point3(-2, 3, 1), vec3(4, 0, 0), vec3(0, 0, 4), upper_orange));
    world.add(
        new quad(point3(-2, -3, 5), vec3(4, 0, 0), vec3(0, 0, -4), lower_teal));

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

    auto pertext = new noise_texture(4);
    world.add(new sphere(point3(0, -1000, 0), 1000, new lambertian(pertext)));
    world.add(new sphere(point3(0, 2, 0), 2, new lambertian(pertext)));

    auto difflight = new diffuse_light(color(4, 4, 4));
    world.add(new sphere(point3(0, 7, 0), 2, difflight));
    world.add(
        new quad(point3(3, 1, -2), vec3(2, 0, 0), vec3(0, 2, 0), difflight));

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

    auto red = new lambertian(color(.65, .05, .05));
    auto white = new lambertian(color(.73, .73, .73));
    auto green = new lambertian(color(.12, .45, .15));
    auto light = new diffuse_light(color(15, 15, 15));

    world.add(
        new quad(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), green));
    world.add(new quad(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), red));
    world.add(new quad(point3(343, 554, 332), vec3(-130, 0, 0),
                       vec3(0, 0, -105), light));
    world.add(
        new quad(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555), white));
    world.add(new quad(point3(555, 555, 555), vec3(-555, 0, 0),
                       vec3(0, 0, -555), white));
    world.add(
        new quad(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0), white));

    hittable *box1 = box(point3(0, 0, 0), point3(165, 330, 165), white);
    box1 = new rotate_y(box1, 15);
    box1 = new translate(box1, vec3(265, 0, 295));
    world.add(box1);

    hittable *box2 = box(point3(0, 0, 0), point3(165, 165, 165), white);
    box2 = new rotate_y(box2, -18);
    box2 = new translate(box2, vec3(130, 0, 65));
    world.add(box2);

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

    auto red = new lambertian(color(.65, .05, .05));
    auto white = new lambertian(color(.73, .73, .73));
    auto green = new lambertian(color(.12, .45, .15));
    auto light = new diffuse_light(color(7, 7, 7));

    world.add(
        new quad(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), green));
    world.add(new quad(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), red));
    world.add(new quad(point3(113, 554, 127), vec3(330, 0, 0), vec3(0, 0, 305),
                       light));
    world.add(
        new quad(point3(0, 555, 0), vec3(555, 0, 0), vec3(0, 0, 555), white));
    world.add(
        new quad(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555), white));
    world.add(
        new quad(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0), white));

    hittable *box1 = box(point3(0, 0, 0), point3(165, 330, 165), white);
    box1 = new rotate_y(box1, 15);
    box1 = new translate(box1, vec3(265, 0, 295));

    hittable *box2 = box(point3(0, 0, 0), point3(165, 165, 165), white);
    box2 = new rotate_y(box2, -18);
    box2 = new translate(box2, vec3(130, 0, 65));

    world.add(new constant_medium(box1, 0.01, color(0, 0, 0)));
    world.add(new constant_medium(box2, 0.01, color(1, 1, 1)));

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
    hittable_list boxes1;
    auto ground = new lambertian(color(0.48, 0.83, 0.53));

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

            boxes1.add(box(point3(x0, y0, z0), point3(x1, y1, z1), ground));
        }
    }

    hittable_list world;

    world.add(new bvh_node(boxes1));

    auto light = new diffuse_light(color(7, 7, 7));
    world.add(new quad(point3(123, 554, 147), vec3(300, 0, 0), vec3(0, 0, 265),
                       light));

    auto center1 = point3(400, 400, 200);
    auto center2 = center1 + vec3(30, 0, 0);
    auto sphere_material = new lambertian(color(0.7, 0.3, 0.1));
    world.add(new sphere(center1, center2, 50, sphere_material));

    world.add(new sphere(point3(260, 150, 45), 50, new dielectric(1.5)));
    world.add(new sphere(point3(0, 150, 145), 50,
                         new metal(color(0.8, 0.8, 0.9), 1.0)));

    auto boundary = new sphere(point3(360, 150, 145), 70, new dielectric(1.5));
    world.add(boundary);
    world.add(new constant_medium(boundary, 0.2, color(0.2, 0.4, 0.9)));
    boundary = new sphere(point3(0, 0, 0), 5000, new dielectric(1.5));
    world.add(new constant_medium(boundary, .0001, color(1, 1, 1)));

    auto emat = new lambertian(new image_texture("earthmap.jpg"));
    world.add(new sphere(point3(400, 200, 400), 100, emat));
    auto pertext = new noise_texture(0.2);
    world.add(new sphere(point3(220, 280, 300), 80, new lambertian(pertext)));

    hittable_list boxes2;
    auto white = new lambertian(color(.73, .73, .73));
    int ns = 1000;
    for (int j = 0; j < ns; j++) {
        boxes2.add(new sphere(point3::random(0, 165), 10, white));
    }

    world.add(new translate(new rotate_y(new bvh_node(boxes2), 15),
                            vec3(-100, 270, 395)));

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
            final_scene(400, 250, 4);
            break;
    }
}
