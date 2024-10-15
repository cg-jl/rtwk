#pragma once

#include <chrono>
#include <string_view>
#include <print>
#include <iosfwd>

namespace rtwk {

struct stopwatch {
    std::chrono::time_point<std::chrono::steady_clock> start_tp;

    void start() {
        start_tp = std::chrono::steady_clock::now();
    }
    auto stop() {
        auto end = std::chrono::steady_clock::now();
        return end - start_tp;
    }
};

void print_duration(std::ostream& s, std::string_view name, auto dur) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur);

    std::println(s, "{} took {} [{}]", name, ms, dur);
}



}  // namespace rtwk
