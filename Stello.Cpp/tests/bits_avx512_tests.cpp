#include "tests/test_harness.hpp"

#include "core/board.hpp"
#include "core/cpu.hpp"
#include "endgame/bits.hpp"
#include "endgame/bits_avx512.hpp"
#include "endgame/stable.hpp"
#include "search/engine.hpp"

#include <bit>
#include <cstdint>
#include <fstream>
#include <string>

namespace stello::search::detail {
uint64_t stable_of_v4_for_test(uint64_t own, uint64_t opponent) noexcept;
int32_t solve_last_v4_for_test(uint64_t own, uint32_t square) noexcept;
} // namespace stello::search::detail

namespace stello::tests {

namespace {

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

struct TierRestore {
    cpu::CpuTier prev{cpu::active_tier()};
    ~TierRestore() { (void)cpu::set_forced_tier(prev); }
};

void check_board_avx512(uint64_t own, uint64_t opponent) {
#if STELLO_TU_HAS_AVX512_V4
    STELLO_CHECK_EQ(endgame::bits::legal_moves_avx512(own, opponent), endgame::bits::legal_moves(own, opponent));
    STELLO_CHECK_EQ(endgame::bits::legal_moves_avx512(opponent, own), endgame::bits::legal_moves(opponent, own));
    {
        uint64_t leg = 0;
        int32_t pot = 0;
        endgame::bits::legal_and_potential_avx512(own, opponent, leg, pot);
        STELLO_CHECK_EQ(leg, endgame::bits::legal_moves(own, opponent));
        STELLO_CHECK_EQ(pot, endgame::bits::potential_mobility(opponent, ~(own | opponent)));
    }
    STELLO_CHECK_EQ(endgame::bits::stable_of_avx512(own, opponent), endgame::stable::of(own, opponent));
    STELLO_CHECK_EQ(search::detail::stable_of_v4_for_test(own, opponent), endgame::stable::of(own, opponent));
    STELLO_CHECK_EQ(endgame::bits::horizontal_mirror_gfni(own), endgame::bits::horizontal_mirror_portable(own));
#else
    (void)own;
    (void)opponent;
#endif
}

} // namespace

STELLO_TEST(avx512_bits_parity_positions_and_random) {
    if (!cpu::has_avx512_v4()) {
        return;
    }
    endgame::stable::init();

    const auto path = parity_data_dir() / "positions.txt";
    std::ifstream file(path, std::ios::binary);
    STELLO_CHECK_MSG(static_cast<bool>(file), path.string());

    std::string line;
    size_t checked_positions = 0;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const size_t s1 = line.find(' ');
        const size_t s2 = line.find(' ', s1 + 1);
        const size_t s3 = line.find(' ', s2 + 1);
        const uint64_t black = parse_hex64(line.substr(s1 + 1, s2 - s1 - 1));
        const uint64_t white = parse_hex64(line.substr(s2 + 1, s3 - s2 - 1));
        check_board_avx512(black, white);
        ++checked_positions;
    }
    STELLO_CHECK(checked_positions > 5000);

