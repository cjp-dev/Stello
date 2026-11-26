#pragma once

#include "eval/eval.hpp"
#include "eval/patterns.hpp"

#if (defined(__x86_64__) || defined(_M_X64)) && defined(__AVX2__) && !defined(__EMSCRIPTEN__)

#include <bit>
#include <cstdint>
#include <immintrin.h>

namespace stello::search::detail::eval_avx2 {

template <bool OddSlot>
STELLO_FORCEINLINE int32_t sum_gather_avx2(
    const eval::PatternSet& set,
    __m256i l0,
    __m256i l1,
    __m256i l2,
    __m256i l3,
    const int32_t* pair_weights
) noexcept {
    const __m256i* const off_v = reinterpret_cast<const __m256i*>(set.slot_offsets());

    auto gather_half = [&](__m128i h, size_t off_idx) noexcept -> __m256i {
        const __m256i idx = _mm256_add_epi32(_mm256_cvtepu16_epi32(h), off_v[off_idx]);
        const __m256i d = _mm256_i32gather_epi32(pair_weights, idx, 4);
        if constexpr (OddSlot) {
            return _mm256_srai_epi32(d, 16);
        } else {
            return _mm256_srai_epi32(_mm256_slli_epi32(d, 16), 16);
        }
    };

    const size_t n = set.feature_count();
    __m256i ss = gather_half(_mm256_castsi256_si128(l0), 0);
    ss = _mm256_add_epi32(ss, gather_half(_mm256_extracti128_si256(l0, 1), 1));
    ss = _mm256_add_epi32(ss, gather_half(_mm256_castsi256_si128(l1), 2));
    ss = _mm256_add_epi32(ss, gather_half(_mm256_extracti128_si256(l1, 1), 3));
    ss = _mm256_add_epi32(ss, gather_half(_mm256_castsi256_si128(l2), 4));
    if (n > 40U) {
        ss = _mm256_add_epi32(ss, gather_half(_mm256_extracti128_si256(l2, 1), 5));
        if (n > 48U) {
            ss = _mm256_add_epi32(ss, gather_half(_mm256_castsi256_si128(l3), 6));
            if (n > 56U) {
                ss = _mm256_add_epi32(ss, gather_half(_mm256_extracti128_si256(l3, 1), 7));
            }
        }
    }

    if (n == 64U) [[likely]] {
        __m128i s = _mm_add_epi32(_mm256_castsi256_si128(ss), _mm256_extracti128_si256(ss, 1));
        s = _mm_add_epi32(s, _mm_unpackhi_epi64(s, s));
        return _mm_cvtsi128_si32(_mm_add_epi32(s, _mm_shufflelo_epi16(s, 0x4e)));
    }

    alignas(32) uint16_t loc[eval::MAX_PATTERN_FEATURES];
    __m256i* const dst = reinterpret_cast<__m256i*>(loc);
    _mm256_store_si256(dst + 0, l0);
    _mm256_store_si256(dst + 1, l1);
    _mm256_store_si256(dst + 2, l2);
    _mm256_store_si256(dst + 3, l3);
    const uint32_t* const off = set.slot_offsets();
    int32_t sum = 0;
    for (size_t f = 0; f < n; ++f) {
        const int32_t packed = pair_weights[off[f] + loc[f]];
        sum += OddSlot ? static_cast<int16_t>(packed >> 16) : static_cast<int16_t>(static_cast<uint16_t>(packed));
    }
    return sum;
}

STELLO_FORCEINLINE int32_t sum_local_avx2(
    const eval::PatternSet& set,
    __m256i l0,
    __m256i l1,
    __m256i l2,
    __m256i l3,
    const int32_t* pair_weights,
    bool odd_slot
) noexcept {
    if (set.feature_count() != 64U) [[unlikely]] {
        alignas(32) uint16_t loc[eval::MAX_PATTERN_FEATURES];
        __m256i* const dst = reinterpret_cast<__m256i*>(loc);
        _mm256_store_si256(dst + 0, l0);
        _mm256_store_si256(dst + 1, l1);
        _mm256_store_si256(dst + 2, l2);
        _mm256_store_si256(dst + 3, l3);
        const uint32_t* const off = set.slot_offsets();
        const size_t n = set.feature_count();
        int32_t sum = 0;
        for (size_t f = 0; f < n; ++f) {
            const int32_t packed = pair_weights[off[f] + loc[f]];
            sum += odd_slot ? static_cast<int16_t>(packed >> 16) : static_cast<int16_t>(static_cast<uint16_t>(packed));
        }
        return sum;
    }
    return odd_slot
        ? sum_gather_avx2<true>(set, l0, l1, l2, l3, pair_weights)
        : sum_gather_avx2<false>(set, l0, l1, l2, l3, pair_weights);
}

STELLO_FORCEINLINE void accum_masks_avx2(
    const eval::PatternSet& set,
    uint64_t mask,
    __m256i& a0,
    __m256i& a1,
    __m256i& a2,
    __m256i& a3
) noexcept {
    while (mask != 0) {
        const uint32_t bit = static_cast<uint32_t>(std::countr_zero(mask));
        mask &= mask - 1U;
        const __m256i* const d = reinterpret_cast<const __m256i*>(set.square_deltas(bit));
        a0 = _mm256_add_epi16(a0, d[0]);
        a1 = _mm256_add_epi16(a1, d[1]);
        a2 = _mm256_add_epi16(a2, d[2]);
        a3 = _mm256_add_epi16(a3, d[3]);
    }
}

STELLO_FORCEINLINE eval::PatternState eval_state_of_avx2(
    const eval::PatternSet& set,
    uint64_t digit1,
    uint64_t digit2
) noexcept {
    const uint64_t empty = ~(digit1 | digit2);
    const uint32_t c0 = static_cast<uint32_t>(std::popcount(empty));
    const uint32_t c1 = static_cast<uint32_t>(std::popcount(digit1));
    const uint32_t c2 = static_cast<uint32_t>(std::popcount(digit2));

    __m256i a0 = _mm256_setzero_si256();
    __m256i a1 = _mm256_setzero_si256();
    __m256i a2 = _mm256_setzero_si256();
    __m256i a3 = _mm256_setzero_si256();
    __m256i b0 = _mm256_setzero_si256();
    __m256i b1 = _mm256_setzero_si256();
    __m256i b2 = _mm256_setzero_si256();
    __m256i b3 = _mm256_setzero_si256();

    eval::PatternState out;
    __m256i* const dst = reinterpret_cast<__m256i*>(out.features.data());

    if (c0 >= c1 && c0 >= c2) {
        accum_masks_avx2(set, digit1, a0, a1, a2, a3);
        accum_masks_avx2(set, digit2, b0, b1, b2, b3);
        const __m256i* const base = reinterpret_cast<const __m256i*>(set.initial_state().features.data());
        _mm256_storeu_si256(dst + 0, _mm256_add_epi16(base[0], _mm256_add_epi16(a0, _mm256_add_epi16(b0, b0))));
        _mm256_storeu_si256(dst + 1, _mm256_add_epi16(base[1], _mm256_add_epi16(a1, _mm256_add_epi16(b1, b1))));
        _mm256_storeu_si256(dst + 2, _mm256_add_epi16(base[2], _mm256_add_epi16(a2, _mm256_add_epi16(b2, b2))));
        _mm256_storeu_si256(dst + 3, _mm256_add_epi16(base[3], _mm256_add_epi16(a3, _mm256_add_epi16(b3, b3))));
    } else if (c1 >= c2) {
        accum_masks_avx2(set, digit2, a0, a1, a2, a3);
        accum_masks_avx2(set, empty, b0, b1, b2, b3);
        const __m256i* const base = reinterpret_cast<const __m256i*>(set.all_1_state().features.data());
        _mm256_storeu_si256(dst + 0, _mm256_sub_epi16(_mm256_add_epi16(base[0], a0), b0));
        _mm256_storeu_si256(dst + 1, _mm256_sub_epi16(_mm256_add_epi16(base[1], a1), b1));
        _mm256_storeu_si256(dst + 2, _mm256_sub_epi16(_mm256_add_epi16(base[2], a2), b2));
        _mm256_storeu_si256(dst + 3, _mm256_sub_epi16(_mm256_add_epi16(base[3], a3), b3));
    } else {
        accum_masks_avx2(set, digit1, a0, a1, a2, a3);
        accum_masks_avx2(set, empty, b0, b1, b2, b3);
        const __m256i* const base = reinterpret_cast<const __m256i*>(set.all_2_state().features.data());
        _mm256_storeu_si256(dst + 0, _mm256_sub_epi16(base[0], _mm256_add_epi16(a0, _mm256_add_epi16(b0, b0))));
        _mm256_storeu_si256(dst + 1, _mm256_sub_epi16(base[1], _mm256_add_epi16(a1, _mm256_add_epi16(b1, b1))));
        _mm256_storeu_si256(dst + 2, _mm256_sub_epi16(base[2], _mm256_add_epi16(a2, _mm256_add_epi16(b2, b2))));
        _mm256_storeu_si256(dst + 3, _mm256_sub_epi16(base[3], _mm256_add_epi16(a3, _mm256_add_epi16(b3, b3))));
    }
    return out;
}

STELLO_FORCEINLINE void advance_eval_state_avx2(
    const eval::PatternSet& set,
    eval::PatternState& next,
    const eval::PatternState& cur,
    bool mover_is_even_disc,
    uint32_t square,
    uint64_t flips
) noexcept {
    const __m256i* const in = reinterpret_cast<const __m256i*>(cur.features.data());
    const __m256i* const sq = reinterpret_cast<const __m256i*>(set.square_deltas(square));
    __m256i* const out = reinterpret_cast<__m256i*>(next.features.data());

    if (mover_is_even_disc) {
        __m256i l0 = _mm256_add_epi16(_mm256_loadu_si256(in + 0), sq[0]);
        __m256i l1 = _mm256_add_epi16(_mm256_loadu_si256(in + 1), sq[1]);
        __m256i l2 = _mm256_add_epi16(_mm256_loadu_si256(in + 2), sq[2]);
        __m256i l3 = _mm256_add_epi16(_mm256_loadu_si256(in + 3), sq[3]);
        uint64_t rem = flips;
        while (rem != 0) {
            const uint32_t bit = static_cast<uint32_t>(std::countr_zero(rem));
            rem &= rem - 1U;
            const __m256i* const d = reinterpret_cast<const __m256i*>(set.square_deltas(bit));
            l0 = _mm256_sub_epi16(l0, d[0]);
            l1 = _mm256_sub_epi16(l1, d[1]);
            l2 = _mm256_sub_epi16(l2, d[2]);
            l3 = _mm256_sub_epi16(l3, d[3]);
        }
        _mm256_storeu_si256(out + 0, l0);
        _mm256_storeu_si256(out + 1, l1);
        _mm256_storeu_si256(out + 2, l2);
        _mm256_storeu_si256(out + 3, l3);
    } else {
        __m256i l0 = _mm256_add_epi16(_mm256_loadu_si256(in + 0), _mm256_add_epi16(sq[0], sq[0]));
        __m256i l1 = _mm256_add_epi16(_mm256_loadu_si256(in + 1), _mm256_add_epi16(sq[1], sq[1]));
        __m256i l2 = _mm256_add_epi16(_mm256_loadu_si256(in + 2), _mm256_add_epi16(sq[2], sq[2]));
        __m256i l3 = _mm256_add_epi16(_mm256_loadu_si256(in + 3), _mm256_add_epi16(sq[3], sq[3]));
        uint64_t rem = flips;
        while (rem != 0) {
            const uint32_t bit = static_cast<uint32_t>(std::countr_zero(rem));
            rem &= rem - 1U;
            const __m256i* const d = reinterpret_cast<const __m256i*>(set.square_deltas(bit));
            l0 = _mm256_add_epi16(l0, d[0]);
            l1 = _mm256_add_epi16(l1, d[1]);
            l2 = _mm256_add_epi16(l2, d[2]);
            l3 = _mm256_add_epi16(l3, d[3]);
        }
        _mm256_storeu_si256(out + 0, l0);
        _mm256_storeu_si256(out + 1, l1);
        _mm256_storeu_si256(out + 2, l2);
        _mm256_storeu_si256(out + 3, l3);
    }
}

STELLO_FORCEINLINE int32_t sum_eval_state_avx2(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    const int32_t* packed_pair_weights,
    bool odd_slot
) noexcept {
    const __m256i* const in = reinterpret_cast<const __m256i*>(cur.features.data());
    return sum_local_avx2(
        set,
        _mm256_loadu_si256(in + 0),
        _mm256_loadu_si256(in + 1),
        _mm256_loadu_si256(in + 2),
        _mm256_loadu_si256(in + 3),
        packed_pair_weights,
        odd_slot);
}

STELLO_FORCEINLINE int32_t sum_eval_after_move_avx2(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    bool mover_is_even_disc,
    uint32_t square,
    uint64_t flips,
    const int32_t* packed_child_pair_weights,
    bool child_odd_slot
) noexcept {
    const __m256i* const in = reinterpret_cast<const __m256i*>(cur.features.data());
    const __m256i* const sq = reinterpret_cast<const __m256i*>(set.square_deltas(square));

    __m256i l0, l1, l2, l3;
    if (mover_is_even_disc) {
        l0 = _mm256_add_epi16(_mm256_loadu_si256(in + 0), sq[0]);
        l1 = _mm256_add_epi16(_mm256_loadu_si256(in + 1), sq[1]);
        l2 = _mm256_add_epi16(_mm256_loadu_si256(in + 2), sq[2]);
        l3 = _mm256_add_epi16(_mm256_loadu_si256(in + 3), sq[3]);
        uint64_t rem = flips;
        while (rem != 0) {
            const uint32_t bit = static_cast<uint32_t>(std::countr_zero(rem));
            rem &= rem - 1U;
            const __m256i* const d = reinterpret_cast<const __m256i*>(set.square_deltas(bit));
            l0 = _mm256_sub_epi16(l0, d[0]);
            l1 = _mm256_sub_epi16(l1, d[1]);
            l2 = _mm256_sub_epi16(l2, d[2]);
            l3 = _mm256_sub_epi16(l3, d[3]);
        }
    } else {
        l0 = _mm256_add_epi16(_mm256_loadu_si256(in + 0), _mm256_add_epi16(sq[0], sq[0]));
        l1 = _mm256_add_epi16(_mm256_loadu_si256(in + 1), _mm256_add_epi16(sq[1], sq[1]));
        l2 = _mm256_add_epi16(_mm256_loadu_si256(in + 2), _mm256_add_epi16(sq[2], sq[2]));
        l3 = _mm256_add_epi16(_mm256_loadu_si256(in + 3), _mm256_add_epi16(sq[3], sq[3]));
        uint64_t rem = flips;
        while (rem != 0) {
            const uint32_t bit = static_cast<uint32_t>(std::countr_zero(rem));
            rem &= rem - 1U;
            const __m256i* const d = reinterpret_cast<const __m256i*>(set.square_deltas(bit));
            l0 = _mm256_add_epi16(l0, d[0]);
            l1 = _mm256_add_epi16(l1, d[1]);
            l2 = _mm256_add_epi16(l2, d[2]);
            l3 = _mm256_add_epi16(l3, d[3]);
        }
    }
    return sum_local_avx2(set, l0, l1, l2, l3, packed_child_pair_weights, child_odd_slot);
}

} // namespace stello::search::detail::eval_avx2

#endif
