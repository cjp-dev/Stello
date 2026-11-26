#pragma once

#include "endgame/stable.hpp"
#include "search/kernel.hpp"

namespace stello::search {

template <cpu::CpuTier Tier>
STELLO_FORCEINLINE uint64_t Kernel<Tier>::flips(
    uint64_t own,
    uint64_t opponent,
    uint32_t square
) noexcept {
    return endgame::bits::flips(own, opponent, square);
}

template <cpu::CpuTier Tier>
STELLO_FORCEINLINE uint64_t Kernel<Tier>::legal_moves(uint64_t own, uint64_t opponent) noexcept {
    return endgame::bits::legal_moves(own, opponent);
}

template <cpu::CpuTier Tier>
STELLO_FORCEINLINE void Kernel<Tier>::dual_legal_moves(
    uint64_t own,
    uint64_t opponent,
    uint64_t& own_out,
    uint64_t& opp_out
) noexcept {
    own_out = legal_moves(own, opponent);
    opp_out = legal_moves(opponent, own);
}

template <cpu::CpuTier Tier>
STELLO_FORCEINLINE void Kernel<Tier>::legal_and_potential(
    uint64_t own,
    uint64_t opponent,
    uint64_t& legal_out,
    int32_t& pot_out
) noexcept {
    legal_out = legal_moves(own, opponent);
    pot_out = endgame::bits::potential_mobility(opponent, ~(own | opponent));
}

template <cpu::CpuTier Tier>
STELLO_FORCEINLINE uint64_t Kernel<Tier>::stable_of(uint64_t own, uint64_t opponent) noexcept {
    return endgame::stable::of(own, opponent);
}

template <cpu::CpuTier Tier>
STELLO_FORCEINLINE uint64_t Kernel<Tier>::transpose(uint64_t x) noexcept {
    return endgame::stable::transpose_portable(x);
}

template <cpu::CpuTier Tier>
STELLO_FORCEINLINE uint16_t Kernel<Tier>::simul_last_flip(uint64_t own, uint32_t square) noexcept {
    return endgame::bits::simul_last_flip_portable(own, square);
}

template <cpu::CpuTier Tier>
STELLO_FORCEINLINE Board Kernel<Tier>::play_unchecked(
    const Board& board,
    Player player,
    uint32_t square
) noexcept {
    const uint64_t f = flips(board.discs(player), board.discs(opponent(player)), square);
    return board.play_flips(player, square, f);
}

template <cpu::CpuTier Tier>
STELLO_FORCEINLINE eval::PatternState Kernel<Tier>::eval_state_of(
    const eval::PatternSet& set,
    uint64_t digit1,
    uint64_t digit2
) noexcept {
    return set.state_of(digit1, digit2);
}

template <cpu::CpuTier Tier>
STELLO_FORCEINLINE void Kernel<Tier>::advance_eval_state(
    const eval::PatternSet& set,
    eval::PatternState& next,
    const eval::PatternState& cur,
    bool mover_is_even_disc,
    uint32_t square,
    uint64_t flips
) noexcept {
    set.advance_state(next, cur, mover_is_even_disc, square, flips);
}

template <cpu::CpuTier Tier>
STELLO_FORCEINLINE int32_t Kernel<Tier>::sum_eval_state(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    const int32_t* packed_pair_weights,
    bool odd_slot,
    bool
) noexcept {
    return set.sum_weights_from_state(cur, packed_pair_weights, odd_slot);
}

template <cpu::CpuTier Tier>
STELLO_FORCEINLINE int32_t Kernel<Tier>::sum_eval_after_move(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    bool mover_is_even_disc,
    uint32_t square,
    uint64_t flips,
    const int32_t* packed_child_pair_weights,
    bool child_odd_slot,
    bool
) noexcept {
    return set.sum_weights_after_move(cur, mover_is_even_disc, square, flips, packed_child_pair_weights, child_odd_slot);
}

} // namespace stello::search
