#pragma once

#include "core/compiler.hpp"
#include "endgame/bits.hpp"
#include "endgame/stable.hpp"

#if (defined(__x86_64__) || defined(_M_X64)) && defined(__AVX2__) && !defined(__EMSCRIPTEN__)

#include <immintrin.h>

#include <array>
#include <cstdint>

namespace stello::endgame::bits {

namespace avx2_detail {

struct alignas(64) LrMask {
    uint64_t lower[4];
    uint64_t upper[4];
};

static_assert(sizeof(LrMask) == 64);

constexpr std::array<LrMask, 64> make_lr_mask() noexcept {
    std::array<LrMask, 64> table{};
    for (size_t sq = 0; sq < 64; ++sq) {
        table[sq] = LrMask{
            {RAYS[sq][4], RAYS[sq][5], RAYS[sq][6], RAYS[sq][7]},
            {RAYS[sq][0], RAYS[sq][1], RAYS[sq][2], RAYS[sq][3]},
        };
    }
    return table;
}

inline constexpr std::array<LrMask, 64> LR_MASK = make_lr_mask();

} // namespace avx2_detail

inline __m128i STELLO_VECTORCALL flips_avx2_v128_acepck(__m128i op, uint32_t square) noexcept {
    const __m256i pp = _mm256_broadcastq_epi64(op);
    const __m256i oo = _mm256_broadcastq_epi64(_mm_unpackhi_epi64(op, op));

    const __m256i shift1 = _mm256_set_epi64x(9, 7, 8, 1);
    const __m256i shift2 = _mm256_set_epi64x(18, 14, 16, 2);
    const __m256i shift4 = _mm256_set_epi64x(36, 28, 32, 4);

    // Lower rays (shifts -1, -8, -7, -9): bit 63 is never set in lower rays, so cmpgt_epi64 compares unsigned indices safely.
    const __m256i rmask = _mm256_load_si256(reinterpret_cast<const __m256i*>(avx2_detail::LR_MASK[square].lower));
    const __m256i rp = _mm256_and_si256(pp, rmask);
    __m256i rs = _mm256_or_si256(rp, _mm256_srlv_epi64(rp, shift1));
    rs = _mm256_or_si256(rs, _mm256_srlv_epi64(rs, shift2));
    rs = _mm256_or_si256(rs, _mm256_srlv_epi64(rs, shift4));
    const __m256i re = _mm256_xor_si256(_mm256_andnot_si256(oo, rmask), rp);
    __m256i f4 = _mm256_and_si256(_mm256_andnot_si256(rs, rmask), _mm256_cmpgt_epi64(rp, re));

    // Upper rays (shifts +1, +8, +7, +9): isolate lowest non-opponent bit via blsmsk and verify it is `own`.
    const __m256i lmask = _mm256_load_si256(reinterpret_cast<const __m256i*>(avx2_detail::LR_MASK[square].upper));
    __m256i lo = _mm256_andnot_si256(oo, lmask);
    lo = _mm256_and_si256(_mm256_xor_si256(_mm256_add_epi64(lo, _mm256_set1_epi64x(-1)), lo), lmask);
    const __m256i lf = _mm256_andnot_si256(pp, lo);
    f4 = _mm256_or_si256(f4, _mm256_andnot_si256(_mm256_cmpeq_epi64(lf, lo), lf));

    return _mm_or_si128(_mm256_castsi256_si128(f4), _mm256_extracti128_si256(f4, 1));
}

inline __m128i STELLO_VECTORCALL flips_avx2_v128_smartclr(__m128i op, uint32_t square) noexcept {
    const __m256i pp = _mm256_broadcastq_epi64(op);
    const __m256i oo = _mm256_broadcastq_epi64(_mm_unpackhi_epi64(op, op));

    const __m256i shift1 = _mm256_set_epi64x(9, 7, 8, 1);
    const __m256i shift2 = _mm256_set_epi64x(18, 14, 16, 2);
    const __m256i shift4 = _mm256_set_epi64x(36, 28, 32, 4);

    const __m256i rmask = _mm256_load_si256(reinterpret_cast<const __m256i*>(avx2_detail::LR_MASK[square].lower));
    __m256i eraser = _mm256_andnot_si256(oo, rmask);
    __m256i outflank = _mm256_sllv_epi64(_mm256_and_si256(pp, rmask), shift1);
    eraser = _mm256_or_si256(eraser, _mm256_srlv_epi64(eraser, shift1));
    eraser = _mm256_or_si256(
        _mm256_srlv_epi64(eraser, shift4),
        _mm256_or_si256(eraser, _mm256_srlv_epi64(eraser, shift2)));
    outflank = _mm256_andnot_si256(eraser, outflank);
    __m256i flip = _mm256_and_si256(rmask, _mm256_sub_epi64(_mm256_setzero_si256(), outflank));

    const __m256i lmask = _mm256_load_si256(reinterpret_cast<const __m256i*>(avx2_detail::LR_MASK[square].upper));
    outflank = _mm256_andnot_si256(oo, lmask);
    outflank = _mm256_and_si256(outflank, _mm256_sub_epi64(_mm256_setzero_si256(), outflank));
    outflank = _mm256_and_si256(outflank, pp);
    eraser = _mm256_sub_epi64(_mm256_cmpeq_epi64(outflank, _mm256_setzero_si256()), outflank);
    flip = _mm256_or_si256(flip, _mm256_andnot_si256(eraser, lmask));

    return _mm_or_si128(_mm256_castsi256_si128(flip), _mm256_extracti128_si256(flip, 1));
}

inline uint64_t reduce_flip_v128(__m128i f2) noexcept {
    return static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_or_si128(f2, _mm_unpackhi_epi64(f2, f2))));
}

