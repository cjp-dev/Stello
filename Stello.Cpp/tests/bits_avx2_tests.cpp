#include "tests/test_harness.hpp"

#include "core/board.hpp"
#include "core/cpu.hpp"
#include "endgame/bits.hpp"
#include "endgame/bits_avx2.hpp"
#include "endgame/stable.hpp"
#include "search/engine.hpp"

#include <bit>
#include <cstdint>
#include <fstream>
#include <string>

namespace stello::search::detail {
uint64_t stable_of_v3_for_test(uint64_t own, uint64_t opponent) noexcept;
} // namespace stello::search::detail

namespace stello::tests {

namespace {

uint64_t reference_line_stable_pair(int32_t b, int32_t w) noexcept {
    // Ground-truth per-square edge stability matching original line_stability & b.
    auto play_line = [](int32_t p, int32_t o, int32_t place) noexcept -> std::pair<int32_t, int32_t> {
        const uint32_t pu = static_cast<uint32_t>(p);
        const uint32_t ou = static_cast<uint32_t>(o);
        uint32_t np = pu | (uint32_t{1} << place);
        int32_t i = place - 1;
        while (i > 0 && ((ou >> i) & 1U) != 0) --i;
        if (((pu >> (i & 31)) & 1U) != 0) {
            for (int32_t j = place - 1; j >= i + 1; --j) np ^= (uint32_t{1} << j);
        }
        i = place + 1;
        while (i < 7 && ((ou >> i) & 1U) != 0) ++i;
        if (((pu >> (i & 31)) & 1U) != 0) {
            for (int32_t j = place + 1; j < i; ++j) np ^= (uint32_t{1} << j);
        }
        return {static_cast<int32_t>(np), static_cast<int32_t>(ou & ~np)};
    };
    auto solve = [&](auto& self, int32_t p, int32_t o, int32_t mask) noexcept -> int32_t {
        mask &= p;
        const int32_t empties = (~(p | o)) & 0xFF;
        if (mask == 0 || empties == 0) return mask;
        for (int32_t i = 0; i < 8; ++i) {
            if (((empties >> i) & 1) != 0) {
                const auto [np1, no1] = play_line(p, o, i);
                mask = self(self, np1, no1, mask);
                if (mask == 0) return 0;
                const auto [no2, np2] = play_line(o, p, i);
                mask = self(self, np2, no2, mask);
                if (mask == 0) return 0;
            }
        }
        return mask;
    };
    return static_cast<uint64_t>(solve(solve, b, w, b));
}

uint64_t reference_stable_edge(uint64_t own, uint64_t opponent) noexcept {
    auto col = [](uint64_t x, uint32_t c) noexcept -> int32_t {
        int32_t out = 0;
        for (int32_t r = 0; r < 8; ++r) {
            if (((x >> (r * 8 + static_cast<int32_t>(c))) & 1ULL) != 0) out |= (1 << r);
        }
        return out;
    };
    auto unpack_col = [](uint64_t row_bits, uint32_t c) noexcept -> uint64_t {
        uint64_t out = 0;
        for (int32_t r = 0; r < 8; ++r) {
            if (((row_bits >> r) & 1ULL) != 0) out |= uint64_t{1} << (r * 8 + static_cast<int32_t>(c));
        }
        return out;
    };
    uint64_t res = 0;
    res |= reference_line_stable_pair(static_cast<int32_t>(own & 0xFFU), static_cast<int32_t>(opponent & 0xFFU));
    res |= reference_line_stable_pair(static_cast<int32_t>(own >> 56), static_cast<int32_t>(opponent >> 56)) << 56;
    res |= unpack_col(reference_line_stable_pair(col(own, 0), col(opponent, 0)), 0);
    res |= unpack_col(reference_line_stable_pair(col(own, 7), col(opponent, 7)), 7);
    return res;
}

uint64_t reference_stable_of(uint64_t own, uint64_t opponent) noexcept {
    const uint64_t discs = own | opponent;
    auto is_full = [&](int r, int c, int dr, int dc) noexcept {
        for (;;) {
            if (r < 0 || r >= 8 || c < 0 || c >= 8) break;
            if (((discs >> (r * 8 + c)) & 1ULL) == 0) return false;
            r += dr;
            c += dc;
        }
        return true;
    };
    uint64_t fh = 0, fv = 0, fd7 = 0, fd9 = 0;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            const uint64_t bit = uint64_t{1} << (r * 8 + c);
            if (is_full(r, c, 0, 1) && is_full(r, c, 0, -1)) fh |= bit;
            if (is_full(r, c, 1, 0) && is_full(r, c, -1, 0)) fv |= bit;
            if (is_full(r, c, 1, -1) && is_full(r, c, -1, 1)) fd7 |= bit;
            if (is_full(r, c, 1, 1) && is_full(r, c, -1, -1)) fd9 |= bit;
        }
    }
    const uint64_t inner = own & 0x007E'7E7E'7E7E'7E00ULL;
    uint64_t s = (reference_stable_edge(own, opponent) | (fh & fv & fd7 & fd9)) & own;
    for (;;) {
        const uint64_t prev = s;
        s |= ((s >> 1) | (s << 1) | fh)
           & ((s >> 8) | (s << 8) | fv)
           & ((s >> 7) | (s << 7) | fd7)
           & ((s >> 9) | (s << 9) | fd9)
           & inner;
        if (s == prev) break;
    }
    return s;
}

