#include <external/stb_image_write.h>
#include <renderer.h>
#include <texture_impls.h>
#include <trace_colors.h>

#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <print>
#include <ranges>
#include <thread>
#include <tracy/Tracy.hpp>

#include "hittable_list.h"
#include "rtweekend.h"
#include "timer.h"

using deferNoise = std::pair<texture::noise_data, point3>;

// TODO: @maybe I could collect images by their pointer?
using deferImage = std::pair<rtw_shared_image, uvs>;

// Origin is at world origin.
struct camera {
    int image_height; // Rendered image height
    point3 pixel00_loc; // Location of pixel 0, 0
    vec3 pixel_delta_u; // Offset to pixel to the right
    vec3 pixel_delta_v; // Offset to pixel below
    vec3 u, v, w; // Camera frame basis vectors
    vec3 defocus_disk_u; // Defocus disk horizontal radius
    vec3 defocus_disk_v; // Defocus disk vertical radius
};

// @cleanup this is no longer a matrix, just arrays
struct sampleMat {
    color *solids;
    deferNoise *noises;
    deferImage *images;

    static sampleMat request(uint32 spp, uint32 maxDepth)
    {
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

        void accept(commitSave const &other)
        {
            solids += other.solids;
            noises += other.noises;
            images += other.images;
        }
    };

    sampleMat ptrs;
    commitSave tally;

    void emplaceSolid(color solid) { ptrs.solids[tally.solids++] = solid; }