/// 256-bit AVX2 equivalent of `stello::endgame::bits::flips` (all 8 rays evaluated in parallel, zero zmm).
inline uint64_t flips_avx2(uint64_t own, uint64_t opponent, uint32_t square) noexcept {
    const __m128i op = _mm_set_epi64x(static_cast<int64_t>(opponent), static_cast<int64_t>(own));
    return reduce_flip_v128(flips_avx2_v128_smartclr(op, square));
}

inline uint64_t flips_avx2_smartclr(uint64_t own, uint64_t opponent, uint32_t square) noexcept {
    const __m128i op = _mm_set_epi64x(static_cast<int64_t>(opponent), static_cast<int64_t>(own));
    return reduce_flip_v128(flips_avx2_v128_smartclr(op, square));
}

/// 256-bit AVX2 equivalent of `stello::endgame::bits::legal_moves` (4 axes x 2 directions in parallel, zero zmm).
inline uint64_t legal_moves_avx2(uint64_t own, uint64_t opponent) noexcept {
    const __m256i shift1 = _mm256_set_epi64x(9, 7, 8, 1);
    const __m256i shift2 = _mm256_set_epi64x(18, 14, 16, 2);
    const __m256i inner_mask = _mm256_set_epi64x(
        0x7E7E'7E7E'7E7E'7E7ELL,
        0x7E7E'7E7E'7E7E'7E7ELL,
        -1LL,
        0x7E7E'7E7E'7E7E'7E7ELL
    );

    const __m256i pp = _mm256_set1_epi64x(static_cast<int64_t>(own));
    const __m256i moo = _mm256_and_si256(_mm256_set1_epi64x(static_cast<int64_t>(opponent)), inner_mask);

    __m256i flip_l = _mm256_and_si256(moo, _mm256_sllv_epi64(pp, shift1));
    __m256i flip_r = _mm256_and_si256(moo, _mm256_srlv_epi64(pp, shift1));
    flip_l = _mm256_or_si256(flip_l, _mm256_and_si256(moo, _mm256_sllv_epi64(flip_l, shift1)));
    flip_r = _mm256_or_si256(flip_r, _mm256_and_si256(moo, _mm256_srlv_epi64(flip_r, shift1)));

    const __m256i pre_l = _mm256_and_si256(moo, _mm256_sllv_epi64(moo, shift1));
    const __m256i pre_r = _mm256_srlv_epi64(pre_l, shift1);

    flip_l = _mm256_or_si256(flip_l, _mm256_and_si256(pre_l, _mm256_sllv_epi64(flip_l, shift2)));
    flip_r = _mm256_or_si256(flip_r, _mm256_and_si256(pre_r, _mm256_srlv_epi64(flip_r, shift2)));
    flip_l = _mm256_or_si256(flip_l, _mm256_and_si256(pre_l, _mm256_sllv_epi64(flip_l, shift2)));
    flip_r = _mm256_or_si256(flip_r, _mm256_and_si256(pre_r, _mm256_srlv_epi64(flip_r, shift2)));

    const __m256i mm = _mm256_or_si256(_mm256_sllv_epi64(flip_l, shift1), _mm256_srlv_epi64(flip_r, shift1));
    const __m128i m2 = _mm_or_si128(_mm256_castsi256_si128(mm), _mm256_extracti128_si256(mm, 1));
    const __m128i m1 = _mm_or_si128(m2, _mm_unpackhi_epi64(m2, m2));
    return static_cast<uint64_t>(_mm_cvtsi128_si64(m1)) & ~(own | opponent);
}

