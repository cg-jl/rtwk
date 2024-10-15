#include <external/stb_image_write.h>
#include <renderer.h>
#include <texture_impls.h>
#include <trace_colors.h>

#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <print>
#include <thread>
#include <tracy/Tracy.hpp>

#include "hittable_list.h"
#include "timer.h"

using uint32 = uint32_t;

using deferNoise = std::pair<texture::noise_data, point3>;

// TODO: @maybe I could collect images by their pointer?
using deferImage = std::pair<rtw_shared_image, uvs>;

struct camera {
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
};

// @cleanup this is no longer a matrix, just arrays
struct sampleMat {
    color *solids;
    deferNoise *noises;
    deferImage *images;

    static sampleMat request(uint32 spp, uint32 maxDepth) {
        return {
            .solids = new color[spp * maxDepth],
            .noises = new deferNoise[spp * maxDepth],
            .images = new deferImage[spp * maxDepth],
        };
    }
};

struct px_sampleq {
    struct commitSave {
        int solids;
        int noises;
        int images;

        void accept(commitSave const &other) {
            solids += other.solids;
            noises += other.noises;
            images += other.images;
        }
    };

    sampleMat ptrs;
    commitSave tally;

    void emplaceSolid(color solid) { ptrs.solids[tally.solids++] = solid; }

    void emplace(texture const *tex, uvs uv, point3 p) {
        tex = traverseChecker(tex, p);
        switch (tex->kind) {
            case texture::tag::solid:
                emplaceSolid(tex->as.solid);
                break;
            case texture::tag::noise:
                ptrs.noises[tally.noises++] = {tex->as.noise, p};
                break;
            case texture::tag::image:
                ptrs.images[tally.images++] = {tex->as.image, uv};
                break;
            case texture::tag::checker:
                // Should be unreachable since we did the traverseChecker
                std::unreachable();
                break;
        }
    }

    commitSave commit() { return tally; }
    void reset() { tally = {}; }
};
static vec3 sample_square() {
    // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit
    // square.
    return vec3(random_double() - 0.5, random_double() - 0.5, 0);
}
static point3 random_in_unit_disk() {
    while (true) {
        auto p = vec3{random_double(-1., 1.), random_double(-1., 1.), 0};
        if (p.length_squared() < 1) return p;
    }
}
static point3 defocus_disk_sample(camera const &cam) {
    // Returns a random point in the camera defocus disk.
    auto p = random_in_unit_disk();
    return cam.center + (p[0] * cam.defocus_disk_u) +
           (p[1] * cam.defocus_disk_v);
}

static timed_ray get_ray(settings const &s, camera const &cam, int i, int j) {
    // Construct a camera ray originating from the defocus disk and directed
    // at a randomly sampled point around the pixel location i, j.

    auto offset = sample_square();
    auto pixel_sample = cam.pixel00_loc +
                        ((i + offset.x()) * cam.pixel_delta_u) +
                        ((j + offset.y()) * cam.pixel_delta_v);

    auto ray_origin =
        (s.defocus_angle <= 0) ? cam.center : defocus_disk_sample(cam);
    auto ray_direction = pixel_sample - ray_origin;
    auto ray_time = random_double();

    return {ray(ray_origin, ray_direction), ray_time};
}

// Aligns the normal so that it always points towards the ray origin.
// Returs whether the face is at the front.
static bool set_face_normal(vec3 in_dir, vec3 &normal) {
    auto front_face = dot(in_dir, normal) < 0;
    normal = front_face ? normal : -normal;
    return front_face;
}

static vec3 random_in_unit_sphere() {
    while (true) {
        auto p = random_vec(-1, 1);
        if (p.length_squared() < 1) return p;
    }
}

