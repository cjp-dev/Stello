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

} // namespace stello::search
