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
#include <sstream>
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

    [[nodiscard]] ray defocusRay(vec3 origin, vec3 pixel_center) const& {
        auto pixel_deviation = pixel_sample_square();
        auto new_origin = origin + defocus_disk_sample();

        return {new_origin,
                unit_vector(pixel_center + pixel_deviation - new_origin)};
    }

    [[nodiscard]] vec3 getPixelCenter(float i, float j) const {
        return pixel00_loc + (i * pixel_delta_u) + (j * pixel_delta_v);
    }

    [[nodiscard]] vec3 pixel_sample_square() const {
        // Returns a random point in the square surrounding a pixel at the
        // origin.
        auto px = -0.5f + random_float();
        auto py = -0.5f + random_float();
        return (px * pixel_delta_u) + (py * pixel_delta_v);
    }

    [[nodiscard]] vec3 defocus_disk_sample() const {
        if (defocus_angle == 0) return vec3(0);
        // Returns a random point in the camera defocus disk.
        auto p = random_in_unit_disk();
        return (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
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

    struct trq_memory {
        checker_request* checkers;
        color __restrict_arr* solids;
        image_request* images;
        noise_request* noises;
    };

    struct sample_memory {
        float __restrict_arr* grays;
        uint16_t __restrict_arr *checkers, __restrict_arr *solids,
            __restrict_arr *images, __restrict_arr *noises;
    };

    struct sample_request {
        uint32_t checkers{}, images{}, noises{};
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
    static void multiply_init(auto src, uint16_t const* counts_per_sample,
                              size_t total_samples, color* dst) {
        size_t sample = 0;
        for (size_t k = 0; k < total_samples; ++k) {
            auto num_samples = counts_per_sample[k];
            if (num_samples == 0) continue;
            color col = src(sample);

            ++sample;
            for (; --num_samples; ++sample) col *= src(sample);

            dst[k] = col;
        }
    }

    // Assumes `dst` is initialized and uses it as the source.
    static void multiply_samples(auto src, uint16_t const* counts_per_sample,
                                 size_t total_samples, color* dst) {
        size_t sample = 0;
        for (size_t k = 0; k < total_samples; ++k) {
            auto num_samples = counts_per_sample[k];
            if (num_samples == 0) continue;
            auto col = dst[k];

            for (;num_samples--; ++sample) col *= src(sample);

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

   public:
    // NOTE: may use dyn collection to avoid templating on collection?
    // Compile times don't seem too bad rn.
    void start(is_collection auto const& world, bool enable_progress,
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
            ZoneScopedN("worker thread");
            // We do one per thread.
            auto const checker_requests =
                std::make_unique<checker_request[]>(max_rays);
            auto const image_requests =
                std::make_unique<image_request[]>(max_rays);
            auto const noise_requests =
                std::make_unique<noise_request[]>(max_rays);
            auto const sampled_grays = std::make_unique<float[]>(max_rays);
            auto const sampled_colors = std::make_unique<color[]>(max_rays);

            auto const trq_mem =
                trq_memory(checker_requests.get(), sampled_colors.get(),
                           image_requests.get(), noise_requests.get());

            auto const checker_counts =
                std::make_unique<uint16_t[]>(max_samples_per_batch);
            auto const solid_counts =
                std::make_unique<uint16_t[]>(max_samples_per_batch);
            auto const image_counts =
                std::make_unique<uint16_t[]>(max_samples_per_batch);
            auto const noise_counts =
                std::make_unique<uint16_t[]>(max_samples_per_batch);

            // TODO: reuse ray_colors for both tex samples and reducing.
            // The code might be a bit slower due to aliasing, but we'll reduce
            // the amount of memory we eat.
            auto ray_colors = std::make_unique<color[]>(max_samples_per_batch);

            auto const sample_mem = sample_memory(
                sampled_grays.get(), checker_counts.get(), solid_counts.get(),
                image_counts.get(), noise_counts.get());

            for (auto work = q->next(max_batch_size); work;
                 work = q->next(max_batch_size)) {
#ifndef _OPENMP
                // NOTE: doing this in MT will enable locks in I/O and
                // synchronize the threads, which I don't want to :(
                std::clog << "\rScanlines remaining: " << (image_height - j)
                          << ' ' << std::flush;
#endif
                auto [lane_start, lane_size] = *work;
                sample_request sreq;
                uint32_t solid_count = 0;
                sampleGeometries(world, texes, trq_mem, sample_mem, sreq,
                                 solid_count, lane_start, lane_size);
                sampleTextures(texes, fcol_ptr, perlin_noise, trq_mem,
                               sample_mem, ray_colors.get(), lane_start,
                               lane_size, sreq);
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

    void sampleGeometries(is_collection auto const& world, tex_view texes,
                          trq_memory const& trq_mem,
                          sample_memory const& sample_mem, sample_request& sreq,
                          uint32_t& solid_count, uint32_t lane_start,
                          uint32_t lane_size) {
        ZoneScopedN("geometry");
        for (uint32_t i = 0; i < lane_size; ++i) {
            // geometry

            auto const solid_counts_px =
                sample_mem.solids + i * samples_per_pixel;
            auto const noise_counts_px =
                sample_mem.noises + i * samples_per_pixel;
            auto const checker_counts_px =
                sample_mem.checkers + i * samples_per_pixel;
            auto const image_counts_px =
                sample_mem.images + i * samples_per_pixel;

            auto const j = (lane_start + i) / image_width;
            // Get a randomly-sampled camera ray for the pixel at
            // location i,j,
            // originating from the camera defocus disk.
            auto pixelCenter = getPixelCenter(float(i), float(j));

            for (uint32_t sample = 0; sample < samples_per_pixel; ++sample) {
                tex_request_queue tex_reqs(
                    trq_mem.checkers + sreq.checkers,
                    // NOTE: all solids get directly copied into the
                    // shared samples array. They will get
                    // multiplied the first.
                    trq_mem.solids + solid_count, trq_mem.images + sreq.images,
                    trq_mem.noises + sreq.noises, max_depth, texes);

                ray const r = defocusRay(center, pixelCenter);
                auto const time = random_float();

                if (simulate_ray(r, world, time, tex_reqs)) {
                    sreq.checkers += tex_reqs.checker_count;
                    solid_count += tex_reqs.solid_count;
                    sreq.images += tex_reqs.image_count;
                    sreq.noises += tex_reqs.noise_count;

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
                    trq_mem.solids[solid_count] = color(0);
                    ++solid_count;
                }
            }
        }
    }

    void sampleTextures(tex_view texes, color __restrict_arr* fcol_ptr,
                        perlin const* perlin_noise, trq_memory reqs,
                        sample_memory smpl, color __restrict_arr* ray_colors,
                        uint32_t lane_start, uint32_t lane_size,
                        sample_request sreq) const noexcept {
        // TODO: alias reqs.solids and ray_colors to get better memory
        // consumption
        ZoneScopedN("tex sample");
        multiply_init(
            [solids = reqs.solids](size_t sample) { return solids[sample]; },
            smpl.solids, size_t(samples_per_pixel) * lane_size, ray_colors);

        if (sreq.noises != 0) {
            // sample all noise functions
            for (uint32_t k = 0; k < sreq.noises; ++k) {
                auto const& info = reqs.noises[k];
                smpl.grays[k] = info.noise.value(info.p, perlin_noise);
            }
            multiply_samples(
                [grays = smpl.grays](size_t sample) { return grays[sample]; },
                smpl.noises, size_t(samples_per_pixel) * lane_size, ray_colors);
        }
        if (sreq.images != 0) {
            // sample all images
            // NOTE: I could have some pointer based mappings.
            // Sorting is out of the picture, otherwise it will move
            // samples between pixels.
            for (uint32_t k = 0; k < sreq.images; ++k) {
                auto const& info = reqs.images[k];
                reqs.solids[k] =
                    texes.images[info.image_id].sample(info.u, info.v);
            }

            multiply_samples(
                [solids = reqs.solids](size_t sample) {
                    return solids[sample];
                },
                smpl.images, size_t(samples_per_pixel) * lane_size, ray_colors);
        }
        if (sreq.checkers != 0) {
            // sample all textures
            for (uint32_t req = 0; req < sreq.checkers; ++req) {
                auto const& info = reqs.checkers[req];
                reqs.solids[req] = info.checker->value(info.p);
            }

            // multiply sampled textures
            multiply_samples([solids = reqs.solids](
                                 size_t sample) { return solids[sample]; },
                             smpl.checkers,
                             size_t(samples_per_pixel) * lane_size, ray_colors);
        }
        for (size_t i = 0; i < lane_size; ++i) {
            auto ray_colors_px = ray_colors + i * samples_per_pixel;
            // average samples
            color pixel_color(0, 0, 0);
            for (uint32_t k = 0; k < samples_per_pixel; ++k) {
                pixel_color += ray_colors_px[k];
            }

            fcol_ptr[lane_start + i] = pixel_color / float(samples_per_pixel);
        }
    }
};
