#pragma once

#include "endgame/bits.hpp"

#include <bit>
#include <cstdint>

namespace stello::bits {

namespace detail {

inline constexpr uint64_t NOT_COLUMN_A = 0xFEFE'FEFE'FEFE'FEFEULL;
inline constexpr uint64_t NOT_COLUMN_H = 0x7F7F'7F7F'7F7F'7F7FULL;

} // namespace detail

inline uint64_t legal_moves(uint64_t own, uint64_t opponent) noexcept {
    return endgame::bits::legal_moves(own, opponent);
}

inline uint64_t flips(uint64_t own, uint64_t opponent, uint32_t square) noexcept {
    return endgame::bits::flips(own, opponent, square);
}

inline int32_t potential_mobility(uint64_t own, uint64_t opponent) noexcept {
    const uint64_t empty = ~(own | opponent);
    return static_cast<int32_t>(
        std::popcount((opponent << 1) & detail::NOT_COLUMN_A & empty)
        + std::popcount((opponent >> 1) & detail::NOT_COLUMN_H & empty)
        + std::popcount((opponent << 8) & empty)
        + std::popcount((opponent >> 8) & empty)
        + std::popcount((opponent << 7) & detail::NOT_COLUMN_H & empty)
        + std::popcount((opponent << 9) & detail::NOT_COLUMN_A & empty)
        + std::popcount((opponent >> 7) & detail::NOT_COLUMN_A & empty)
        + std::popcount((opponent >> 9) & detail::NOT_COLUMN_H & empty));
}

} // namespace stello::bits
