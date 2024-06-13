#pragma once
#include <format>

#include "../interval.h"

template <>
struct std::formatter<interval> {
    constexpr auto parse(auto &ctx) { return ctx.begin(); }

    auto format(interval const &itv, auto &ctx) const {
        return std::format_to(ctx.out(), "[{}, {}]", itv.min, itv.max);
    }
};
