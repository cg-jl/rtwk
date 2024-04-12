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
#include "hittable.h"
#include "material.h"
#include "rtweekend.h"
#include "soa.h"
#include "storage.h"
#include "texture.h"
#include "transform.h"
#include "wrappers.h"

#ifdef TRACY_ENABLE
void* operator new(size_t size) {
    void* ptr = malloc(size);
    TracyAlloc(ptr, size);
    return ptr;
}

void operator delete(void* ptr, size_t size) {
    TracyFree(ptr);
    free(ptr);
}
#endif

static bool enable_progress = true;
static constexpr auto num_threads = 12;

static void random_spheres() {
    tex_storage texes;

    auto checker = texes.checker(0.32, color(.2, .3, .1), color(.9, .9, .9));

    list<tex_wrapper<sphere>> stationary_spheres;
    stationary_spheres.add(sphere(point3(0, -1000, 0), 1000),
                           material::lambertian(), checker);
    // light
    stationary_spheres.add(sphere(point3(0, 0, 0), 10000),
                           material::diffuse_light(), texes.solid(.7));

    auto dielectric = material::dielectric(1.5);

    list<transformed<tex_wrapper<sphere>>> moving_spheres;

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
                    moving_spheres.add(
                        xforms.finish(),
                        tex_wrapper(sphere(point3(center), 0.2),
                                    sphere_material, sphere_texture));
                } else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_float(0, 0.5);
                    auto sphere_material = material::metal(fuzz);
                    stationary_spheres.add(tex_wrapper(sphere(center, 0.2),
                                                       sphere_material,
                                                       texes.solid(albedo)));
                } else {
                    // glass
                    stationary_spheres.add(
                        (tex_wrapper(sphere(center, 0.2), dielectric,
                                     texes.solid(1, 1, 1))));
                }
            }
        }
    }

    auto material1 = material::dielectric(1.5);
    stationary_spheres.add(tex_wrapper(sphere(point3(0, 1, 0), 1.0), material1,
                                       texes.solid(1, 1, 1)));

    auto material2 = material::lambertian();
    stationary_spheres.add(tex_wrapper(sphere(point3(-4, 1, 0), 1.0), material2,
                                       texes.solid(.4, .2, .1)));

    auto material3 = material::metal(0.0);
    stationary_spheres.add(tex_wrapper(sphere(point3(4, 1, 0), 1.0), material3,
                                       texes.solid(0.7, .6, .5)));

    bvh::builder bvh_builder;
    auto stationary_bvh =
        bvh::over(bvh::must_split(stationary_spheres.span(), bvh_builder),
                  stationary_spheres.finish());
    auto moving_bvh =
        bvh::over(bvh::must_split(moving_spheres.span(), bvh_builder),
                  moving_spheres.finish());

    auto world = tuple_wrapper<decltype(stationary_bvh), decltype(moving_bvh)>(
        std::move(stationary_bvh), std::move(moving_bvh));

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

    cam.start(world, enable_progress, texes.view(), num_threads,
              bvh_builder.lock());
}

static void two_spheres() {
    list<sphere> world;

    tex_storage texes;

    auto checker = texes.checker(0.8, color(.2, .3, .1), color(.9, .9, .9));

    world.add(sphere(point3(0, -10, 0), 10));
    world.add(sphere(point3(0, 10, 0), 10));

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

    cam.start(time_wrapper(
                  tex_wrapper(world.finish(), material::lambertian(), checker)),
              enable_progress, texes.view(), num_threads, {});
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

    cam.start(time_wrapper(world.finish()), enable_progress, texes.view(),
              num_threads, {});
}

static void two_perlin_spheres() {
    list<sphere> world;
    tex_storage texes;

    auto pertext = texes.noise(4);
    world.add(sphere(point3(0, -1000, 0), 1000));
    world.add(sphere(point3(0, 2, 0), 2));

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

    cam.start(time_wrapper(
                  tex_wrapper(world.finish(), material::lambertian(), pertext)),
              enable_progress, texes.view(), num_threads, {});
}

static void quads() {
    list<tex_wrapper<quad>> world;
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
        tex_wrapper(quad(point3(-3, -2, 5), vec3(0, 0, -4), vec3(0, 4, 0)),
                    all_mat, left_red));
    world.add(tex_wrapper(quad(point3(-2, -2, 0), vec3(4, 0, 0), vec3(0, 4, 0)),
                          all_mat, back_green));
    world.add(tex_wrapper(quad(point3(3, -2, 1), vec3(0, 0, 4), vec3(0, 4, 0)),
                          all_mat, right_blue));
    world.add(tex_wrapper(quad(point3(-2, 3, 1), vec3(4, 0, 0), vec3(0, 0, 4)),
                          all_mat, upper_orange));
    world.add(
        tex_wrapper(quad(point3(-2, -3, 5), vec3(4, 0, 0), vec3(0, 0, -4)),
                    all_mat, lower_teal));

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

    cam.start(time_wrapper(world.finish()), enable_progress, texes.view(),
              num_threads, {});
}