uint64_t splitmix64(uint64_t& state) noexcept {
    state += 0x9e37'79b9'7f4a'7c15ULL;
    uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xbf58'476d'1ce4'e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d0'49bb'1331'11ebULL;
    return z ^ (z >> 31);
}

uint64_t parse_hex64(const std::string& text) {
    return std::stoull(text, nullptr, 16);
}

struct PortableStateRestore {
    bool prev{false};
    explicit PortableStateRestore() noexcept : prev(!cpu::fast_pext()) {}
    ~PortableStateRestore() { cpu::set_portable_only(prev); }
};

void check_board_avx2(uint64_t own, uint64_t opponent) {
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__AVX2__) && !defined(__EMSCRIPTEN__)
    STELLO_CHECK_EQ(endgame::bits::legal_moves_avx2(own, opponent), endgame::bits::legal_moves(own, opponent));
    STELLO_CHECK_EQ(endgame::bits::legal_moves_avx2(opponent, own), endgame::bits::legal_moves(opponent, own));
    {
        uint64_t leg = 0;
        int32_t pot = 0;
        endgame::bits::legal_and_potential_avx2(own, opponent, leg, pot);
        STELLO_CHECK_EQ(leg, endgame::bits::legal_moves(own, opponent));
        STELLO_CHECK_EQ(pot, endgame::bits::potential_mobility(opponent, ~(own | opponent)));
    }
    for (uint32_t sq = 0; sq < 64; ++sq) {
        if (((own | opponent) & (uint64_t{1} << sq)) != 0) {
            continue;
        }
        const uint64_t ref = endgame::bits::flips(own, opponent, sq);
        STELLO_CHECK_EQ(endgame::bits::flips_avx2(own, opponent, sq), ref);
        STELLO_CHECK_EQ(endgame::bits::flips_avx2_smartclr(own, opponent, sq), ref);
        STELLO_CHECK_EQ(endgame::bits::flips_avx2(opponent, own, sq), endgame::bits::flips(opponent, own, sq));
        STELLO_CHECK_EQ(endgame::bits::flips_avx2_smartclr(opponent, own, sq), endgame::bits::flips(opponent, own, sq));
    }
#else
    (void)own;
    (void)opponent;
#endif
}

} // namespace

