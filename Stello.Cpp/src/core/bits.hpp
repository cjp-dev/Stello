#pragma once

#include "endgame/bits.hpp"

#include <bit>
#include <cstdint>

namespace stello::bits {

namespace detail {

inline constexpr uint64_t NOT_COLUMN_A = 0xFEFE'FEFE'FEFE'FEFEULL;
inline constexpr uint64_t NOT_COLUMN_H = 0x7F7F'7F7F'7F7F'7F7FULL;
inline constexpr uint64_t INNER_COLUMNS = 0x7E7E'7E7E'7E7E'7E7EULL;
inline constexpr uint64_t INNER_ROWS = 0x00FF'FFFF'FFFF'FF00ULL;
inline constexpr uint64_t INNER = INNER_COLUMNS & INNER_ROWS;

inline uint64_t scan_left(uint64_t own, uint64_t mask, uint32_t shift) noexcept {
    uint64_t run = mask & (own << shift);
    run |= mask & (run << shift);
    run |= mask & (run << shift);
    run |= mask & (run << shift);
    run |= mask & (run << shift);
    run |= mask & (run << shift);
    return run << shift;
}

inline uint64_t scan_right(uint64_t own, uint64_t mask, uint32_t shift) noexcept {
    uint64_t run = mask & (own >> shift);
    run |= mask & (run >> shift);
    run |= mask & (run >> shift);
    run |= mask & (run >> shift);
    run |= mask & (run >> shift);
    run |= mask & (run >> shift);
    return run >> shift;
}

} // namespace detail

inline uint64_t legal_moves(uint64_t own, uint64_t opponent) noexcept {
    const uint64_t empty = ~(own | opponent);
    const uint64_t horizontal = opponent & detail::INNER_COLUMNS;
    const uint64_t vertical = opponent & detail::INNER_ROWS;
    const uint64_t diagonal = opponent & detail::INNER;

    const uint64_t moves = detail::scan_left(own, horizontal, 1)
                         | detail::scan_right(own, horizontal, 1)
                         | detail::scan_left(own, vertical, 8)
                         | detail::scan_right(own, vertical, 8)
                         | detail::scan_left(own, diagonal, 7)
                         | detail::scan_right(own, diagonal, 7)
                         | detail::scan_left(own, diagonal, 9)
                         | detail::scan_right(own, diagonal, 9);

    return moves & empty;
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
