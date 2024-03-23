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
        if (cached_current >= max_capacity) return {};

        while (!current.compare_exchange_weak(cached_current,
                                              cached_current + max_load,
                                              std::memory_order_acq_rel)) {
            if (cached_current >= max_capacity) return {};
            // NOTE: use syscall to sleep (lock) or not?
        }

        if (cached_current >= max_capacity) return {};

        return std::pair{cached_current,
                         std::min(max_capacity - cached_current, max_load)};

#else
        if (current >= max_capacity) return {};
        current += max_load;
        return std::pair{current, std::min(max_capacity - current, max_load)};
#endif
    }
};
