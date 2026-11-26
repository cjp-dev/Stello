#pragma once

#include "search/kernel.hpp"

#include <bit>
#include <cstdint>
#include <utility>

#if (defined(__x86_64__) || defined(_M_X64)) && !defined(__EMSCRIPTEN__) && defined(__AVX2__)
#include "endgame/bits_avx2.hpp"
#endif
#if STELLO_TU_HAS_AVX512_V4
#include "endgame/bits_avx512.hpp"
#endif

namespace stello::search {

namespace detail::last {

STELLO_FORCEINLINE int32_t last_flip_count(uint64_t own, uint32_t square) noexcept {
    return static_cast<int32_t>((endgame::bits::simul_last_flip_portable(own, square) & 0xFFU) >> 1U);
}

struct LastStats {
    uint64_t nodes{0};
    uint64_t evaluations{0};
};

STELLO_FORCEINLINE AbortVoid flush_last_stats(Searcher& s, const LastStats& st) {
    s.nodes += st.nodes;
    s.evaluations += st.evaluations;
    if (s.nodes >= s.next_abort_check) [[unlikely]] {
        s.next_abort_check = s.nodes + 1024;
        return s.check_abort();
    }
    STELLO_RETURN_OK_VOID();
}

template <cpu::CpuTier Tier>
STELLO_FORCEINLINE uint64_t probe_flips_fast(uint64_t own, uint64_t opponent, uint32_t square) noexcept {
#if STELLO_HAS_X86_64_V4 && STELLO_TU_HAS_AVX512_V4
    if constexpr (Tier == cpu::CpuTier::V4) {
        const __m128i op = _mm_set_epi64x(static_cast<int64_t>(opponent), static_cast<int64_t>(own));
        const __m128i fv = endgame::bits::flips_avx512cd_v128(op, square);
        if (_mm_testz_si128(fv, fv)) {
            return 0ULL;
        }
        return endgame::bits::reduce_flip_v128(fv);
    }
#endif
#if STELLO_HAS_X86_64_V3 && defined(__AVX2__)
    if constexpr (Tier != cpu::CpuTier::Portable) {
        const __m128i op = _mm_set_epi64x(static_cast<int64_t>(opponent), static_cast<int64_t>(own));
        const __m128i fv = endgame::bits::flips_avx2_v128_smartclr(op, square);
        if (_mm_testz_si128(fv, fv)) {
            return 0ULL;
        }
        return endgame::bits::reduce_flip_v128(fv);
    }
#endif
    return Kernel<Tier>::flips(own, opponent, square);
}

template <cpu::CpuTier Tier>
STELLO_FORCEINLINE int32_t last1_fast(LastStats& st, uint64_t own, int32_t alpha, uint32_t square) noexcept {
    const uint16_t op_flip = Kernel<Tier>::simul_last_flip(own, square);
    const int32_t p2 = static_cast<int32_t>(op_flip & 0xFFU);
    int32_t score = 2 * static_cast<int32_t>(std::popcount(own)) - 62 + p2;
    st.evaluations += 1;
    if (p2 != 0) {
        st.nodes += 1;
        return score;
    }

    st.nodes += 2;
    const int32_t pass_score = score - 2;
    if (score <= 0) {
        score = pass_score;
    }
    if (score > alpha) {
        const int32_t o2 = static_cast<int32_t>(op_flip >> 8U);
        if (o2 != 0) {
            score = pass_score - o2;
        }
    }
    return score;
}

template <cpu::CpuTier Tier>
inline int32_t last2_fast(
    LastStats& st,
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    uint32_t p0,
    uint32_t p1
) noexcept {
    using endgame::bits::FLIP_NEIGHBOURS;
    using endgame::bits::score_with_empties;

    st.nodes += 1;
    int32_t v = 0;
    const uint64_t f0 = ((FLIP_NEIGHBOURS[p0] & opponent) != 0)
        ? probe_flips_fast<Tier>(own, opponent, p0)
        : 0ULL;

    if (f0 != 0) {
        v = last1_fast<Tier>(st, opponent ^ f0, alpha, p1);
        if (v > alpha && (FLIP_NEIGHBOURS[p1] & opponent) != 0) {
            const uint64_t f1 = probe_flips_fast<Tier>(own, opponent, p1);
            if (f1 != 0) {
                const int32_t g = last1_fast<Tier>(st, opponent ^ f1, alpha, p0);
                if (g < v) {
                    v = g;
                }
            }
        }
        return v;
    }

    const uint64_t f1 = ((FLIP_NEIGHBOURS[p1] & opponent) != 0)
        ? probe_flips_fast<Tier>(own, opponent, p1)
        : 0ULL;
    if (f1 != 0) {
        return last1_fast<Tier>(st, opponent ^ f1, alpha, p0);
    }

    st.nodes += 1;
    alpha = -alpha - 1;
    const uint64_t g0 = probe_flips_fast<Tier>(opponent, own, p0);
    if (g0 != 0) {
        v = last1_fast<Tier>(st, own ^ g0, alpha, p1);
        if (v > alpha) {
            const uint64_t g1 = probe_flips_fast<Tier>(opponent, own, p1);
            if (g1 != 0) {
                const int32_t g = last1_fast<Tier>(st, own ^ g1, alpha, p0);
                if (g < v) {
                    v = g;
                }
            }
        }
    } else {
        const uint64_t g1 = probe_flips_fast<Tier>(opponent, own, p1);
        if (g1 != 0) {
            v = last1_fast<Tier>(st, own ^ g1, alpha, p0);
        } else {
            st.evaluations += 1;
            v = score_with_empties(own, 2);
        }
    }
    return -v;
}

template <cpu::CpuTier Tier>
inline int32_t last3_fast(
    LastStats& st,
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    uint32_t p0,
    uint32_t p1,
    uint32_t p2,
    uint64_t empties4
) noexcept {
    using endgame::bits::FLIP_NEIGHBOURS;
    using endgame::bits::is_lone_empty;
    using endgame::bits::score_with_odd_empties;

    if (is_lone_empty(p2, empties4)) {
        std::swap(p2, p0);
    } else if (is_lone_empty(p1, empties4)) {
        std::swap(p1, p0);
    }

    uint64_t p = own;
    uint64_t o = opponent;
    int32_t v = -END_INFINITY;
    int32_t pol = 1;

    for (;;) {
        st.nodes += 1;

        if ((FLIP_NEIGHBOURS[p0] & o) != 0) {
            const uint64_t f = probe_flips_fast<Tier>(p, o, p0);
            if (f != 0) {
                v = last2_fast<Tier>(st, o ^ f, p ^ f ^ (uint64_t{1} << p0), alpha, p1, p2);
                if (alpha < v) {
                    return v * pol;
                }
            }
        }

        if ((FLIP_NEIGHBOURS[p1] & o) != 0) {
            const uint64_t f = probe_flips_fast<Tier>(p, o, p1);
            if (f != 0) {
                const int32_t g = last2_fast<Tier>(st, o ^ f, p ^ f ^ (uint64_t{1} << p1), alpha, p2, p0);
                if (alpha < g) {
                    return g * pol;
                }
                if (g > v) {
                    v = g;
                }
            }
        }

        if ((FLIP_NEIGHBOURS[p2] & o) != 0) {
            const uint64_t f = probe_flips_fast<Tier>(p, o, p2);
            if (f != 0) {
                const int32_t g = last2_fast<Tier>(st, o ^ f, p ^ f ^ (uint64_t{1} << p2), alpha, p1, p0);
                const int32_t best = (g > v) ? g : v;
                return best * pol;
            }
        }

        if (v > -END_INFINITY) {
            return v * pol;
        }

        std::swap(p, o);
        alpha = -alpha - 1;
        pol = -pol;
        if (pol >= 0) {
            break;
        }
    }

    st.evaluations += 1;
    return score_with_odd_empties(own, 3);
}

template <cpu::CpuTier Tier>
inline int32_t last4_fast(
    LastStats& st,
    uint64_t own,
    uint64_t opponent,
    int32_t alpha
) noexcept {
    using endgame::bits::FLIP_NEIGHBOURS;
    using endgame::bits::lone_empties;
    using endgame::bits::score_with_empties;

    const uint64_t empties = ~(own | opponent);
    uint64_t lone = lone_empties(own, opponent);
    if (lone == 0) {
        lone = empties;
    }

    const uint64_t rest = empties & ~lone;
    const uint32_t p0 = static_cast<uint32_t>(std::countr_zero(lone));
    lone &= (lone - 1ULL);
    if (lone == 0) {
        lone = rest;
    }

    const uint32_t p1 = static_cast<uint32_t>(std::countr_zero(lone));
    lone &= (lone - 1ULL);
    if (lone == 0) {
        lone = rest;
    }

    const uint32_t p2 = static_cast<uint32_t>(std::countr_zero(lone));
    lone &= (lone - 1ULL);
    const uint32_t p3 = static_cast<uint32_t>(std::countr_zero((lone != 0) ? lone : rest));

    uint64_t p = own;
    uint64_t o = opponent;
    int32_t v = END_INFINITY;
    int32_t pol = 1;

    for (;;) {
        st.nodes += 1;

        if ((FLIP_NEIGHBOURS[p0] & o) != 0) {
            const uint64_t f = probe_flips_fast<Tier>(p, o, p0);
            if (f != 0) {
                v = last3_fast<Tier>(st, o ^ f, p ^ f ^ (uint64_t{1} << p0), alpha, p1, p2, p3, empties);
                if (alpha >= v) {
                    return v * pol;
                }
            }
        }

        if ((FLIP_NEIGHBOURS[p1] & o) != 0) {
            const uint64_t f = probe_flips_fast<Tier>(p, o, p1);
            if (f != 0) {
                const int32_t g = last3_fast<Tier>(st, o ^ f, p ^ f ^ (uint64_t{1} << p1), alpha, p0, p2, p3, empties);
                if (alpha >= g) {
                    return g * pol;
                }
                if (g < v) {
                    v = g;
                }
            }
        }

        if ((FLIP_NEIGHBOURS[p2] & o) != 0) {
            const uint64_t f = probe_flips_fast<Tier>(p, o, p2);
            if (f != 0) {
                const int32_t g = last3_fast<Tier>(st, o ^ f, p ^ f ^ (uint64_t{1} << p2), alpha, p0, p1, p3, empties);
                if (alpha >= g) {
                    return g * pol;
                }
                if (g < v) {
                    v = g;
                }
            }
        }

        if ((FLIP_NEIGHBOURS[p3] & o) != 0) {
            const uint64_t f = probe_flips_fast<Tier>(p, o, p3);
            if (f != 0) {
                const int32_t g = last3_fast<Tier>(st, o ^ f, p ^ f ^ (uint64_t{1} << p3), alpha, p0, p1, p2, empties);
                const int32_t best = (g < v) ? g : v;
                return best * pol;
            }
        }

        if (v < END_INFINITY) {
            return v * pol;
        }

        std::swap(p, o);
        alpha = -alpha - 1;
        pol = -pol;
        if (pol >= 0) {
            break;
        }
    }

    st.evaluations += 1;
    return -score_with_empties(own, 4);
}

} // namespace detail::last

template <cpu::CpuTier Tier>
AbortResult<int32_t> Kernel<Tier>::last4(Searcher& s, uint64_t own, uint64_t opponent, int32_t alpha) {
    detail::last::LastStats st{};
    const int32_t score = detail::last::last4_fast<Tier>(st, own, opponent, alpha);
    STELLO_TRY_VOID_I32(detail::last::flush_last_stats(s, st));
    STELLO_RETURN_OK_R(int32_t, score);
}

template <cpu::CpuTier Tier>
AbortResult<int32_t> Kernel<Tier>::last3(
    Searcher& s,
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    uint32_t p0,
    uint32_t p1,
    uint32_t p2,
    uint64_t empties4
) {
    detail::last::LastStats st{};
    const int32_t score = detail::last::last3_fast<Tier>(st, own, opponent, alpha, p0, p1, p2, empties4);
    STELLO_TRY_VOID_I32(detail::last::flush_last_stats(s, st));
    STELLO_RETURN_OK_R(int32_t, score);
}

template <cpu::CpuTier Tier>
AbortResult<int32_t> Kernel<Tier>::last2(
    Searcher& s,
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    uint32_t p0,
    uint32_t p1
) {
    detail::last::LastStats st{};
    const int32_t score = detail::last::last2_fast<Tier>(st, own, opponent, alpha, p0, p1);
    STELLO_TRY_VOID_I32(detail::last::flush_last_stats(s, st));
    STELLO_RETURN_OK_R(int32_t, score);
}

template <cpu::CpuTier Tier>
AbortResult<int32_t> Kernel<Tier>::last1(Searcher& s, uint64_t own, int32_t alpha, uint32_t square) {
    detail::last::LastStats st{};
    const int32_t score = detail::last::last1_fast<Tier>(st, own, alpha, square);
    STELLO_TRY_VOID_I32(detail::last::flush_last_stats(s, st));
    STELLO_RETURN_OK_R(int32_t, score);
}

} // namespace stello::search