static color geometrySim(color const &background, timed_ray r, int depth,
                         hittable_list const &world, px_sampleq &attenuations) {
    for (;;) {
        // Too deep and haven't found a light source.
        if (depth <= 0) {
            attenuations.reset();
            return color(0, 0, 0);
        }
        ZoneScopedN("ray frame");

        // If the ray hits nothing, return the background color.
        auto [res, closestHit] = world.hitSelect(r);

        auto maxT = res ? closestHit : infinity;

        uvs uv;

        // NOTE: I have a constant problem where the camera is inside a
        // constant medium, so getting a ray through means getting through a
        // medium. I can't predict where/if the ray is going to disperse. So I
        // just haveh to run both hitSelect and sampleConstantMediums.

        // Try sampling a constant medium
        double cmHit;
        if (auto *cmColor = world.sampleConstantMediums(r, maxT, &cmHit)) {
            // Don't need UVs/normal; we have an isotropic material.
            r.r.orig = r.r.at(cmHit);
            attenuations.emplaceSolid(*cmColor);

            r.r.dir = unit_vector(random_in_unit_sphere());
            --depth;
            continue;
        }

        if (!res) {
            attenuations.reset();
            return background;
        }

        auto p = r.r.at(closestHit);
        auto normal = res.getNormal(p, r.time);

        auto front_face = set_face_normal(r.r.dir, normal);
        {
            ZoneScopedN("getUVs");
            ZoneColor(tracy::Color::SteelBlue);
            uv = res.getUVs(p, normal);
        }

        vec3 scattered;

        auto const &[mat, tex] = world.objects[res.relIndex];

        // here we'll have to use the emit value as the 'attenuation' value.
        if (mat.tag == material::kind::diffuse_light) {
            attenuations.emplace(tex, uv, p);
            return color(1, 1, 1);
        }

        if (!mat.scatter(r.r.dir, normal, front_face, scattered)) {
            attenuations.reset();
            return color(0, 0, 0);
        }

        depth = depth - 1;
        attenuations.emplace(tex, uv, p);
        r.r = ray(p, scattered);
    }
}

// NOTE: @misname Not really RLE, just avoiding zeros.
struct RLE {
    int location;
    int count;
};

struct countArrays {
    RLE *solids;
    RLE *noises;
    RLE *images;

    static countArrays request(uint32 spp) {
        return {
            .solids = new RLE[spp],
            .noises = new RLE[spp],
            .images = new RLE[spp],
        };
    }
};

// NOTE: @maybe @perf We can't do the transpose of the depth/sample matrix
// because we have to multiply first. We still could gather multiple rays once
// we have the scene separated by kind.

// Things to keep:
// - samples >>> depth
// - depth < saturated threshold: The number of bounces is not enough
//   for a bulk algorithm (which relies on most of the work being saturated)
//   to be beneficial.
// - depth might be > transitory threshold, but that's the only option we have
// right now.
//   if it's in between, any algorithm (saturated or transitory) will behave
//   mostly the same.

struct Scanline_Buffers {
    sampleMat attMat;
    countArrays counts;
    double *multiplyBuffer;
    color *samples;

    static Scanline_Buffers request(uint32 spp, uint32 maxDepth) {
        return {
            .attMat = sampleMat::request(spp, maxDepth),
            .counts = countArrays::request(spp),
            // @cleanup could make these part of the same allocation
            .multiplyBuffer = new double[spp * maxDepth],
            .samples = new color[spp],
        };
    }
};