/// Computes `(legal_moves_avx2(own, opponent), legal_moves_avx2(opponent, own))` interleaved to saturate shift ports.
inline void dual_legal_moves_avx2(uint64_t own, uint64_t opponent, uint64_t& own_out, uint64_t& opp_out) noexcept {
    const __m256i shift1 = _mm256_set_epi64x(9, 7, 8, 1);
    const __m256i shift2 = _mm256_set_epi64x(18, 14, 16, 2);
    const __m256i inner_mask = _mm256_set_epi64x(
        0x7E7E'7E7E'7E7E'7E7ELL,
        0x7E7E'7E7E'7E7E'7E7ELL,
        -1LL,
        0x7E7E'7E7E'7E7E'7E7ELL
    );

    const __m256i pp1 = _mm256_set1_epi64x(static_cast<int64_t>(own));
    const __m256i pp2 = _mm256_set1_epi64x(static_cast<int64_t>(opponent));
    const __m256i moo1 = _mm256_and_si256(pp2, inner_mask);
    const __m256i moo2 = _mm256_and_si256(pp1, inner_mask);

    __m256i fl1 = _mm256_and_si256(moo1, _mm256_sllv_epi64(pp1, shift1));
    __m256i fr1 = _mm256_and_si256(moo1, _mm256_srlv_epi64(pp1, shift1));
    __m256i fl2 = _mm256_and_si256(moo2, _mm256_sllv_epi64(pp2, shift1));
    __m256i fr2 = _mm256_and_si256(moo2, _mm256_srlv_epi64(pp2, shift1));

    fl1 = _mm256_or_si256(fl1, _mm256_and_si256(moo1, _mm256_sllv_epi64(fl1, shift1)));
    fr1 = _mm256_or_si256(fr1, _mm256_and_si256(moo1, _mm256_srlv_epi64(fr1, shift1)));
    fl2 = _mm256_or_si256(fl2, _mm256_and_si256(moo2, _mm256_sllv_epi64(fl2, shift1)));
    fr2 = _mm256_or_si256(fr2, _mm256_and_si256(moo2, _mm256_srlv_epi64(fr2, shift1)));

    const __m256i pre_l1 = _mm256_and_si256(moo1, _mm256_sllv_epi64(moo1, shift1));
    const __m256i pre_r1 = _mm256_srlv_epi64(pre_l1, shift1);
    const __m256i pre_l2 = _mm256_and_si256(moo2, _mm256_sllv_epi64(moo2, shift1));
    const __m256i pre_r2 = _mm256_srlv_epi64(pre_l2, shift1);

    fl1 = _mm256_or_si256(fl1, _mm256_and_si256(pre_l1, _mm256_sllv_epi64(fl1, shift2)));
    fr1 = _mm256_or_si256(fr1, _mm256_and_si256(pre_r1, _mm256_srlv_epi64(fr1, shift2)));
    fl2 = _mm256_or_si256(fl2, _mm256_and_si256(pre_l2, _mm256_sllv_epi64(fl2, shift2)));
    fr2 = _mm256_or_si256(fr2, _mm256_and_si256(pre_r2, _mm256_srlv_epi64(fr2, shift2)));

    fl1 = _mm256_or_si256(fl1, _mm256_and_si256(pre_l1, _mm256_sllv_epi64(fl1, shift2)));
    fr1 = _mm256_or_si256(fr1, _mm256_and_si256(pre_r1, _mm256_srlv_epi64(fr1, shift2)));
    fl2 = _mm256_or_si256(fl2, _mm256_and_si256(pre_l2, _mm256_sllv_epi64(fl2, shift2)));
    fr2 = _mm256_or_si256(fr2, _mm256_and_si256(pre_r2, _mm256_srlv_epi64(fr2, shift2)));

    const __m256i mm1 = _mm256_or_si256(_mm256_sllv_epi64(fl1, shift1), _mm256_srlv_epi64(fr1, shift1));
    const __m256i mm2 = _mm256_or_si256(_mm256_sllv_epi64(fl2, shift1), _mm256_srlv_epi64(fr2, shift1));
    const __m128i a2 = _mm_or_si128(_mm256_castsi256_si128(mm1), _mm256_extracti128_si256(mm1, 1));
    const __m128i b2 = _mm_or_si128(_mm256_castsi256_si128(mm2), _mm256_extracti128_si256(mm2, 1));
    const __m128i a1 = _mm_or_si128(a2, _mm_unpackhi_epi64(a2, a2));
    const __m128i b1 = _mm_or_si128(b2, _mm_unpackhi_epi64(b2, b2));
    const uint64_t empty = ~(own | opponent);
    own_out = static_cast<uint64_t>(_mm_cvtsi128_si64(a1)) & empty;
    opp_out = static_cast<uint64_t>(_mm_cvtsi128_si64(b1)) & empty;
}

