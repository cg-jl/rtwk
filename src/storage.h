#pragma once
#include <concepts>
#include <vector>

#include "rtweekend.h"

// Storage that maintains pointer stability for created objects.

// NOTE: should rename this to 'type storage' or something like that, and maybe
// switch the underlying impl to be some sort of arena allocator
template <typename T>
struct typed_storage {
    std::vector<shared_ptr<T>> ptrs;

    template <typename U>
        requires std::is_base_of_v<T, U>
    T* add(shared_ptr<U> obj) {
        auto ptr = obj.get();
        ptrs.emplace_back(std::move(obj));
        return ptr;
    }

    template <typename U, typename... Args>
        requires std::is_base_of_v<T, U>
    T* make(Args&&... args) & {
        auto ptr = make_shared<U>(std::forward<Args&&>(args)...);
        return add(std::move(ptr));
    }
};
