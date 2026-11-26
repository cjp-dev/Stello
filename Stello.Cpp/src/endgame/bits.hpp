#pragma once

#include "core/compiler.hpp"

#include <array>
#include <bit>
#include <cstdint>

#ifdef __wasm_simd128__
#include <wasm_simd128.h>
#endif

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

#ifdef __wasm_simd128__
namespace detail {

struct alignas(16) UpperRays128 {
    uint64_t r0, r1, r2, r3;
};

constexpr std::array<UpperRays128, 64> create_upper_rays_128() noexcept {
    std::array<UpperRays128, 64> out{};
    for (size_t sq = 0; sq < 64; ++sq) {
        out[sq] = UpperRays128{RAYS[sq][0], RAYS[sq][1], RAYS[sq][2], RAYS[sq][3]};
    }
    return out;
}

inline constexpr auto UPPER_RAYS_128 = create_upper_rays_128();

} // namespace detail
#endif

inline uint64_t flips(uint64_t own, uint64_t opponent, uint32_t square) noexcept {
#ifdef __wasm_simd128__
    const v128_t pp = wasm_i64x2_splat(static_cast<int64_t>(own));
    const v128_t oo = wasm_i64x2_splat(static_cast<int64_t>(opponent));
    const v128_t neg1 = wasm_i64x2_splat(-1);
    const auto& ur = detail::UPPER_RAYS_128[static_cast<size_t>(square)];
    auto eval_upper = [&](const uint64_t* pair) noexcept -> v128_t {
        const v128_t mask = wasm_v128_load(pair);
        v128_t lo = wasm_v128_andnot(mask, oo);
        lo = wasm_v128_and(wasm_v128_xor(wasm_i64x2_add(lo, neg1), lo), mask);
        const v128_t lf = wasm_v128_andnot(lo, pp);
        return wasm_v128_andnot(lf, wasm_i64x2_eq(lf, lo));
    };
    const v128_t f_up = wasm_v128_or(eval_upper(&ur.r0), eval_upper(&ur.r2));
    uint64_t flipped = static_cast<uint64_t>(wasm_i64x2_extract_lane(f_up, 0))
                     | static_cast<uint64_t>(wasm_i64x2_extract_lane(f_up, 1));
    const uint64_t not_opponent = ~opponent;
    const auto& rays = RAYS[static_cast<size_t>(square)];
    for (size_t d = 4; d < 8; ++d) {
        const uint64_t ray = rays[d];
        const uint64_t top = 0x8000'0000'0000'0000ULL >> std::countl_zero((ray & not_opponent) | 1ULL);
        const uint64_t outflanked = 0ULL - static_cast<uint64_t>((top & ray & own) != 0);
        flipped |= ray & ((0ULL - top) << 1) & outflanked;
    }
    return flipped;
#else
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
#endif
}