STELLO_TEST(avx2_bits_parity_positions_and_random) {
    if (!cpu::has_avx2_bmi2()) {
        return;
    }

    const auto path = parity_data_dir() / "positions.txt";
    std::ifstream file(path, std::ios::binary);
    STELLO_CHECK_MSG(static_cast<bool>(file), path.string());

    std::string line;
    size_t checked_positions = 0;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const size_t s1 = line.find(' ');
        STELLO_CHECK(s1 != std::string::npos);
        const size_t s2 = line.find(' ', s1 + 1);
        STELLO_CHECK(s2 != std::string::npos);
        const size_t s3 = line.find(' ', s2 + 1);
        STELLO_CHECK(s3 != std::string::npos);

        const uint64_t black = parse_hex64(line.substr(s1 + 1, s2 - s1 - 1));
        const uint64_t white = parse_hex64(line.substr(s2 + 1, s3 - s2 - 1));
        check_board_avx2(black, white);
        ++checked_positions;
    }
    STELLO_CHECK(checked_positions > 5000);

    uint64_t rng = 0x51e1'1040'0000'5001ULL;
    for (size_t i = 0; i < 2000; ++i) {
        const uint64_t a = splitmix64(rng);
        const uint64_t b = splitmix64(rng) & ~a;
        check_board_avx2(a, b);
    }
}

STELLO_TEST(avx2_search_node_parity_with_portable) {
    if (!cpu::has_avx2_bmi2()) {
        return;
    }
    PortableStateRestore restore;

    const auto mid_board = Board::parse("..O.X.....OOXX...OXXXXO..OXOXX...OOXXOX..OOXOO....O..O..........");
    STELLO_CHECK(mid_board.has_value());

    cpu::set_portable_only(true);
    search::SearchEngine portable_engine(18);
    const auto portable_mid = portable_engine.search(*mid_board, Player::Black, SearchLimits::fixed_depth(8));
    STELLO_CHECK(portable_mid.ok);

    cpu::set_portable_only(false);
    search::SearchEngine v3_engine(18);
    const auto v3_mid = v3_engine.search(*mid_board, Player::Black, SearchLimits::fixed_depth(8));
    STELLO_CHECK(v3_mid.ok);

    STELLO_CHECK_EQ(v3_mid.result.mv.to_string(), portable_mid.result.mv.to_string());
    STELLO_CHECK_EQ(v3_mid.result.score, portable_mid.result.score);
    STELLO_CHECK_EQ(v3_mid.result.nodes, portable_mid.result.nodes);
    STELLO_CHECK_EQ(v3_mid.result.evaluations, portable_mid.result.evaluations);

    const auto ffo40 = Board::parse("O..OOOOX.OOOOOOXOOXXOOOXOOXOOOXXOOOOOOXX..OOOOX....O..X.........");
    STELLO_CHECK(ffo40.has_value());

    cpu::set_portable_only(true);
    const auto portable_end = portable_engine.search(*ffo40, Player::Black, SearchLimits::solve());
    STELLO_CHECK(portable_end.ok);

    cpu::set_portable_only(false);
    const auto v3_end = v3_engine.search(*ffo40, Player::Black, SearchLimits::solve());
    STELLO_CHECK(v3_end.ok);

    STELLO_CHECK_EQ(v3_end.result.mv.to_string(), portable_end.result.mv.to_string());
    STELLO_CHECK_EQ(v3_end.result.score, portable_end.result.score);
    STELLO_CHECK_EQ(v3_end.result.nodes, portable_end.result.nodes);
    STELLO_CHECK_EQ(v3_end.result.evaluations, portable_end.result.evaluations);
}

