#pragma once

#include "core/compiler.hpp"
#include "endgame/bits.hpp"
#include "endgame/bits_avx2.hpp"
#include "endgame/stable.hpp"

#if STELLO_TU_HAS_AVX512_V4

#include <immintrin.h>

#include <array>
#include <bit>
#include <cstdint>

namespace stello::endgame::bits {

namespace avx512_detail {

struct alignas(64) GfniLrMask {
    uint64_t upper[4];
    uint64_t rev_lower[4];
};

static_assert(sizeof(GfniLrMask) == 64);

constexpr uint64_t reverse_bits64(uint64_t x) noexcept {
    uint64_t r = 0;
    for (uint32_t i = 0; i < 64U; ++i) {
        if (((x >> i) & 1ULL) != 0ULL) {
            r |= uint64_t{1} << (63U - i);
        }
    }
    return r;
}

constexpr std::array<GfniLrMask, 64> make_gfni_lr_mask() noexcept {
    std::array<GfniLrMask, 64> table{};
    for (size_t sq = 0; sq < 64U; ++sq) {
        table[sq] = GfniLrMask{
            {RAYS[sq][0], RAYS[sq][1], RAYS[sq][2], RAYS[sq][3]},
            {reverse_bits64(RAYS[sq][4]), reverse_bits64(RAYS[sq][5]),
             reverse_bits64(RAYS[sq][6]), reverse_bits64(RAYS[sq][7])},
        };
    }
    return table;
}

inline constexpr std::array<GfniLrMask, 64> GFNI_LR_MASK = make_gfni_lr_mask();

} // namespace avx512_detail

/// 256-bit AVX-512VL+CD partial-reduction flip computation (`flips_avx512cd`, zero zmm).
inline __m128i STELLO_VECTORCALL flips_avx512cd_v128(__m128i op, uint32_t square) noexcept {
    const __m256i pp = _mm256_broadcastq_epi64(op);
    const __m256i oo = _mm256_broadcastq_epi64(_mm_unpackhi_epi64(op, op));
    const __m256i minusone = _mm256_set1_epi64x(-1);

    const __m256i rm = _mm256_load_si256(reinterpret_cast<const __m256i*>(avx2_detail::LR_MASK[square].lower));
    const __m256i rp = _mm256_and_si256(pp, rm);
    const __m256i t0 = _mm256_srlv_epi64(minusone, _mm256_lzcnt_epi64(rp));
    const __m256i re = _mm256_ternarylogic_epi64(oo, pp, rm, 0x02);
    __m256i ff = _mm256_ternarylogic_epi64(_mm256_cmpgt_epi64(rp, re), t0, rm, 0x20);

    const __m256i lm = _mm256_load_si256(reinterpret_cast<const __m256i*>(avx2_detail::LR_MASK[square].upper));
    const __m256i lo = _mm256_andnot_si256(oo, lm);
    const __m256i t2 = _mm256_ternarylogic_epi64(lm, _mm256_add_epi64(lo, minusone), lo, 0x60);
    ff = _mm256_mask_ternarylogic_epi64(ff, _mm256_test_epi64_mask(pp, t2), pp, t2, 0xf2);

    return _mm_or_si128(_mm256_castsi256_si128(ff), _mm256_extracti128_si256(ff, 1));
}

inline uint64_t flips_avx512cd(uint64_t own, uint64_t opponent, uint32_t square) noexcept {
    const __m128i op = _mm_set_epi64x(static_cast<int64_t>(opponent), static_cast<int64_t>(own));
    return reduce_flip_v128(flips_avx512cd_v128(op, square));
}

/// 256-bit AVX-512VL+GFNI partial-reduction flip computation (zero zmm).
inline __m128i STELLO_VECTORCALL flips_avx512_gfni_v128(__m128i op, uint32_t square) noexcept {
    const __m256i opop = _mm256_insertf128_si256(_mm256_castsi128_si256(op), op, 1);
    const __m256i ropop = _mm256_shuffle_epi8(
        _mm256_gf2p8affine_epi64_epi8(opop, _mm256_set1_epi64x(static_cast<int64_t>(0x8040'2010'0804'0201ULL)), 0),
        _mm256_set_epi8(
            8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7,
            8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7));

    const __m256i p_upper = _mm256_broadcastq_epi64(_mm256_castsi256_si128(opop));
    const __m256i o_upper = _mm256_broadcastq_epi64(_mm_unpackhi_epi64(_mm256_castsi256_si128(opop), _mm256_castsi256_si128(opop)));
    const __m256i rp_lower = _mm256_broadcastq_epi64(_mm256_castsi256_si128(ropop));
    const __m256i ro_lower = _mm256_broadcastq_epi64(_mm_unpackhi_epi64(_mm256_castsi256_si128(ropop), _mm256_castsi256_si128(ropop)));

    const __m256i minusone = _mm256_set1_epi64x(-1);
    const __m256i ml = _mm256_load_si256(reinterpret_cast<const __m256i*>(avx512_detail::GFNI_LR_MASK[square].upper));
    const __m256i mh = _mm256_load_si256(reinterpret_cast<const __m256i*>(avx512_detail::GFNI_LR_MASK[square].rev_lower));

    const __m256i lo_u = _mm256_andnot_si256(o_upper, ml);
    const __m256i t_u = _mm256_ternarylogic_epi64(ml, _mm256_add_epi64(lo_u, minusone), lo_u, 0x60);
    const __m256i f_u = _mm256_maskz_andnot_epi64(_mm256_test_epi64_mask(p_upper, t_u), p_upper, t_u);

    const __m256i lo_l = _mm256_andnot_si256(ro_lower, mh);
    const __m256i t_l = _mm256_ternarylogic_epi64(mh, _mm256_add_epi64(lo_l, minusone), lo_l, 0x60);
    const __m256i f_l_rev = _mm256_maskz_andnot_epi64(_mm256_test_epi64_mask(rp_lower, t_l), rp_lower, t_l);

    const __m128i u2 = _mm_or_si128(_mm256_castsi256_si128(f_u), _mm256_extracti128_si256(f_u, 1));
    const __m128i l2_rev = _mm_or_si128(_mm256_castsi256_si128(f_l_rev), _mm256_extracti128_si256(f_l_rev, 1));
    const __m128i l2 = _mm_shuffle_epi8(
        _mm_gf2p8affine_epi64_epi8(l2_rev, _mm_set1_epi64x(static_cast<int64_t>(0x8040'2010'0804'0201ULL)), 0),
        _mm_set_epi8(8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7));

    return _mm_or_si128(u2, l2);
}

inline uint64_t flips_avx512_gfni(uint64_t own, uint64_t opponent, uint32_t square) noexcept {
    const __m128i op = _mm_set_epi64x(static_cast<int64_t>(opponent), static_cast<int64_t>(own));
    return reduce_flip_v128(flips_avx512_gfni_v128(op, square));
}

/// 1-instruction GFNI horizontal mirror across files A-H.
inline uint64_t horizontal_mirror_gfni(uint64_t x) noexcept {
    const __m128i v = _mm_cvtsi64_si128(static_cast<int64_t>(x));
    return static_cast<uint64_t>(_mm_cvtsi128_si64(
        _mm_gf2p8affine_epi64_epi8(v, _mm_set1_epi64x(static_cast<int64_t>(0x8040'2010'0804'0201ULL)), 0)));
}

/// 256-bit AVX-512VL+CD simultaneous (P_flips_mask, O_flips_mask) computation (`qword0=P_flips, qword1=O_flips`).
inline __m128i simul_last_flip_pair_avx512(uint64_t own, uint32_t square) noexcept {
    const __m256i minusone = _mm256_set1_epi64x(-1);
    const __m256i one = _mm256_srli_epi64(minusone, 63);
    const __m256i mask3f = _mm256_srli_epi64(minusone, 58);
    const __m256i pp = _mm256_set1_epi64x(static_cast<int64_t>(own));

    // Upper rays: first matching/opposite LS1B.
    const __m256i umask = _mm256_load_si256(reinterpret_cast<const __m256i*>(avx2_detail::LR_MASK[square].upper));
    const __m256i p_up = _mm256_and_si256(pp, umask);
    const __m256i o_up = _mm256_andnot_si256(pp, umask);
    __m256i p_flip = _mm256_sub_epi64(p_up, _mm256_min_epu64(p_up, one));
    __m256i o_flip = _mm256_sub_epi64(o_up, _mm256_min_epu64(o_up, one));
    p_flip = _mm256_ternarylogic_epi64(p_up, p_flip, umask, 0x08);
    o_flip = _mm256_ternarylogic_epi64(o_up, o_flip, umask, 0x08);

    // Lower rays: clear all bits lower than outflank.
    const __m256i lmask = _mm256_load_si256(reinterpret_cast<const __m256i*>(avx2_detail::LR_MASK[square].lower));
    const __m256i p_lo = _mm256_and_si256(pp, lmask);
    const __m256i o_lo = _mm256_andnot_si256(pp, lmask);
    const __m256i p_eraser = _mm256_srlv_epi64(minusone, _mm256_and_si256(_mm256_lzcnt_epi64(p_lo), mask3f));
    const __m256i o_eraser = _mm256_srlv_epi64(minusone, _mm256_and_si256(_mm256_lzcnt_epi64(o_lo), mask3f));
    p_flip = _mm256_ternarylogic_epi64(p_flip, p_eraser, lmask, 0xf2);
    o_flip = _mm256_ternarylogic_epi64(o_flip, o_eraser, lmask, 0xf2);

    const __m256i opop = _mm256_or_si256(_mm256_unpacklo_epi64(p_flip, o_flip), _mm256_unpackhi_epi64(p_flip, o_flip));
    return _mm_or_si128(_mm256_castsi256_si128(opop), _mm256_extracti128_si256(opop, 1));
}

/// Branchless LUT-free 256-bit AVX-512VL+CD `solve_last` matching Phase-1 terminal rules.
inline int32_t solve_last_avx512(uint64_t own, uint32_t square) noexcept {
    const __m256i pp = _mm256_set1_epi64x(static_cast<int64_t>(own));
    const __m128i p2 = _mm256_castsi256_si128(pp);

    const __m256i minusone256 = _mm256_set1_epi64x(-1);
    const __m256i one = _mm256_srli_epi64(minusone256, 63);
    const __m256i mask3f = _mm256_srli_epi64(minusone256, 58);

    const __m256i umask = _mm256_load_si256(reinterpret_cast<const __m256i*>(avx2_detail::LR_MASK[square].upper));
    const __m256i p_up = _mm256_and_si256(pp, umask);
    const __m256i o_up = _mm256_andnot_si256(pp, umask);
    __m256i p_flip = _mm256_sub_epi64(p_up, _mm256_min_epu64(p_up, one));
    __m256i o_flip = _mm256_sub_epi64(o_up, _mm256_min_epu64(o_up, one));
    p_flip = _mm256_ternarylogic_epi64(p_up, p_flip, umask, 0x08);
    o_flip = _mm256_ternarylogic_epi64(o_up, o_flip, umask, 0x08);

    const __m256i lmask = _mm256_load_si256(reinterpret_cast<const __m256i*>(avx2_detail::LR_MASK[square].lower));
    const __m256i p_lo = _mm256_and_si256(pp, lmask);
    const __m256i o_lo = _mm256_andnot_si256(pp, lmask);
    const __m256i p_eraser = _mm256_srlv_epi64(minusone256, _mm256_and_si256(_mm256_lzcnt_epi64(p_lo), mask3f));
    const __m256i o_eraser = _mm256_srlv_epi64(minusone256, _mm256_and_si256(_mm256_lzcnt_epi64(o_lo), mask3f));
    p_flip = _mm256_ternarylogic_epi64(p_flip, p_eraser, lmask, 0xf2);
    o_flip = _mm256_ternarylogic_epi64(o_flip, o_eraser, lmask, 0xf2);

    const __m256i opop = _mm256_or_si256(_mm256_unpacklo_epi64(p_flip, o_flip), _mm256_unpackhi_epi64(p_flip, o_flip));
    __m128i op = _mm_ternarylogic_epi64(
        _mm256_castsi256_si128(opop),
        _mm256_extracti128_si256(opop, 1),
        p2,
        0x56);
    const __mmask8 op_pass = _mm_cmpeq_epi64_mask(op, p2);
    op = _mm_mask_unpackhi_epi64(op, op_pass, op, op);
    int32_t score = static_cast<int32_t>(std::popcount(static_cast<uint64_t>(_mm_cvtsi128_si64(op))));
    score += (~op_pass | ((op_pass >> 1U) & (score >> 5U))) & 1;
    return score * 2 - 64;
}

/// 256-bit AVX-512VL legal moves using `vpternlogq` for fused `or + and`.
inline uint64_t legal_moves_avx512(uint64_t own, uint64_t opponent) noexcept {
    const __m256i shift1 = _mm256_set_epi64x(9, 7, 8, 1);
    const __m256i shift2 = _mm256_set_epi64x(18, 14, 16, 2);
    const __m256i inner_mask = _mm256_set_epi64x(
        0x7E7E'7E7E'7E7E'7E7ELL,
        0x7E7E'7E7E'7E7E'7E7ELL,
        -1LL,
        0x7E7E'7E7E'7E7E'7E7ELL);

    const __m256i pp = _mm256_set1_epi64x(static_cast<int64_t>(own));
    const __m256i moo = _mm256_and_si256(_mm256_set1_epi64x(static_cast<int64_t>(opponent)), inner_mask);

    __m256i flip_l = _mm256_and_si256(moo, _mm256_sllv_epi64(pp, shift1));
    __m256i flip_r = _mm256_and_si256(moo, _mm256_srlv_epi64(pp, shift1));
    flip_l = _mm256_ternarylogic_epi64(flip_l, moo, _mm256_sllv_epi64(flip_l, shift1), 0xf8);
    flip_r = _mm256_ternarylogic_epi64(flip_r, moo, _mm256_srlv_epi64(flip_r, shift1), 0xf8);

    const __m256i pre_l = _mm256_and_si256(moo, _mm256_sllv_epi64(moo, shift1));
    const __m256i pre_r = _mm256_and_si256(moo, _mm256_srlv_epi64(moo, shift1));

    flip_l = _mm256_ternarylogic_epi64(flip_l, pre_l, _mm256_sllv_epi64(flip_l, shift2), 0xf8);
    flip_r = _mm256_ternarylogic_epi64(flip_r, pre_r, _mm256_srlv_epi64(flip_r, shift2), 0xf8);
    flip_l = _mm256_ternarylogic_epi64(flip_l, pre_l, _mm256_sllv_epi64(flip_l, shift2), 0xf8);
    flip_r = _mm256_ternarylogic_epi64(flip_r, pre_r, _mm256_srlv_epi64(flip_r, shift2), 0xf8);

    const __m256i mm = _mm256_or_si256(_mm256_sllv_epi64(flip_l, shift1), _mm256_srlv_epi64(flip_r, shift1));
    const __m128i m2 = _mm_or_si128(_mm256_castsi256_si128(mm), _mm256_extracti128_si256(mm, 1));
    const __m128i m1 = _mm_or_si128(m2, _mm_unpackhi_epi64(m2, m2));
    return static_cast<uint64_t>(_mm_cvtsi128_si64(m1)) & ~(own | opponent);
}

inline void legal_and_potential_avx512(
    uint64_t own,
    uint64_t opponent,
    uint64_t& legal_out,
    int32_t& pot_out
) noexcept {
    const __m256i shift1 = _mm256_set_epi64x(9, 7, 8, 1);
    const __m256i shift2 = _mm256_set_epi64x(18, 14, 16, 2);
    const __m256i inner_mask = _mm256_set_epi64x(
        0x007E'7E7E'7E7E'7E00LL,
        0x007E'7E7E'7E7E'7E00LL,
        0x00FF'FFFF'FFFF'FF00LL,
        0x7E7E'7E7E'7E7E'7E7ELL);

    const __m256i pp = _mm256_set1_epi64x(static_cast<int64_t>(own));
    const __m256i moo = _mm256_and_si256(_mm256_set1_epi64x(static_cast<int64_t>(opponent)), inner_mask);

    __m256i flip_l = _mm256_and_si256(moo, _mm256_sllv_epi64(pp, shift1));
    __m256i flip_r = _mm256_and_si256(moo, _mm256_srlv_epi64(pp, shift1));
    flip_l = _mm256_ternarylogic_epi64(flip_l, moo, _mm256_sllv_epi64(flip_l, shift1), 0xf8);
    flip_r = _mm256_ternarylogic_epi64(flip_r, moo, _mm256_srlv_epi64(flip_r, shift1), 0xf8);

    __m256i pre_l = _mm256_sllv_epi64(moo, shift1);
    __m256i pre_r = _mm256_srlv_epi64(moo, shift1);
    const __m256i potmob = _mm256_or_si256(pre_l, pre_r);
    pre_l = _mm256_and_si256(moo, pre_l);
    pre_r = _mm256_and_si256(moo, pre_r);

    flip_l = _mm256_ternarylogic_epi64(flip_l, pre_l, _mm256_sllv_epi64(flip_l, shift2), 0xf8);
    flip_r = _mm256_ternarylogic_epi64(flip_r, pre_r, _mm256_srlv_epi64(flip_r, shift2), 0xf8);
    flip_l = _mm256_ternarylogic_epi64(flip_l, pre_l, _mm256_sllv_epi64(flip_l, shift2), 0xf8);
    flip_r = _mm256_ternarylogic_epi64(flip_r, pre_r, _mm256_srlv_epi64(flip_r, shift2), 0xf8);

    __m256i mm = _mm256_or_si256(_mm256_sllv_epi64(flip_l, shift1), _mm256_srlv_epi64(flip_r, shift1));
    mm = _mm256_or_si256(_mm256_unpacklo_epi64(mm, potmob), _mm256_unpackhi_epi64(mm, potmob));
    const __m128i occupied = _mm_set1_epi64x(static_cast<int64_t>(own | opponent));
    const __m128i out = _mm_andnot_si128(
        occupied,
        _mm_or_si128(_mm256_castsi256_si128(mm), _mm256_extracti128_si256(mm, 1)));
    legal_out = static_cast<uint64_t>(_mm_cvtsi128_si64(out));
    pot_out = static_cast<int32_t>(std::popcount(static_cast<uint64_t>(_mm_extract_epi64(out, 1))));
}

/// Vectorized AVX-512VL+DQ full-lines and stability engine (`vpternlogq` fused propagation).
inline __m256i full_lines_avx512(uint64_t discs) noexcept {
    const __m256i mcpyswap = _mm256_set_epi8(
        0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7,
        7, 6, 5, 4, 3, 2, 1, 0, 7, 6, 5, 4, 3, 2, 1, 0);
    const __m128i mbswapll = _mm_set_epi8(8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7);
    const __m256i s0 = _mm256_set_epi64x(9, 7, 7, 9);
    const __m256i s1 = _mm256_set_epi64x(18, 14, 14, 18);
    const __m256i s2 = _mm256_set_epi64x(36, 28, 28, 36);
    const __m256i e0 = _mm256_set_epi64x(
        static_cast<int64_t>(0xff80'8080'8080'8080ULL),
        static_cast<int64_t>(0xff01'0101'0101'0101ULL),
        static_cast<int64_t>(0xff01'0101'0101'0101ULL),
        static_cast<int64_t>(0xff80'8080'8080'8080ULL));
    const __m256i e1 = _mm256_set_epi64x(
        static_cast<int64_t>(0xffff'c0c0'c0c0'c0c0ULL),
        static_cast<int64_t>(0xffff'0303'0303'0303ULL),
        static_cast<int64_t>(0xffff'0303'0303'0303ULL),
        static_cast<int64_t>(0xffff'c0c0'c0c0'c0c0ULL));
    const __m256i e2 = _mm256_set_epi64x(
        static_cast<int64_t>(0xffff'ffff'f0f0'f0f0ULL),
        static_cast<int64_t>(0xffff'ffff'0f0f'0f0fULL),
        static_cast<int64_t>(0xffff'ffff'0f0f'0f0fULL),
        static_cast<int64_t>(0xffff'ffff'f0f0'f0f0ULL));

    const __m256i v4_disc = _mm256_shuffle_epi8(
        _mm256_set1_epi64x(static_cast<int64_t>(discs)), mcpyswap);
    __m128i l8 = _mm256_castsi256_si128(v4_disc);
    const __m128i l1 = _mm_cmpeq_epi8(l8, _mm_set1_epi8(-1));
    l8 = _mm_and_si128(l8, _mm_srli_si128(l8, 1));
    l8 = _mm_and_si128(l8, _mm_shufflelo_epi16(l8, 0x39));
    l8 = _mm_and_si128(l8, _mm_shufflelo_epi16(l8, 0x4e));

    __m256i lr79 = _mm256_ternarylogic_epi64(v4_disc, e0, _mm256_srlv_epi64(v4_disc, s0), 0xe0);
    lr79 = _mm256_ternarylogic_epi64(lr79, e1, _mm256_srlv_epi64(lr79, s1), 0xe0);
    lr79 = _mm256_ternarylogic_epi64(lr79, e2, _mm256_srlv_epi64(lr79, s2), 0xe0);
    const __m128i l79 = _mm_and_si128(
        _mm_shuffle_epi8(_mm256_extracti128_si256(lr79, 1), mbswapll),
        _mm256_castsi256_si128(lr79));

    return _mm256_set_m128i(l79, _mm_unpacklo_epi64(l1, l8));
}

inline uint64_t stable_of_full_lines_avx512(uint64_t own, uint64_t opponent, __m256i full_lines) noexcept {
    const uint8_t* const edge_tbl = stable::edge_table();
    const __m128i p0 = _mm_cvtsi64_si128(static_cast<int64_t>(own));
    const __m128i o0 = _mm_cvtsi64_si128(static_cast<int64_t>(opponent));
    const __m128i po_rows = _mm_unpacklo_epi8(o0, p0);
    uint64_t found = static_cast<uint64_t>(edge_tbl[static_cast<uint16_t>(_mm_extract_epi16(po_rows, 0))])
                   | (static_cast<uint64_t>(edge_tbl[static_cast<uint16_t>(_mm_extract_epi16(po_rows, 7))]) << 56);

    const __m128i po_cols = _mm_unpacklo_epi64(o0, p0);
    found |= stable::unpack_a2a7(edge_tbl[_mm_movemask_epi8(_mm_slli_epi64(po_cols, 7))]);
    found |= stable::unpack_h2h7(edge_tbl[_mm_movemask_epi8(po_cols)]);

    const uint64_t p_central = own & 0x007E'7E7E'7E7E'7E00ULL;
    const __m128i full_hv = _mm256_castsi256_si128(full_lines);
    const __m128i full_79 = _mm256_extracti128_si256(full_lines, 1);
    const __m128i full_2 = _mm_and_si128(full_hv, full_79);
    const __m128i full_1 = _mm_and_si128(full_2, _mm_unpackhi_epi64(full_2, full_2));
    found |= p_central & static_cast<uint64_t>(_mm_cvtsi128_si64(full_1));
    if (found == 0) {
        return 0;
    }

    __m128i v2_stable = _mm_cvtsi64_si128(static_cast<int64_t>(found));
    const __m128i v2_p_central = _mm_cvtsi64_si128(static_cast<int64_t>(p_central));
    const __m256i shift1897 = _mm256_set_epi64x(7, 9, 8, 1);
    __m128i v2_old;
    do {
        v2_old = v2_stable;
        const __m256i v4_s = _mm256_broadcastq_epi64(v2_stable);
        const __m256i dirs = _mm256_ternarylogic_epi64(
            _mm256_srlv_epi64(v4_s, shift1897),
            _mm256_sllv_epi64(v4_s, shift1897),
            full_lines,
            0xfe);
        const __m128i d2 = _mm_and_si128(_mm256_castsi256_si128(dirs), _mm256_extracti128_si256(dirs, 1));
        const __m128i d1 = _mm_and_si128(d2, _mm_unpackhi_epi64(d2, d2));
        v2_stable = _mm_ternarylogic_epi64(v2_old, d1, v2_p_central, 0xf8);
    } while (!_mm_testc_si128(v2_old, v2_stable));

    return static_cast<uint64_t>(_mm_cvtsi128_si64(v2_stable));
}

inline uint64_t stable_of_avx512(uint64_t own, uint64_t opponent) noexcept {
    return stable_of_full_lines_avx512(own, opponent, full_lines_avx512(own | opponent));
}

} // namespace stello::endgame::bits

#endif
