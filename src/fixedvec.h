#pragma once

#include <cstdint>
#include <span>

template <typename T>
struct uncapped_vecview {
    constexpr explicit uncapped_vecview(T* values) : values(values) {}
    T* values;
    uint32_t count{};

    [[nodiscard]] constexpr bool empty() const& { return count == 0; }
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

// a writeable view into a fixed array.
template <typename T>
struct vecview : public uncapped_vecview<T> {
    uint32_t cap;

    constexpr vecview(T* values, uint32_t cap)
        : uncapped_vecview<T>(values), cap(cap) {}

    [[nodiscard]] constexpr bool full() const& { return cap == this->count; }
};
