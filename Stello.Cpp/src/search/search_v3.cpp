#include "search/kernel.hpp"

#if STELLO_HAS_X86_64_V3

#include "endgame/bits_avx2.hpp"

#include <bit>
#include <immintrin.h>

namespace stello::search {

namespace {

// On Zen 4, storing the AVX2-computed uint32_t indices to L1 stack and summing with a
// 4-way unrolled scalar loop beats microcoded _mm256_i32gather_epi32 and avoids 2-byte
// misaligned int* casts on odd-length stage tables.
STELLO_FORCEINLINE int32_t sum_local_avx2(
    const eval::PatternSet& set,
    __m256i l0,
    __m256i l1,
    __m256i l2,
    __m256i l3,
    const int16_t* weights
) noexcept {
    const uint32_t* const off = set.feature_offsets();
    const __m256i* const off_v = reinterpret_cast<const __m256i*>(off);

    alignas(32) uint32_t indices[eval::MAX_PATTERN_FEATURES];
    __m256i* const dst = reinterpret_cast<__m256i*>(indices);
    dst[0] = _mm256_add_epi32(_mm256_cvtepu16_epi32(_mm256_castsi256_si128(l0)), off_v[0]);
    dst[1] = _mm256_add_epi32(_mm256_cvtepu16_epi32(_mm256_extracti128_si256(l0, 1)), off_v[1]);
    dst[2] = _mm256_add_epi32(_mm256_cvtepu16_epi32(_mm256_castsi256_si128(l1)), off_v[2]);
    dst[3] = _mm256_add_epi32(_mm256_cvtepu16_epi32(_mm256_extracti128_si256(l1, 1)), off_v[3]);
    dst[4] = _mm256_add_epi32(_mm256_cvtepu16_epi32(_mm256_castsi256_si128(l2)), off_v[4]);
    dst[5] = _mm256_add_epi32(_mm256_cvtepu16_epi32(_mm256_extracti128_si256(l2, 1)), off_v[5]);
    dst[6] = _mm256_add_epi32(_mm256_cvtepu16_epi32(_mm256_castsi256_si128(l3)), off_v[6]);
    dst[7] = _mm256_add_epi32(_mm256_cvtepu16_epi32(_mm256_extracti128_si256(l3, 1)), off_v[7]);

    const size_t n = set.feature_count();
    int32_t s0 = 0;
    int32_t s1 = 0;
    int32_t s2 = 0;
    int32_t s3 = 0;
    size_t f = 0;
    for (; f + 4U <= n; f += 4U) {
        s0 += weights[indices[f]];
        s1 += weights[indices[f + 1U]];
        s2 += weights[indices[f + 2U]];
        s3 += weights[indices[f + 3U]];
    }
    for (; f < n; ++f) {
        s0 += weights[indices[f]];
    }
    return s0 + s1 + s2 + s3;
}

} // namespace

template <>
STELLO_FORCEINLINE uint64_t Kernel<true>::flips(
    uint64_t own,
    uint64_t opponent,
    uint32_t square
) noexcept {
    return endgame::bits::flips_avx2(own, opponent, square);
}

template <>
STELLO_FORCEINLINE uint64_t Kernel<true>::legal_moves(uint64_t own, uint64_t opponent) noexcept {
    return endgame::bits::legal_moves_avx2(own, opponent);
}

template <>
STELLO_FORCEINLINE void Kernel<true>::advance_eval_state(
    const eval::PatternSet& set,
    eval::PatternState& next,
    const eval::PatternState& cur,
    Player mover,
    uint32_t square,
    uint64_t flips
) noexcept {
    const bool mover_is_black = (mover == Player::Black);
    const __m256i* const m_in = reinterpret_cast<const __m256i*>(mover_is_black ? cur.black.data() : cur.white.data());
    const __m256i* const o_in = reinterpret_cast<const __m256i*>(mover_is_black ? cur.white.data() : cur.black.data());
    __m256i* const m_out = reinterpret_cast<__m256i*>(mover_is_black ? next.black.data() : next.white.data());
    __m256i* const o_out = reinterpret_cast<__m256i*>(mover_is_black ? next.white.data() : next.black.data());

    __m256i f0 = _mm256_setzero_si256();
    __m256i f1 = _mm256_setzero_si256();
    __m256i f2 = _mm256_setzero_si256();
    __m256i f3 = _mm256_setzero_si256();
    uint64_t rem = flips;
    while (rem != 0) {
        const uint32_t bit = static_cast<uint32_t>(std::countr_zero(rem));
        rem &= rem - 1U;
        const __m256i* const d = reinterpret_cast<const __m256i*>(set.square_deltas(bit));
        f0 = _mm256_add_epi16(f0, d[0]);
        f1 = _mm256_add_epi16(f1, d[1]);
        f2 = _mm256_add_epi16(f2, d[2]);
        f3 = _mm256_add_epi16(f3, d[3]);
    }

    const __m256i* const sq = reinterpret_cast<const __m256i*>(set.square_deltas(square));
    m_out[0] = _mm256_add_epi16(m_in[0], _mm256_add_epi16(sq[0], f0));
    m_out[1] = _mm256_add_epi16(m_in[1], _mm256_add_epi16(sq[1], f1));
    m_out[2] = _mm256_add_epi16(m_in[2], _mm256_add_epi16(sq[2], f2));
    m_out[3] = _mm256_add_epi16(m_in[3], _mm256_add_epi16(sq[3], f3));

    o_out[0] = _mm256_sub_epi16(o_in[0], f0);
    o_out[1] = _mm256_sub_epi16(o_in[1], f1);
    o_out[2] = _mm256_sub_epi16(o_in[2], f2);
    o_out[3] = _mm256_sub_epi16(o_in[3], f3);
}

template <>
STELLO_FORCEINLINE void Kernel<true>::build_eval_base(
    const eval::PatternSet& /*set*/,
    const eval::PatternState& cur,
    Player next_own,
    std::array<uint16_t, eval::MAX_PATTERN_FEATURES>& out
) noexcept {
    const bool next_is_black = (next_own == Player::Black);
    const __m256i* const own = reinterpret_cast<const __m256i*>(next_is_black ? cur.black.data() : cur.white.data());
    const __m256i* const opp = reinterpret_cast<const __m256i*>(next_is_black ? cur.white.data() : cur.black.data());
    __m256i* const dst = reinterpret_cast<__m256i*>(out.data());
    dst[0] = _mm256_add_epi16(_mm256_add_epi16(opp[0], opp[0]), own[0]);
    dst[1] = _mm256_add_epi16(_mm256_add_epi16(opp[1], opp[1]), own[1]);
    dst[2] = _mm256_add_epi16(_mm256_add_epi16(opp[2], opp[2]), own[2]);
    dst[3] = _mm256_add_epi16(_mm256_add_epi16(opp[3], opp[3]), own[3]);
}

template <>
STELLO_FORCEINLINE int32_t Kernel<true>::sum_eval_state(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    Player own_player,
    const int16_t* weights
) noexcept {
    const bool own_is_black = (own_player == Player::Black);
    const __m256i* const own = reinterpret_cast<const __m256i*>(own_is_black ? cur.black.data() : cur.white.data());
    const __m256i* const opp = reinterpret_cast<const __m256i*>(own_is_black ? cur.white.data() : cur.black.data());
    const __m256i l0 = _mm256_add_epi16(_mm256_add_epi16(opp[0], opp[0]), own[0]);
    const __m256i l1 = _mm256_add_epi16(_mm256_add_epi16(opp[1], opp[1]), own[1]);
    const __m256i l2 = _mm256_add_epi16(_mm256_add_epi16(opp[2], opp[2]), own[2]);
    const __m256i l3 = _mm256_add_epi16(_mm256_add_epi16(opp[3], opp[3]), own[3]);
    return sum_local_avx2(set, l0, l1, l2, l3, weights);
}

template <>
STELLO_FORCEINLINE int32_t Kernel<true>::sum_eval_after_move(
    const eval::PatternSet& set,
    const std::array<uint16_t, eval::MAX_PATTERN_FEATURES>& parent_base,
    uint32_t square,
    uint64_t flips,
    const int16_t* weights
) noexcept {
    const __m256i* const base = reinterpret_cast<const __m256i*>(parent_base.data());
    const __m256i* const sq = reinterpret_cast<const __m256i*>(set.square_deltas(square));
    __m256i l0 = _mm256_add_epi16(base[0], _mm256_add_epi16(sq[0], sq[0]));
    __m256i l1 = _mm256_add_epi16(base[1], _mm256_add_epi16(sq[1], sq[1]));
    __m256i l2 = _mm256_add_epi16(base[2], _mm256_add_epi16(sq[2], sq[2]));
    __m256i l3 = _mm256_add_epi16(base[3], _mm256_add_epi16(sq[3], sq[3]));

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
    return sum_local_avx2(set, l0, l1, l2, l3, weights);
}

namespace detail {

void advance_eval_v3_for_test(
    const eval::PatternSet& set,
    eval::PatternState& next,
    const eval::PatternState& cur,
    Player mover,
    uint32_t square,
    uint64_t flips
) noexcept {
    Kernel<true>::advance_eval_state(set, next, cur, mover, square, flips);
}

void build_eval_base_v3_for_test(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    Player next_own,
    std::array<uint16_t, eval::MAX_PATTERN_FEATURES>& out
) noexcept {
    Kernel<true>::build_eval_base(set, cur, next_own, out);
}

int32_t sum_eval_state_v3_for_test(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    Player own,
    const int16_t* weights
) noexcept {
    return Kernel<true>::sum_eval_state(set, cur, own, weights);
}

int32_t sum_eval_after_move_v3_for_test(
    const eval::PatternSet& set,
    const std::array<uint16_t, eval::MAX_PATTERN_FEATURES>& parent_base,
    uint32_t square,
    uint64_t flips,
    const int16_t* weights
) noexcept {
    return Kernel<true>::sum_eval_after_move(set, parent_base, square, flips, weights);
}

} // namespace detail

} // namespace stello::search

#include "search/kernel_impl.hpp"

#include "endgame/last_impl.hpp"
#include "endgame/search_impl.hpp"
#include "search/engine_impl.hpp"
#include "search/ybwc_impl.hpp"

namespace stello::search {

// Isolated in its own x86-64-v3 translation unit so baseline x86-64 builds never execute
// AVX2/BMI1/BMI2/POPCNT/LZCNT instructions outside this CPUID-dispatched specialization.
template struct Kernel<true>;

template AbortVoid Kernel<true>::split<MidgameChild>(
    Searcher&,
    std::span<const MidgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&
);

template AbortVoid Kernel<true>::split<EndgameChild>(
    Searcher&,
    std::span<const EndgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&
);

} // namespace stello::search

#endif
