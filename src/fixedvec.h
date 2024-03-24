#pragma once

#include <cstdint>
#include <span>

// a writeable view into a fixed array.
template <typename T>
struct vecview {
    T* values;
    uint32_t count;
    uint32_t cap;

    constexpr vecview(T* values, uint32_t cap)
        : values(values), count(0), cap(cap) {}

    [[nodiscard]] constexpr bool empty() const& { return count == 0; }
    [[nodiscard]] constexpr bool full() const& { return cap == count; }

    constexpr std::span<T> span() const& { return std::span<T>(values, count); }

    void clear() & { count = 0; }

    T* reserve() { return &values[count++]; }

    T const& back() { return values[count - 1]; }

    T pop() { return values[--count]; }

    void pop_back() { --count; }

    template <typename... Args>
    void emplace_back(Args&&... args) {
        T* __restrict__ ptr = reserve();
        new (ptr) T(std::forward<Args>(args)...);
    }
};
