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

#include <iostream>
#include <span>

#include "color.h"
#include "fixedvec.h"
#include "hittable.h"
#include "interval.h"
#include "material.h"
#include "ray.h"
#include "rtweekend.h"

struct camera {
   public:
    double aspect_ratio = 1.0;   // Ratio of image width over height
    int image_width = 100;       // Rendered image width in pixel count
    int samples_per_pixel = 10;  // Count of random samples for each pixel
    int max_depth = 10;          // Maximum number of ray bounces into scene
    color background;            // Scene background color

    double vfov = 90;                    // Vertical view angle (field of view)
    point3 lookfrom = point3(0, 0, -1);  // Point camera is looking from
    point3 lookat = point3(0, 0, 0);     // Point camera is looking at
    vec3 vup = vec3(0, 1, 0);            // Camera-relative "up" direction

    double defocus_angle = 0;  // Variation angle of rays through each pixel
    double focus_dist =
        10;  // Distance from camera lookfrom point to plane of perfect focus

    void render(hittable const& world) {
        initialize();

        auto px_count = image_width * image_height;

        auto colors_mem = std::make_unique<icolor[]>(px_count);

        auto col_ptr = colors_mem.get();

#pragma omp parallel for num_threads(12) \
    firstprivate(col_ptr, image_height, image_width)
        for (int j = 0; j < image_height; ++j) {
// NOTE: doing this in MT will enable locks in I/O and synchronize
// the threads, which I don't want to :(
#ifndef _OPENMP
            std::clog << "\rScanlines remaining: " << (image_height - j) << ' '
                      << std::flush;
#endif
            for (int i = 0; i < image_width; ++i) {
                color pixel_color(0, 0, 0);
                for (int sample = 0; sample < samples_per_pixel; ++sample) {
                    ray r = get_ray(i, j);
                    pixel_color += ray_color(r, max_depth, background, world);
                }
                pixel_color /= samples_per_pixel;
                // NOTE: if discretizing becomes a problem, we can always have a
                // first render buffer and then a discretized buffer.
                // Perhaps we can even re-use the memory or smth.
                // NOTE: consider aligning col_ptr?
                discretize(pixel_color, col_ptr[j * image_width + i]);
            }
        }

#pragma omp single
        {
            // NOTE: this is not cleanly freeing its resources.
            // It's ok, since all of the error exit points here are asserts.

            FILE* fp = fopen("test.png", "wb");
            assert(fp && "cannot create file for writing");

            auto png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
                                                   NULL, NULL);
            assert(png_ptr && "cannot create write struct");

            auto info_ptr = png_create_info_struct(png_ptr);
            assert(info_ptr && "cannot create info struct");

            assert(!setjmp(png_ptr->jmpbuf) && "libpng error");

            png_init_io(png_ptr, fp);

            // NOTE: not required to set compression, libpng will choose one for
            // us.

            png_set_IHDR(png_ptr, info_ptr, image_width, image_height, 8,
                         PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                         PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

            // TODO: do gamma correction on libpng instead of ourselves?
            png_write_info(png_ptr, info_ptr);

            auto row_pointers = std::make_unique<uint8_t*[]>(image_height);
            for (int i = 0; i < image_height; ++i) {
                row_pointers[i] = reinterpret_cast<uint8_t*>(colors_mem.get() +
                                                             i * image_width);
            }

            png_write_image(png_ptr, row_pointers.get());

            png_write_end(png_ptr, info_ptr);

            png_destroy_write_struct(&png_ptr, &info_ptr);
            fclose(fp);
        }
    }

   private:
    int image_height;     // Rendered image height
    point3 center;        // Camera center
    point3 pixel00_loc;   // Location of pixel 0, 0
    vec3 pixel_delta_u;   // Offset to pixel to the right
    vec3 pixel_delta_v;   // Offset to pixel below
    vec3 u, v, w;         // Camera frame basis vectors
    vec3 defocus_disk_u;  // Defocus disk horizontal radius
    vec3 defocus_disk_v;  // Defocus disk vertical radius

    void initialize() {
        image_height = static_cast<int>(image_width / aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;

        center = lookfrom;

        // Determine viewport dimensions.
        auto theta = degrees_to_radians(vfov);
        auto h = tan(theta / 2);
        auto viewport_height = 2 * h * focus_dist;
        auto viewport_width =
            viewport_height * (static_cast<double>(image_width) / image_height);

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
        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / image_height;

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

    ray get_ray(int i, int j) const {
        // Get a randomly-sampled camera ray for the pixel at location i,j,
        // originating from the camera defocus disk.

        auto pixel_center =
            pixel00_loc + (i * pixel_delta_u) + (j * pixel_delta_v);
        auto pixel_sample = pixel_center + pixel_sample_square();

        auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin;
        auto ray_time = random_double();

        return ray(ray_origin, ray_direction, ray_time);
    }

    vec3 pixel_sample_square() const {
        // Returns a random point in the square surrounding a pixel at the
        // origin.
        auto px = -0.5 + random_double();
        auto py = -0.5 + random_double();
        return (px * pixel_delta_u) + (py * pixel_delta_v);
    }

    vec3 pixel_sample_disk(double radius) const {
        // Generate a sample from the disk of given radius around a pixel at the
        // origin.
        auto p = radius * random_in_unit_disk();
        return (p[0] * pixel_delta_u) + (p[1] * pixel_delta_v);
    }

    point3 defocus_disk_sample() const {
        // Returns a random point in the camera defocus disk.
        auto p = random_in_unit_disk();
        return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
    }

    struct light_info {
        material const* mat;
        point3 p;
        vec3 u, v;
    };

    enum class ray_result {
        shadow,  // the ray bounced too many times, so it's considered a shadow
        light,   // the ray hit a light emitter
        background,  // the ray escaped the world
    };

    static color ray_color(ray r, int depth, color background,
                           hittable const& world) {
        color att_acc = color(1, 1, 1);
        for (;; depth--) {
            // If we've exceeded the ray bounce limit, no more light is
            // gathered.
            if (depth <= 0) return color(0, 0, 0);

            hit_record rec;

            // If the ray hits nothing, return the background color.
            if (!world.hit(r, rec)) {
                return att_acc * background;
            }

            auto face = hit_record::face(r.direction, rec.normal);

            vec3 scattered;

            // NOTE: this could be done in a separate thread, since it's
            // independent from geometries.
            // At least we could first separate the lighting stage for
            // later, simulating rays first, collect materials & hit info,
            // then emulate materials.
            // Also could reduce it to a set of commands (e.g perlin)
            color sample = rec.mat->tex->value(rec.u, rec.v, rec.p);
            if (rec.mat->is_light_source) {
                // NOTE: could split material taxonomy in two.
                // Perhaps even put he tag into the result?
                color color_from_emission = sample;
                return att_acc * color_from_emission;
            }

            rec.mat->scatter(r.direction, face, scattered);

            r = ray(rec.p, scattered, r.time);
            att_acc *= sample;
        }
    }
};