/// 3-instruction AVX2 transpose across the A1-H8 diagonal.
inline uint64_t transpose_avx2(uint64_t b) noexcept {
    const __m256i v = _mm256_sllv_epi64(
        _mm256_broadcastq_epi64(_mm_cvtsi64_si128(static_cast<int64_t>(b))),
        _mm256_set_epi64x(0, 1, 2, 3));
    return (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(v))) << 32)
         | static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_slli_epi64(v, 4))));
}

/// Vectorized full-line masks `[full_h, full_v, full_d9, full_d7]` for `discs`.
inline __m256i full_lines_avx2(uint64_t discs) noexcept {
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

    __m256i lr79 = _mm256_and_si256(v4_disc, _mm256_or_si256(e0, _mm256_srlv_epi64(v4_disc, s0)));
    lr79 = _mm256_and_si256(lr79, _mm256_or_si256(e1, _mm256_srlv_epi64(lr79, s1)));
    lr79 = _mm256_and_si256(lr79, _mm256_or_si256(e2, _mm256_srlv_epi64(lr79, s2)));
    const __m128i l79 = _mm_and_si128(
        _mm_shuffle_epi8(_mm256_extracti128_si256(lr79, 1), mbswapll),
        _mm256_castsi256_si128(lr79));

    return _mm256_set_m128i(l79, _mm_unpacklo_epi64(l1, l8));
}

