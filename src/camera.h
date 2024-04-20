#ifndef CAMERA_H
#define CAMERA_H
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

#include <linux/futex.h>

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <thread>
#include <tracy/Tracy.hpp>

#include "hittable.h"
#include "hittable_list.h"
#include "material.h"
#include "rtweekend.h"

class camera {
   public:
    double aspect_ratio = 1.0;   // Ratio of image width over height
    int image_width = 100;       // Rendered image width in pixel count
    int samples_per_pixel = 10;  // Count of random samples for each pixel
    int max_depth = 10;          // Maximum number of ray bounces into scene
    color background;            // Scene background color

    double vfov = 90;                   // Vertical view angle (field of view)
    point3 lookfrom = point3(0, 0, 0);  // Point camera is looking from
    point3 lookat = point3(0, 0, -1);   // Point camera is looking at
    vec3 vup = vec3(0, 1, 0);           // Camera-relative "up" direction

    double defocus_angle = 0;  // Variation angle of rays through each pixel
    double focus_dist =
        10;  // Distance from camera lookfrom point to plane of perfect focus

    void render(hittable_list const &world) {
        initialize();

        auto pixels = std::make_unique<color[]>(size_t(image_width) *
                                                size_t(image_height));

        std::atomic<int> remain_scanlines{image_height};
        std::condition_variable cv;
        // lock progress mutex before launching the progress thread so we don't
        // end in a deadlock.

#if TRACY_ENABLE
        static constexpr int stop_at = 390;
#else
        static constexpr int stop_at = 0;
#endif
        auto progress_thread = std::thread([limit = image_height,
                                            &remain_scanlines, &cv]() {
            std::mutex progress_mux;
            std::unique_lock<std::mutex> lock(progress_mux);
            auto last_remain = limit + 1;
            while (true) {
                // wait till progress has been done.
                cv.wait(lock, [last_remain, &remain_scanlines] {
                    return last_remain >
                           remain_scanlines.load(std::memory_order_acquire);
                });
                auto remain = remain_scanlines.load(std::memory_order_acquire);
                last_remain = remain;
                std::clog << "\r\x1b[2K\x1b[?25lScanlines remaining: " << remain
                          << "\x1b[?25h" << std::flush;
                if (remain == stop_at) break;
            }
            std::clog << "\r\x1b[2K" << std::flush;
        });

        // worker loop
#pragma omp parallel
        {
            for (;;) {
                auto j = remain_scanlines.load(std::memory_order_acquire);

                // contend for our j (CAS)
                do {
                    if (j == stop_at) goto finish_work;
                } while (!remain_scanlines.compare_exchange_weak(
                    j, j - 1, std::memory_order_acq_rel));
                --j;

                scanLine(world, j, pixels.get());

                cv.notify_one();
            }
        finish_work:;
        }

        progress_thread.join();

        std::clog << "\r\x1b[2KWriting image...\n";

        std::ofstream imout("test.ppm");

        imout << "P3\n" << image_width << ' ' << image_height << "\n255\n";
        for (int i = 0; i < image_width * image_height; ++i) {
            write_color(imout, pixels[i]);
        }

        std::clog << "Done.\n";
    }

    void scanLine(hittable_list const &world, int j,
                  color *__restrict_arr pixels) {
        for (int i = 0; i < image_width; i++) {
            color pixel_color(0, 0, 0);
            for (int sample = 0; sample < samples_per_pixel; sample++) {
                ray r = get_ray(i, j);
                pixel_color += ray_color(r, max_depth, world);
            }
            pixels[j * image_width + i] = pixel_samples_scale * pixel_color;
        }
    }

   private:
    int image_height;  // Rendered image height
    double
        pixel_samples_scale;  // Color scale factor for a sum of pixel samples
    point3 center;            // Camera center
    point3 pixel00_loc;       // Location of pixel 0, 0
    vec3 pixel_delta_u;       // Offset to pixel to the right
    vec3 pixel_delta_v;       // Offset to pixel below
    vec3 u, v, w;             // Camera frame basis vectors
    vec3 defocus_disk_u;      // Defocus disk horizontal radius
    vec3 defocus_disk_v;      // Defocus disk vertical radius

    void initialize() {
        image_height = int(image_width / aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;

        pixel_samples_scale = 1.0 / samples_per_pixel;

        center = lookfrom;

        // Determine viewport dimensions.
        auto theta = degrees_to_radians(vfov);
        auto h = tan(theta / 2);
        auto viewport_height = 2 * h * focus_dist;
        auto viewport_width =
            viewport_height * (double(image_width) / image_height);

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

        // Calculate the horizontal and vertical delta vectors from pixel to
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
        // Construct a camera ray originating from the defocus disk and directed
        // at a randomly sampled point around the pixel location i, j.

        auto offset = sample_square();
        auto pixel_sample = pixel00_loc + ((i + offset.x()) * pixel_delta_u) +
                            ((j + offset.y()) * pixel_delta_v);

        auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin;
        auto ray_time = random_double();

        return ray(ray_origin, ray_direction, ray_time);
    }

    vec3 sample_square() const {
        // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit
        // square.
        return vec3(random_double() - 0.5, random_double() - 0.5, 0);
    }

    vec3 sample_disk(double radius) const {
        // Returns a random point in the unit (radius 0.5) disk centered at the
        // origin.
        return radius * random_in_unit_disk();
    }

    point3 defocus_disk_sample() const {
        // Returns a random point in the camera defocus disk.
        auto p = random_in_unit_disk();
        return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
    }

    color ray_color(ray r, int depth, hittable_list const &world) const {
        color emit_acc = color(0, 0, 0);
        color att_acc = color(1, 1, 1);
        for (;;) {
            if (depth <= 0) return emit_acc + att_acc;
            ZoneScopedN("ray frame");

            hit_record rec;

            // If the ray hits nothing, return the background color.
            auto res = world.hitSelect(r, interval(0.001, infinity), rec.geom);
            if (!res) return emit_acc + att_acc * background;

            rec.set_face_normal(r, rec.geom.normal);
            res->getUVs(rec.uv, rec.geom.p, rec.geom.normal);

            vec3 scattered;
            color attenuation;
            color color_from_emission = res->mat->emitted(rec.uv, rec.geom.p);

            if (!res->mat->scatter(r.dir, rec, attenuation, scattered))
                return emit_acc + att_acc * color_from_emission;

            depth = depth - 1;
            emit_acc = emit_acc + color_from_emission;
            att_acc = att_acc * attenuation;
            r = ray(rec.geom.p, scattered, r.time);
        }
    }
};

#endif