static void scanLine(settings const &s, camera const &cam,
                     hittable_list const &world, int const j, color *pixels,
                     Scanline_Buffers buffers, perlin const &noise) {
    // NOTE: @maybe a matrix only for the solids and vectors for the  other
    // types works better. geometrySim could also return whether it is
    // cancelling/light/background to find what the last (or first) color
    // multiplier is, and store it in one go. This would allow to do all the
    // lane multiplies in one loop.

    for (int i = 0; i < s.image_width; i++) {
        color pixel_color(0, 0, 0);

        int rleSolids = 0;
        int rleNoises = 0;
        int rleImages = 0;

        px_sampleq::commitSave tally{};

        for (int sample = 0; sample < s.samples_per_pixel; sample++) {
            // NOTE: @trace The first (bottom) lines (black, 399) are pretty bad
            // (~3.52us)
            ZoneScopedN("pixel sample");
            ZoneValue(j);
            ZoneValue(i);
            auto r = get_ray(s, cam, i, j);

            auto offset_mat = buffers.attMat;

            offset_mat.images += tally.images;
            offset_mat.noises += tally.noises;
            offset_mat.solids += tally.solids;

            px_sampleq q{offset_mat, px_sampleq::commitSave{}};

            auto bg = geometrySim(s.background, r, s.max_depth, world, q);
            tally.accept(q.tally);

            // @perf It may be better to log these counts separately so that
            // I can paint them in a 2D/3D frame.

            auto att_count = q.tally;
            ZoneTextL("tally:");
            ZoneValue(att_count.solids);
            ZoneValue(att_count.noises);
            ZoneValue(att_count.images);

            if (att_count.solids) {
                buffers.counts.solids[rleSolids++] = {sample, att_count.solids};
            }
            if (att_count.noises) {
                buffers.counts.noises[rleNoises++] = {sample, att_count.noises};
            }
            if (att_count.images) {
                buffers.counts.images[rleImages++] = {sample, att_count.images};
            }

            buffers.samples[sample] = bg;
        }

        // NOTE: @maybe consider filling the color matrix with 1s where samples
        // shouldn't be recorded. That way loops could be fixed at least.

        {
            ZoneScopedNC("attenuation samples", Ctp::Peach);
            {
                ZoneScopedN("noises");
                ZoneColor(tracy::Color::Blue4);

                {
                    // @perf I can further simplify this because
                    // `sample_noise`'s components are all the same, so I could
                    // just fill an array of doubles.
                    ZoneScopedN("sample");
                    for (int i = 0; i < tally.noises; ++i) {
                        auto const &[noiseData, p] = buffers.attMat.noises[i];
                        buffers.multiplyBuffer[i] =
                            sample_noise(noiseData, p, noise);
                    }
                }

                // @cutnpaste with images, solids.
                {
                    ZoneScopedN("mul");
                    int start = 0;
                    for (int rleI = 0; rleI < rleNoises; ++rleI) {
                        auto [sample, count] = buffers.counts.noises[rleI];
                        color res = buffers.samples[sample];
                        for (auto grayscale :
                             std::span(buffers.multiplyBuffer + start, count)) {
                            res = res * grayscale;
                        }
                        start += count;
                        buffers.samples[sample] = res;
                    }
                }
            }

            {
                ZoneScopedN("images");
                ZoneColor(tracy::Color::Lavender);

                //  @perf This is pretty slow. The loop takes most
                //  of the credit, where Tracy shows two big stalls on loop
                //  entry & exit. Self time is ~50%
                int start = 0;
                for (int rleI = 0; rleI < rleImages; ++rleI) {
                    auto [sample, count] = buffers.counts.images[rleI];
                    color res = buffers.samples[sample];
                    for (auto const &[image, uv] :
                         std::span(buffers.attMat.images + start, count)) {
                        res = res * sample_image(image, uv);
                    }
                    start += count;
                    buffers.samples[sample] = res;
                }
            }

            {
                ZoneScopedN("solids");
                ZoneColor(Ctp::Pink);

                // @cutnpaste with noises, images.
                int start = 0;
                for (int rleI = 0; rleI < rleSolids; ++rleI) {
                    auto [sample, count] = buffers.counts.solids[rleI];
                    color res = buffers.samples[sample];

                    for (auto const &col :
                         std::span(buffers.attMat.solids + start, count)) {
                        res = res * col;
                    }

                    start += count;
                    buffers.samples[sample] = res;
                }
            }
        }

        for (int sample = 0; sample < s.samples_per_pixel; ++sample) {
            pixel_color += buffers.samples[sample];
        }

        pixels[j * s.image_width + i] = cam.pixel_samples_scale * pixel_color;
    }
}

static void renderThread(settings const &s, camera const &cam,
                         std::atomic<int> &__restrict__ tileid,
                         std::atomic<int> &__restrict__ remain_scanlines,
                         size_t const stop_at, hittable_list const &world,
                         color *pixels) noexcept {
    // NOTE: @waste @mem Could reuse a solids lane (maybe the last/first one)
    // for the final lane.

    auto buffers = Scanline_Buffers::request(s.samples_per_pixel, s.max_depth);

    auto noise = std::make_unique<perlin>();

    for (;;) {
        auto j = tileid.fetch_add(1, std::memory_order_acq_rel);

        if (j >= s.image_width) return;

        // TODO: render worker state struct
        scanLine(s, cam, world, j, pixels, buffers, *noise.get());

        remain_scanlines.fetch_sub(1, std::memory_order_acq_rel);
        remain_scanlines.notify_one();
    }
}