/// Computes stable discs of `own` given precomputed `full_lines = [full_h, full_v, full_d9, full_d7]`.
inline uint64_t stable_of_full_lines_avx2(uint64_t own, uint64_t opponent, __m256i full_lines) noexcept {
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
        const __m256i dirs = _mm256_or_si256(
            _mm256_or_si256(_mm256_srlv_epi64(v4_s, shift1897), _mm256_sllv_epi64(v4_s, shift1897)),
            full_lines);
        const __m128i d2 = _mm_and_si128(_mm256_castsi256_si128(dirs), _mm256_extracti128_si256(dirs, 1));
        const __m128i d1 = _mm_and_si128(d2, _mm_unpackhi_epi64(d2, d2));
        v2_stable = _mm_or_si128(v2_old, _mm_and_si128(d1, v2_p_central));
    } while (!_mm_testc_si128(v2_old, v2_stable));

    return static_cast<uint64_t>(_mm_cvtsi128_si64(v2_stable));
}

inline uint64_t stable_of_avx2(uint64_t own, uint64_t opponent) noexcept {
    return stable_of_full_lines_avx2(own, opponent, full_lines_avx2(own | opponent));
}

/// Fused AVX2 computation of `legal_moves(own, opponent)` and `potential_mobility(opponent, ~(own | opponent))`.
inline void legal_and_potential_avx2(
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
    const __m256i oo = _mm256_set1_epi64x(static_cast<int64_t>(opponent));
    const __m256i moo = _mm256_and_si256(oo, inner_mask);

    __m256i flip_l = _mm256_and_si256(moo, _mm256_sllv_epi64(pp, shift1));
    __m256i flip_r = _mm256_and_si256(moo, _mm256_srlv_epi64(pp, shift1));
    flip_l = _mm256_or_si256(flip_l, _mm256_and_si256(moo, _mm256_sllv_epi64(flip_l, shift1)));
    flip_r = _mm256_or_si256(flip_r, _mm256_and_si256(moo, _mm256_srlv_epi64(flip_r, shift1)));

    __m256i pre_l = _mm256_sllv_epi64(moo, shift1);
    __m256i pre_r = _mm256_srlv_epi64(moo, shift1);
    const __m256i potmob = _mm256_or_si256(pre_l, pre_r);
    pre_l = _mm256_and_si256(moo, pre_l);
    pre_r = _mm256_and_si256(moo, pre_r);

    flip_l = _mm256_or_si256(flip_l, _mm256_and_si256(pre_l, _mm256_sllv_epi64(flip_l, shift2)));
    flip_r = _mm256_or_si256(flip_r, _mm256_and_si256(pre_r, _mm256_srlv_epi64(flip_r, shift2)));
    flip_l = _mm256_or_si256(flip_l, _mm256_and_si256(pre_l, _mm256_sllv_epi64(flip_l, shift2)));
    flip_r = _mm256_or_si256(flip_r, _mm256_and_si256(pre_r, _mm256_srlv_epi64(flip_r, shift2)));

    __m256i mm = _mm256_or_si256(_mm256_sllv_epi64(flip_l, shift1), _mm256_srlv_epi64(flip_r, shift1));
    mm = _mm256_or_si256(_mm256_unpacklo_epi64(mm, potmob), _mm256_unpackhi_epi64(mm, potmob));
    const __m128i occupied = _mm_set1_epi64x(static_cast<int64_t>(own | opponent));
    const __m128i out = _mm_andnot_si128(
        occupied,
        _mm_or_si128(_mm256_castsi256_si128(mm), _mm256_extracti128_si256(mm, 1)));
    legal_out = static_cast<uint64_t>(_mm_cvtsi128_si64(out));
    pot_out = static_cast<int32_t>(std::popcount(static_cast<uint64_t>(_mm_extract_epi64(out, 1))));
}

