#pragma once

#include <chrono>
#include <string_view>

namespace rtwk {
struct timer {
    std::string_view name;
    std::chrono::time_point<std::chrono::steady_clock> start;

    timer(std::string_view name);
    ~timer();
};

}  // namespace rtwk
