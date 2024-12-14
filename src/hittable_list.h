#pragma once
#include <functional>
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

#include <vector>

#include "bvh.h"
#include "constant_medium.h"
#include "geometry.h"
#include "hittable.h"
#include "interval.h"
#include "ray.h"

struct SampleCM_Buffers {
    float *currentHit;
    std::optional<uint32_t> *selected;
    interval_buffer traversals;
    float *thit;
    aabb::Traverse_Buffers bb;

    static SampleCM_Buffers request(uint32_t const spp)
    {
        return {
            .currentHit = new float[spp],
            .selected = new std::optional<uint32_t>[spp],
            .traversals = interval_buffer::request(spp),
            .thit = new float[spp],
            // @mem I want to reuse memory
            .bb = aabb::Traverse_Buffers::request(spp),
        };
    }
};

struct Select_Buffers {
    float *hit_span_backbuf;
    bvh::Hit_Buffer bvh;

    static Select_Buffers request(uint32 const spp)
    {
        return {
            .hit_span_backbuf = new float[spp],
            .bvh = bvh::Hit_Buffer::request(spp),
        };
    }
};

struct hittable_list {
    bvh::tree_builder treebld;
    std::vector<lightInfo> objects;
    std::vector<geometry> selectGeoms;
    std::vector<constant_medium> cms {};
    std::vector<color> cmAlbedos {};

    hittable_list() { }
    hittable_list(lightInfo object, geometry geom)
    {
        add(object, std::move(geom));
    }

    void clear() { objects.clear(); }

    // Links the geomery with the lighting information.
    void add(lightInfo object, geometry geom);
    void addTree(lightInfo object, geometry geom);
    void add(constant_medium medium, color albedo);

    void transformAll(transform tf);

    void select(ray_buffer rays, uint32 const len, Select_Buffers buffers, hit_span_buf results, std::function<void(uint32, uint32)> const &swap_rays) const noexcept;

    void sampleCMs(
        transposed_ray_array rays, float const *times, uint32_t const len,
        std::pair<color const *, float> *results, SampleCM_Buffers buffers,
        std::function<void(uint32_t, uint32_t)> const &swap_rays) const noexcept;
};
