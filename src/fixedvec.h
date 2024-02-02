#pragma once

#include <cstdint>
#include <span>

// a writeable view into a fixed array.
template <typename T>
struct vecview {
    T* values;
    uint32_t cap;
    uint32_t count;

    constexpr vecview(T* values, uint32_t cap)
        : values(values), cap(cap), count(0) {}

    constexpr bool is_empty() const& { return count == 0; }
    constexpr bool is_full() const& { return cap == count; }

    constexpr std::span<T> span() const& { return std::span<T>(values, count); }

    void clear() & { count = 0; }

    T* reserve() { return &values[count++]; }

    template <typename... Args>
    void emplace_back(Args&&... args) {
        T* __restrict__ ptr = reserve();
        new (ptr) T(std::forward<Args>(args)...);
    }
};
