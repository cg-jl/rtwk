#include "rtweekend.h"
#include <immintrin.h>

// Numbers in documentation are indices in memory order.
// 'first', 'last' are in memory order.
// 'left' and 'right' are in memory order.
namespace simd {

using floats = __m256;
using ints = __m256i;

// Given the layout:
// in:      [ 7 6 5 4 3 2 1 0 ]
// result:  [ 5 4 3 2 1 0 7 6 ]
[[clang::always_inline]] static inline floats rotate_right2(floats in)
{

    return _mm256_permute4x64_pd(in, 0b10'01'00'11);
}

// Given the layouts:
// insert_from: [ 7f 6f 5f 4f 3f 2f 1f 0f ]
// insert_to:   [ 7t 6t 5t 4t 3t 2t 1t 0t ]
// result:      [ 7t 6t 7t 4t 3t 2t 1f 0f ]
[[clang::always_inline]] static inline floats insert_first2_in_first2(floats insert_from, floats insert_to)
{
    return _mm256_blend_pd(insert_from, insert_to, 0b1'1'1'0);
}

// Given the layouts:
// insert_from: [ 7f 6f 5f 4f 3f 2f 1f 0f ]
// insert_to:   [ 7t 6t 5t 4t 3t 2t 1t 0t ]
// result:      [ 7t 6t 7t 4t 3t 2t 7f 6f ]
[[clang::always_inline]] static inline floats insert_last2_in_first2(floats insert_from, floats insert_to)
{
    // Here we don't care about the right most two bits.
    // Rotating and shifting are the same instruction, anyway.
    auto shifted = rotate_right2(insert_from);
    return insert_first2_in_first2(shifted, insert_to);
}

// Given the layouts
// insert_from: [ 7a 6a 5a 4a 3a 2a 1a 0a ]
// insert_to:   [ 7b 6b 5b 4b 3b 2b 1b 0b ]
// We want:     [ 7b 6b 5b 4b 3b 2b 1b 7a ]
[[clang::always_inline]] static inline floats insert_last_in_first(floats insert_from, floats insert_to)
{
    // Here we only care about '7a' moving to the low f128. The rest select '1a' and '0a': some of them
    // leak through the next instruction, but they are later filtered out.
    auto res0 = _mm256_permute4x64_pd(insert_from, 0b00'00'00'11);
    // res0:    [ 1a 0a 1a 0a 1a 0a 7a 6a ]
    // Here we set everything but positions '3' and '2'. They select '1a' and '7a', which are filtered out later.
    auto res1 = _mm256_shuffle_ps(res0, insert_to, 0b11'10'00'01);
    // res1:    [ 7b 6b 0a 1a 3b 2b 7a 7a ]
    // Now we correct the extra 'a's in 'res1' by replacing those positions with the values in ikm1. Note
    // that only bits '5', '4', '1' and '0' are set explicitly to keep or correct the values. The other four
    // are already in place so choosing from either register is fine.
    auto res2 = _mm256_blend_ps(res1, insert_to, 0b00'11'00'10);
    // res2:    [ 7b 6b 5b 4b 3b 2b 1b 7a ]
    return res2;
}

// Given the layout:
// in:      [ 7 6 5 4 3 2 1 0 ]
// We want: [ 6 5 4 3 2 1 0 7 ]
[[clang::always_inline]] static inline floats rotate_right(floats in)
{

    // It's not as easy as rotating by 2 since vpermilps (the nearest equivalent to vpermpd for 8x32)
    // cannot bring values across 128-bit lanes, which we need so that the index '3' represented below is
    // shifted.

    // This has latency 5 and requires a series compute (i.e 5 CPSeq). I was thinking of a similar
    // insn sequence that *could* have better throughput (same latency), but I don't think it's going to matter.
    // It saves one cycle in the parallel case where I can do an extra `vpermpd` or `vpermilps` for the next loop.
    //
    // sorry for the variable names! I will show how it works.
    // We start by having fresults_ik_reg with the following order (memory indices):
    // [7 6 5 4 3 2 1 0]
    // Here we'll have [6 5 4 7 2 1 0 3]
    auto res0 = _mm256_permute_ps(in, 0b10'01'00'11);
    // Here we only care about the '00' and the '10' (first and third). We will swap them
    // around in blocks: [6 5 0 3 2 1 4 7]
    auto res1 = _mm256_permute4x64_pd(res0, 0b11'00'01'10);
    // We select from res0 and res1 such that we obtain the desired sequence:
    // [ 6 5 4 3 2 1 0 7 ]
    return _mm256_blend_ps(res0, res1, 0b0'0'0'1'0'0'0'1);
}

[[clang::always_inline]] static inline floats loadf_aligned(float const *src)
{
    return _mm256_load_ps(src);
}

[[clang::always_inline]] static inline void storef_aligned(floats const reg, float *out)
{
    return _mm256_store_ps(out, reg);
}

[[clang::always_inline]] static inline floats zero()
{
    return _mm256_setzero_ps();
}

// Constructs a register such that argument `iX` goes to
// `reg[4 * X + 4:4 * X]`
[[clang::always_inline]] static inline ints seti(
    uint32 i7,
    uint32 i6,
    uint32 i5,
    uint32 i4,
    uint32 i3,
    uint32 i2,
    uint32 i1,
    uint32 i0)
{
    return _mm256_set_epi32(i7, i6, i5, i4, i3, i2, i1, i0);
}
// Constructs a register such that argument `iX` goes to
// `reg[4 * X + 4:4 * X]`
[[clang::always_inline]] static inline floats setf(
    float i7,
    float i6,
    float i5,
    float i4,
    float i3,
    float i2,
    float i1,
    float i0)
{
    return _mm256_set_ps(i7, i6, i5, i4, i3, i2, i1, i0);
}

[[clang::always_inline]] static inline ints splati(uint32 value)
{
    return _mm256_set1_epi32(value);
}
[[clang::always_inline]] static inline ints splatf(float value)
{
    return _mm256_set1_ps(value);
}

// Given an address to four-byte elements, find its misalignment to the nearest
// 32-byte boundary before the address.
// NOTE: Never use the aligned address directly. We want to keep provenance!
[[clang::always_inline]] static inline uint32 find_misalignment(uintptr_t addr4)
{
    auto const aligned_addr = uintptr_t(addr4) & ~31;
    return (addr4 - aligned_addr) / 4;
}

// Find the offset (in 4-byte elements) to the next address that is aligned to a 32-byte
// boundary. Assumes `addr4` is aligned to 4 bytes.
[[clang::always_inline]] static inline uint32 offset_till_aligned(uintptr_t addr4)
{
    auto const next_aligned = (addr4 + 31) & ~31;
    return (next_aligned - addr4) / 4;
}

// Stores `block` into `aligned_ptr`, but only the contiguous range that is `misalignment` to the right and `valid_count` elements wide.
// Useful for storing overcommitted head/tail blocks, i.e blocks where we used a SIMD register but only used the beginning/end of it.
[[clang::always_inline]] static inline void storef_overcommit(floats block, float *aligned_ptr, uint32 const misalignment, uint32 const valid_count)
{
    // Build a mask for the indices to be written.
    auto ixes = seti(7, 6, 5, 4, 3, 2, 1, 0);
    auto store_mask = (ixes >= splati(misalignment)) & (ixes < splati(valid_count));
    _mm256_maskstore_ps(aligned_ptr, store_mask, block);
}

[[clang::always_inline]] static inline floats minf(floats a, floats b)
{
    return _mm256_min_ps(a, b);
}
[[clang::always_inline]] static inline floats absf(floats v)
{
    return _mm256_and_ps(v, _mm256_set1_ps(std::bit_cast<float>(0x7FFFFFFFu)));
}

}