inline uint64_t legal_moves(uint64_t p, uint64_t o) noexcept {
#ifdef __wasm_simd128__
    const v128_t p_v = wasm_i64x2_splat(static_cast<int64_t>(p));
    const v128_t pp = wasm_i8x16_shuffle(p_v, p_v, 0, 1, 2, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10, 9, 8);
    const v128_t o_v = wasm_i64x2_splat(static_cast<int64_t>(o));
    const v128_t oo = wasm_i8x16_shuffle(o_v, o_v, 0, 1, 2, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10, 9, 8);
    const v128_t moo = wasm_v128_and(oo, wasm_i64x2_splat(0x7E7E'7E7E'7E7E'7E7ELL));
    const v128_t occupied = wasm_v128_or(pp, oo);

    // shift=+1
    v128_t mm = wasm_i32x4_add(wasm_v128_and(wasm_i8x16_add(pp, pp), moo), moo);
    // shift=-1
    v128_t r1 = wasm_v128_andnot(moo, wasm_i8x16_add(occupied, occupied));
    r1 = wasm_v128_and(r1, wasm_i64x2_make(0x0E0E'0E0E'0E0E'0E0ELL, 0x7070'7070'7070'7070LL));
    r1 = wasm_u8x16_min(r1, wasm_i64x2_make(0x0808'0808'0808'0808LL, 0x4040'4040'4040'4040LL));
    v128_t flip = wasm_u8x16_sub_sat(wasm_v128_and(wasm_i8x16_add(moo, r1), pp), r1);
    mm = wasm_v128_or(mm, wasm_u64x2_shr(flip, 1));

    // shift=-8:+8
    flip = wasm_v128_and(oo, wasm_i64x2_shl(pp, 8));
    flip = wasm_v128_or(flip, wasm_v128_and(oo, wasm_i64x2_shl(flip, 8)));
    v128_t pre = wasm_v128_and(oo, wasm_i64x2_shl(oo, 8));
    flip = wasm_v128_or(flip, wasm_v128_and(pre, wasm_i64x2_shl(flip, 16)));
    flip = wasm_v128_or(flip, wasm_v128_and(pre, wasm_i64x2_shl(flip, 16)));
    mm = wasm_v128_or(mm, wasm_i64x2_shl(flip, 8));

    // shift=-9:+7
    flip = wasm_v128_and(moo, wasm_i64x2_shl(pp, 7));
    flip = wasm_v128_or(flip, wasm_v128_and(moo, wasm_i64x2_shl(flip, 7)));
    pre = wasm_v128_and(moo, wasm_i64x2_shl(moo, 7));
    flip = wasm_v128_or(flip, wasm_v128_and(pre, wasm_i64x2_shl(flip, 14)));
    flip = wasm_v128_or(flip, wasm_v128_and(pre, wasm_i64x2_shl(flip, 14)));
    mm = wasm_v128_or(mm, wasm_i64x2_shl(flip, 7));

    // shift=+9:-7
    flip = wasm_v128_and(moo, wasm_u64x2_shr(pp, 7));
    flip = wasm_v128_or(flip, wasm_v128_and(moo, wasm_u64x2_shr(flip, 7)));
    pre = wasm_u64x2_shr(pre, 7);
    flip = wasm_v128_or(flip, wasm_v128_and(pre, wasm_u64x2_shr(flip, 14)));
    flip = wasm_v128_or(flip, wasm_v128_and(pre, wasm_u64x2_shr(flip, 14)));
    mm = wasm_v128_or(mm, wasm_u64x2_shr(flip, 7));

    mm = wasm_v128_andnot(mm, occupied);
    const v128_t mm_rev = wasm_i8x16_shuffle(mm, mm, 15, 14, 13, 12, 11, 10, 9, 8, 0, 1, 2, 3, 4, 5, 6, 7);
    return static_cast<uint64_t>(wasm_i64x2_extract_lane(wasm_v128_or(mm, mm_rev), 0));
#else
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
#endif
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

namespace last_flip_detail {

constexpr uint16_t cf_entry_1d(uint32_t len, uint32_t place, uint32_t pattern) noexcept {
    uint32_t p_flips = 0;
    uint32_t o_flips = 0;
    if (place + 2U < len) {
        const uint32_t first = (pattern >> (place + 1U)) & 1U;
        for (uint32_t i = place + 2U; i < len; ++i) {
            if (((pattern >> i) & 1U) != first) {
                const uint32_t count = i - place - 1U;
                if (first == 0U) {
                    p_flips += count;
                } else {
                    o_flips += count;
                }
                break;
            }
        }
    }
    if (place >= 2U) {
        const uint32_t first = (pattern >> (place - 1U)) & 1U;
        for (int32_t i = static_cast<int32_t>(place) - 2; i >= 0; --i) {
            if (((pattern >> static_cast<uint32_t>(i)) & 1U) != first) {
                const uint32_t count = place - 1U - static_cast<uint32_t>(i);
                if (first == 0U) {
                    p_flips += count;
                } else {
                    o_flips += count;
                }
                break;
            }
        }
    }
    return static_cast<uint16_t>(((o_flips * 2U) << 8U) | (p_flips * 2U));
}

struct PortableLastLines {
    uint64_t col_mask{0};
    uint64_t col_mult{0};
    uint64_t d1_mask{0};
    uint64_t d2_mask{0};
    uint16_t d1_ofs{2048};
    uint16_t d2_ofs{2048};
    uint8_t d1_shift{56};
    uint8_t d2_shift{56};
};

inline constexpr size_t PORTABLE_CF_SIZE =
    2048U + 1U + (3U * 8U) + (4U * 16U) + (5U * 32U) + (6U * 64U) + (7U * 128U);

constexpr uint16_t short_diag_offset(uint32_t len, uint32_t local_idx) noexcept {
    if (len < 3U) {
        return 2048U;
    }
    if (len == 8U) {
        return static_cast<uint16_t>(local_idx * 256U);
    }
    uint32_t base = 2049U;
    for (uint32_t l = 3U; l < len; ++l) {
        base += l * (1U << l);
    }
    return static_cast<uint16_t>(base + local_idx * (1U << len));
}

constexpr std::array<uint16_t, PORTABLE_CF_SIZE> make_portable_cf() noexcept {
    std::array<uint16_t, PORTABLE_CF_SIZE> cf{};
    for (uint32_t place = 0; place < 8U; ++place) {
        for (uint32_t pat = 0; pat < 256U; ++pat) {
            cf[place * 256U + pat] = cf_entry_1d(8U, place, pat);
        }
    }
    cf[2048] = 0;
    for (uint32_t len = 3U; len <= 7U; ++len) {
        const uint32_t size = 1U << len;
        for (uint32_t place = 0; place < len; ++place) {
            const uint32_t ofs = short_diag_offset(len, place);
            for (uint32_t pat = 0; pat < size; ++pat) {
                cf[ofs + pat] = cf_entry_1d(len, place, pat);
            }
        }
    }
    return cf;
}

constexpr std::array<PortableLastLines, 64> make_portable_last_lines() noexcept {
    std::array<PortableLastLines, 64> lines{};
    for (int32_t square = 0; square < 64; ++square) {
        const int32_t col = square & 7;
        const int32_t row = square >> 3;
        PortableLastLines entry{};
        for (int32_t r = 0; r < 8; ++r) {
            entry.col_mask |= uint64_t{1} << (r * 8 + col);
            entry.col_mult |= uint64_t{1} << (56 - col - 7 * r);
        }

        const int32_t d1_c0 = (row > col) ? 0 : (col - row);
        const int32_t d1_c1 = (col - row + 7 < 7) ? (col - row + 7) : 7;
        const uint32_t d1_len = static_cast<uint32_t>(d1_c1 - d1_c0 + 1);
        if (d1_len >= 3U) {
            for (int32_t c = d1_c0; c <= d1_c1; ++c) {
                const int32_t r = row + (c - col);
                entry.d1_mask |= uint64_t{1} << (r * 8 + c);
            }
            entry.d1_shift = static_cast<uint8_t>(56 + d1_c0);
            entry.d1_ofs = short_diag_offset(d1_len, static_cast<uint32_t>(col - d1_c0));
        }

        const int32_t sum_cr = col + row;
        const int32_t d2_c0 = (sum_cr > 7) ? (sum_cr - 7) : 0;
        const int32_t d2_c1 = (sum_cr < 7) ? sum_cr : 7;
        const uint32_t d2_len = static_cast<uint32_t>(d2_c1 - d2_c0 + 1);
        if (d2_len >= 3U) {
            for (int32_t c = d2_c0; c <= d2_c1; ++c) {
                const int32_t r = sum_cr - c;
                entry.d2_mask |= uint64_t{1} << (r * 8 + c);
            }
            entry.d2_shift = static_cast<uint8_t>(56 + d2_c0);
            entry.d2_ofs = short_diag_offset(d2_len, static_cast<uint32_t>(col - d2_c0));
        }

        lines[static_cast<size_t>(square)] = entry;
    }
    return lines;
}

inline constexpr auto PORTABLE_CF = make_portable_cf();
inline constexpr auto PORTABLE_LAST_LINES = make_portable_last_lines();

} // namespace last_flip_detail

/// Branchless simultaneous (2*O_flips << 8) | (2*P_flips) lookup for square `square` (portable & WASM).
STELLO_FORCEINLINE uint16_t simul_last_flip_portable(uint64_t own, uint32_t square) noexcept {
    const uint32_t col = square & 7U;
    const uint32_t row8 = square & 56U;
    const auto& pl = last_flip_detail::PORTABLE_LAST_LINES[static_cast<size_t>(square)];
    uint16_t op_flip = last_flip_detail::PORTABLE_CF[col * 256U + static_cast<uint32_t>((own >> row8) & 0xFFULL)];
    op_flip = static_cast<uint16_t>(
        op_flip + last_flip_detail::PORTABLE_CF[(row8 << 5U) + static_cast<uint32_t>(((own & pl.col_mask) * pl.col_mult) >> 56U)]);
    op_flip = static_cast<uint16_t>(
        op_flip + last_flip_detail::PORTABLE_CF[pl.d1_ofs + static_cast<uint32_t>(((own & pl.d1_mask) * 0x0101'0101'0101'0101ULL) >> pl.d1_shift)]);
    op_flip = static_cast<uint16_t>(
        op_flip + last_flip_detail::PORTABLE_CF[pl.d2_ofs + static_cast<uint32_t>(((own & pl.d2_mask) * 0x0101'0101'0101'0101ULL) >> pl.d2_shift)]);
    return op_flip;
}

/// Bitwise horizontal mirror across files A-H (portable implementation).
STELLO_FORCEINLINE uint64_t horizontal_mirror_portable(uint64_t x) noexcept {
    x = ((x >> 1U) & 0x5555'5555'5555'5555ULL) | ((x & 0x5555'5555'5555'5555ULL) << 1U);
    x = ((x >> 2U) & 0x3333'3333'3333'3333ULL) | ((x & 0x3333'3333'3333'3333ULL) << 2U);
    return ((x >> 4U) & 0x0F0F'0F0F'0F0F'0F0FULL) | ((x & 0x0F0F'0F0F'0F0F'0F0FULL) << 4U);
}

} // namespace stello::endgame::bits
