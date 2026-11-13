#pragma once

#include "search/kernel.hpp"

namespace stello::search {

template <bool UseAvx2>
STELLO_FORCEINLINE uint64_t Kernel<UseAvx2>::flips(
    uint64_t own,
    uint64_t opponent,
    uint32_t square
) noexcept {
    return endgame::bits::flips(own, opponent, square);
}

template <bool UseAvx2>
STELLO_FORCEINLINE uint64_t Kernel<UseAvx2>::legal_moves(uint64_t own, uint64_t opponent) noexcept {
    return endgame::bits::legal_moves(own, opponent);
}

template <bool UseAvx2>
STELLO_FORCEINLINE Board Kernel<UseAvx2>::play_unchecked(
    const Board& board,
    Player player,
    uint32_t square
) noexcept {
    const uint64_t f = flips(board.discs(player), board.discs(opponent(player)), square);
    return board.play_flips(player, square, f);
}

template <bool UseAvx2>
STELLO_FORCEINLINE void Kernel<UseAvx2>::advance_eval_state(
    const eval::PatternSet& set,
    eval::PatternState& next,
    const eval::PatternState& cur,
    Player mover,
    uint32_t square,
    uint64_t flips
) noexcept {
    set.advance_state(next, cur, mover == Player::Black, square, flips);
}

template <bool UseAvx2>
STELLO_FORCEINLINE void Kernel<UseAvx2>::build_eval_base(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    Player next_own,
    std::array<uint16_t, eval::MAX_PATTERN_FEATURES>& out
) noexcept {
    set.build_base(cur, next_own == Player::Black, out);
}

template <bool UseAvx2>
STELLO_FORCEINLINE int32_t Kernel<UseAvx2>::sum_eval_state(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    Player own,
    const int16_t* weights
) noexcept {
    return set.sum_weights_from_state(cur, own == Player::Black, {weights, set.raw_size()});
}

template <bool UseAvx2>
STELLO_FORCEINLINE int32_t Kernel<UseAvx2>::sum_eval_after_move(
    const eval::PatternSet& set,
    const std::array<uint16_t, eval::MAX_PATTERN_FEATURES>& parent_base,
    uint32_t square,
    uint64_t flips,
    const int16_t* weights
) noexcept {
    return set.sum_weights_after_move(parent_base, square, flips, {weights, set.raw_size()});
}

} // namespace stello::search
