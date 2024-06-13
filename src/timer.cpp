#include "timer.h"

#include <iostream>
#include <print>

namespace rtwk {
timer::timer(std::string_view name)
    : name(name), start(std::chrono::steady_clock::now()) {}
timer::~timer() {
    auto end = std::chrono::steady_clock::now();

    auto dur = end - start;

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur);

    std::println(std::clog, "{} took {} [{}]", name, ms, dur);
}
}  // namespace rtwk
