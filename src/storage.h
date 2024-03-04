#pragma once
#include <sys/types.h>

#include <cstdint>
#include <span>
#include <vector>

template <typename T>
T* leak(T&& val) {
    return new T(std::forward<T&&>(val));
}

template <typename T>
struct id_storage {
    std::vector<T> ids;

    template <typename... Args>
    uint32_t add(Args&&... args) {
        ids.emplace_back(std::forward<Args>(args)...);
        return uint32_t(ids.size() - 1);
    }

    constexpr std::span<T const> view() const noexcept { return ids; }
};