static void simple_light() {
    list<sphere> perlin_spheres_geom;
    tex_storage texes;

    auto pertext = texes.noise(4);
    perlin_spheres_geom.add(sphere(point3(0, -1000, 0), 1000));
    perlin_spheres_geom.add(sphere(point3(0, 2, 0), 2));

    auto perlin_spheres = tex_wrapper(perlin_spheres_geom.finish(),
                                      material::lambertian(), pertext);

    auto difflight = material::diffuse_light();
    auto difflight_color = texes.solid(4, 4, 4);
    auto light_sphere =
        tex_wrapper(sphere(point3(0, 7, 0), 2), difflight, difflight_color);
    auto light_quad =
        tex_wrapper(quad(point3(3, 1, -2), vec3(2, 0, 0), vec3(0, 2, 0)),
                    difflight, difflight_color);

    auto world = tuple_wrapper<decltype(light_quad), decltype(light_sphere),
                               decltype(perlin_spheres)>(
        std::move(light_quad), std::move(light_sphere),
        std::move(perlin_spheres));

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

    cam.start(world, enable_progress, texes.view(), num_threads, {});
}

static void cornell_box() {
    tex_storage texes;

    auto red = texes.solid(.65, .05, .05);
    auto white = texes.solid(.73, .73, .73);
    auto green = texes.solid(.12, .45, .15);
    auto light = material::diffuse_light();
    auto light_color = texes.solid(15, 15, 15);

    list<tex_wrapper<quad>> b_walls_and_light;

    b_walls_and_light.add(
        tex_wrapper(quad(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555)),
                    material::lambertian(), green));
    b_walls_and_light.add(
        tex_wrapper(quad(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555)),
                    material::lambertian(), red));
    b_walls_and_light.add(tex_wrapper(
        quad(point3(343, 554, 332), vec3(-130, 0, 0), vec3(0, 0, -105)), light,
        light_color));
    b_walls_and_light.add(
        tex_wrapper(quad(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555)),
                    material::lambertian(), white));
    b_walls_and_light.add(tex_wrapper(
        quad(point3(555, 555, 555), vec3(-555, 0, 0), vec3(0, 0, -555)),
        material::lambertian(), white));
    b_walls_and_light.add(
        tex_wrapper(quad(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0)),
                    material::lambertian(), white));

    auto walls_and_light = time_wrapper(b_walls_and_light.finish());

    list<transformed<box>> b_boxes;

    xform_builder xforms;
    xforms.translate(vec3(265, 0, 295)), xforms.rotate_y(15);
    b_boxes.add(transformed(xforms.finish(),
                            box(point3(0, 0, 0), point3(165, 330, 165))));

    xforms.translate(vec3(130, 0, 65)), xforms.rotate_y(-18);
    b_boxes.add(transformed(xforms.finish(),
                            box(point3(0, 0, 0), point3(165, 165, 165))));

    auto boxes = tex_wrapper(b_boxes.finish(), material::lambertian(), white);

    auto world = tuple_wrapper<decltype(walls_and_light), decltype(boxes)>(
        std::move(walls_and_light), std::move(boxes));

    static_assert(is_collection<decltype(walls_and_light)>);
    static_assert(is_collection<decltype(boxes)>);

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

    cam.start(world, enable_progress, texes.view(), num_threads, {});
}

static void cornell_smoke() {
    tex_storage texes;

    auto red = texes.solid(.65, .05, .05);
    auto white = texes.solid(.73, .73, .73);
    auto green = texes.solid(.12, .45, .15);
    auto light = texes.solid(7, 7, 7);

    list<tex_wrapper<quad>> quads;

    quads.add(
        tex_wrapper(quad(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555)),
                    material::lambertian(), green));
    quads.add(
        tex_wrapper(quad(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555)),
                    material::lambertian(), red));
    quads.add(tex_wrapper(
        quad(point3(113, 554, 127), vec3(330, 0, 0), vec3(0, 0, 305)),
        material::diffuse_light(), light));
    quads.add(
        tex_wrapper(quad(point3(0, 555, 0), vec3(555, 0, 0), vec3(0, 0, 555)),
                    material::lambertian(), white));
    quads.add(
        tex_wrapper(quad(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555)),
                    material::lambertian(), white));
    quads.add(
        tex_wrapper(quad(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0)),
                    material::lambertian(), white));

    list<transformed<constant_medium<box>>> cms;
    xform_builder xforms;
    xforms.translate(vec3(265, 0, 295)), xforms.rotate_y(15);
    cms.add(

        transformed(xforms.finish(),
                    constant_medium(box(point3(0, 0, 0), point3(165, 330, 165)),
                                    0.01, color(0, 0, 0), texes)));
    xforms.translate(vec3(130, 0, 65)), xforms.rotate_y(-18);
    cms.add(
        transformed(xforms.finish(),
                    constant_medium(box(point3(0, 0, 0), point3(165, 165, 165)),
                                    0.01, color(1, 1, 1), texes)));

    auto world =
        tuple_wrapper<decltype(cms.finish()), decltype(quads.finish())>(
            cms.finish(), quads.finish());
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

    cam.start(world, enable_progress, texes.view(), num_threads, {});
}