    uint64_t rng = 0x51e1'1040'0000'5404ULL;
    for (size_t i = 0; i < 2000; ++i) {
        const uint64_t own = splitmix64(rng);
        const uint64_t opp = splitmix64(rng) & ~own;
        check_board_avx512(own, opp);

#if STELLO_TU_HAS_AVX512_V4
        for (uint32_t sq = 0; sq < 64U; ++sq) {
            const uint64_t bit = uint64_t{1} << sq;
            if (((own | opp) & bit) == 0ULL) {
                const uint64_t ref = endgame::bits::flips(own, opp, sq);
                STELLO_CHECK_EQ(endgame::bits::flips_avx512cd(own, opp, sq), ref);
                STELLO_CHECK_EQ(endgame::bits::flips_avx512_gfni(own, opp, sq), ref);
            }

            const uint64_t p_last = own & ~bit;
            const __m128i pair = endgame::bits::simul_last_flip_pair_avx512(p_last, sq);
            const uint64_t p_flip = static_cast<uint64_t>(_mm_cvtsi128_si64(pair));
            const uint64_t o_flip = static_cast<uint64_t>(_mm_extract_epi64(pair, 1));
            STELLO_CHECK_EQ(p_flip, endgame::bits::flips(p_last, ~p_last & ~bit, sq));
            STELLO_CHECK_EQ(o_flip, endgame::bits::flips(~p_last & ~bit, p_last, sq));

            const uint16_t port_op = endgame::bits::simul_last_flip_portable(p_last, sq);
            const int32_t p2 = static_cast<int32_t>(port_op & 0xFFU);
            const int32_t base_s = 2 * static_cast<int32_t>(std::popcount(p_last)) - 62 + p2;
            const int32_t expected_solve = (p2 != 0)
                ? base_s
                : (base_s - static_cast<int32_t>(port_op >> 8U)
                   - static_cast<int32_t>((-static_cast<int32_t>(port_op >> 8U) | (base_s - 1)) < 0) * 2);
            STELLO_CHECK_EQ(search::detail::solve_last_v4_for_test(p_last, sq), expected_solve);
        }
#endif
    }
}

STELLO_TEST(avx512_v4_search_node_parity_with_portable) {
    if (!cpu::has_avx512_v4()) {
        return;
    }
    TierRestore restore;

    const auto mid_board = Board::parse("..O.X.....OOXX...OXXXXO..OXOXX...OOXXOX..OOXOO....O..O..........");
    STELLO_CHECK(mid_board.has_value());

    STELLO_CHECK(cpu::set_forced_tier(cpu::CpuTier::Portable));
    search::SearchEngine engine(18);
    const auto portable_mid = engine.search(*mid_board, Player::Black, SearchLimits::fixed_depth(8));
    STELLO_CHECK(portable_mid.ok);

    STELLO_CHECK(cpu::set_forced_tier(cpu::CpuTier::V4));
    engine.clear_hash();
    const auto v4_mid = engine.search(*mid_board, Player::Black, SearchLimits::fixed_depth(8));
    STELLO_CHECK(v4_mid.ok);

    STELLO_CHECK_EQ(v4_mid.result.mv.to_string(), portable_mid.result.mv.to_string());
    STELLO_CHECK_EQ(v4_mid.result.score, portable_mid.result.score);
    STELLO_CHECK_EQ(v4_mid.result.nodes, portable_mid.result.nodes);
    STELLO_CHECK_EQ(v4_mid.result.evaluations, portable_mid.result.evaluations);

    const auto ffo40 = Board::parse("O..OOOOX.OOOOOOXOOXXOOOXOOXOOOXXOOOOOOXX..OOOOX....O..X.........");
    STELLO_CHECK(ffo40.has_value());

    STELLO_CHECK(cpu::set_forced_tier(cpu::CpuTier::Portable));
    engine.clear_hash();
    const auto portable_end = engine.search(*ffo40, Player::Black, SearchLimits::solve());
    STELLO_CHECK(portable_end.ok);

    STELLO_CHECK(cpu::set_forced_tier(cpu::CpuTier::V4));
    engine.clear_hash();
    const auto v4_end = engine.search(*ffo40, Player::Black, SearchLimits::solve());
    STELLO_CHECK(v4_end.ok);

    STELLO_CHECK_EQ(v4_end.result.mv.to_string(), portable_end.result.mv.to_string());
    STELLO_CHECK_EQ(v4_end.result.score, portable_end.result.score);
    STELLO_CHECK_EQ(v4_end.result.nodes, portable_end.result.nodes);
    STELLO_CHECK_EQ(v4_end.result.evaluations, portable_end.result.evaluations);
}

} // namespace stello::tests
