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

#include <png.h>

#include <cstdint>
#include <iostream>
#include <span>

#include "collection.h"
#include "color.h"
#include "fixedvec.h"
#include "hittable.h"
#include "interval.h"
#include "material.h"
#include "progress.h"
#include "ray.h"
#include "rtweekend.h"
#include "transform.h"

struct camera {
   public:
    float aspect_ratio = 1.0;         // Ratio of image width over height
    uint32_t image_width = 100;       // Rendered image width in pixel count
    uint32_t samples_per_pixel = 10;  // Count of random samples for each pixel
    uint32_t max_depth = 10;  // Maximum number of ray bounces into scene
    color background;         // Scene background color

    float vfov = 90;                     // Vertical view angle (field of view)
    point3 lookfrom = point3(0, 0, -1);  // Point camera is looking from
    point3 lookat = point3(0, 0, 0);     // Point camera is looking at
    vec3 vup = vec3(0, 1, 0);            // Camera-relative "up" direction

    float defocus_angle = 0;  // Variation angle of rays through each pixel
    float focus_dist =
        10;  // Distance from camera look-from point to plane of perfect focus

    void render(hittable_collection const& world, bool enable_progress) {
        initialize();

        auto px_count = image_width * image_height;

        auto fcolors_mem = std::make_unique<color[]>(px_count);

        auto fcol_ptr = fcolors_mem.get();

        auto colors_mem = std::make_unique<uint8_t[]>(3ull * px_count);
        auto ichannels = colors_mem.get();
        auto channels = reinterpret_cast<float const*>(fcol_ptr);

#ifdef _OPENMP
        progress::progress_state progress;

        if (enable_progress) {
            int res = progress.setup(image_height);
            if (res != 0) {
                fprintf(stderr, "Could not setup progress thread! (%s)\n",
                        strerror(res));
                // NOTE: This is not really something we can't live without.
                exit(1);
            }
        }

#endif

        // NOTE: 30% of the time (5 seconds) is spent just on a couple of
        // threads. Consider sharing work, e.g lighting?

#pragma omp parallel num_threads(12)
        {
            // We do one per thread.
            auto light_info_uniq = std::make_unique<light_info[]>(
                size_t(max_depth) * size_t(samples_per_pixel));

            auto light_counts = std::make_unique<uint32_t[]>(samples_per_pixel);

#pragma omp for firstprivate(fcol_ptr, image_height, image_width)
            for (uint32_t j = 0; j < image_height; ++j) {
#ifndef _OPENMP
                // NOTE: doing this in MT will enable locks in I/O and
                // synchronize the threads, which I don't want to :(
                std::clog << "\rScanlines remaining: " << (image_height - j)
                          << ' ' << std::flush;
#endif
                for (uint32_t i = 0; i < image_width; ++i) {
                    // geometry

                    uint32_t total_geometry_samples = 0;
                    for (uint32_t sample = 0; sample < samples_per_pixel;
                         ++sample) {
                        vecview light_infos(
                            light_info_uniq.get() + total_geometry_samples,
                            max_depth);

                        ray r = get_ray(i, j);

                        simulate_ray(r, world, light_infos);
                        total_geometry_samples += light_infos.count;
                        light_counts[sample] = light_infos.count;
                    }

                    // color

                    uint32_t used_light_count = 0;
                    color pixel_color(0, 0, 0);
                    for (uint32_t sample = 0; sample < samples_per_pixel;
                         ++sample) {
                        auto const num_samples = light_counts[sample];

                        auto const* lights =
                            light_info_uniq.get() + used_light_count;

                        used_light_count += num_samples;

                        pixel_color += sample_textures(lights, num_samples);
                    }

                    fcol_ptr[j * image_width + i] =
                        pixel_color / float(samples_per_pixel);
                }
#ifdef _OPENMP
                progress.increment();
#endif
            }
        }

#ifdef _OPENMP
        progress.manual_teardown();
#endif
        puts("\r\x1b[2KRender done. Outputting colors...\n");

#pragma omp parallel for
        for (uint32_t i = 0; i < 3 * px_count; ++i) {
            auto channel = channels[i];
            // Apply the linear to gamma transform.
            channel = sqrtf(channel);
            // Write the translated [0,255] value of each color
            // component.
            static interval const intensity(0.000, 0.999);
            ichannels[i] = static_cast<uint8_t>(256 * intensity.clamp(channel));
        }

#pragma omp single
        {
            // NOTE: this is not cleanly freeing its resources.
            // It's ok, since all the error exit points here are asserts.

            FILE* fp = fopen("test.png", "wb");
            assert(fp && "cannot create file for writing");

            auto png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                   nullptr, nullptr, nullptr);
            assert(png_ptr && "cannot create write struct");

            auto info_ptr = png_create_info_struct(png_ptr);
            assert(info_ptr && "cannot create info struct");

            // HACK: for some reason, GCC loses the definition for non-debug
            // builds?
#ifdef NDEBUG
            assert(!setjmp(png_ptr->jmpbuf) && "libpng error");
#endif

            png_init_io(png_ptr, fp);

            // NOTE: not required to set compression, libpng will choose one for
            // us.

            png_set_IHDR(png_ptr, info_ptr, image_width, image_height, 8,
                         PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                         PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

            // TODO: do gamma correction on libpng instead of ourselves?
            png_write_info(png_ptr, info_ptr);

            auto row_pointers = std::make_unique<uint8_t*[]>(image_height);
            for (size_t i = 0; i < image_height; ++i) {
                row_pointers[i] = reinterpret_cast<uint8_t*>(
                    colors_mem.get() + i * image_width * 3ull);
            }

            png_write_image(png_ptr, row_pointers.get());

            png_write_end(png_ptr, info_ptr);

            png_destroy_write_struct(&png_ptr, &info_ptr);
            fclose(fp);
        }
    }

