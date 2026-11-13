#include "tests/test_harness.hpp"

#include "core/board.hpp"
#include "core/cpu.hpp"
#include "endgame/bits.hpp"
#include "endgame/bits_avx2.hpp"
#include "search/engine.hpp"

#include <cstdint>
#include <fstream>
#include <string>

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

struct PortableStateRestore {
    bool prev{false};
    explicit PortableStateRestore() noexcept : prev(!cpu::fast_pext()) {}
    ~PortableStateRestore() { cpu::set_portable_only(prev); }
};

void check_board_avx2(uint64_t own, uint64_t opponent) {
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__AVX2__) && !defined(__EMSCRIPTEN__)
    STELLO_CHECK_EQ(endgame::bits::legal_moves_avx2(own, opponent), endgame::bits::legal_moves(own, opponent));
    STELLO_CHECK_EQ(endgame::bits::legal_moves_avx2(opponent, own), endgame::bits::legal_moves(opponent, own));
    for (uint32_t sq = 0; sq < 64; ++sq) {
        if (((own | opponent) & (uint64_t{1} << sq)) != 0) {
            continue;
        }
        STELLO_CHECK_EQ(endgame::bits::flips_avx2(own, opponent, sq), endgame::bits::flips(own, opponent, sq));
        STELLO_CHECK_EQ(endgame::bits::flips_avx2(opponent, own, sq), endgame::bits::flips(opponent, own, sq));
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

} // namespace stello::tests
