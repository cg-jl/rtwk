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

struct SampleCM_Buffers {
    double *currentHit;
    double *rayLength;
    std::optional<uint32_t> *selected;
    interval *traversals;
    double *thit;

    static SampleCM_Buffers request(uint32_t const spp)
    {
        return {
            .currentHit = new double[spp],
            .rayLength = new double[spp],
            .selected = new std::optional<uint32_t>[spp],
            .traversals = new interval[spp],
            .thit = new double[spp],
        };
    }
};

struct Select_Buffers {
    std::pair<geometry_ptr, double> *tree_hits;
    bvh::Hit_Buffer bvh;

    static Select_Buffers request(uint32 const spp)
    {
        return {
            .tree_hits = new std::pair<geometry_ptr, double>[spp],
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

    void select(ray_buffer rays, uint32 const len, Select_Buffers buffers, std::pair<geometry_ptr, double> *results, std::function<void(uint32, uint32)> swap_rays) const noexcept;

    void sampleCMs(
        ray_buffer rays, uint32_t const len,
        std::pair<color const *, double> *results, SampleCM_Buffers buffers,
        std::function<void(uint32_t, uint32_t)> swap_rays) const noexcept;
};
