#include <camera.h>
#include <renderer.h>
#include <texture_impls.h>
#include <trace_colors.h>

#include <atomic>
#include <cstdint>
#include <print>
#include <tracy/Tracy.hpp>

#include "hittable_list.h"

using uint32 = uint32_t;

// NOTE: doesn't manage the lifetimes of `T`.
template <typename T>
struct boundedDynArr {
    T *elems{};
    size_t count{};
#if _DEBUG
    size_t cap;
#endif

    auto as_span() const { return std::span{elems, count}; }

    void push_back(T val) { elems[count++] = val; }
};

using deferNoise = std::pair<texture::noise_data, point3>;

// TODO: @maybe I could collect images by their pointer?
using deferImage = std::pair<rtw_shared_image, uvs>;

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

    sampleMat offset(uint32 sample, uint32 maxDepth) const noexcept {
        return {
            .solids = solids + sample * maxDepth,
            .noises = noises + sample * maxDepth,
            .images = images + sample * maxDepth,
        };
    }
};

struct px_sampleq {
    struct commitSave {
        int solids;
        int noises;
        int images;

        void accept(commitSave const &other) {
            solids |= other.solids;
            noises |= other.noises;
            images |= other.images;
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

    constexpr std::span<color> solids() const {
        return {ptrs.solids, size_t(tally.solids)};
    }
    constexpr std::span<deferNoise> noises() const {
        return {ptrs.noises, size_t(tally.noises)};
    }
    constexpr std::span<deferImage> images() const {
        return {ptrs.images, size_t(tally.images)};
    }
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

static ray get_ray(camera const &cam, int i, int j) {
    // Construct a camera ray originating from the defocus disk and directed
    // at a randomly sampled point around the pixel location i, j.

    auto offset = sample_square();
    auto pixel_sample = cam.pixel00_loc +
                        ((i + offset.x()) * cam.pixel_delta_u) +
                        ((j + offset.y()) * cam.pixel_delta_v);

    auto ray_origin =
        (cam.defocus_angle <= 0) ? cam.center : defocus_disk_sample(cam);
    auto ray_direction = pixel_sample - ray_origin;
    auto ray_time = random_double();

    return ray(ray_origin, ray_direction, ray_time);
}

// Aligns the normal so that it always points towards the ray origin.
// Returs whether the face is at the front.
static bool set_face_normal(ray const &r, vec3 &normal) {
    auto front_face = dot(r.dir, normal) < 0;
    normal = front_face ? normal : -normal;
    return front_face;
}

static vec3 random_in_unit_sphere() {
    while (true) {
        auto p = random_vec(-1, 1);
        if (p.length_squared() < 1) return p;
    }
}

static color geometrySim(color const &background, ray r, int depth,
                         hittable_list const &world, px_sampleq &attenuations) {
    for (;;) {
        // Too deep and haven't found a light source.
        if (depth <= 0) {
            attenuations.reset();
            return color(0, 0, 0);
        }
        ZoneScopedN("ray frame");

        double closestHit;

        // If the ray hits nothing, return the background color.
        auto *res = world.hitSelect(r, &closestHit);

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
            r.orig = r.at(cmHit);
            attenuations.emplaceSolid(*cmColor);

            r.dir = unit_vector(random_in_unit_sphere());
            --depth;
            continue;
        }

        if (!res) {
            attenuations.reset();
            return background;
        }

        auto p = r.at(closestHit);
        auto normal = res->getNormal(p, r.time);

        auto front_face = set_face_normal(r, normal);
        {
            ZoneScopedN("getUVs");
            ZoneColor(tracy::Color::SteelBlue);
            uv = res->getUVs(p, normal);
        }

        vec3 scattered;

        auto const &[mat, tex] = world.objects[res->relIndex];

        // here we'll have to use the emit value as the 'attenuation' value.
        if (mat.tag == material::kind::diffuse_light) {
            attenuations.emplace(tex, uv, p);
            return color(1, 1, 1);
        }

        if (!mat.scatter(r.dir, normal, front_face, scattered)) {
            attenuations.reset();
            return color(0, 0, 0);
        }

        depth = depth - 1;
        attenuations.emplace(tex, uv, p);
        r = ray(p, scattered, r.time);
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

static void scanLine(camera const &cam, hittable_list const &world, int const j,
                     color *pixels, sampleMat attMat, countArrays counts,
                     color *samples, perlin const &noise) {
    // NOTE: @maybe a matrix only for the solids and vectors for the  other
    // types works better. geometrySim could also return whether it is
    // cancelling/light/background to find what the last (or first) color
    // multiplier is, and store it in one go. This would allow to do all the
    // lane multiplies in one loop.

    for (int i = 0; i < cam.image_width; i++) {
        color pixel_color(0, 0, 0);

        int rleSolids = 0;
        int rleNoises = 0;
        int rleImages = 0;

        for (int sample = 0; sample < cam.samples_per_pixel; sample++) {
            // NOTE: @trace The first (bottom) lines (black, 399) are pretty bad
            // (~3.52us)
            ZoneScopedN("pixel sample");
            ZoneValue(j);
            ZoneValue(i);
            ray r = get_ray(cam, i, j);

            px_sampleq q{attMat.offset(sample, cam.max_depth),
                         px_sampleq::commitSave{}};

            auto bg = geometrySim(cam.background, r, cam.max_depth, world, q);

            // @perf It may be better to log these counts separately so that
            // I can paint them in a 2D/3D frame.

            auto att_count = q.tally;
            ZoneTextL("tally:");
            ZoneValue(att_count.solids);
            ZoneValue(att_count.noises);
            ZoneValue(att_count.images);

            if (att_count.solids) {
                counts.solids[rleSolids++] = {sample, att_count.solids};
            }
            if (att_count.noises) {
                counts.noises[rleNoises++] = {sample, att_count.noises};
            }
            if (att_count.images) {
                counts.images[rleImages++] = {sample, att_count.images};
            }

            samples[sample] = bg;
        }

        // NOTE: @maybe consider filling the color matrix with 1s where samples
        // shouldn't be recorded. That way loops could be fixed at least.

        // NOTE: @waste There's no point in having lanes for noises/images
        // if I still have to encode the samples and lengths. They are also much
        // less frequent than their solid counterparts. @maybe having one vector
        // each and sampling those in bulk is better.

        {
            ZoneScopedNC("attenuation samples", Ctp::Peach);
            {
                ZoneScopedN("noises");
                ZoneColor(tracy::Color::Blue4);

                for (int rleI = 0; rleI < rleNoises; ++rleI) {
                    auto [sample, count] = counts.noises[rleI];
                    color res = samples[sample];
                    // NOTE: @maybe Separating each attenuation matrix for each
                    // type of texture may have impact in loading the length
                    // here.
                    // @maybe Keeping an iterable set of which samples don't
                    // have zero of these may be also interesting, to skip
                    // loading zeros without relying on the loop.
                    for (auto const &[noiseData, p] : std::span(
                             attMat.noises + sample * cam.max_depth, count)) {
                        res = res * sample_noise(noiseData, p, noise);
                    }
                    samples[sample] = res;
                }
            }

            {
                // NOTE: This is pretty slow. The loop takes most of the credit,
                // where Tracy shows a ton of stalls (99% in loop, 1% in
                // sample_image)
                ZoneScopedN("images");
                ZoneColor(tracy::Color::Lavender);
                for (int rleI = 0; rleI < rleImages; ++rleI) {
                    auto [sample, count] = counts.images[rleI];
                    color res = samples[sample];
                    for (auto const &[image, uv] : std::span(
                             attMat.images + sample * cam.max_depth, count)) {
                        res = res * sample_image(image, uv);
                    }
                    samples[sample] = res;
                }
            }

            {
                ZoneScopedN("solids");
                ZoneColor(Ctp::Pink);
                for (int rleI = 0; rleI < rleSolids; ++rleI) {
                    auto [sample, count] = counts.solids[rleI];
                    color res = samples[sample];

                    for (auto const &col : std::span(
                             attMat.solids + sample * cam.max_depth, count)) {
                        res = res * col;
                    }

                    samples[sample] = res;
                }
            }
        }

        for (int sample = 0; sample < cam.samples_per_pixel; ++sample) {
            pixel_color += samples[sample];
        }

        pixels[j * cam.image_width + i] = cam.pixel_samples_scale * pixel_color;
    }
}

void render(camera const &cam, std::atomic<int> &tileid,
            std::atomic<int> &remain_scanlines, size_t const stop_at,
            hittable_list const &world, color *pixels) noexcept {
    // NOTE: @waste @mem Could reuse a solids lane (maybe the last/first one)
    // for the final lane.
    auto samples = std::make_unique<color[]>(size_t(cam.samples_per_pixel));
    auto attMat = sampleMat::request(cam.samples_per_pixel, cam.max_depth);
    auto counts = countArrays::request(cam.samples_per_pixel);

    perlin noise;

    for (;;) {
        auto j = tileid.fetch_add(1, std::memory_order_acq_rel);

        if (j >= cam.image_width) return;

        // TODO: render worker state struct
        scanLine(cam, world, j, pixels, attMat, counts, samples.get(), noise);

        remain_scanlines.fetch_sub(1, std::memory_order_acq_rel);
        remain_scanlines.notify_one();
    }
}
