#include "search/kernel.hpp"
#include "endgame/bits_avx2.hpp"
#include "search/eval_avx2_detail.hpp"
#include "search/microbench_impl.hpp"

#if STELLO_HAS_X86_64_V3

#include <cstring>
#include <immintrin.h>

#if defined(__has_feature)
#define STELLO_HAS_ASAN __has_feature(address_sanitizer)
#elif defined(__SANITIZE_ADDRESS__)
#define STELLO_HAS_ASAN 1
#else
#define STELLO_HAS_ASAN 0
#endif

namespace stello::search {

namespace detail {

void zero_slots_nt_avx2(void* dst, size_t bytes) noexcept {
#if STELLO_HAS_ASAN || !defined(__AVX2__)
    std::memset(dst, 0, bytes);
#else
    auto* p = static_cast<__m256i*>(dst);
    const __m256i z = _mm256_setzero_si256();
    const size_t chunks = bytes / sizeof(__m256i);
    for (size_t i = 0; i < chunks; ++i) {
        _mm256_stream_si256(p + i, z);
    }
    _mm_sfence();
#endif
}

} // namespace detail

template <>
STELLO_FORCEINLINE uint64_t Kernel<cpu::CpuTier::V3>::flips(
    uint64_t own,
    uint64_t opponent,
    uint32_t square
) noexcept {
    return endgame::bits::flips_avx2(own, opponent, square);
}

template <>
STELLO_FORCEINLINE uint64_t Kernel<cpu::CpuTier::V3>::legal_moves(uint64_t own, uint64_t opponent) noexcept {
    return endgame::bits::legal_moves_avx2(own, opponent);
}

template <>
STELLO_FORCEINLINE void Kernel<cpu::CpuTier::V3>::dual_legal_moves(
    uint64_t own,
    uint64_t opponent,
    uint64_t& own_out,
    uint64_t& opp_out
) noexcept {
    endgame::bits::dual_legal_moves_avx2(own, opponent, own_out, opp_out);
}

template <>
STELLO_FORCEINLINE void Kernel<cpu::CpuTier::V3>::legal_and_potential(
    uint64_t own,
    uint64_t opponent,
    uint64_t& legal_out,
    int32_t& pot_out
) noexcept {
    endgame::bits::legal_and_potential_avx2(own, opponent, legal_out, pot_out);
}

template <>
STELLO_FORCEINLINE uint64_t Kernel<cpu::CpuTier::V3>::stable_of(uint64_t own, uint64_t opponent) noexcept {
    return endgame::bits::stable_of_avx2(own, opponent);
}

template <>
STELLO_FORCEINLINE uint64_t Kernel<cpu::CpuTier::V3>::transpose(uint64_t x) noexcept {
    return endgame::bits::transpose_avx2(x);
}

template <>
STELLO_FORCEINLINE uint16_t Kernel<cpu::CpuTier::V3>::simul_last_flip(uint64_t own, uint32_t square) noexcept {
    return endgame::bits::simul_last_flip_bmi2(own, square);
}

template <>
STELLO_FORCEINLINE eval::PatternState Kernel<cpu::CpuTier::V3>::eval_state_of(
    const eval::PatternSet& set,
    uint64_t digit1,
    uint64_t digit2
) noexcept {
    return detail::eval_avx2::eval_state_of_avx2(set, digit1, digit2);
}

template <>
STELLO_FORCEINLINE void Kernel<cpu::CpuTier::V3>::advance_eval_state(
    const eval::PatternSet& set,
    eval::PatternState& next,
    const eval::PatternState& cur,
    bool mover_is_even_disc,
    uint32_t square,
    uint64_t flips
) noexcept {
    detail::eval_avx2::advance_eval_state_avx2(set, next, cur, mover_is_even_disc, square, flips);
}

template <>
STELLO_FORCEINLINE int32_t Kernel<cpu::CpuTier::V3>::sum_eval_state(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    const int32_t* packed_pair_weights,
    bool odd_slot
) noexcept {
    return detail::eval_avx2::sum_eval_state_avx2(set, cur, packed_pair_weights, odd_slot);
}

template <>
STELLO_FORCEINLINE int32_t Kernel<cpu::CpuTier::V3>::sum_eval_after_move(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    bool mover_is_even_disc,
    uint32_t square,
    uint64_t flips,
    const int32_t* packed_child_pair_weights,
    bool child_odd_slot
) noexcept {
    return detail::eval_avx2::sum_eval_after_move_avx2(
        set, cur, mover_is_even_disc, square, flips, packed_child_pair_weights, child_odd_slot);
}

namespace detail {

eval::PatternState state_of_eval_v3_for_test(
    const eval::PatternSet& set,
    uint64_t digit1,
    uint64_t digit2
) noexcept {
    return Kernel<cpu::CpuTier::V3>::eval_state_of(set, digit1, digit2);
}

uint64_t stable_of_v3_for_test(uint64_t own, uint64_t opponent) noexcept {
    return Kernel<cpu::CpuTier::V3>::stable_of(own, opponent);
}

void advance_eval_v3_for_test(
    const eval::PatternSet& set,
    eval::PatternState& next,
    const eval::PatternState& cur,
    bool mover_is_even_disc,
    uint32_t square,
    uint64_t flips
) noexcept {
    Kernel<cpu::CpuTier::V3>::advance_eval_state(set, next, cur, mover_is_even_disc, square, flips);
}

int32_t sum_eval_state_v3_for_test(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    const int32_t* packed_pair_weights,
    bool odd_slot
) noexcept {
    return Kernel<cpu::CpuTier::V3>::sum_eval_state(set, cur, packed_pair_weights, odd_slot);
}

int32_t sum_eval_after_move_v3_for_test(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    bool mover_is_even_disc,
    uint32_t square,
    uint64_t flips,
    const int32_t* packed_child_pair_weights,
    bool child_odd_slot
) noexcept {
    return Kernel<cpu::CpuTier::V3>::sum_eval_after_move(
        set, cur, mover_is_even_disc, square, flips, packed_child_pair_weights, child_odd_slot);
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
template struct Kernel<cpu::CpuTier::V3>;

template AbortVoid Kernel<cpu::CpuTier::V3>::split<MidgameChild>(
    Searcher&,
    std::span<const MidgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);

template AbortVoid Kernel<cpu::CpuTier::V3>::split<EndgameChild>(
    Searcher&,
    std::span<const EndgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);

namespace detail::microbench {

template std::vector<MicrobenchStats> run_kernel<cpu::CpuTier::V3>(const MicrobenchOptions& opts);

} // namespace detail::microbench

} // namespace stello::search

#endif
