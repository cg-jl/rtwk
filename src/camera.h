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
#include <omp.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <memory>
#include <optional>
#include <thread>
#include <tracy/Tracy.hpp>

#include "hittable.h"
#include "hittable_list.h"
#include "material.h"
#include "rtweekend.h"
#include "texture.h"

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

    struct sample_request {
        texture tex;
        uvs uv;
        point3 p;

        sample_request(texture tex, uvs uv, point3 p)
            : tex(std::move(tex)), uv(uv), p(p) {}

        color sample() const { return tex.value(uv, p); }
    };

    struct px_sampleq {
        std::vector<sample_request> &reqs;
        size_t prev_fill;

        template <typename... Ts>
        void emplace_back(Ts &&...ts) {
            reqs.emplace_back(std::forward<Ts &&>(ts)...);
        }
        void clear() {
            reqs.erase(reqs.cbegin() + intptr_t(prev_fill), reqs.cend());
        }
    };

    // The constructors for `T` have a time limit, the moment they modify the
    // value successfully is the moment they should be flushing the writes,
    // otherwise other thread could read garbage memory.
    template <typename T>
    struct ring_q {
        std::span<T> const buffer;
        std::atomic<size_t> write{0}, read{0};

        explicit constexpr ring_q(std::span<T> buffer) : buffer(buffer) {}

        constexpr size_t mask2(size_t val) { return val % (2 * buffer.size()); }
        constexpr size_t mask(size_t val) { return val % buffer.size(); }

        // Do not wait if the queue is full -> does not move from val
        bool pushIfNotFull(T &val) {
            auto old_head = write.load(std::memory_order_acquire);

            size_t new_head;
            do {
                auto cur_read = read.load(std::memory_order::acquire);
                // queue is full.
                if (cur_read == mask2(new_head + buffer.size())) return false;

                new_head = mask2(old_head + 1);

            } while (!write.compare_exchange_weak(old_head, new_head,
                                                  std::memory_order::acq_rel));

            // Overwrite the old value. We don't run destructors, since we
            // should have popped the value when removing it.
            new (&buffer[mask(old_head)]) T(std::move(val));
            return true;
        }

        // Do not wait if queue is empty. Returns whether `target` was
        // constructed with the value from the queue.
        // `target` is not valid memory for `T` until this function returns
        // `true`.
        bool pop(void *target) {
            auto old_read = read.load(std::memory_order::acquire);

            size_t new_read;
            do {
                auto cur_write = write.load(std::memory_order::acquire);

                if (cur_write == old_read) return false;

                new_read = mask2(cur_write + 1);
            } while (!read.compare_exchange_weak(old_read, new_read,
                                                 std::memory_order::acq_rel));

            // We aren't running destructors, so the target should not be an
            // initialized piece.
            new (target) T(std::move(buffer[mask(old_read)]));
        }
    };

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

#ifdef _OPENMP
        size_t thread_count = omp_get_num_threads();
        auto att_requests_mem =
            std::make_unique<std::vector<sample_request>[]>(thread_count);
        auto att_rq = ring_q(std::span{att_requests_mem.get(), thread_count});

#else
        // TODO: if I keep adding atomic things, then single threaded
        // performance will be lost.
#endif

        // worker loop
#pragma omp parallel
        {
            std::vector<sample_request> attenuations;
            auto sample_counts =
                std::make_unique<size_t[]>(size_t(samples_per_pixel));
            auto samples = std::make_unique<color[]>(size_t(samples_per_pixel));
            for (;;) {
                auto j = remain_scanlines.load(std::memory_order_acquire);

                // contend for our j (CAS)
                do {
                    if (j == stop_at) goto finish_work;
                } while (!remain_scanlines.compare_exchange_weak(
                    j, j - 1, std::memory_order_acq_rel));
                --j;

                scanLine(world, j, pixels.get(), attenuations,
                         sample_counts.get(), samples.get());

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
                  color *__restrict_arr pixels,
                  std::vector<sample_request> &attenuations,
                  size_t *__restrict_arr sample_counts,
                  color *__restrict_arr samples) {
        for (int i = 0; i < image_width; i++) {
            color pixel_color(0, 0, 0);
            attenuations.clear();
            px_sampleq q{attenuations, 0};
            for (int sample = 0; sample < samples_per_pixel; sample++) {
                ZoneScopedN("pixel sample");
                ray r = get_ray(i, j);

                auto bg = geometrySim(r, max_depth, world, q);

                auto att_count = attenuations.size() - q.prev_fill;
                q.prev_fill = attenuations.size();
                sample_counts[sample] = att_count;
                samples[sample] = bg;
            }

            {
                std::span atts = attenuations;
                size_t processed = 0;
                for (int sample = 0; sample < samples_per_pixel; ++sample) {
                    auto count = sample_counts[sample];
                    samples[sample] = multiplySamples(
                        atts.subspan(processed, count), samples[sample]);
                    processed += count;
                }
            }

            for (int sample = 0; sample < samples_per_pixel; ++sample) {
                pixel_color += samples[sample];
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

    static color multiplySamples(std::span<sample_request const> samples,
                                 color initial) {
        color acc = initial;
        for (auto const &s : samples) acc = acc * s.sample();
        return acc;
    }

    // Fills the `attenuations` vector and returns the background color
    color geometrySim(ray r, int depth, hittable_list const &world,
                      px_sampleq &attenuations) const {
        for (;;) {
            if (depth <= 0) return color(1, 1, 1);
            ZoneScopedN("ray frame");

            hit_record rec;

            // If the ray hits nothing, return the background color.
            auto res = world.hitSelect(r, interval(0.001, infinity), rec.geom);
            if (!res) return background;

            rec.set_face_normal(r, rec.geom.normal);
            res->getUVs(rec.uv, rec.geom.p, rec.geom.normal);

            vec3 scattered;
            color attenuation;

            // here we'll have to use the emit value as the 'attenuation' value.
            if (res->mat->tag == material::kind::diffuse_light) {
                attenuations.emplace_back(*res->tex, rec.uv, rec.geom.p);
                return color(1, 1, 1);
            }

            if (!res->mat->scatter(r.dir, rec, scattered)) {
                attenuations.clear();
                return color(0, 0, 0);
            }

            depth = depth - 1;
            attenuations.emplace_back(*res->tex, rec.uv, rec.geom.p);
            r = ray(rec.geom.p, scattered, r.time);
        }
    }
};

#endif
