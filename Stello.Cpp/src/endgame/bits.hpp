#pragma once

#include <array>
#include <bit>
#include <cstdint>

namespace stello::endgame::bits {

/// A value no legal-move set can have (the four centre squares), for "not computed yet".
inline constexpr uint64_t LEGAL_UNDEFINED = 0x0000'0018'1800'0000ULL;

inline constexpr std::array<uint64_t, 64> FLIP_NEIGHBOURS = {
    0x0000000000000302ULL, 0x0000000000000604ULL, 0x0000000000000e0aULL, 0x0000000000001c14ULL,
    0x0000000000003828ULL, 0x0000000000007050ULL, 0x0000000000006020ULL, 0x000000000000c040ULL,
    0x0000000000030200ULL, 0x0000000000060400ULL, 0x00000000000e0a00ULL, 0x00000000001c1400ULL,
    0x0000000000382800ULL, 0x0000000000705000ULL, 0x0000000000602000ULL, 0x0000000000c04000ULL,
    0x0000000003020300ULL, 0x0000000006040600ULL, 0x000000000e0a0e00ULL, 0x000000001c141c00ULL,
    0x0000000038283800ULL, 0x0000000070507000ULL, 0x0000000060206000ULL, 0x00000000c040c000ULL,
    0x0000000302030000ULL, 0x0000000604060000ULL, 0x0000000e0a0e0000ULL, 0x0000001c141c0000ULL,
    0x0000003828380000ULL, 0x0000007050700000ULL, 0x0000006020600000ULL, 0x000000c040c00000ULL,
    0x0000030203000000ULL, 0x0000060406000000ULL, 0x00000e0a0e000000ULL, 0x00001c141c000000ULL,
    0x0000382838000000ULL, 0x0000705070000000ULL, 0x0000602060000000ULL, 0x0000c040c0000000ULL,
    0x0003020300000000ULL, 0x0006040600000000ULL, 0x000e0a0e00000000ULL, 0x001c141c00000000ULL,
    0x0038283800000000ULL, 0x0070507000000000ULL, 0x0060206000000000ULL, 0x00c040c000000000ULL,
    0x0002030000000000ULL, 0x0004060000000000ULL, 0x000a0e0000000000ULL, 0x00141c0000000000ULL,
    0x0028380000000000ULL, 0x0050700000000000ULL, 0x0020600000000000ULL, 0x0040c00000000000ULL,
    0x0203000000000000ULL, 0x0406000000000000ULL, 0x0a0e000000000000ULL, 0x141c000000000000ULL,
    0x2838000000000000ULL, 0x5070000000000000ULL, 0x2060000000000000ULL, 0x40c0000000000000ULL,
};

inline constexpr std::array<int32_t, 64> QUADRANT_BIT = {
    1, 1, 1, 1, 2, 2, 2, 2,
    1, 1, 1, 1, 2, 2, 2, 2,
    1, 1, 1, 1, 2, 2, 2, 2,
    1, 1, 1, 1, 2, 2, 2, 2,
    4, 4, 4, 4, 8, 8, 8, 8,
    4, 4, 4, 4, 8, 8, 8, 8,
    4, 4, 4, 4, 8, 8, 8, 8,
    4, 4, 4, 4, 8, 8, 8, 8,
};

inline constexpr std::array<int32_t, 65> STABILITY_THRESHOLD_NWS = {
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 64, 64, 64, 64, 64, 64, 64,
    64, 62, 62, 60, 60, 58, 58, 56,
    56, 54, 54, 52, 52, 50, 50, 48,
    48, 46, 44, 42, 40, 38, 36, 34,
    32, 30, 28, 26, 24, 22, 20, 16,
    14, 12, 10, 8, 6, 99, 99, 99,
    99,
};

inline constexpr std::array<int32_t, 65> STABILITY_THRESHOLD = {
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 62, 62, 60, 60, 58, 58, 56,
    56, 54, 54, 52, 52, 50, 50, 48,
    48, 46, 46, 44, 44, 42, 42, 40,
    40, 38, 36, 34, 32, 30, 28, 26,
    24, 22, 20, 18, 16, 14, 12, 8,
    6, 4, 2, 0, -2, 99, 99, 99,
    99,
};

namespace detail {

constexpr std::array<uint64_t, 64> create_all_neighbours() noexcept {
    std::array<uint64_t, 64> neighbours{};
    for (int32_t square = 0; square < 64; ++square) {
        const int32_t row = square >> 3;
        const int32_t column = square & 7;
        for (int32_t r = row - 1; r <= row + 1; ++r) {
            for (int32_t c = column - 1; c <= column + 1; ++c) {
                if (r >= 0 && r < 8 && c >= 0 && c < 8 && (r != row || c != column)) {
                    neighbours[static_cast<size_t>(square)] |= uint64_t{1} << (r * 8 + c);
                }
            }
        }
    }
    return neighbours;
}

constexpr std::array<std::array<uint64_t, 8>, 64> create_rays() noexcept {
    constexpr std::array<std::pair<int32_t, int32_t>, 8> kSteps = {{
        {0, 1}, {1, 0}, {1, -1}, {1, 1}, {0, -1}, {-1, 0}, {-1, 1}, {-1, -1}
    }};
    std::array<std::array<uint64_t, 8>, 64> rays{};
    for (int32_t square = 0; square < 64; ++square) {
        const int32_t row = square / 8;
        const int32_t column = square % 8;
        for (size_t d = 0; d < 8; ++d) {
            uint64_t ray = 0;
            int32_t r = row + kSteps[d].first;
            int32_t c = column + kSteps[d].second;
            while (r >= 0 && r < 8 && c >= 0 && c < 8) {
                ray |= uint64_t{1} << (r * 8 + c);
                r += kSteps[d].first;
                c += kSteps[d].second;
            }
            rays[static_cast<size_t>(square)][d] = ray;
        }
    }
    return rays;
}

} // namespace detail

inline constexpr auto ALL_NEIGHBOURS = detail::create_all_neighbours();
inline constexpr auto RAYS = detail::create_rays();

inline uint64_t flips(uint64_t own, uint64_t opponent, uint32_t square) noexcept {
    uint64_t flipped = 0;
    const uint64_t not_opponent = ~opponent;
    const auto& rays = RAYS[static_cast<size_t>(square)];
#if defined(__clang__)
#pragma clang loop vectorize(disable)
#pragma clang loop interleave(disable)
#endif
    for (size_t d = 0; d < 4; ++d) {
        const uint64_t ray = rays[d];
        uint64_t outflank = ray & not_opponent;
        outflank &= (0ULL - outflank);
        if ((outflank & own) != 0) {
            flipped |= (outflank - 1ULL) & ray;
        }
#if defined(__clang__) && (defined(__AVX2__) || defined(__AVX512F__)) && !defined(__EMSCRIPTEN__)
        // Keep Clang -march=native unrolled in scalar GPRs instead of SLP-vectorizing rays into ymm/zmm.
        __asm__ volatile("" : "+r"(flipped));
#endif
    }
#if defined(__clang__)
#pragma clang loop vectorize(disable)
#pragma clang loop interleave(disable)
#endif
    for (size_t d = 4; d < 8; ++d) {
        const uint64_t ray = rays[d];
        const uint64_t top = 0x8000'0000'0000'0000ULL >> std::countl_zero((ray & not_opponent) | 1ULL);
        const uint64_t outflanked = 0ULL - static_cast<uint64_t>((top & ray & own) != 0);
        flipped |= ray & ((0ULL - top) << 1) & outflanked;
#if defined(__clang__) && (defined(__AVX2__) || defined(__AVX512F__)) && !defined(__EMSCRIPTEN__)
        __asm__ volatile("" : "+r"(flipped));
#endif
    }
    return flipped;
}

inline uint64_t legal_moves(uint64_t p, uint64_t o) noexcept {
    const uint64_t m_o = o & 0x7e7e'7e7e'7e7e'7e7eULL;
    uint64_t flip1 = m_o & (p << 1);
    uint64_t flip7 = m_o & (p << 7);
    uint64_t flip9 = m_o & (p << 9);
    uint64_t flip8 = o & (p << 8);
    flip1 |= m_o & (flip1 << 1);
    flip7 |= m_o & (flip7 << 7);
    flip9 |= m_o & (flip9 << 9);
    flip8 |= o & (flip8 << 8);
    uint64_t pre1 = m_o & (m_o << 1);
    uint64_t pre7 = m_o & (m_o << 7);
    uint64_t pre9 = m_o & (m_o << 9);
    uint64_t pre8 = o & (o << 8);
    flip1 |= pre1 & (flip1 << 2);
    flip7 |= pre7 & (flip7 << 14);
    flip9 |= pre9 & (flip9 << 18);
    flip8 |= pre8 & (flip8 << 16);
    flip1 |= pre1 & (flip1 << 2);
    flip7 |= pre7 & (flip7 << 14);
    flip9 |= pre9 & (flip9 << 18);
    flip8 |= pre8 & (flip8 << 16);
    uint64_t moves = (flip1 << 1) | (flip7 << 7) | (flip9 << 9) | (flip8 << 8);

    flip1 = m_o & (p >> 1);
    flip7 = m_o & (p >> 7);
    flip9 = m_o & (p >> 9);
    flip8 = o & (p >> 8);
    flip1 |= m_o & (flip1 >> 1);
    flip7 |= m_o & (flip7 >> 7);
    flip9 |= m_o & (flip9 >> 9);
    flip8 |= o & (flip8 >> 8);
    pre1 >>= 1;
    pre7 >>= 7;
    pre9 >>= 9;
    pre8 >>= 8;
    flip1 |= pre1 & (flip1 >> 2);
    flip7 |= pre7 & (flip7 >> 14);
    flip9 |= pre9 & (flip9 >> 18);
    flip8 |= pre8 & (flip8 >> 16);
    flip1 |= pre1 & (flip1 >> 2);
    flip7 |= pre7 & (flip7 >> 14);
    flip9 |= pre9 & (flip9 >> 18);
    flip8 |= pre8 & (flip8 >> 16);
    moves |= (flip1 >> 1) | (flip7 >> 7) | (flip9 >> 9) | (flip8 >> 8);
    return moves & ~(p | o);
}

inline uint64_t lone_empties(uint64_t own, uint64_t opponent) noexcept {
    const uint64_t empty = ~(own | opponent);
    uint64_t t0 = (empty & 0x7f7f'7f7f'7f7f'7f7fULL) << 1;
    uint64_t t1 = (empty & 0xfefe'fefe'fefe'fefeULL) >> 1;
    const uint64_t e1 = t0 | t1;
    t0 = empty | e1;
    t1 = t0 << 8;
    t0 >>= 8;
    return ~(e1 | t0 | t1) & empty;
}

inline bool is_lone_empty(uint32_t square, uint64_t empties) noexcept {
    return (ALL_NEIGHBOURS[static_cast<size_t>(square)] & empties) == 0;
}

inline int32_t parity(uint64_t empty) noexcept {
    return (std::popcount(empty & 0x0000'0000'0F0F'0F0FULL) & 1)
         | ((std::popcount(empty & 0x0000'0000'F0F0'F0F0ULL) & 1) << 1)
         | ((std::popcount(empty & 0x0F0F'0F0F'0000'0000ULL) & 1) << 2)
         | ((std::popcount(empty & 0xF0F0'F0F0'0000'0000ULL) & 1) << 3);
}

inline int32_t moves_corners_twice(uint64_t legal) noexcept {
    return static_cast<int32_t>(std::popcount(legal) + std::popcount(legal & 0x8100'0000'0000'0081ULL));
}

inline int32_t weighted_moves(uint64_t legal) noexcept {
    return static_cast<int32_t>(std::popcount(legal) * 2 + std::popcount(legal & 0x8100'0000'0000'0081ULL));
}

inline int32_t potential_mobility(uint64_t discs, uint64_t empties) noexcept {
    const uint64_t h = discs & 0x7E7E'7E7E'7E7E'7E7EULL;
    const uint64_t v = discs & 0x00FF'FFFF'FFFF'FF00ULL;
    const uint64_t hv = discs & 0x007E'7E7E'7E7E'7E00ULL;
    const uint64_t around = (h << 1) | (h >> 1) | (v << 8) | (v >> 8)
                          | (hv << 7) | (hv >> 7) | (hv << 9) | (hv >> 9);
    return static_cast<int32_t>(std::popcount(empties & around));
}

inline int32_t score_with_empties(uint64_t own, int32_t empties) noexcept {
    int32_t score = static_cast<int32_t>(std::popcount(own)) * 2 + empties;
    score += (((score >> 6) & 1) + (((score + 63) >> 7) & 1) - 1) * empties;
    return score - 64;
}

inline int32_t score_with_odd_empties(uint64_t own, int32_t empties) noexcept {
    int32_t score = static_cast<int32_t>(std::popcount(own)) * 2 + empties;
    score += (((score >> 5) & 2) - 1) * empties;
    return score - 64;
}

} // namespace stello::endgame::bits