static camera make_camera(settings const &s) {
    camera cam;
    cam.image_height = int(s.image_width / s.aspect_ratio);
    cam.image_height = (cam.image_height < 1) ? 1 : cam.image_height;

    cam.pixel_samples_scale = 1.0 / s.samples_per_pixel;

    cam.center = s.lookfrom;

    // Determine viewport dimensions.
    auto theta = degrees_to_radians(s.vfov);
    auto h = tan(theta / 2);
    auto viewport_height = 2 * h * s.focus_dist;
    auto viewport_width =
        viewport_height * (double(s.image_width) / cam.image_height);

    // Calculate the u,v,w unit basis vectors for the camera coordinate
    // frame.
    cam.w = unit_vector(s.lookfrom - s.lookat);
    cam.u = unit_vector(cross(s.vup, cam.w));
    cam.v = cross(cam.w, cam.u);

    // Calculate the vectors across the horizontal and down the vertical
    // viewport edges.
    vec3 viewport_u =
        viewport_width * cam.u;  // Vector across viewport horizontal edge
    vec3 viewport_v =
        viewport_height * -cam.v;  // Vector down viewport vertical edge

    // Calculate the horizontal and vertical delta vectors from pixel to
    // pixel.
    cam.pixel_delta_u = viewport_u / s.image_width;
    cam.pixel_delta_v = viewport_v / cam.image_height;

    // Calculate the location of the upper left pixel.
    auto viewport_upper_left =
        cam.center - (s.focus_dist * cam.w) - viewport_u / 2 - viewport_v / 2;
    cam.pixel00_loc =
        viewport_upper_left + 0.5 * (cam.pixel_delta_u + cam.pixel_delta_v);

    // Calculate the camera defocus disk basis vectors.
    auto defocus_radius =
        s.focus_dist * tan(degrees_to_radians(s.defocus_angle / 2));
    cam.defocus_disk_u = cam.u * defocus_radius;
    cam.defocus_disk_v = cam.v * defocus_radius;
    return cam;
}

void render(hittable_list world, settings const &s) {
    auto cam = make_camera(s);
    auto pixels = std::make_unique<color[]>(size_t(s.image_width) *
                                            size_t(cam.image_height));

    int start = cam.image_height;
    static constexpr int stop_at = 0;
    std::atomic<int> remain_scanlines alignas(64){start};

    auto progress_thread = std::thread([limit = start, &remain_scanlines]() {
        auto last_remain = limit + 1;
        while (true) {
            remain_scanlines.wait(last_remain, std::memory_order_acquire);

            auto remain = remain_scanlines.load(std::memory_order_acquire);
            last_remain = remain;
            std::clog << "\r\x1b[2K\x1b[?25lScanlines remaining: " << remain
                      << "\x1b[?25h" << std::flush;
            if (remain == stop_at) break;
        }
        std::clog << "\r\x1b[2K" << std::flush;
    });

#ifdef _OPENMP
#else
    // TODO: if I keep adding atomic things, then single threaded
    // performance will be lost.
#endif

    rtwk::stopwatch render_timer;
    render_timer.start();
    std::atomic<int> tileid alignas(64);
    // worker loop
#pragma omp parallel
    {
        ::renderThread(s, cam, tileid, remain_scanlines, stop_at, world,
                       pixels.get());
    }
    auto render_time = render_timer.stop();
    rtwk::print_duration(std::cout, "Render", render_time);
    progress_thread.join();

    std::clog << "\r\x1b[2KWriting image...\n";

    // 1. Encode the image into RGB
    auto bytes =
        std::make_unique<uint8_t[]>(s.image_width * cam.image_height * 3);

    for (size_t i = 0; i < s.image_width * cam.image_height; ++i) {
        auto const &pixel_color = pixels[i];
        auto r = pixel_color.x();
        auto g = pixel_color.y();
        auto b = pixel_color.z();

        // Apply a linear to gamma transform for gamma 2
        r = linear_to_gamma(r);
        g = linear_to_gamma(g);
        b = linear_to_gamma(b);

        // Translate the [0,1] component values to the byte range [0,255].
        static interval const intensity(0.000, 0.999);
        bytes[3 * i + 0] = uint8_t(256 * intensity.clamp(r));
        bytes[3 * i + 1] = uint8_t(256 * intensity.clamp(g));
        bytes[3 * i + 2] = uint8_t(256 * intensity.clamp(b));
    }

    stbi_write_png("test.png", s.image_width, cam.image_height, 3, &bytes[0],
                   0);

    std::clog << "Done.\n";
}