STELLO_TEST(stability_and_bit_utils_exact_parity) {
    endgame::stable::init();
    const bool check_avx2 = cpu::has_avx2_bmi2();

    auto check_pair = [&](uint64_t own, uint64_t opp) {
        const uint64_t ref = reference_stable_of(own, opp);
        const uint64_t port = endgame::stable::of(own, opp);
        STELLO_CHECK_EQ(port, ref);
        STELLO_CHECK_EQ(endgame::stable::edge_stability_count(own, opp),
                        static_cast<int32_t>(std::popcount(reference_stable_edge(own, opp))));
        STELLO_CHECK_EQ(endgame::stable::bit_weighted_count(own),
                        static_cast<int32_t>(std::popcount(own) + std::popcount(own & 0x8100'0000'0000'0081ULL)));
        const uint64_t ref_corner = (
            ((0x0100'0000'0000'0001ULL & own) << 1)
            | ((0x8000'0000'0000'0080ULL & own) >> 1)
            | ((0x0000'0000'0000'0081ULL & own) << 8)
            | ((0x8100'0000'0000'0000ULL & own) >> 8)
            | 0x8100'0000'0000'0081ULL) & own;
        STELLO_CHECK_EQ(endgame::stable::corner_stability(own), static_cast<int32_t>(std::popcount(ref_corner)));

        uint64_t ref_tr = 0;
        for (uint32_t r = 0; r < 8; ++r) {
            for (uint32_t c = 0; c < 8; ++c) {
                if (((own >> (r * 8U + c)) & 1ULL) != 0) {
                    ref_tr |= uint64_t{1} << (c * 8U + r);
                }
            }
        }
        STELLO_CHECK_EQ(endgame::stable::transpose_portable(own), ref_tr);
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__AVX2__) && !defined(__EMSCRIPTEN__)
        if (check_avx2) {
            STELLO_CHECK_EQ(endgame::bits::stable_of_avx2(own, opp), ref);
            STELLO_CHECK_EQ(search::detail::stable_of_v3_for_test(own, opp), ref);
            STELLO_CHECK_EQ(endgame::bits::transpose_avx2(own), ref_tr);
        }
#else
        (void)check_avx2;
#endif
    };

    const auto path = parity_data_dir() / "positions.txt";
    std::ifstream file(path, std::ios::binary);
    STELLO_CHECK_MSG(static_cast<bool>(file), path.string());
    std::string line;
    size_t pos_count = 0;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const size_t s1 = line.find(' ');
        const size_t s2 = line.find(' ', s1 + 1);
        const size_t s3 = line.find(' ', s2 + 1);
        const uint64_t black = parse_hex64(line.substr(s1 + 1, s2 - s1 - 1));
        const uint64_t white = parse_hex64(line.substr(s2 + 1, s3 - s2 - 1));
        check_pair(black, white);
        check_pair(white, black);
        ++pos_count;
    }
    STELLO_CHECK(pos_count > 5000);

    uint64_t rng = 0xCAFE'BABE'0000'2202ULL;
    for (size_t i = 0; i < 100'000; ++i) {
        const uint64_t a = splitmix64(rng);
        const uint64_t b = splitmix64(rng) & ~a;
        check_pair(a, b);
    }
}

STELLO_TEST(simul_last_flip_exact_parity) {
    const bool check_avx2 = cpu::has_avx2_bmi2();
    uint64_t rng = 0x51A5'7F1F'0000'2401ULL;
    for (size_t i = 0; i < 2'000; ++i) {
        uint64_t own = 0;
        switch (i & 3U) {
            case 0: own = splitmix64(rng); break;
            case 1: own = splitmix64(rng) & splitmix64(rng); break;
            case 2: own = splitmix64(rng) | splitmix64(rng); break;
            default: own = splitmix64(rng) & splitmix64(rng) & splitmix64(rng); break;
        }
        for (uint32_t sq = 0; sq < 64U; ++sq) {
            const uint64_t bit = uint64_t{1} << sq;
            const uint32_t pf_expected = 2U * static_cast<uint32_t>(std::popcount(
                endgame::bits::flips(own & ~bit, ~own & ~bit, sq)));
            const uint32_t of_expected = 2U * static_cast<uint32_t>(std::popcount(
                endgame::bits::flips(~own & ~bit, own & ~bit, sq)));
            const uint16_t expected = static_cast<uint16_t>((of_expected << 8U) | pf_expected);
            STELLO_CHECK_EQ(endgame::bits::simul_last_flip_portable(own, sq), expected);
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__AVX2__) && !defined(__EMSCRIPTEN__)
            if (check_avx2) {
                STELLO_CHECK_EQ(endgame::bits::simul_last_flip_bmi2(own, sq), expected);
            }
#else
            (void)check_avx2;
#endif
        }
    }
}

} // namespace stello::tests
