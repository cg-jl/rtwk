#include "camera.h"

#include <external/stb_image_write.h>
#include <omp.h>

#include <condition_variable>
#include <iostream>
#include <memory>
#include <thread>
#include <tracy/Tracy.hpp>

#include "material.h"
#include "perlin.h"
#include "random.h"
#include "timer.h"

static vec3 random_in_unit_sphere() {
    while (true) {
        auto p = random_vec(-1, 1);
        if (p.length_squared() < 1) return p;
    }
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

static vec3 sample_square() {
    // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit
    // square.
    return vec3(random_double() - 0.5, random_double() - 0.5, 0);
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

// TODO: separate request things into more renderer files

struct sample_request {
    texture tex;
    uvs uv;
    point3 p;

    sample_request(texture tex, uvs uv, point3 p)
        : tex(std::move(tex)), uv(uv), p(p) {}

    color sample(perlin const &noise) const { return tex.value(uv, p, noise); }
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

// TODO: pick another place/namespace for these

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

static color multiplySamples(std::span<sample_request const> samples,
                             color initial, perlin const &noise) {
    color acc = initial;
    for (auto const &s : samples) acc = acc * s.sample(noise);
    return acc;
}

static color geometrySim(camera const &cam, ray r, int depth,
                         hittable_list const &world, px_sampleq &attenuations) {
    for (;;) {
        if (depth <= 0) return color(1, 1, 1);
        ZoneScopedN("ray frame");

        hit_record rec;

        // If the ray hits nothing, return the background color.
        auto res = world.hitSelect(r, interval(0.001, infinity), rec.geom);

        auto maxT = res ? rec.geom.t : infinity;

        // Try sampling a constant medium
        double cmHit;
        if (auto cm =
                world.sampleConstantMediums(r, interval{0.001, maxT}, &cmHit)) {
            // Don't need UVs/normal; we have an isotropic material.
            // TODO: I could split the scattering too here.
            rec.geom.p = r.at(cmHit);
            // Be consistent with the normal from constant_medium::hit.
            rec.geom.normal = vec3(1., 0., 0);
            r.orig = rec.geom.p;
            r.dir = unit_vector(random_in_unit_sphere());

            attenuations.emplace_back(*cm->tex, rec.uv, rec.geom.p);
            --depth;
            continue;
        }

        if (!res) return cam.background;

        rec.set_face_normal(r, rec.geom.normal);
        {
            ZoneScopedN("getUVs");
            ZoneColor(tracy::Color::SteelBlue);
            res->getUVs(rec.uv, rec.geom.p, r.time);
        }

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

static void scanLine(camera const &cam, hittable_list const &world, int j,
                     color *pixels, std::vector<sample_request> &attenuations,
                     size_t *sample_counts, color *samples,
                     perlin const &noise) {
    for (int i = 0; i < cam.image_width; i++) {
        color pixel_color(0, 0, 0);
        attenuations.clear();
        px_sampleq q{attenuations, 0};
        for (int sample = 0; sample < cam.samples_per_pixel; sample++) {
            ZoneScopedN("pixel sample");
            ray r = get_ray(cam, i, j);

            auto bg = geometrySim(cam, r, cam.max_depth, world, q);

            auto att_count = attenuations.size() - q.prev_fill;
            q.prev_fill = attenuations.size();
            sample_counts[sample] = att_count;
            samples[sample] = bg;
        }

        {
            ZoneScopedN("attenuation sample");
            ZoneColor(tracy::Color::LawnGreen);
            std::span atts = attenuations;
            size_t processed = 0;
            for (int sample = 0; sample < cam.samples_per_pixel; ++sample) {
                auto count = sample_counts[sample];
                samples[sample] = multiplySamples(
                    atts.subspan(processed, count), samples[sample], noise);
                processed += count;
            }
        }

        for (int sample = 0; sample < cam.samples_per_pixel; ++sample) {
            pixel_color += samples[sample];
        }

        pixels[j * cam.image_width + i] = cam.pixel_samples_scale * pixel_color;
    }
}

static void initialize(camera &cam) {
    cam.image_height = int(cam.image_width / cam.aspect_ratio);
    cam.image_height = (cam.image_height < 1) ? 1 : cam.image_height;

    cam.pixel_samples_scale = 1.0 / cam.samples_per_pixel;

    cam.center = cam.lookfrom;

    // Determine viewport dimensions.
    auto theta = degrees_to_radians(cam.vfov);
    auto h = tan(theta / 2);
    auto viewport_height = 2 * h * cam.focus_dist;
    auto viewport_width =
        viewport_height * (double(cam.image_width) / cam.image_height);

    // Calculate the u,v,w unit basis vectors for the camera coordinate
    // frame.
    cam.w = unit_vector(cam.lookfrom - cam.lookat);
    cam.u = unit_vector(cross(cam.vup, cam.w));
    cam.v = cross(cam.w, cam.u);

    // Calculate the vectors across the horizontal and down the vertical
    // viewport edges.
    vec3 viewport_u =
        viewport_width * cam.u;  // Vector across viewport horizontal edge
    vec3 viewport_v =
        viewport_height * -cam.v;  // Vector down viewport vertical edge

    // Calculate the horizontal and vertical delta vectors from pixel to
    // pixel.
    cam.pixel_delta_u = viewport_u / cam.image_width;
    cam.pixel_delta_v = viewport_v / cam.image_height;

    // Calculate the location of the upper left pixel.
    auto viewport_upper_left =
        cam.center - (cam.focus_dist * cam.w) - viewport_u / 2 - viewport_v / 2;
    cam.pixel00_loc =
        viewport_upper_left + 0.5 * (cam.pixel_delta_u + cam.pixel_delta_v);

    // Calculate the camera defocus disk basis vectors.
    auto defocus_radius =
        cam.focus_dist * tan(degrees_to_radians(cam.defocus_angle / 2));
    cam.defocus_disk_u = cam.u * defocus_radius;
    cam.defocus_disk_v = cam.v * defocus_radius;
}

void camera::render(hittable_list const &world) {
    initialize(*this);

    auto pixels =
        std::make_unique<color[]>(size_t(image_width) * size_t(image_height));

    std::condition_variable cv;
    // lock progress mutex before launching the progress thread so we don't
    // end in a deadlock.

#if TRACY_ENABLE
    int start = 395;
    static constexpr int stop_at = 390;
#else
    int start = image_height;
    static constexpr int stop_at = 0;
#endif
    std::atomic<int> remain_scanlines{start};

    auto progress_thread =
        std::thread([limit = start, &remain_scanlines, &cv]() {
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

    auto render_timer = new rtwk::timer("Render");
    // worker loop
#pragma omp parallel
    {
        std::vector<sample_request> attenuations;
        auto sample_counts =
            std::make_unique<size_t[]>(size_t(samples_per_pixel));
        auto samples = std::make_unique<color[]>(size_t(samples_per_pixel));

        perlin noise;

        for (;;) {
            auto j = remain_scanlines.load(std::memory_order_acquire);

            // contend for our j (CAS)
            do {
                if (j == stop_at) goto finish_work;
            } while (!remain_scanlines.compare_exchange_weak(
                j, j - 1, std::memory_order_acq_rel));
            --j;

            // TODO: render worker state struct
            scanLine(*this, world, j, pixels.get(), attenuations,
                     sample_counts.get(), samples.get(), noise);

            cv.notify_one();
        }
    finish_work:;
    }

    delete render_timer;
    progress_thread.join();

    std::clog << "\r\x1b[2KWriting image...\n";

    // 1. Encode the image into RGB
    auto bytes = std::make_unique<uint8_t[]>(image_width * image_height * 3);

    for (size_t i = 0; i < image_width * image_height; ++i) {
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

    stbi_write_png("test.png", image_width, image_height, 3, &bytes[0], 0);

    std::clog << "Done.\n";
}
