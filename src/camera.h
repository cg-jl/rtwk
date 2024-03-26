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

#include <omp.h>
#include <png.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <tracy/Tracy.hpp>

#include "collection.h"
#include "color.h"
#include "fixedvec.h"
#include "hittable.h"
#include "interval.h"
#include "material.h"
#include "progress.h"
#include "ray.h"
#include "rtw_stb_image.h"
#include "rtweekend.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"
#include "workq.h"

struct camera {
   public:
    float aspect_ratio = 1.0;         // Ratio of image width over height
    uint32_t image_width = 100;       // Rendered image width in pixel count
    uint16_t samples_per_pixel = 10;  // Count of random samples for each pixel
    uint32_t max_depth = 10;  // Maximum number of ray bounces into scene
    color background;         // Scene background color

    float vfov = 90;                     // Vertical view angle (field of view)
    point3 lookfrom = point3(0, 0, -1);  // Point camera is looking from
    point3 lookat = point3(0, 0, 0);     // Point camera is looking at
    vec3 vup = vec3(0, 1, 0);            // Camera-relative "up" direction

    float defocus_angle = 0;  // Variation angle of rays through each pixel
    float focus_dist =
        10;  // Distance from camera look-from point to plane of perfect focus

    // NOTE: may use dyn collection to avoid templating on collection?
    // Compile times don't seem too bad rn.
    void render(is_collection auto const& world, bool enable_progress,
                tex_view texes, int thread_count) {
        initialize();

        auto px_count = image_width * image_height;

        auto fcolors_mem = std::make_unique<color[]>(px_count);

        auto fcol_ptr = fcolors_mem.get();

        auto colors_mem = std::make_unique<uint8_t[]>(3ull * px_count);
        auto ichannels = colors_mem.get();
        auto channels = reinterpret_cast<float const*>(fcol_ptr);

        auto* q = new range_queue(px_count);

#ifdef _OPENMP
        progress::progress_state progress;

        if (enable_progress) {
            int res = progress.setup(q->max_capacity);
            if (res != 0) {
                fprintf(stderr, "Could not setup progress thread! (%s)\n",
                        strerror(res));
                // NOTE: This is not really something we can't live without.
                exit(1);
            }
        }

        omp_set_num_threads(thread_count);
#endif

        auto perlin_noise = leak(perlin());

        auto const max_batch_size = size_t(image_width);
        auto const max_samples_per_batch =
            size_t(samples_per_pixel) * max_batch_size;
        auto const max_rays_ppx = size_t(max_depth) * size_t(samples_per_pixel);
        auto const max_rays = max_rays_ppx * size_t(max_batch_size);

        // NOTE: 30% of the time (5 seconds) is spent just on a couple of
        // threads. Consider sharing work, e.g lighting?

#pragma omp parallel
        {
            // We do one per thread.
            auto const checker_requests =
                std::make_unique<checker_request[]>(max_rays);
            auto const image_requests =
                std::make_unique<image_request[]>(max_rays);
            auto const noise_requests =
                std::make_unique<noise_request[]>(max_rays);
            auto const sampled_grays = std::make_unique<float[]>(max_rays);
            auto const sampled_colors = std::make_unique<color[]>(max_rays);

            auto const checker_counts =
                std::make_unique<uint16_t[]>(max_samples_per_batch);
            auto const solid_counts =
                std::make_unique<uint16_t[]>(max_samples_per_batch);
            auto const image_counts =
                std::make_unique<uint16_t[]>(max_samples_per_batch);
            auto const noise_counts =
                std::make_unique<uint16_t[]>(max_samples_per_batch);

            auto ray_colors = std::make_unique<color[]>(max_samples_per_batch);

            for (auto work = q->next(max_batch_size); work;
                 work = q->next(max_batch_size)) {
#ifndef _OPENMP
                // NOTE: doing this in MT will enable locks in I/O and
                // synchronize the threads, which I don't want to :(
                std::clog << "\rScanlines remaining: " << (image_height - j)
                          << ' ' << std::flush;
#endif
                auto [lane_start, lane_size] = *work;
                uint32_t checker_count = 0;
                uint32_t image_count = 0;
                uint32_t noise_count = 0;
                uint32_t solid_count = 0;
                ZoneScoped;
                for (uint32_t i = 0; i < lane_size; ++i) {
                    // geometry

                    auto const solid_counts_px =
                        solid_counts.get() + i * samples_per_pixel;
                    auto const noise_counts_px =
                        noise_counts.get() + i * samples_per_pixel;
                    auto const checker_counts_px =
                        checker_counts.get() + i * samples_per_pixel;
                    auto const image_counts_px =
                        image_counts.get() + i * samples_per_pixel;

                    for (uint32_t sample = 0; sample < samples_per_pixel;
                         ++sample) {
                        tex_request_queue tex_reqs(
                            checker_requests.get() + checker_count,
                            // NOTE: all solids get directly copied into the
                            // shared samples array. They will get multiplied
                            // the first.
                            sampled_colors.get() + solid_count,
                            image_requests.get() + image_count,
                            noise_requests.get() + noise_count, max_depth,
                            texes);

                        ray const r =
                            get_ray(i, (lane_start + i) / image_width);
                        auto const time = random_float();

                        if (simulate_ray(r, world, time, tex_reqs)) {
                            checker_count += tex_reqs.checker_count;
                            solid_count += tex_reqs.solid_count;
                            image_count += tex_reqs.image_count;
                            noise_count += tex_reqs.noise_count;

                            checker_counts_px[sample] = tex_reqs.checker_count;
                            solid_counts_px[sample] = tex_reqs.solid_count;
                            image_counts_px[sample] = tex_reqs.image_count;
                            noise_counts_px[sample] = tex_reqs.noise_count;
                        } else {
                            // cancelled!
                            checker_counts_px[sample] = 0;
                            image_counts_px[sample] = 0;
                            noise_counts_px[sample] = 0;

                            solid_counts_px[sample] = 1;
                            sampled_colors[solid_count] = color(0);
                            ++solid_count;
                        }
                    }
                }

                multiply_init(sampled_colors.get(), solid_counts.get(),
                              size_t(samples_per_pixel) * lane_size,
                              ray_colors.get());

                if (noise_count != 0) {
                    // sample all noise functions
                    for (uint32_t k = 0; k < noise_count; ++k) {
                        auto const& info = noise_requests.get()[k];
                        // TODO: have perlin noise be a parameter to
                        // noise.value()
                        sampled_grays[k] =
                            info.noise.value(info.p, perlin_noise);
                    }
                    multiply_grays(sampled_grays.get(), noise_counts.get(),
                                   size_t(samples_per_pixel) * lane_size,
                                   ray_colors.get());
                }
                if (image_count != 0) {
                    // sample all images
                    // NOTE: I could have some pointer based mappings.
                    // Sorting is out of the picture, otherwise it will move
                    // samples between pixels.
                    for (uint32_t k = 0; k < image_count; ++k) {
                        auto const& info = image_requests.get()[k];
                        sampled_colors[k] =
                            texes.images[info.image_id].sample(info.u, info.v);
                    }

                    multiply_samples(sampled_colors.get(), image_counts.get(),
                                     size_t(samples_per_pixel) * lane_size,
                                     ray_colors.get());
                }
                if (checker_count != 0) {
                    // sample all textures
                    for (uint32_t req = 0; req < checker_count; ++req) {
                        auto const& info = checker_requests.get()[req];
                        sampled_colors[req] = info.checker->value(info.p);
                    }

                    // multiply sampled textures
                    multiply_samples(sampled_colors.get(), checker_counts.get(),
                                     size_t(samples_per_pixel) * lane_size,
                                     ray_colors.get());
                }
                for (size_t i = 0; i < lane_size; ++i) {
                    auto ray_colors_px =
                        ray_colors.get() + i * samples_per_pixel;
                    // average samples
                    color pixel_color(0, 0, 0);
                    for (uint32_t k = 0; k < samples_per_pixel; ++k) {
                        pixel_color += ray_colors_px[k];
                    }

                    fcol_ptr[lane_start + i] =
                        pixel_color / float(samples_per_pixel);
                }
#ifdef _OPENMP
                progress.increment(lane_size);
#endif
            }
        }

        delete q;

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

        return {ray_origin, unit_vector(ray_direction)};
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

    // NOTE: Should have some way of only keeping the sets that are active in
    // the system...
    struct image_request {
        uint32_t image_id;
        float u, v;
    };

    struct noise_request {
        noise_texture noise;
        point3 p;
    };

    struct checker_request {
        checker_texture const* checker;
        point3 p;
    };

    struct tex_request_queue {
        checker_request* checkers;
        color* solids;
        image_request* images;
        noise_request* noises;
        // NOTE: Maybe these could be like a u16 to avoid going over the
        // cacheline barrier? That would make the upper limit 60k spp, which is
        // a lot of spp!
        uint16_t remaining;
        uint16_t checker_count;
        uint16_t solid_count;
        uint16_t image_count;
        uint16_t noise_count;
        tex_view texes;

        constexpr tex_request_queue(checker_request* reqs, color* solids,
                                    image_request* images,
                                    noise_request* noises, uint16_t cap,
                                    tex_view texes)
            : checkers(reqs),
              solids(solids),
              images(images),
              noises(noises),
              remaining(cap),
              checker_count(0),
              solid_count(0),
              image_count(0),
              noise_count(0),
              texes(texes) {}

        constexpr bool is_full() const noexcept { return remaining == 0; }

        void add(texture const tex, point3 const& p, float u,
                 float v) noexcept {
            --remaining;
            switch (tex.tag) {
                case texture::kind::solid:
                    solids[solid_count++] = texes.solids[tex.id];
                    break;
                case texture::kind::checker:
                    new (&checkers[checker_count++])
                        checker_request{&texes.checkers[tex.id], p};
                    break;
                case texture::kind::noise:
                    new (&noises[noise_count++])
                        noise_request{texes.noises[tex.id], p};
                    break;
                case texture::kind::image:
                    new (&images[image_count++]) image_request{tex.id, u, v};
                    break;
            }
        }
    };

    // Assumes `dst` is uninitialized so uses `src` as the source.
    static void multiply_init(color const* src,
                              uint16_t const* counts_per_sample,
                              size_t total_samples, color* dst) {
        for (size_t k = 0; k < total_samples; ++k) {
            auto num_samples = counts_per_sample[k];
            if (num_samples == 0) continue;
            auto col = *src++;

            while (--num_samples) col *= *src++;

            dst[k] = col;
        }
    }

    // Assumes `dst` is initialized and uses it as the source
    static void multiply_grays(float const* src,
                               uint16_t const* counts_per_sample,
                               size_t total_samples, color* dst) {
        for (size_t k = 0; k < total_samples; ++k) {
            auto num_samples = counts_per_sample[k];
            if (num_samples == 0) continue;

            auto accum = *src++;
            while (--num_samples) accum *= *src++;

            dst[k] *= accum;
        }
    }

    // Assumes `dst` is initialized and uses it as the source.
    static void multiply_samples(color const* src,
                                 uint16_t const* counts_per_sample,
                                 size_t total_samples, color* dst) {
        for (size_t k = 0; k < total_samples; ++k) {
            auto num_samples = counts_per_sample[k];
            if (num_samples == 0) continue;
            auto col = dst[k];

            while (num_samples--) col *= *src++;

            dst[k] = col;
        }
    }

    // NOTE: Could have the background as another light source.
    // NOTE: Maybe it's interesting to tell the renderer that it's going to get
    // a collection of geometries?

    // Simulates a ray until either it hits too many times, hits a light, or
    // hits the skybox.
    // Returns whether it hits a light or not
    static bool simulate_ray(ray r, is_collection auto const& world, float time,
                             tex_request_queue& texes) {
        transform_set xforms{};
        while (!texes.is_full()) {
            hit_record rec;

            interval ray_t{0.001, 10e10f};

            // TODO: maybe it's better to have a bounce() method that redirects
            // the ray, since texture is the only thing that is completely
            // separate from geometry (except for hit point).
            hit_status status{ray_t};
            world.propagate(r, status, rec, xforms, time);
            if (!status.hit_anything) return false;

            apply_reverse_transforms(xforms, rec.geom.p, time, rec.geom.normal);
            texes.add(rec.tex, rec.geom.p, rec.u, rec.v);

            if (rec.mat.tag == material::kind::diffuse_light) return true;

            auto face = ::face(r.direction, rec.geom.normal);

            vec3 scattered;

            rec.mat.scatter(r.direction, face, scattered);

            r = ray(rec.geom.p, scattered);
        }
        // The ray didn't converge to a light source, so no light is to be
        // returned
        return false;
    }
};
