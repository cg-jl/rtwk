#pragma once
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "collection/view.h"
#include "hittable.h"

namespace soa {
namespace detail {

template <size_t index, typename T, typename... Ts>
struct find_type;

template <size_t index, typename T, typename U, typename... Ts>
struct find_type<index, T, U, Ts...> {
    static consteval size_t i() {
        if constexpr (std::is_same_v<T, U>)
            return index;
        else
            return find_type<index + 1, T, Ts...>::i();
    }
};

template <size_t index, typename T>
struct find_type<index, T> {};

template <typename... Ts>
struct type_indexable_tuple {
    constexpr type_indexable_tuple(Ts &&...elems)
        : elems(std::forward<Ts>(elems)...) {}
    constexpr type_indexable_tuple()
        requires(std::default_initializable<Ts> && ...)
    {}

    std::tuple<Ts...> elems{};

    template <typename T>
    [[nodiscard]] constexpr T &get() & {
        return std::get<find_type<0, T, Ts...>::i()>(elems);
    }

    template <typename T>
    [[nodiscard]] constexpr T const &get() const & {
        return std::get<find_type<0, T, Ts...>::i()>(elems);
    }
};
}  // namespace detail
template <typename... Ts>
struct span {
    detail::type_indexable_tuple<std::span<Ts const>...> spans{};
    constexpr span(std::span<Ts const>... spans)
        : spans(std::forward<std::span<Ts const>>(spans)...) {}

    template <typename T>
    [[nodiscard]] constexpr std::span<T const> get() const noexcept {
        return spans.template get<std::span<T const>>();
    }
};

template <typename... Ts>
struct builder {
    detail::type_indexable_tuple<std::vector<Ts>...> builders{};

    template <typename T>
    std::vector<T> &builder_for() & {
        return builders.template get<std::vector<T>>();
    }

    template <typename T>
    void push(T &&val) {
        builder_for<T>().emplace_back(std::forward<T>(val));
    }

    template <typename T, typename... Args>
    void add(Args &&...args) {
        builder_for<T>().emplace_back(std::forward<Args>(args)...);
    }

    [[nodiscard]] constexpr span<Ts...> finish() const noexcept {
        return span{
            std::span<Ts const>(builders.template get<std::vector<Ts>>())...};
    }
};

template <typename... Ts>
struct view {
    explicit view(soa::span<Ts...> span) : span(std::move(span)) {}
    soa::span<Ts...> span;

    [[nodiscard]] aabb boundingBox() const &
        requires(has_bb<Ts> && ...)
    {
        aabb bb = empty_bb;

        ((bb = aabb(bb, ::view<Ts>(span.template get<Ts>()).boundingBox())),
         ...);

        return bb;
    }

    template <typename T>
    static void propagate_choose(std::span<T const> values, ray const &r,
                                 hit_status &status, hit_record &rec,
                                 float time) {
        if constexpr (is_hittable<T>) {
            return ::view<T>::propagate(r, status, rec, values, time);
        } else {
            static_assert(time_invariant_hittable<T>);
            return ::view<T>::propagate(r, status, rec, values);
        }
    }

    void propagate(ray const &r, hit_status &status, hit_record &rec) const &
        requires(time_invariant_hittable<Ts> && ...)

    {
        ((::view<Ts>::propagate(r, status, rec, span.template get<Ts>()), ...));
    }

    void propagate(ray const &r, hit_status &status, hit_record &rec,
                   float time) const &
        requires((is_hittable<Ts> || time_invariant_hittable<Ts>) && ...)
    {
        ((propagate_choose<Ts>(span.template get<Ts>(), r, status, rec, time)),
         ...);
    }
};

}  // namespace soa