// owned data that we've got to pass from make_final_scene(), otherwise it will
// be freed.
template <typename T>
struct final_scene_data {
    T world;
    tex_storage texes;
    std::vector<box> boxes1;
    bvh::builder bvh_builder;
    list<constant_medium<sphere>> cms_builder;
    list<tex_wrapper<sphere>> wrapped_spheres;
    std::vector<sphere> box_of_spheres;
};

static auto make_final_scene() {
    // NOTE: scene occupies ~2MB.
    ZoneScopedN("scene build");
    tex_storage texes;

    std::vector<box> boxes1;
    auto ground_col = texes.solid(0.48, 0.83, 0.53);

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

    bvh::builder bvh_builder;

    auto ground = tex_wrapper(
        bvh::over(bvh::must_split(std::span<box>(boxes1), bvh_builder),
                  std::span<box const>(boxes1)),
        material::lambertian(), ground_col);

    auto light_color = texes.solid(7, 7, 7);

    auto light = tex_wrapper(
        quad(point3(123, 554, 147), vec3(300, 0, 0), vec3(0, 0, 265)),
        material::diffuse_light(), light_color);

    auto center = point3(400, 400, 200);
    auto sphere_material = material::lambertian();
    auto sphere_color = texes.solid(0.7, 0.3, 0.1);
    xform_builder xforms;
    xforms.move(vec3(30, 0, 0));
    auto moving_sphere = transformed(
        xforms.finish(),
        tex_wrapper(sphere(center, 50), sphere_material, sphere_color));

    auto dielectric = material::dielectric(1.5);

    // NOTE: Lookuout for duplication of materials/colors!
    list<tex_wrapper<sphere>> wrapped_spheres;

    auto full_white = texes.solid(1);
    wrapped_spheres.add(sphere(point3(260, 150, 45), 50), dielectric,
                        full_white);
    wrapped_spheres.add(sphere(point3(0, 150, 145), 50), material::metal(1.0),
                        texes.solid(0.8, 0.8, 0.9));

    static_assert(
        time_invariant_collection<decltype(wrapped_spheres.finish())>);
    sphere boundary_geom(point3(360, 150, 145), 70);
    // NOTE: This addition is necessary so that the medium is contained within
    // the boundary.
    // So these two are one (constant_medium) inside another (dielectric).
    wrapped_spheres.add(boundary_geom, dielectric, full_white);

    list<constant_medium<sphere>> cms_builder;
    cms_builder.add(sphere(boundary_geom.center, boundary_geom.radius - 0.001f),
                    0.2, color(0.2, 0.4, 0.9), texes);
    cms_builder.add(sphere(point3(0, 0, 0), 5000), .0001, color(1), texes);

    auto emat = texes.image("earthmap.jpg");
    wrapped_spheres.add(sphere(point3(400, 200, 400), 100),
                        material::lambertian(), emat);
    auto pertext = texes.noise(0.1);
    wrapped_spheres.add(sphere(point3(220, 280, 300), 80),
                        material::lambertian(), pertext);

    std::vector<sphere> box_of_spheres;
    auto white = texes.solid(.73, .73, .73);
    static constexpr int ns = 1000;
    box_of_spheres.reserve(ns);
    for (int j = 0; j < ns; j++) {
        box_of_spheres.emplace_back(point3::random(0, 165), 10);
    }

    xforms.translate(vec3(-100, 270, 395)), xforms.rotate_y(15);

    auto white_spheres = transformed(
        xforms.finish(),
        tex_wrapper(bvh::over(bvh::must_split(std::span<sphere>(box_of_spheres),
                                              bvh_builder),
                              std::span<sphere const>(box_of_spheres)),
                    material::lambertian(), white));

    // FIXME: There's some bug in propagation that makes the spheres up there ^
    // appear before the perlin-textured sphere. Maybe it's something related to
    // the BVH, or maybe it's something that makes propagation not
    // order-independent. In any case, it's a bug.

    auto cms = cms_builder.finish();

    static_assert(time_invariant_collection<decltype(ground)>);

    auto world = tuple_wrapper<
        decltype(white_spheres), decltype(cms), decltype(moving_sphere),
        decltype(wrapped_spheres.finish()), decltype(ground), decltype(light)>(
        std::move(white_spheres), std::move(cms), std::move(moving_sphere),
        wrapped_spheres.finish(), std::move(ground), std::move(light));

    return final_scene_data{
        std::move(world),         std::move(texes),
        std::move(boxes1),        std::move(bvh_builder),
        std::move(cms_builder),   std::move(wrapped_spheres),
        std::move(box_of_spheres)};
}

static void final_scene(int image_width, int samples_per_pixel, int max_depth) {
    auto data = make_final_scene();

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

    cam.start(data.world, enable_progress, data.texes.view(), num_threads,
              data.bvh_builder.lock());
}

int main(int argc, char const* argv[]) {
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
        case 0:
            /* final_scene(400, 250, 4); */
            final_scene(400, 250, 40);
            break;
    }
}