    void emplace(texture const *tex, uvs uv, point3 p)
    {
        tex = traverseChecker(tex, p);
        switch (tex->kind) {
        case texture::tag::solid:
            emplaceSolid(tex->as.solid);
            break;
        case texture::tag::noise:
            ptrs.noises[tally.noises++] = { tex->as.noise, p };
            break;
        case texture::tag::image:
            ptrs.images[tally.images++] = { tex->as.image, uv };
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
static vec3 sample_square()
{
    // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit
    // square.
    return vec3(random_double() - 0.5, random_double() - 0.5, 0);
}
static point3 random_in_unit_disk()
{
    while (true) {
        auto p = vec3 { random_double(-1., 1.), random_double(-1., 1.), 0 };
        if (p.length_squared() < 1)
            return p;
    }
}
static point3 defocus_disk_sample(camera const &cam)
{
    // Returns a random point in the camera defocus disk.
    auto p = random_in_unit_disk();
    return (p[0] * cam.defocus_disk_u) + (p[1] * cam.defocus_disk_v);
}

static ray get_ray(settings const &s, camera const &cam, int i, int j)
{
    // Construct a camera ray originating from the defocus disk and directed
    // at a randomly sampled point around the pixel location i, j.

    auto offset = sample_square();
    auto pixel_sample = cam.pixel00_loc + ((i + offset.x()) * cam.pixel_delta_u) + ((j + offset.y()) * cam.pixel_delta_v);

    auto ray_origin = (s.defocus_angle <= 0) ? vec3 { 0, 0, 0 } : defocus_disk_sample(cam);
    auto ray_direction = pixel_sample - ray_origin;

    return ray(ray_origin, ray_direction);
}

// Aligns the normal so that it always points towards the ray origin.
// Returs whether the face is at the front.
static bool set_face_normal(vec3 in_dir, vec3 &normal)
{
    auto front_face = dot(in_dir, normal) < 0;
    normal = front_face ? normal : -normal;
    return front_face;
}

static vec3 random_in_unit_sphere()
{
    while (true) {
        auto p = random_vec(-1, 1);
        if (p.length_squared() < 1)
            return p;
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

    static countArrays request(uint32 spp)
    {
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

using select_res = std::pair<geometry_ptr, double>;
using cm_res = std::pair<color const *, double>;

struct hit_record {
    vec3 normal;
    uvs uv;
    bool is_front;
};

struct GSim_Buffers {
    // inputs
    ray_buffer rays;
    px_sampleq *atts;
    select_res *hit_selects;
    cm_res *constant_mediums;
    hit_record *hit_recs;
    vec3 *scatters;
    SampleCM_Buffers sample_cms;
    Select_Buffers select;

    static GSim_Buffers request(uint32 spp)
    {
        return {
            .rays = {
                .rays = new ray[spp],
                .times = new double[spp],
            },
            .atts = new px_sampleq[spp],
            .hit_selects = new select_res[spp],
            .constant_mediums = new cm_res[spp],
            .hit_recs = new hit_record[spp],
            .scatters = new vec3[spp],
            .sample_cms = SampleCM_Buffers::request(spp),
            .select = Select_Buffers::request(spp),
        };
    }
};

struct Scanline_Buffers {
    sampleMat attMat;
    countArrays counts;
    double *multiplyBuffer;
    color *samples;
    GSim_Buffers gsim;

    static Scanline_Buffers request(uint32 spp, uint32 maxDepth)
    {
        return {
            .attMat = sampleMat::request(spp, maxDepth),
            .counts = countArrays::request(spp),
            // @cleanup could make these part of the same allocation
            .multiplyBuffer = new double[spp * maxDepth],
            .samples = new color[spp],
            .gsim = GSim_Buffers::request(spp),
        };
    }
};

// FIXME: I'm in the middle of a refactoring.
// I am trying to make everything reflect the multi-to-multi dynamism of rays,
// so I have to think about processing multiple rays faster. Since I'm already
// using parallelism in the form of threading, I have to include parallelism
// on single threaded, which means trying to process rays in bulk as fast as
// possible.
//
// First I'm going to make everything slower by introducing the dependencies and
// results of geometrySim as arrays: generate rays - run simulations for
// px_sampleq's independently - join px_sampleq's buffers so I can keep the bulk
// processing of color sampling

static void gsim(color const &background, uint32 const spp,
    uint32 const max_depth, hittable_list const &world,
    GSim_Buffers buffers, color *samples)
{
    // TODO: to transpose the loop, I will need a 'multi slice swap' or
    // 'multiswap' where I swap the current ray being sampled, its queue and its
    // output color with the last one on the array. This way I will only run the
    // rays I care about, filtering out the rest. I need multiswap because I
    // want to keep the correspondence of rays[index] with atts[index] and
    // samples[index].

    auto swap = [&](auto i, decltype(i) j) {
        assert(i != j);
        // @perf samples don't actually need swapping.
        // Reason is we're not touching them once they're set.
        std::swap(samples[i], samples[j]);
        buffers.rays.swap(i, j);
        std::swap(buffers.atts[i], buffers.atts[j]);
        // @perf from `cms_end` onwards the constant_mediums array is not used.
        std::swap(buffers.constant_mediums[i], buffers.constant_mediums[j]);
        // @perf hit_selects is initialized when drawing constant mediums.
        std::swap(buffers.hit_selects[i], buffers.hit_selects[j]);
        // @perf hit_recs is only initialized for cms_end..nohits_begin
        std::swap(buffers.hit_recs[i], buffers.hit_recs[j]);
    };

    auto hit_select_swap = [&](auto const i, auto const k) {
        buffers.rays.swap(i, k);
        std::swap(buffers.atts[i], buffers.atts[k]);
        std::swap(buffers.hit_selects[i], buffers.hit_selects[k]);
    };

    auto cms_and_hitsels_swap = [&](auto const i, auto const k) {
        buffers.rays.swap(i, k);
        std::swap(buffers.atts[i], buffers.atts[k]);
        std::swap(buffers.hit_selects[i], buffers.hit_selects[k]);
        std::swap(buffers.constant_mediums[i], buffers.constant_mediums[k]);
    };

    auto swap_hitsels_hitrecs = [&](auto const i, auto const k) {
        buffers.rays.swap(i, k);
        std::swap(buffers.atts[i], buffers.atts[k]);
        std::swap(buffers.hit_selects[i], buffers.hit_selects[k]);
        std::swap(buffers.hit_recs[i], buffers.hit_recs[k]);
    };

    // @perf check if fill() with nontemporal writes does something interesting.

    auto remaining = spp;
    for (auto depth = max_depth; remaining; --depth) {
        if (depth == 0) {
            std::for_each(buffers.atts, buffers.atts + remaining,
                [](auto &att) { att.reset(); });
            std::fill(samples, samples + remaining, color { 0, 0, 0 });
            break;
        }
        ZoneScopedN("ray tick");
        ZoneValue(remaining);

        // NOTE: I have a constant problem where the camera is
        // inside a constant medium, so getting a ray through means
        // getting through a medium. I can't predict where/if the
        // ray is going to disperse. So I just have to run both
        // hitSelect and sampleConstantMediums.

        // We only need to swap hit_selects, atts and rays here.
        // Color samples and other kinds of samples are uninitialized.
        // Initialized color samples are left untouched.
        world.select(buffers.rays, remaining, buffers.select,
            buffers.hit_selects, hit_select_swap);

        // What happens here? why is doing less swaps more costly here?
        world.sampleCMs(buffers.rays, remaining, buffers.constant_mediums,
            buffers.sample_cms, hit_select_swap);

        using std::views::iota;

        auto const cms_end = partition(decltype(remaining)(0), remaining, cms_and_hitsels_swap, [&](auto i) {
            auto [res, closestHit] = buffers.hit_selects[i];
            auto [cmColor, cmHit] = buffers.constant_mediums[i];
            return cmColor and (!res or cmHit < closestHit);
        });

        auto const nohits_begin = partition(cms_end, remaining, hit_select_swap, [&](auto i) {
            auto const &res = buffers.hit_selects[i].first;

            return bool(res);
        });

        for (uint32 start = cms_end; start < nohits_begin;) {
            auto const res = buffers.hit_selects[start].first;
            auto const end = partition(start + 1, nohits_begin, hit_select_swap, [&](auto const i) { return buffers.hit_selects[i].first == res; });

            std::transform(
                buffers.hit_selects + start, buffers.hit_selects + end,
                std::views::iota(start).begin(),
                buffers.hit_recs + start,
                [&](auto const &hit_res, auto const i) {
                    auto [res, closestHit] = hit_res;
                    auto const &r = buffers.rays[i];
                    // @perf p is cheap, rest aren't.
                    auto p = r.r.at(closestHit);
                    auto normal = res.getNormal(p, r.time);
                    auto front_face = set_face_normal(r.r.dir, normal);
                    // @perf getUVs not always depends on both 'p' and 'normal'.
                    // Since 'normal' is not cheap, maybe we want to know when
                    // normal is required and when it isn't (partition?)
                    auto uv = res.getUVs(p, normal);

                    return hit_record { normal, uv, front_face };
                });

            start = end;
        }

        // @perf Think about making lights be at the end of all so that the
        // zeroed-queue region is conntiguous.
        auto const lights_begin = partition(cms_end, nohits_begin, swap_hitsels_hitrecs, [&](auto i) {
            auto const &res = buffers.hit_selects[i].first;
            auto const &mat = world.objects[res.relIndex].mat;
            return mat.tag != material::kind::diffuse_light;
        });

        for (uint32 start = cms_end; start < lights_begin;) {
            auto const mat_index = buffers.hit_selects[start].first.relIndex;
            auto const end = partition(start + 1, lights_begin, swap_hitsels_hitrecs, [&](auto const i) {
                return buffers.hit_selects[i].first.relIndex == mat_index;
            });

            std::transform(
                buffers.hit_selects + start, buffers.hit_selects + end,
                std::views::iota(start).begin(),
                buffers.scatters + start, [&](auto const &hit_res, auto i) {
                    auto [res, closestHit] = hit_res;
                    auto const &r = buffers.rays[i];

                    // @perf p is cheap, rest aren't.
                    auto const &[normal, uv, front_face] = buffers.hit_recs[i];

                    auto const &[mat, tex] = world.objects[res.relIndex];

                    return mat.scatter(r.r.dir, normal, front_face);
                });

            start = end;
        }

        auto const bounces_end = partition(cms_end, lights_begin, swap,
            [&](auto i) { return !buffers.scatters[i].near_zero(); });

        // cms_end | <unsorted> | lights | nohit

        // @perf we only know whether a ray is bounced when all the checks for
        // other things have failed. Since we want bounces to the beginning, try
        // sorting the other partitions first towards the right (negated) so
        // that the bounces are left in the same place as now (after the cms).

        // cms_end | bounces | no scatter | lights | nohit

        // Constant mediums
        // @perf separate the constant medium results in two so that these two
        // loops run on independent data structures.
        for (decltype(remaining) i = 0; i < cms_end; ++i) {
            auto &q = buffers.atts[i];
            auto cmColor = buffers.constant_mediums[i].first;
            q.emplaceSolid(*cmColor);
        }
        for (decltype(remaining) i = 0; i < cms_end; ++i) {
            auto &r = buffers.rays.rays[i];
            auto cmHit = buffers.constant_mediums[i].second;
            r.orig = r.at(cmHit);
        }

        // @perf This contains random samples.
        for (decltype(remaining) i = 0; i < cms_end; ++i) {
            auto &r = buffers.rays.rays[i];
            r.dir = unit_vector(random_in_unit_sphere());
        }

        // Bounces (but not constant mediums)
        for (decltype(remaining) i = cms_end; i < bounces_end; ++i) {
            auto &q = buffers.atts[i];
            auto [res, closestHit] = buffers.hit_selects[i];
            auto const &r = buffers.rays[i];

            // @perf p is cheap, rest aren't.
            auto p = r.r.at(closestHit);
            auto const &[normal, uv, front_face] = buffers.hit_recs[i];

            auto const &tex = world.objects[res.relIndex].tex;

            q.emplace(tex, uv, p);
        }

        for (decltype(remaining) i = cms_end; i < bounces_end; ++i) {
            auto closestHit = buffers.hit_selects[i].second;
            auto &r = buffers.rays.rays[i];

            // @perf p is cheap, rest aren't.
            auto p = r.at(closestHit);
            auto const &scattered = buffers.scatters[i];
            r = ray(p, scattered);
        }

        // Non-bounces: lights, no hits and no scatters.

        // cms_end | bounces | no scatter | lights | nohit
        for (decltype(remaining) i = lights_begin; i < nohits_begin; ++i) {
            auto &q = buffers.atts[i];
            auto [res, closestHit] = buffers.hit_selects[i];
            auto const &r = buffers.rays[i];

            // @perf p is cheap, rest aren't.
            auto p = r.r.at(closestHit);
            auto const &[normal, uv, front_face] = buffers.hit_recs[i];

            auto const &[mat, tex] = world.objects[res.relIndex];

            q.emplace(tex, uv, p);
        }

        // @perf this loop is equivalent to fill with skips due to commitSave
        // offset.
        // cms_end | bounces | no scatter | lights | nohit
        for (decltype(remaining) i = bounces_end; i < lights_begin; ++i) {
            auto &q = buffers.atts[i];
            q.reset();
        }
        for (decltype(remaining) i = nohits_begin; i < remaining; ++i) {
            auto &q = buffers.atts[i];
            q.reset();
        }

        std::fill(samples + lights_begin, samples + nohits_begin,
            color { 1, 1, 1 });

        std::fill(samples + bounces_end, samples + lights_begin,
            color { 0, 0, 0 });
        std::fill(samples + nohits_begin, samples + remaining, background);

        // only cmResults and bounces get to the next level.
        remaining = bounces_end;
    }
}

static void scanLine(settings const &s, camera const &cam,
    hittable_list const &world, int const j, color *pixels,
    Scanline_Buffers buffers, perlin const &noise)
{
    // NOTE: @maybe a matrix only for the solids and vectors for the  other
    // types works better. geometrySim could also return whether it is
    // cancelling/light/background to find what the last (or first) color
    // multiplier is, and store it in one go. This would allow to do all the
    // lane multiplies in one loop.

    for (int i = 0; i < s.image_width; i++) {
        // Initialize all the rays
        std::generate(buffers.gsim.rays.rays,
            buffers.gsim.rays.rays + s.samples_per_pixel,
            [&]() { return get_ray(s, cam, i, j); });

        std::generate(buffers.gsim.rays.times, buffers.gsim.rays.times + s.samples_per_pixel, []() { return random_double(); });

        // @perf Could do queue init before all of this, and just reset all each
        // iteration. That (filling with zeros with a stride) is easier to do
        // than calculating each offset. Initialize all queues. Since rays are
        // going to be run in parallel, each sample queue is independent from
        // each other.
        for (int sample = 0; sample < s.samples_per_pixel; ++sample) {
            auto offset_mat = buffers.attMat;

            offset_mat.images += sample * s.max_depth;
            offset_mat.noises += sample * s.max_depth;
            offset_mat.solids += sample * s.max_depth;
            new (&buffers.gsim.atts[sample]) px_sampleq { offset_mat, {} };
        }

        gsim(s.background, s.samples_per_pixel, s.max_depth, world,
            buffers.gsim, buffers.samples);

        // @perf I could try to distribute this loop as this is just a reduction
        // loop. Once a queue is finished loading, I can send it and not worry
        // about sampling till I have to do anything else. What we did here was
        // reducing multiple queues into a single buffer to then be processed.
        // Might be just another step that we have to queue the reduction of and
        // when submitted we can then do the color computations.
        px_sampleq::commitSave tally {};

        int rleSolids = 0;
        int rleNoises = 0;
        int rleImages = 0;
        for (int sample = 0; sample < s.samples_per_pixel; ++sample) {
            auto const &q = buffers.gsim.atts[sample];

            // Use reverse copy because these could be aliasing, in a high load
            // context.
            std::reverse_copy(q.ptrs.solids, q.ptrs.solids + q.tally.solids,
                buffers.attMat.solids + tally.solids);
            std::reverse_copy(q.ptrs.noises, q.ptrs.noises + q.tally.noises,
                buffers.attMat.noises + tally.noises);
            std::reverse_copy(q.ptrs.images, q.ptrs.images + q.tally.images,
                buffers.attMat.images + tally.images);
            tally.accept(q.tally);

            // @perf It may be better to log these counts separately so that
            // I can paint them in a 2D/3D frame.

            auto att_count = q.tally;
            ZoneTextL("tally:");
            ZoneValue(att_count.solids);
            ZoneValue(att_count.noises);
            ZoneValue(att_count.images);

            if (att_count.solids) {
                buffers.counts.solids[rleSolids++] = { sample, att_count.solids };
            }
            if (att_count.noises) {
                buffers.counts.noises[rleNoises++] = { sample, att_count.noises };
            }
            if (att_count.images) {
                buffers.counts.images[rleImages++] = { sample, att_count.images };
            }
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
                        buffers.multiplyBuffer[i] = sample_noise(noiseData, p, noise);
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

        color pixel_color(0, 0, 0);

        for (int sample = 0; sample < s.samples_per_pixel; ++sample) {
            pixel_color += buffers.samples[sample];
        }

        pixels[j * s.image_width + i] = pixel_color / s.samples_per_pixel;
    }
}

static void renderThread(settings const &s, camera const &cam,
    std::atomic<int> &__restrict__ tileid,
    std::atomic<int> &__restrict__ remain_scanlines,
    size_t const stop_at, hittable_list const &world,
    color *pixels) noexcept
{
    // NOTE: @waste @mem Could reuse a solids lane (maybe the last/first one)
    // for the final lane.

    auto buffers = Scanline_Buffers::request(s.samples_per_pixel, s.max_depth);

    auto noise = std::make_unique<perlin>();

    for (;;) {
        auto j = tileid.fetch_add(1, std::memory_order_acq_rel);

        if (j >= s.image_width)
            return;

        // TODO: render worker state struct
        scanLine(s, cam, world, j, pixels, buffers, *noise.get());

        remain_scanlines.fetch_sub(1, std::memory_order_acq_rel);
        remain_scanlines.notify_one();
    }
}

static camera make_camera(settings const &s)
{
    camera cam;
    cam.image_height = int(s.image_width / s.aspect_ratio);
    cam.image_height = (cam.image_height < 1) ? 1 : cam.image_height;

    // Determine viewport dimensions.
    auto theta = degrees_to_radians(s.vfov);
    auto h = tan(theta / 2);
    auto viewport_height = 2 * h * s.focus_dist;
    auto viewport_width = viewport_height * (double(s.image_width) / cam.image_height);

    // Calculate the u,v,w unit basis vectors for the camera coordinate
    // frame.
    cam.w = unit_vector(-s.lookat);
    cam.u = unit_vector(cross(s.vup, cam.w));
    cam.v = cross(cam.w, cam.u);

    // Calculate the vectors across the horizontal and down the vertical
    // viewport edges.
    vec3 viewport_u = viewport_width * cam.u; // Vector across viewport horizontal edge
    vec3 viewport_v = viewport_height * -cam.v; // Vector down viewport vertical edge

    // Calculate the horizontal and vertical delta vectors from pixel to
    // pixel.
    cam.pixel_delta_u = viewport_u / s.image_width;
    cam.pixel_delta_v = viewport_v / cam.image_height;

    // Calculate the location of the upper left pixel.
    auto viewport_upper_left = -(s.focus_dist * cam.w) - viewport_u / 2 - viewport_v / 2;
    cam.pixel00_loc = viewport_upper_left + 0.5 * (cam.pixel_delta_u + cam.pixel_delta_v);

    // Calculate the camera defocus disk basis vectors.
    auto defocus_radius = s.focus_dist * tan(degrees_to_radians(s.defocus_angle / 2));
    cam.defocus_disk_u = cam.u * defocus_radius;
    cam.defocus_disk_v = cam.v * defocus_radius;
    return cam;
}

void render(hittable_list world, settings s)
{
    // offset everything so that what was at s.lookfrom is at 0, 0, 0.
    world.transformAll(transform(0, -s.lookfrom));
    world.treebld.prepareForRender();
    // I can't rotate the world because how noise is generated (the sin pattern)
    // depends on absolute world position and not the position relative to the
    // camera.
    s.lookat = s.lookat - s.lookfrom;
    auto cam = make_camera(s);
    auto pixels = std::make_unique<color[]>(size_t(s.image_width) * size_t(cam.image_height));

    int start = cam.image_height;
    static constexpr int stop_at = 0;
    std::atomic<int> remain_scanlines alignas(64) { start };

    auto progress_thread = std::thread([limit = start, &remain_scanlines]() {
        auto last_remain = limit + 1;
        while (true) {
            remain_scanlines.wait(last_remain, std::memory_order_acquire);

            auto remain = remain_scanlines.load(std::memory_order_acquire);
            last_remain = remain;
            std::clog << "\r\x1b[2K\x1b[?25lScanlines remaining: " << remain
                      << "\x1b[?25h" << std::flush;
            if (remain == stop_at)
                break;
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
    auto bytes = std::make_unique<uint8_t[]>(s.image_width * cam.image_height * 3);

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
