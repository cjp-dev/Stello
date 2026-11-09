#include "search/engine.hpp"

#include "core/compiler.hpp"
#include "endgame/bits.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <utility>

namespace stello::search {

namespace {

struct LastLines {
    uint64_t column{0};
    uint64_t column_multiplier{0};
    uint64_t diagonal{0};
    uint64_t anti_diagonal{0};
};

constexpr std::array<LastLines, 64> make_last_lines() noexcept {
    std::array<LastLines, 64> lines{};
    for (int32_t square = 0; square < 64; ++square) {
        const int32_t col = square & 7;
        const int32_t row = square >> 3;
        LastLines line{};
        for (int32_t r = 0; r < 8; ++r) {
            line.column |= uint64_t{1} << (r * 8 + col);
            line.column_multiplier |= uint64_t{1} << (56 - col - 7 * r);
            const int32_t c_diag = col + (r - row);
            if (c_diag >= 0 && c_diag < 8) {
                line.diagonal |= uint64_t{1} << (r * 8 + c_diag);
            }
            const int32_t c_anti = col - (r - row);
            if (c_anti >= 0 && c_anti < 8) {
                line.anti_diagonal |= uint64_t{1} << (r * 8 + c_anti);
            }
        }
        lines[static_cast<size_t>(square)] = line;
    }
    return lines;
}

constexpr std::array<std::array<uint8_t, 256>, 8> make_last_flip_counts() noexcept {
    std::array<std::array<uint8_t, 256>, 8> counts{};
    for (int32_t place = 0; place < 8; ++place) {
        for (int32_t own = 0; own < 256; ++own) {
            uint8_t count = 0;
            uint8_t run = 0;
            int32_t i = place + 1;
            while (i < 8 && (own & (1 << i)) == 0) {
                ++run;
                ++i;
            }
            if (i < 8) {
                count = static_cast<uint8_t>(count + run);
            }

            run = 0;
            i = place - 1;
            while (i >= 0 && (own & (1 << i)) == 0) {
                ++run;
                --i;
            }
            if (i >= 0) {
                count = static_cast<uint8_t>(count + run);
            }
            counts[static_cast<size_t>(place)][static_cast<size_t>(own)] = count;
        }
    }
    return counts;
}

inline constexpr auto LAST_LINES = make_last_lines();
inline constexpr auto LAST_FLIP_COUNTS = make_last_flip_counts();

STELLO_FORCEINLINE int32_t last_flip_count(uint64_t own, uint32_t square) noexcept {
    const uint32_t col = square & 7U;
    const uint32_t row = square >> 3U;
    own &= ~(uint64_t{1} << square);
    auto line = [&](uint64_t mask, uint64_t mult) noexcept -> size_t {
        return static_cast<size_t>(((own & mask) * mult) >> 56U);
    };
    const LastLines& lines = LAST_LINES[static_cast<size_t>(square)];
    return static_cast<int32_t>(
        LAST_FLIP_COUNTS[col][static_cast<size_t>((own >> (8U * row)) & 0xFFULL)]
        + LAST_FLIP_COUNTS[row][line(lines.column, lines.column_multiplier)]
        + LAST_FLIP_COUNTS[col][line(lines.diagonal, 0x0101'0101'0101'0101ULL)]
        + LAST_FLIP_COUNTS[col][line(lines.anti_diagonal, 0x0101'0101'0101'0101ULL)]);
}

} // namespace

namespace detail {

int32_t last_flip_count_for_test(uint64_t own, uint32_t square) noexcept {
    return last_flip_count(own, square);
}

} // namespace detail

AbortResult<int32_t> Searcher::last4(uint64_t own, uint64_t opponent, int32_t alpha) {
    using endgame::bits::FLIP_NEIGHBOURS;
    using endgame::bits::flips;
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
        STELLO_TRY_VOID_I32(count_node());

        if ((FLIP_NEIGHBOURS[p0] & o) != 0) {
            const uint64_t f = flips(p, o, p0);
            if (f != 0) {
                STELLO_TRY_ASSIGN_EXISTING_I32(v, last3(o ^ f, p ^ f ^ (uint64_t{1} << p0), alpha, p1, p2, p3, empties));
                if (alpha >= v) {
                    STELLO_RETURN_OK_R(int32_t, v * pol);
                }
            }
        }

        if ((FLIP_NEIGHBOURS[p1] & o) != 0) {
            const uint64_t f = flips(p, o, p1);
            if (f != 0) {
                STELLO_TRY_ASSIGN_I32(const int32_t g, last3(o ^ f, p ^ f ^ (uint64_t{1} << p1), alpha, p0, p2, p3, empties));
                if (alpha >= g) {
                    STELLO_RETURN_OK_R(int32_t, g * pol);
                }
                if (g < v) {
                    v = g;
                }
            }
        }

        if ((FLIP_NEIGHBOURS[p2] & o) != 0) {
            const uint64_t f = flips(p, o, p2);
            if (f != 0) {
                STELLO_TRY_ASSIGN_I32(const int32_t g, last3(o ^ f, p ^ f ^ (uint64_t{1} << p2), alpha, p0, p1, p3, empties));
                if (alpha >= g) {
                    STELLO_RETURN_OK_R(int32_t, g * pol);
                }
                if (g < v) {
                    v = g;
                }
            }
        }

        if ((FLIP_NEIGHBOURS[p3] & o) != 0) {
            const uint64_t f = flips(p, o, p3);
            if (f != 0) {
                STELLO_TRY_ASSIGN_I32(const int32_t g, last3(o ^ f, p ^ f ^ (uint64_t{1} << p3), alpha, p0, p1, p2, empties));
                const int32_t best = (g < v) ? g : v;
                STELLO_RETURN_OK_R(int32_t, best * pol);
            }
        }

        if (v < END_INFINITY) {
            STELLO_RETURN_OK_R(int32_t, v * pol);
        }

        std::swap(p, o);
        alpha = -alpha - 1;
        pol = -pol;
        if (pol >= 0) {
            break;
        }
    }

    evaluations += 1;
    STELLO_RETURN_OK_R(int32_t, -score_with_empties(own, 4));
}

AbortResult<int32_t> Searcher::last3(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    uint32_t p0,
    uint32_t p1,
    uint32_t p2,
    uint64_t empties4
) {
    using endgame::bits::FLIP_NEIGHBOURS;
    using endgame::bits::flips;
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
        STELLO_TRY_VOID_I32(count_node());

        if ((FLIP_NEIGHBOURS[p0] & o) != 0) {
            const uint64_t f = flips(p, o, p0);
            if (f != 0) {
                STELLO_TRY_ASSIGN_EXISTING_I32(v, last2(o ^ f, p ^ f ^ (uint64_t{1} << p0), alpha, p1, p2));
                if (alpha < v) {
                    STELLO_RETURN_OK_R(int32_t, v * pol);
                }
            }
        }

        if ((FLIP_NEIGHBOURS[p1] & o) != 0) {
            const uint64_t f = flips(p, o, p1);
            if (f != 0) {
                STELLO_TRY_ASSIGN_I32(const int32_t g, last2(o ^ f, p ^ f ^ (uint64_t{1} << p1), alpha, p2, p0));
                if (alpha < g) {
                    STELLO_RETURN_OK_R(int32_t, g * pol);
                }
                if (g > v) {
                    v = g;
                }
            }
        }

        if ((FLIP_NEIGHBOURS[p2] & o) != 0) {
            const uint64_t f = flips(p, o, p2);
            if (f != 0) {
                STELLO_TRY_ASSIGN_I32(const int32_t g, last2(o ^ f, p ^ f ^ (uint64_t{1} << p2), alpha, p1, p0));
                const int32_t best = (g > v) ? g : v;
                STELLO_RETURN_OK_R(int32_t, best * pol);
            }
        }

        if (v > -END_INFINITY) {
            STELLO_RETURN_OK_R(int32_t, v * pol);
        }

        std::swap(p, o);
        alpha = -alpha - 1;
        pol = -pol;
        if (pol >= 0) {
            break;
        }
    }

    evaluations += 1;
    STELLO_RETURN_OK_R(int32_t, score_with_odd_empties(own, 3));
}

AbortResult<int32_t> Searcher::last2(uint64_t own, uint64_t opponent, int32_t alpha, uint32_t p0, uint32_t p1) {
    using endgame::bits::FLIP_NEIGHBOURS;
    using endgame::bits::flips;
    using endgame::bits::score_with_empties;

    STELLO_TRY_VOID_I32(count_node());
    int32_t v = 0;
    const uint64_t f0 = ((FLIP_NEIGHBOURS[p0] & opponent) != 0) ? flips(own, opponent, p0) : 0ULL;

    if (f0 != 0) {
        STELLO_TRY_ASSIGN_EXISTING_I32(v, last1(opponent ^ f0, alpha, p1));
        if (v > alpha && (FLIP_NEIGHBOURS[p1] & opponent) != 0) {
            const uint64_t f1 = flips(own, opponent, p1);
            if (f1 != 0) {
                STELLO_TRY_ASSIGN_I32(const int32_t g, last1(opponent ^ f1, alpha, p0));
                if (g < v) {
                    v = g;
                }
            }
        }
    } else {
        const uint64_t f1 = ((FLIP_NEIGHBOURS[p1] & opponent) != 0) ? flips(own, opponent, p1) : 0ULL;
        if (f1 != 0) {
            STELLO_TRY_ASSIGN_EXISTING_I32(v, last1(opponent ^ f1, alpha, p0));
        } else {
            STELLO_TRY_VOID_I32(count_node());
            alpha = -alpha - 1;
            const uint64_t g0 = flips(opponent, own, p0);
            if (g0 != 0) {
                STELLO_TRY_ASSIGN_EXISTING_I32(v, last1(own ^ g0, alpha, p1));
                if (v > alpha) {
                    const uint64_t g1 = flips(opponent, own, p1);
                    if (g1 != 0) {
                        STELLO_TRY_ASSIGN_I32(const int32_t g, last1(own ^ g1, alpha, p0));
                        if (g < v) {
                            v = g;
                        }
                    }
                }
            } else {
                const uint64_t g1 = flips(opponent, own, p1);
                if (g1 != 0) {
                    STELLO_TRY_ASSIGN_EXISTING_I32(v, last1(own ^ g1, alpha, p0));
                } else {
                    evaluations += 1;
                    v = score_with_empties(own, 2);
                }
            }
            v = -v;
        }
    }

    STELLO_RETURN_OK_R(int32_t, v);
}

AbortResult<int32_t> Searcher::last1(uint64_t own, int32_t alpha, uint32_t square) {
    STELLO_TRY_VOID_I32(count_node());
    int32_t flipped = last_flip_count(own, square);
    int32_t score = 2 * (static_cast<int32_t>(std::popcount(own)) + flipped + 1) - 64;
    if (flipped == 0) {
        STELLO_TRY_VOID_I32(count_node());
        const int32_t pass_score = score - 2;
        if (score <= 0) {
            score = pass_score;
        }
        if (score > alpha) {
            flipped = last_flip_count(~own, square);
            if (flipped != 0) {
                score = pass_score - 2 * flipped;
            }
        }
    }

    evaluations += 1;
    STELLO_RETURN_OK_R(int32_t, score);
}

} // namespace stello::search
