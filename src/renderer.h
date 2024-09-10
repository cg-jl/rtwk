#pragma once

#include <camera.h>

#include <atomic>

void render(camera const &cam, std::atomic<int> &tileid,
            std::atomic<int> &remain_scanlines, size_t stop_at,
            hittable_list const &world, color *pixels) noexcept;
