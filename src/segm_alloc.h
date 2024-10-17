#include <bit>
#include <cstdint>
#include <span>
#include <vector>

namespace segment {
static constexpr size_t size = 4096;

using segm = uint8_t[size];

// leaking allocator (never resets, never frees).
// Forgets about full segments.
struct Leak_Allocator {
    struct Unfinished_Segment {
        segm *data;
        unsigned int used;
        constexpr operator bool() const { return data != nullptr; }
    };
    std::vector<Unfinished_Segment> unfinished_queue;

    // @incomplete These lose provenance from the original pointer
    // (segment.data)
    uintptr_t allocRaw(size_t size, size_t align);
    template <typename T>
    T *alloc() {
        return std::bit_cast<T *>(allocRaw(sizeof(T), alignof(T)));
    }
    template <typename T>
    std::span<T> alloc(size_t size) {
        // @incomplete `size * sizeof(T)` may overflow.
        return {std::bit_cast<T *>(allocRaw(size * sizeof(T), alignof(T))),
                size};
    }
};

}  // namespace segment
