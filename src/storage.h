#pragma once
#include <concepts>
#include <vector>

#include "rtweekend.h"

template <typename T>
T* leak(T&& val) {
    return new T(std::forward<T&&>(val));
}