namespace bmi2_last_detail {

struct DiagSpec {
    uint64_t mask1{0};
    uint64_t mask2{0};
    uint16_t ofs1{0};
    uint16_t ofs2{0};
};

constexpr std::array<uint64_t, 4> diag_rays(uint32_t square) noexcept {
    constexpr std::array<std::pair<int32_t, int32_t>, 4> kDirs = {{{-1, -1}, {1, 1}, {-1, 1}, {1, -1}}};
    const int32_t row = static_cast<int32_t>(square >> 3U);
    const int32_t col = static_cast<int32_t>(square & 7U);
    std::array<uint64_t, 4> masks{};
    for (size_t d = 0; d < 4U; ++d) {
        int32_t r = row + kDirs[d].first;
        int32_t c = col + kDirs[d].second;
        uint64_t ray = 0;
        uint32_t len = 0;
        while (r >= 0 && r < 8 && c >= 0 && c < 8) {
            ray |= uint64_t{1} << static_cast<uint32_t>(r * 8 + c);
            ++len;
            r += kDirs[d].first;
            c += kDirs[d].second;
        }
        masks[d] = (len >= 2U) ? ray : 0ULL;
    }
    return masks;
}

struct RayBitChain {
    uint8_t bit_indices[7]{};
    uint8_t len{0};
};

struct Bmi2LastTable {
    std::array<DiagSpec, 64> diag{};
    std::array<uint16_t, 5120> cf{};
    uint32_t used{2048};
};

constexpr Bmi2LastTable make_bmi2_last_table() noexcept {
    constexpr std::array<std::pair<int32_t, int32_t>, 4> kDirs = {{{-1, -1}, {1, 1}, {-1, 1}, {1, -1}}};
    Bmi2LastTable tbl{};
    for (uint32_t place = 0; place < 8U; ++place) {
        for (uint32_t pat = 0; pat < 256U; ++pat) {
            tbl.cf[place * 256U + pat] = last_flip_detail::cf_entry_1d(8U, place, pat);
        }
    }

    std::array<uint16_t, 128> starts{};
    std::array<uint16_t, 128> sizes{};
    uint32_t subtable_count = 0;

    auto append_subtable = [&](uint32_t sq, const std::array<uint64_t, 4>& rays, uint64_t mask) noexcept -> uint16_t {
        if (mask == 0ULL) {
            return 0U;
        }
        std::array<int8_t, 64> bit_of_sq{};
        bit_of_sq.fill(-1);
        uint32_t nbits = 0;
        for (uint32_t b = 0; b < 64U; ++b) {
            if (((mask >> b) & 1ULL) != 0ULL) {
                bit_of_sq[b] = static_cast<int8_t>(nbits++);
            }
        }

        const int32_t row = static_cast<int32_t>(sq >> 3U);
        const int32_t col = static_cast<int32_t>(sq & 7U);
        std::array<RayBitChain, 4> chains{};
        for (size_t d = 0; d < 4U; ++d) {
            if ((rays[d] & mask) == 0ULL) {
                continue;
            }
            int32_t r = row + kDirs[d].first;
            int32_t c = col + kDirs[d].second;
            while (r >= 0 && r < 8 && c >= 0 && c < 8) {
                chains[d].bit_indices[chains[d].len++] =
                    static_cast<uint8_t>(bit_of_sq[static_cast<uint32_t>(r * 8 + c)]);
                r += kDirs[d].first;
                c += kDirs[d].second;
            }
        }

        const uint32_t size = 1U << nbits;
        std::array<uint16_t, 128> tmp{};
        for (uint32_t pat = 0; pat < size; ++pat) {
            uint32_t p_flips = 0;
            uint32_t o_flips = 0;
            for (size_t d = 0; d < 4U; ++d) {
                const uint32_t len = chains[d].len;
                if (len < 2U) {
                    continue;
                }
                const uint32_t first = (pat >> chains[d].bit_indices[0]) & 1U;
                for (uint32_t k = 1U; k < len; ++k) {
                    if (((pat >> chains[d].bit_indices[k]) & 1U) != first) {
                        if (first == 0U) {
                            p_flips += k;
                        } else {
                            o_flips += k;
                        }
                        break;
                    }
                }
            }
            tmp[pat] = static_cast<uint16_t>(((o_flips * 2U) << 8U) | (p_flips * 2U));
        }

        for (uint32_t s = 0; s < subtable_count; ++s) {
            if (sizes[s] != size) {
                continue;
            }
            const uint32_t ofs = starts[s];
            bool match = true;
            for (uint32_t pat = 0; pat < size; ++pat) {
                if (tbl.cf[ofs + pat] != tmp[pat]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return static_cast<uint16_t>(ofs);
            }
        }
        const uint16_t ofs = static_cast<uint16_t>(tbl.used);
        for (uint32_t pat = 0; pat < size; ++pat) {
            tbl.cf[tbl.used + pat] = tmp[pat];
        }
        starts[subtable_count] = ofs;
        sizes[subtable_count] = static_cast<uint16_t>(size);
        ++subtable_count;
        tbl.used += size;
        return ofs;
    };

    for (uint32_t sq = 0; sq < 64U; ++sq) {
        const auto rays = diag_rays(sq);
        const uint64_t d1 = rays[0] | rays[1];
        const uint64_t d2 = rays[2] | rays[3];
        const bool inner_box = ((0x0000'3C3C'3C3C'0000ULL & (uint64_t{1} << sq)) != 0ULL);
        if (!inner_box) {
            tbl.diag[sq].mask1 = d1 | d2;
            tbl.diag[sq].mask2 = 0ULL;
            tbl.diag[sq].ofs1 = append_subtable(sq, rays, tbl.diag[sq].mask1);
            tbl.diag[sq].ofs2 = 0U;
        } else {
            tbl.diag[sq].mask1 = d1;
            tbl.diag[sq].mask2 = d2;
            tbl.diag[sq].ofs1 = append_subtable(sq, rays, d1);
            tbl.diag[sq].ofs2 = append_subtable(sq, rays, d2);
        }
    }
    return tbl;
}

inline constexpr Bmi2LastTable BMI2_LAST_TABLE = make_bmi2_last_table();

} // namespace bmi2_last_detail

/// 3-lookup BMI2 simultaneous (2*O_flips << 8) | (2*P_flips) for the 48 outer squares (4th lookup on inner 4x4).
inline uint16_t simul_last_flip_bmi2(uint64_t own, uint32_t square) noexcept {
    const uint32_t col = square & 7U;
    const uint32_t row8 = square & 56U;
    const auto& tbl = bmi2_last_detail::BMI2_LAST_TABLE;
    uint16_t op_flip = tbl.cf[col * 256U + static_cast<uint32_t>((own >> row8) & 0xFFULL)];
    op_flip = static_cast<uint16_t>(
        op_flip + tbl.cf[(row8 << 5U) + static_cast<uint32_t>(_pext_u64(own, 0x0101'0101'0101'0101ULL << col))]);
    const auto& ds = tbl.diag[static_cast<size_t>(square)];
    op_flip = static_cast<uint16_t>(
        op_flip + tbl.cf[ds.ofs1 + static_cast<uint32_t>(_pext_u64(own, ds.mask1))]);
    if ((0x0000'3C3C'3C3C'0000ULL & (uint64_t{1} << square)) != 0ULL) {
        op_flip = static_cast<uint16_t>(
            op_flip + tbl.cf[ds.ofs2 + static_cast<uint32_t>(_pext_u64(own, ds.mask2))]);
    }
    return op_flip;
}

/// Hardware CRC32C 2-instruction board hash (for microbench comparison only in Phase 2).
inline uint64_t crc32c_hash_board(uint64_t own, uint64_t opponent) noexcept {
    const uint64_t h0 = _mm_crc32_u64(0x9E37'79B9'7F4A'7C15ULL, own);
    const uint64_t h1 = _mm_crc32_u64(0xC2B2'AE3D'27D4'EB4FULL, opponent);
    return h0 ^ (h1 << 32);
}

} // namespace stello::endgame::bits

#endif
