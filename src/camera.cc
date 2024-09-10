#include "camera.h"

#include <external/stb_image_write.h>
#include <omp.h>

#include <condition_variable>
#include <iostream>
#include <memory>
#include <thread>
#include <tracy/Tracy.hpp>

#include "renderer.h"
#include "timer.h"

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

    // lock progress mutex before launching the progress thread so we don't
    // end in a deadlock.

    int start = image_height;
    static constexpr int stop_at = 0;
    std::atomic<int> remain_scanlines{start};

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

    auto render_timer = new rtwk::timer("Render");
    std::atomic<int> tileid;
    // worker loop
#pragma omp parallel
    { ::render(*this, tileid, remain_scanlines, stop_at, world, pixels.get()); }

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
