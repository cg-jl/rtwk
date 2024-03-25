#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <optional>

template <std::unsigned_integral Int>
struct range_queue {
#ifdef _OPENMP
    alignas(64) std::atomic<Int> current = 0;
    alignas(64) Int max_capacity;
#else
    Int current = 0;
    Int max_capacity;
#endif

    constexpr range_queue(Int max_capacity) : max_capacity(max_capacity) {}

    std::optional<std::pair<Int, Int>> next(Int max_load) {
#ifdef _OPENMP
        // CAS loop

        Int cached_current = current.load(std::memory_order_acquire);
        Int next;
        do {
            if (cached_current == max_capacity) return {};
            next = std::min(max_capacity, cached_current + max_load);
        } while (!current.compare_exchange_weak(cached_current, next,
                                                std::memory_order_acq_rel));

        if (cached_current == max_capacity) return {};

        return std::pair{cached_current, next - cached_current};

#else
        if (current >= max_capacity) return {};
        current += max_load;
        return std::pair{current, std::min(max_capacity - current, max_load)};
#endif
    }
};