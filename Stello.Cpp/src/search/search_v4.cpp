#include "search/kernel.hpp"
#include "endgame/bits_avx512.hpp"
#include "search/eval_avx2_detail.hpp"
#include "search/microbench_impl.hpp"

#if STELLO_HAS_X86_64_V4 && STELLO_TU_HAS_AVX512_V4

namespace stello::search {

template <>
STELLO_FORCEINLINE uint64_t Kernel<cpu::CpuTier::V4>::flips(
    uint64_t own,
    uint64_t opponent,
    uint32_t square
) noexcept {
    return endgame::bits::flips_avx512cd(own, opponent, square);
}

template <>
STELLO_FORCEINLINE uint64_t Kernel<cpu::CpuTier::V4>::legal_moves(uint64_t own, uint64_t opponent) noexcept {
    return endgame::bits::legal_moves_avx512(own, opponent);
}

template <>
STELLO_FORCEINLINE void Kernel<cpu::CpuTier::V4>::dual_legal_moves(
    uint64_t own,
    uint64_t opponent,
    uint64_t& own_out,
    uint64_t& opp_out
) noexcept {
    own_out = endgame::bits::legal_moves_avx512(own, opponent);
    opp_out = endgame::bits::legal_moves_avx512(opponent, own);
}

template <>
STELLO_FORCEINLINE void Kernel<cpu::CpuTier::V4>::legal_and_potential(
    uint64_t own,
    uint64_t opponent,
    uint64_t& legal_out,
    int32_t& pot_out
) noexcept {
    endgame::bits::legal_and_potential_avx512(own, opponent, legal_out, pot_out);
}

template <>
STELLO_FORCEINLINE uint64_t Kernel<cpu::CpuTier::V4>::stable_of(uint64_t own, uint64_t opponent) noexcept {
    return endgame::bits::stable_of_avx512(own, opponent);
}

template <>
STELLO_FORCEINLINE uint64_t Kernel<cpu::CpuTier::V4>::transpose(uint64_t x) noexcept {
    return endgame::bits::transpose_avx2(x);
}

template <>
STELLO_FORCEINLINE uint16_t Kernel<cpu::CpuTier::V4>::simul_last_flip(uint64_t own, uint32_t square) noexcept {
    return endgame::bits::simul_last_flip_bmi2(own, square);
}

template <>
STELLO_FORCEINLINE eval::PatternState Kernel<cpu::CpuTier::V4>::eval_state_of(
    const eval::PatternSet& set,
    uint64_t digit1,
    uint64_t digit2
) noexcept {
    return detail::eval_avx2::eval_state_of_avx2(set, digit1, digit2);
}

template <>
STELLO_FORCEINLINE void Kernel<cpu::CpuTier::V4>::advance_eval_state(
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
STELLO_FORCEINLINE int32_t Kernel<cpu::CpuTier::V4>::sum_eval_state(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    const int32_t* packed_pair_weights,
    bool odd_slot
) noexcept {
    return detail::eval_avx2::sum_eval_state_avx2(set, cur, packed_pair_weights, odd_slot);
}

template <>
STELLO_FORCEINLINE int32_t Kernel<cpu::CpuTier::V4>::sum_eval_after_move(
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

uint64_t stable_of_v4_for_test(uint64_t own, uint64_t opponent) noexcept {
    return Kernel<cpu::CpuTier::V4>::stable_of(own, opponent);
}

int32_t solve_last_v4_for_test(uint64_t own, uint32_t square) noexcept {
    return endgame::bits::solve_last_avx512(own, square);
}

} // namespace detail

} // namespace stello::search

#include "search/kernel_impl.hpp"

#include "endgame/last_impl.hpp"
#include "endgame/search_impl.hpp"
#include "search/engine_impl.hpp"
#include "search/ybwc_impl.hpp"

namespace stello::search {

// Isolated in its own x86-64-v4 (256-bit EVEX AVX-512VL/CD/DQ/BW + GFNI) translation unit.
template struct Kernel<cpu::CpuTier::V4>;

template AbortVoid Kernel<cpu::CpuTier::V4>::split<MidgameChild>(
    Searcher&,
    std::span<const MidgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);

template AbortVoid Kernel<cpu::CpuTier::V4>::split<EndgameChild>(
    Searcher&,
    std::span<const EndgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);

namespace detail::microbench {

template std::vector<MicrobenchStats> run_kernel<cpu::CpuTier::V4>(const MicrobenchOptions& opts);

} // namespace detail::microbench

} // namespace stello::search

#endif
