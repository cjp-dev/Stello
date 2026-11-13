#pragma once

#include "endgame/bits.hpp"

#if (defined(__x86_64__) || defined(_M_X64)) && defined(__AVX2__) && !defined(__EMSCRIPTEN__)

#include <immintrin.h>

#include <array>
#include <cstdint>

namespace stello::endgame::bits {

namespace avx2_detail {

struct alignas(32) Ray4 {
    uint64_t lane[4];
};

constexpr std::array<Ray4, 64> make_upper_rays() noexcept {
    std::array<Ray4, 64> table{};
    for (size_t sq = 0; sq < 64; ++sq) {
        table[sq] = Ray4{{RAYS[sq][0], RAYS[sq][1], RAYS[sq][2], RAYS[sq][3]}};
    }
    return table;
}

constexpr std::array<Ray4, 64> make_lower_rays() noexcept {
    std::array<Ray4, 64> table{};
    for (size_t sq = 0; sq < 64; ++sq) {
        table[sq] = Ray4{{RAYS[sq][4], RAYS[sq][5], RAYS[sq][6], RAYS[sq][7]}};
    }
    return table;
}

inline constexpr std::array<Ray4, 64> UPPER_RAYS = make_upper_rays();
inline constexpr std::array<Ray4, 64> LOWER_RAYS = make_lower_rays();

} // namespace avx2_detail

/// 256-bit AVX2 equivalent of `stello::endgame::bits::flips` (all 8 rays evaluated in parallel, zero zmm).
inline uint64_t flips_avx2(uint64_t own, uint64_t opponent, uint32_t square) noexcept {
    const __m256i pp = _mm256_set1_epi64x(static_cast<int64_t>(own));
    const __m256i oo = _mm256_set1_epi64x(static_cast<int64_t>(opponent));
    const __m256i op = _mm256_or_si256(oo, pp);

    const __m256i shift1 = _mm256_set_epi64x(9, 7, 8, 1);
    const __m256i shift2 = _mm256_set_epi64x(18, 14, 16, 2);
    const __m256i shift4 = _mm256_set_epi64x(36, 28, 32, 4);

    // Lower rays (shifts -1, -8, -7, -9): bit 63 is never set in LOWER_RAYS, so cmpgt_epi64 compares unsigned indices safely.
    const __m256i rmask = _mm256_load_si256(reinterpret_cast<const __m256i*>(&avx2_detail::LOWER_RAYS[square]));
    const __m256i rp = _mm256_and_si256(pp, rmask);
    __m256i rs = _mm256_or_si256(rp, _mm256_srlv_epi64(rp, shift1));
    rs = _mm256_or_si256(rs, _mm256_srlv_epi64(rs, shift2));
    rs = _mm256_or_si256(rs, _mm256_srlv_epi64(rs, shift4));
    const __m256i re = _mm256_andnot_si256(op, rmask);
    __m256i f4 = _mm256_and_si256(_mm256_andnot_si256(rs, rmask), _mm256_cmpgt_epi64(rp, re));

    // Upper rays (shifts +1, +8, +7, +9): isolate lowest non-opponent bit via blsmsk and verify it is `own`.
    const __m256i lmask = _mm256_load_si256(reinterpret_cast<const __m256i*>(&avx2_detail::UPPER_RAYS[square]));
    __m256i lo = _mm256_andnot_si256(oo, lmask);
    lo = _mm256_and_si256(_mm256_xor_si256(_mm256_add_epi64(lo, _mm256_set1_epi64x(-1)), lo), lmask);
    const __m256i lf = _mm256_andnot_si256(pp, lo);
    f4 = _mm256_or_si256(f4, _mm256_andnot_si256(_mm256_cmpeq_epi64(lf, lo), lf));

    const __m128i f2 = _mm_or_si128(_mm256_castsi256_si128(f4), _mm256_extracti128_si256(f4, 1));
    const __m128i f1 = _mm_or_si128(f2, _mm_unpackhi_epi64(f2, f2));
    return static_cast<uint64_t>(_mm_cvtsi128_si64(f1));
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

} // namespace stello::endgame::bits

#endif
