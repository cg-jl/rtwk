#pragma once

#include <format>

#include "../aabb.h"
#include "format/interval.h"

template <>
struct std::formatter<aabb> : public std::formatter<interval> {
    auto format(aabb const &box, auto &ctx) const {
        using itf = std::formatter<interval>;
        auto out = itf::format(box.x, ctx);
        *out++ = 'x';
        out = itf::format(box.y, ctx);
        *out++ = 'x';
        return itf::format(box.z, ctx);
    }
};
