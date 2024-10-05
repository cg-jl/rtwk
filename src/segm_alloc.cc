#include "segm_alloc.h"

#include <cassert>
#if _WIN32
#include <malloc.h>
#endif
#include <optional>

namespace segment {

static std::optional<std::pair<uintptr_t, uintptr_t>> allocSegment(
    Leak_Allocator::Unfinished_Segment const segm, size_t const size,
    size_t const align_mask) {
    auto avail_start_ptr = uintptr_t(segm.data) + segm.used;
    auto end_ptr = uintptr_t(segm.data) + segment::size;
    auto alloc_start_ptr = (avail_start_ptr + align_mask) & ~align_mask;
    auto alloc_end_ptr = alloc_start_ptr + size;
    if (alloc_start_ptr > end_ptr || alloc_start_ptr < avail_start_ptr)
        return {};
    if (alloc_end_ptr > end_ptr || alloc_end_ptr < avail_start_ptr) return {};
    return std::pair{alloc_start_ptr, alloc_end_ptr};
}

static uintptr_t backupAlloc(size_t size, size_t align) {
#if _WIN32
    return uintptr_t(_aligned_malloc(size, align));
#else
#error Unsupported runtime for aligned allocation
#endif
}

uintptr_t Leak_Allocator::allocRaw(size_t size, size_t align) {
    if (size >= segment::size) {
        // I'm not going to fit it anywhere here and I don't have to manage the
        // memory, so just use the backup allocator.
        return backupAlloc(size, align);
    }

    if (std::popcount(align) != 1) std::unreachable();
    size_t align_mask = align - 1;
    // try to forget a segment first.
    for (int i = 0; i < unfinished_queue.size(); ++i) {
        if (auto alloc_range =
                allocSegment(unfinished_queue[i], size, align_mask);
            alloc_range) {
            auto page_start_ptr = uintptr_t(unfinished_queue[i].data);
            auto page_end_ptr = page_start_ptr + segment::size;
            auto [alloc_start_ptr, alloc_end_ptr] = *alloc_range;
            if (alloc_end_ptr == page_end_ptr) {
                // remove this from the queue. Note that swapping these uses
                // full copies, so there's // no chance of borking due to
                // aliasing with last element.
                std::swap(unfinished_queue.back(), unfinished_queue[i]);
                unfinished_queue.pop_back();
            } else {
                auto page_start_ptr = uintptr_t(unfinished_queue[i].data);
                // put it at the front so that we get to try it sooner.
                std::swap(unfinished_queue.front(), unfinished_queue[i]);
                unfinished_queue.front().used = alloc_end_ptr - page_start_ptr;
            }
            return alloc_start_ptr;
        }
    }

    // make a new segment
    Unfinished_Segment next_seg{
        reinterpret_cast<segm *>(backupAlloc(sizeof(segm), 4096)), 0};
    auto [alloc_start_ptr, alloc_end_ptr] =
        *allocSegment(next_seg, size, align_mask);
    next_seg.used = alloc_end_ptr - uintptr_t(next_seg.data);
    // we had to use a new one, but keep it at the end of the queue because we
    // want to finish packing older segments before this one.
    unfinished_queue.push_back(next_seg);
    return alloc_start_ptr;
}
}  // namespace segment
