#pragma once
#include <concepts>
#include <vector>

#include "rtweekend.h"

// Storage that maintains pointer stability for created objects.

// Storage that only stores values of type T
template <typename T>
struct typed_storage {
    std::vector<shared_ptr<T>> ptrs;

    T* add(shared_ptr<T> obj) {
        auto ptr = obj.get();
        ptrs.emplace_back(std::move(obj));
        return ptr;
    }

    template <typename... Args>
    T* make(Args&&... args) & {
        auto ptr = make_shared<T>(std::forward<Args&&>(args)...);
        return add(std::move(ptr));
    }
};
// Storage that allows multiple classes as long as they are derived from others.
// NOTE: should rename this to 'type storage' or something like that, and maybe
// switch the underlying impl to be some sort of arena allocator
template <typename T>
struct poly_storage {
    std::vector<shared_ptr<T>> ptrs;

    template <typename U>
        requires std::is_base_of_v<T, U>
    T* add(shared_ptr<U> obj) {
        auto ptr = obj.get();
        ptrs.emplace_back(std::move(obj));
        return ptr;
    }

    template <typename U>
        requires std::is_base_of_v<T, U>
    T* move(U obj) {
        return make<U>(std::move(obj));
    }

    template <typename U, typename... Args>
        requires std::is_base_of_v<T, U>
    T* make(Args&&... args) & {
        auto ptr = make_shared<U>(std::forward<Args&&>(args)...);
        return add(std::move(ptr));
    }
};
