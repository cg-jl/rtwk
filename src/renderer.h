#pragma once

#include <camera.h>

#include <atomic>
#include <condition_variable>

void render(camera const &cam, std::atomic<int> &remain_scanlines,
            std::condition_variable &notifyScanline, size_t stop_at,
            hittable_list const &world, color *pixels) noexcept;
