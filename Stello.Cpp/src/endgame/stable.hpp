#pragma once

#include <cstdint>

namespace stello::endgame::stable {

/// Builds the edge stability tables eagerly rather than during the first search.
void init() noexcept;

/// Returns the compact 64 KB `(own_row << 8) | opp_row` edge-stability lookup table.
const uint8_t* edge_table() noexcept;

/// Unpacks row-packed inner edge bits (bits 1..6) to columns A (A2..A7) and H (H2..H7).
inline constexpr uint64_t unpack_a2a7(uint8_t x) noexcept {
    return ((static_cast<uint64_t>(x) & 0x7EULL) * 0x0000'0408'1020'4080ULL) & 0x0001'0101'0101'0100ULL;
}

inline constexpr uint64_t unpack_h2h7(uint8_t x) noexcept {
    return ((static_cast<uint64_t>(x) & 0x7EULL) * 0x0002'0408'1020'4000ULL) & 0x0080'8080'8080'8000ULL;
}

inline constexpr uint32_t pack_a1a8(uint64_t x) noexcept {
    return static_cast<uint32_t>((((x & 0x0101'0101'0101'0101ULL) * 0x0102'0408'1020'4080ULL) >> 56));
}

inline constexpr uint32_t pack_h1h8(uint64_t x) noexcept {
    return static_cast<uint32_t>((((x & 0x8080'8080'8080'8080ULL) * 0x0002'0408'1020'4081ULL) >> 56));
}

/// Counts `own`'s stable edge discs (fast 4-lookup lower estimate without full propagation).
int32_t edge_stability_count(uint64_t own, uint64_t opponent) noexcept;

/// Counts `own`'s stable corner-and-adjacent discs using 32-bit rotate arithmetic.
int32_t corner_stability(uint64_t discs) noexcept;

/// Counts `x` with corners (`A1, H1, A8, H8`) weighted twice.
int32_t bit_weighted_count(uint64_t x) noexcept;

/// Transposes bitboard across the `A1-H8` diagonal (portable SWAR).
uint64_t transpose_portable(uint64_t x) noexcept;

/// The stable discs of `own` (Egaroucid `calc_stability`). C#: `StableDiscs.Of`.
uint64_t of(uint64_t own, uint64_t opponent) noexcept;

} // namespace stello::endgame::stable