   private:
    uint32_t image_height{};  // Rendered image height
    point3 center;            // Camera center
    point3 pixel00_loc;       // Location of pixel 0, 0
    vec3 pixel_delta_u;       // Offset to pixel to the right
    vec3 pixel_delta_v;       // Offset to pixel below
    vec3 u, v, w;             // Camera frame basis vectors
    vec3 defocus_disk_u;      // Defocus disk horizontal radius
    vec3 defocus_disk_v;      // Defocus disk vertical radius

    void initialize() {
        image_height = static_cast<uint32_t>(float(image_width) / aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;

        center = lookfrom;

        // Determine viewport dimensions.
        auto theta = degrees_to_radians(vfov);
        auto h = tan(theta / 2);
        auto viewport_height = 2 * h * focus_dist;
        auto viewport_width =
            viewport_height *
            (static_cast<float>(image_width) / float(image_height));

        // Calculate the u,v,w unit basis vectors for the camera coordinate
        // frame.
        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        // Calculate the vectors across the horizontal and down the vertical
        // viewport edges.
        vec3 viewport_u =
            viewport_width * u;  // Vector across viewport horizontal edge
        vec3 viewport_v =
            viewport_height * -v;  // Vector down viewport vertical edge

        // Calculate the horizontal and vertical delta vectors to the next
        // pixel.
        pixel_delta_u = viewport_u / float(image_width);
        pixel_delta_v = viewport_v / float(image_height);

        // Calculate the location of the upper left pixel.
        auto viewport_upper_left =
            center - (focus_dist * w) - viewport_u / 2 - viewport_v / 2;
        pixel00_loc =
            viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

        // Calculate the camera defocus disk basis vectors.
        auto defocus_radius =
            focus_dist * tan(degrees_to_radians(defocus_angle / 2));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;
    }

    [[nodiscard]] ray get_ray(uint32_t i, uint32_t j) const {
        // Get a randomly-sampled camera ray for the pixel at location i,j,
        // originating from the camera defocus disk.

        auto pixel_center = pixel00_loc + (float(i) * pixel_delta_u) +
                            (float(j) * pixel_delta_v);
        auto pixel_sample = pixel_center + pixel_sample_square();

        auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin;
        auto ray_time = random_float();

        return {ray_origin, unit_vector(ray_direction), ray_time};
    }

    [[nodiscard]] vec3 pixel_sample_square() const {
        // Returns a random point in the square surrounding a pixel at the
        // origin.
        auto px = -0.5f + random_float();
        auto py = -0.5f + random_float();
        return (px * pixel_delta_u) + (py * pixel_delta_v);
    }

    [[nodiscard]] point3 defocus_disk_sample() const {
        // Returns a random point in the camera defocus disk.
        auto p = random_in_unit_disk();
        return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
    }

    // NOTE: would be nice to make this available to 'hittable' so it
    // initializes the correct value.
    struct light_info {
        texture const* tex{};
        point3 p;
        float u{}, v{};
    };

    // NOTE: Could have the background as another light source.
    // NOTE: Maybe it's interesting to tell the renderer that it's going to get
    // a collection of geometries?

    // Simulates a ray until either it hits too many times, hits a light, or
    // hits the skybox.
    static void simulate_ray(ray r, hittable_collection const& world,
                             vecview<light_info>& lights) {
        while (!lights.is_full()) {
            hit_record rec;

            interval ray_t{0.001, 10e10f};

            if (!world.hit(r, ray_t, rec)) break;

            apply_reverse_transforms(rec.xforms, rec.p, r.time, rec.normal);
            lights.emplace_back(rec.tex, rec.p, rec.u, rec.v);

            if (rec.mat.tag == material::kind::diffuse_light) return;

            auto face = ::face(r.direction, rec.normal);

            vec3 scattered;

            rec.mat.scatter(r.direction, face, scattered);

            r = ray(rec.p, scattered, r.time);
        }
        // The ray didn't converge to a light source, so no light is to be
        // returned
        lights.clear();
        lights.emplace_back(&black_texture, point3(0), 0, 0);
    }

    // TODO: can't make constexpr due to some weird thing happening with virtual
    // destructors: 'calling destructor before its definition'
    static solid_color black_texture;

    static color sample_textures(light_info const* lights,
                                 uint32_t lights_count) {
        // lighting
        color begin_color = color(1, 1, 1);

        // NOTE: This is independent and can be done by any thread!
        // Each of these is also independent since it's multiplying.
        // We may even add queues, although that would waste a ton of memory.
        // Do all the color samples for this ray.
        for (uint32_t i = 0; i < lights_count; ++i) {
            auto const& info = lights[i];
            begin_color *= info.tex->value(info.u, info.v, info.p);
        }

        return begin_color;
    }
};

solid_color camera::black_texture = solid_color(0);