#include "tests/test_harness.hpp"

#include "core/bits.hpp"
#include "core/board.hpp"
#include "core/cpu.hpp"
#include "core/square.hpp"
#include "endgame/bits.hpp"
#include "eval/eval.hpp"
#include "eval/gzip.hpp"
#include "eval/patterns.hpp"
#include "eval/symmetry.hpp"
#include "eval/weights.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stello::tests {
namespace {

constexpr uint64_t kFnvOffset = 14'695'981'039'346'656'037ULL;
constexpr uint64_t kFnvPrime = 1'099'511'628'211ULL;

uint64_t fnv32(uint64_t hash, int32_t value) noexcept {
    const uint32_t u = static_cast<uint32_t>(value);
    for (int i = 0; i < 4; ++i) {
        hash = (hash ^ ((u >> (8 * i)) & 0xFFU)) * kFnvPrime;
    }
    return hash;
}

uint64_t fnv16(uint64_t hash, int16_t value) noexcept {
    const uint16_t u = static_cast<uint16_t>(value);
    for (int i = 0; i < 2; ++i) {
        hash = (hash ^ ((u >> (8 * i)) & 0xFFU)) * kFnvPrime;
    }
    return hash;
}

uint64_t parse_hex(std::string_view text) {
    uint64_t v = 0;
    for (char c : text) {
        v <<= 4;
        if (c >= '0' && c <= '9') v |= static_cast<uint64_t>(c - '0');
        else if (c >= 'a' && c <= 'f') v |= static_cast<uint64_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= static_cast<uint64_t>(c - 'A' + 10);
        else throw TestFailure("not hex: " + std::string(text));
    }
    return v;
}

int32_t parse_i32(std::string_view text) {
    return std::stoi(std::string(text));
}

uint64_t parse_u64(std::string_view text) {
    return std::stoull(std::string(text));
}

std::vector<std::string> split_char(std::string_view s, char delim) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        const size_t pos = s.find(delim, start);
        if (pos == std::string_view::npos) {
            out.emplace_back(s.substr(start));
            break;
        }
        out.emplace_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

std::vector<std::string> read_data_lines(const char* name) {
    const auto path = repo_root() / "Stello.Rust" / "stello-core" / "tests" / "data" / name;
    std::ifstream in(path);
    STELLO_CHECK_MSG(static_cast<bool>(in), path.string());
    std::vector<std::string> out;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        out.push_back(std::move(line));
    }
    return out;
}

struct Fixture {
    std::string id;
    Board board;
    Player player;

    uint64_t own() const noexcept { return board.discs(player); }
    uint64_t opp() const noexcept { return board.discs(opponent(player)); }
};

Fixture parse_fixture(const std::vector<std::string>& fields) {
    Player player = Player::Black;
    if (fields[3] == "X") {
        player = Player::Black;
    } else if (fields[3] == "O") {
        player = Player::White;
    } else {
        throw TestFailure("bad side: " + fields[3]);
    }
    return Fixture{fields[0], Board(parse_hex(fields[1]), parse_hex(fields[2])), player};
}

struct PortableGuard {
    bool prev_portable{false};
    explicit PortableGuard(bool portable) noexcept {
        prev_portable = !cpu::fast_pext();
        cpu::set_portable_only(portable);
    }
    ~PortableGuard() {
        cpu::set_portable_only(prev_portable);
    }
};

} // namespace

STELLO_TEST(square_and_move_unit) {
    STELLO_CHECK(Square::parse("a1") == Square(0));
    STELLO_CHECK(Square::parse("H8") == Square(63));
    STELLO_CHECK(!Square::parse("i1").has_value());
    STELLO_CHECK(!Square::parse("a9").has_value());
    STELLO_CHECK_EQ(Square(12).to_string(), std::string("e2"));
    STELLO_CHECK_EQ(Square(63).to_legacy(), 88);
    STELLO_CHECK(Square::from_legacy(11) == Square(0));
    STELLO_CHECK(!Square::from_legacy(19).has_value());
    STELLO_CHECK(Move::parse("PASS") == Move::pass());
    STELLO_CHECK_EQ(Move::pass().to_string(), std::string("pass"));

    std::vector<uint8_t> sqs;
    for (Square s : Square::in_mask(0x8000'0000'0000'0011ULL)) {
        sqs.push_back(s.index());
    }
    STELLO_CHECK(sqs == (std::vector<uint8_t>{0, 4, 63}));
}

STELLO_TEST(board_unit) {
    const auto text = Board::initial().to_string();
    STELLO_CHECK(Board::parse(text) == Board::initial());
    STELLO_CHECK(Board::initial().get(*Square::parse("e4")) == Player::Black);
    STELLO_CHECK(Board::initial().get(*Square::parse("d4")) == Player::White);
    STELLO_CHECK_EQ(std::popcount(Board::initial().legal_moves(Player::Black)), 4);
    BoardParseError err;
    STELLO_CHECK(!Board::parse("X", &err).has_value());
    STELLO_CHECK(err.kind == BoardParseErrorKind::WrongLength);

    const Square d3 = *Square::parse("d3");
    const auto next = Board::initial().play(Player::Black, d3);
    STELLO_CHECK(next.has_value());
    STELLO_CHECK_EQ(next->count(Player::Black), 4u);
    STELLO_CHECK_EQ(next->count(Player::White), 1u);
    STELLO_CHECK(!Board::initial().play(Player::Black, *Square::parse("a1")).has_value());
    STELLO_CHECK(!next->play(Player::White, d3).has_value());
}

STELLO_TEST(cpu_vendor_and_fast_pext) {
    auto le32 = [](const char s[5]) -> uint32_t {
        return static_cast<uint32_t>(static_cast<uint8_t>(s[0]))
             | (static_cast<uint32_t>(static_cast<uint8_t>(s[1])) << 8)
             | (static_cast<uint32_t>(static_cast<uint8_t>(s[2])) << 16)
             | (static_cast<uint32_t>(static_cast<uint8_t>(s[3])) << 24);
    };
    STELLO_CHECK_EQ(le32("Auth"), 0x6874'7541U);
    STELLO_CHECK_EQ(le32("enti"), 0x6974'6E65U);
    STELLO_CHECK_EQ(le32("cAMD"), 0x444D'4163U);
    STELLO_CHECK_EQ(le32("Hygo"), 0x6F67'7948U);
    STELLO_CHECK_EQ(le32("nGen"), 0x6E65'476EU);
    STELLO_CHECK_EQ(le32("uine"), 0x656E'6975U);

    // Portable toggle works deterministically and restores auto detection.
    cpu::set_portable_only(true);
    STELLO_CHECK(!cpu::fast_pext());
    cpu::set_portable_only(false);
    STELLO_CHECK(cpu::fast_pext()); // Ryzen 7 7800X3D (Zen 4) supports fast BMI2 pext.
}

STELLO_TEST(endgame_bits_and_symmetry_unit) {
    const uint64_t own = (uint64_t{1} << 40) - 1ULL;
    STELLO_CHECK_EQ(endgame::bits::score_with_empties(own, 4), 24);
    STELLO_CHECK_EQ(endgame::bits::score_with_odd_empties((uint64_t{1} << 30) - 1ULL, 3), -4);
    STELLO_CHECK_EQ(endgame::bits::score_with_empties((uint64_t{1} << 30) - 1ULL, 4), 0);
    STELLO_CHECK_EQ(endgame::bits::ALL_NEIGHBOURS[0], 0x302ULL);
    STELLO_CHECK_EQ(endgame::bits::ALL_NEIGHBOURS[9], 0x70507ULL);
    STELLO_CHECK_EQ(endgame::bits::RAYS[0][0], 0xFEULL);
    STELLO_CHECK_EQ(endgame::bits::RAYS[0][4], 0ULL);
    STELLO_CHECK_EQ(endgame::bits::RAYS[63][7], 0x0040'2010'0804'0201ULL);

    STELLO_CHECK_EQ(eval::symmetry::map_square(1, 1), 6u);
    STELLO_CHECK_EQ(eval::symmetry::map_square(1, 2), 57u);
    STELLO_CHECK_EQ(eval::symmetry::map_square(1, 4), 8u);
    for (size_t s = 0; s < eval::symmetry::COUNT; ++s) {
        uint64_t seen = 0;
        for (uint32_t sq = 0; sq < 64; ++sq) {
            seen |= uint64_t{1} << eval::symmetry::map_square(sq, s);
        }
        STELLO_CHECK_EQ(seen, ~uint64_t{0});
    }
}

STELLO_TEST(patterns_fast_and_direct_match_slow) {
    using namespace eval;
    STELLO_CHECK_EQ(PatternSet::classic11().feature_count(), size_t{46});
    STELLO_CHECK_EQ(PatternSet::plus15().feature_count(), size_t{62});
    STELLO_CHECK_EQ(PatternSet::egaroucid16().feature_count(), size_t{64});
    STELLO_CHECK(PatternSet::by_name("plus15") == &PatternSet::plus15());
    STELLO_CHECK(PatternSet::by_name("plus16") == nullptr);

    const std::array<const PatternSet*, 3> sets = {
        &PatternSet::classic11(),
        &PatternSet::plus15(),
        &PatternSet::egaroucid16(),
    };

    for (const auto* set : sets) {
        for (const auto& shape : set->shapes()) {
            const auto fold = shape.fold();
            for (uint32_t f : fold) {
                STELLO_CHECK_EQ(fold[f], f);
            }
        }
    }

    uint64_t state = 0x9E37'79B9'7F4A'7C15ULL;
    auto next = [&]() noexcept {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    };

    for (const auto* set : sets) {
        std::vector<uint32_t> indices(set->feature_count());
        std::vector<int16_t> weights(set->raw_size());
        for (size_t i = 0; i < weights.size(); ++i) {
            weights[i] = static_cast<int16_t>(static_cast<uint16_t>((i * 2'654'435'761ULL) >> 7));
        }

        for (int round = 0; round < 2000; ++round) {
            const uint64_t a = next();
            const uint64_t b = next();
            uint64_t occupied = (round % 3 == 0) ? (a | b) : (round % 3 == 1 ? a : (a & b));
            const uint64_t own = occupied & next();
            const uint64_t opp = occupied & ~own;

            set->indices(own, opp, indices);
            int32_t expected_sum = 0;
            for (size_t feat = 0; feat < set->feature_count(); ++feat) {
                const auto& shape = set->shape_of(feat);
                const size_t sym = set->symmetry_of(feat);
                const uint32_t slow = shape.slow_index(symmetry::apply(own, sym), symmetry::apply(opp, sym));
                size_t shape_idx = 0;
                while (&set->shapes()[shape_idx] != &shape) ++shape_idx;
                STELLO_CHECK_EQ(static_cast<size_t>(indices[feat]), set->offsets()[shape_idx] + slow);
                expected_sum += weights[indices[feat]];
            }

            STELLO_CHECK_EQ(set->sum_weights_portable(own, opp, weights), expected_sum);
            if (cpu::fast_pext()) {
                STELLO_CHECK_EQ(set->sum_weights_pext(own, opp, weights), expected_sum);
            }
        }
    }
}

STELLO_TEST(eval_weights_and_gzip_errors) {
    using namespace eval;
    const auto& weights = EvalWeights::built_in();
    STELLO_CHECK_EQ(weights.set().name(), std::string("plus15"));
    STELLO_CHECK(weights.stages() >= 1);
    STELLO_CHECK(weights.units_per_disc() >= 1);

    const std::array<uint8_t, 8> not_gz = {'n', 'o', 't', ' ', 'g', 'z', 'i', 'p'};
    EvalError err;
    STELLO_CHECK(!EvalWeights::read(not_gz, &err).has_value());
    STELLO_CHECK(err.kind == EvalErrorKind::Gzip);

    const auto built = EvalWeights::built_in_file();
    STELLO_CHECK(!EvalWeights::read(built.subspan(0, built.size() / 2), &err).has_value());

    // Multi-member stored-block gzip stream test ("hello" + "world")
    std::vector<uint8_t> two_members = {
        0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
        0x01, 0x05, 0x00, 0xfa, 0xff, 'h', 'e', 'l', 'l', 'o',
        0x86, 0xa6, 0x10, 0x36, 0x05, 0x00, 0x00, 0x00,
        0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
        0x01, 0x05, 0x00, 0xfa, 0xff, 'w', 'o', 'r', 'l', 'd',
        0x43, 0x11, 0x77, 0x3a, 0x05, 0x00, 0x00, 0x00,
    };
    const auto dec = gzip::decompress(two_members);
    STELLO_CHECK_MSG(dec.ok(), dec.error);
    const std::string text(dec.data.begin(), dec.data.end());
    STELLO_CHECK_EQ(text, std::string("helloworld"));
    two_members.back() ^= 1U;
    STELLO_CHECK(!gzip::decompress(two_members).ok());

    const auto stages = EvalWeights::even_stages(10);
    STELLO_CHECK_EQ(stages[0], uint8_t{0});
    STELLO_CHECK_EQ(stages[4], uint8_t{0});
    STELLO_CHECK_EQ(stages[64], uint8_t{9});
    for (size_t i = 1; i < stages.size(); ++i) {
        STELLO_CHECK(stages[i - 1] <= stages[i]);
    }
}

STELLO_TEST(parity_positions_auto_and_portable) {
    const auto& weights = eval::EvalWeights::built_in();
    const auto lines = read_data_lines("positions.txt");
    STELLO_CHECK(lines.size() > 5000);

    for (const bool portable : {false, true}) {
        PortableGuard guard(portable);
        STELLO_CHECK_EQ(cpu::fast_pext(), !portable);

        std::vector<std::pair<Fixture, std::vector<Board>>> fixtures;
        fixtures.reserve(lines.size());
        size_t moves_checked = 0, passes = 0, finished = 0;

        for (const auto& line : lines) {
            const auto fields = split_char(line, ' ');
            const auto pos = parse_fixture(fields);
            const std::string& id = pos.id;
            const Board& board = pos.board;
            const Player player = pos.player;
            const uint64_t own = pos.own();
            const uint64_t opp = pos.opp();
            const uint64_t empty = board.empty();

            const uint64_t legal = parse_hex(fields[4]);
            STELLO_CHECK_EQ_MSG(bits::legal_moves(own, opp), legal, id);
            STELLO_CHECK_EQ_MSG(endgame::bits::legal_moves(own, opp), legal, id);
            STELLO_CHECK_EQ_MSG(board.legal_moves(player), legal, id);
            STELLO_CHECK_EQ_MSG(board.legal_moves(opponent(player)), parse_hex(fields[5]), id);
            STELLO_CHECK_EQ_MSG(bits::potential_mobility(own, opp), parse_i32(fields[6]), id);
            STELLO_CHECK_EQ_MSG(endgame::bits::potential_mobility(own, empty), parse_i32(fields[7]), id);
            STELLO_CHECK_EQ_MSG(endgame::bits::potential_mobility(opp, empty), parse_i32(fields[8]), id);
            STELLO_CHECK_EQ_MSG(endgame::bits::lone_empties(own, opp), parse_hex(fields[9]), id);
            STELLO_CHECK_EQ_MSG(endgame::bits::parity(empty), parse_i32(fields[10]), id);
            STELLO_CHECK_EQ_MSG(weights.stage_of(static_cast<uint32_t>(std::popcount(own | opp))), static_cast<size_t>(parse_i32(fields[11])), id);
            STELLO_CHECK_EQ_MSG(weights.evaluate(own, opp), parse_i32(fields[12]), id);
            STELLO_CHECK_EQ_MSG(eval::pattern_score(weights, own, opp), parse_i32(fields[13]), id);
            STELLO_CHECK_EQ_MSG(eval::disc_score(weights, own, opp), parse_i32(fields[14]), id);

            std::vector<Square> squares;
            for (Square s : Square::in_mask(legal)) squares.push_back(s);
            const size_t move_count = fields.size() - 15;
            STELLO_CHECK_EQ_MSG(move_count, squares.size(), id);

            std::vector<Board> children;
            children.reserve(squares.size());
            for (size_t m = 0; m < squares.size(); ++m) {
                const auto parts = split_char(fields[15 + m], ':');
                const Square sq = squares[m];
                STELLO_CHECK_EQ_MSG(parse_i32(parts[0]), static_cast<int32_t>(sq.index()), id);
                const uint64_t expected_flips = parse_hex(parts[1]);
                STELLO_CHECK_EQ_MSG(bits::flips(own, opp, sq.index()), expected_flips, id);
                STELLO_CHECK_EQ_MSG(endgame::bits::flips(own, opp, sq.index()), expected_flips, id);
                STELLO_CHECK_EQ_MSG(board.flips(player, sq), expected_flips, id);
                STELLO_CHECK(expected_flips != 0);

                const uint64_t placed = expected_flips | sq.bit();
                const Board expected = player == Player::Black
                    ? Board(board.black() | placed, board.white() & ~expected_flips)
                    : Board(board.black() & ~expected_flips, board.white() | placed);
                const auto child = board.play(player, sq);
                STELLO_CHECK(child.has_value());
                STELLO_CHECK(*child == expected);
                STELLO_CHECK(board.play_unchecked(player, sq.index()) == expected);
                STELLO_CHECK_EQ_MSG(weights.evaluate(child->discs(opponent(player)), child->discs(player)), parse_i32(parts[2]), id);
                children.push_back(*child);
                ++moves_checked;
            }

            // Only run exhaustive illegal-move check once to keep sanitized run crisp.
            if (!portable) {
                for (Square sq : Square::in_mask(empty & ~legal)) {
                    STELLO_CHECK_EQ(board.flips(player, sq), uint64_t{0});
                    STELLO_CHECK(!board.play(player, sq).has_value());
                }
            }

            const bool game_over = (legal == 0 && parse_hex(fields[5]) == 0);
            STELLO_CHECK_EQ(board.is_game_over(), game_over);
            if (game_over) ++finished;
            else if (legal == 0) ++passes;

            fixtures.emplace_back(pos, std::move(children));
        }

        size_t successions = 0;
        for (size_t i = 1; i < fixtures.size(); ++i) {
            const auto& [before, children] = fixtures[i - 1];
            const auto& [after, _unused] = fixtures[i];
            const auto d1 = before.id.rfind('.');
            const auto d2 = after.id.rfind('.');
            if (d1 == std::string::npos || d2 == std::string::npos) continue;
            if (before.id.substr(0, d1) != after.id.substr(0, d2)) continue;
            if (parse_i32(after.id.substr(d2 + 1)) != parse_i32(before.id.substr(d1 + 1)) + 1) continue;
            STELLO_CHECK(after.player == opponent(before.player));
            if (children.empty()) {
                STELLO_CHECK(after.board == before.board);
            } else {
                STELLO_CHECK(std::find(children.begin(), children.end(), after.board) != children.end());
            }
            ++successions;
        }

        STELLO_CHECK(passes >= 20 && finished >= 100);
        STELLO_CHECK(moves_checked > 30'000 && successions > 5000);
    }
}

STELLO_TEST(parity_pattern_sets) {
    std::vector<Fixture> positions;
    for (const auto& line : read_data_lines("positions.txt")) {
        positions.push_back(parse_fixture(split_char(line, ' ')));
    }

    const auto lines = read_data_lines("patterns.txt");
    size_t checked_sets = 0;
    size_t i = 0;
    while (i < lines.size()) {
        const auto fields = split_char(lines[i++], ' ');
        if (fields[0] == "set") {
            const auto* set = eval::PatternSet::by_name(fields[1]);
            STELLO_CHECK(set != nullptr);
            STELLO_CHECK_EQ(set->feature_count(), static_cast<size_t>(parse_i32(fields[2])));
            STELLO_CHECK_EQ(set->raw_size(), static_cast<size_t>(parse_i32(fields[3])));

            const auto sym_strs = split_char(fields[4], ',');
            STELLO_CHECK_EQ(sym_strs.size(), set->feature_count());
            for (size_t f = 0; f < set->feature_count(); ++f) {
                STELLO_CHECK_EQ(set->symmetry_of(f), static_cast<size_t>(parse_i32(sym_strs[f])));
            }

            uint64_t hash = kFnvOffset;
            std::vector<uint32_t> indices(set->feature_count());
            for (const auto& pos : positions) {
                set->indices(pos.own(), pos.opp(), indices);
                for (uint32_t index : indices) {
                    hash = fnv32(hash, static_cast<int32_t>(index));
                }
            }
            STELLO_CHECK_EQ_MSG(hash, parse_hex(fields[5]), fields[1]);

            for (const auto& shape : set->shapes()) {
                const auto sf = split_char(lines[i++], ' ');
                STELLO_CHECK_EQ(sf[0], std::string("shape"));
                STELLO_CHECK_EQ(shape.name(), sf[1]);
                STELLO_CHECK_EQ(shape.size(), static_cast<size_t>(parse_i32(sf[2])));
                const auto pl_strs = split_char(sf[3], ',');
                STELLO_CHECK_EQ(pl_strs.size(), shape.placements().size());
                for (size_t p = 0; p < pl_strs.size(); ++p) {
                    STELLO_CHECK_EQ(shape.placements()[p], static_cast<size_t>(parse_i32(pl_strs[p])));
                }
                STELLO_CHECK_EQ(shape.representatives().size(), static_cast<size_t>(parse_i32(sf[4])));

                uint64_t fold_hash = kFnvOffset;
                for (uint32_t f : shape.fold()) {
                    fold_hash = fnv32(fold_hash, static_cast<int32_t>(f));
                }
                STELLO_CHECK_EQ_MSG(fold_hash, parse_hex(sf[5]), shape.name());
            }
            ++checked_sets;
        } else if (fields[0] == "weights") {
            const auto& weights = eval::EvalWeights::built_in();
            STELLO_CHECK_EQ(weights.set().name(), fields[1]);
            STELLO_CHECK_EQ(weights.stages(), static_cast<size_t>(parse_i32(fields[2])));
            STELLO_CHECK_EQ(weights.units_per_disc(), parse_i32(fields[3]));
            const auto stage_strs = split_char(fields[4], ',');
            STELLO_CHECK_EQ(stage_strs.size(), size_t{65});
            for (uint32_t d = 0; d <= 64; ++d) {
                STELLO_CHECK_EQ(weights.stage_of(d), static_cast<size_t>(parse_i32(stage_strs[d])));
            }
            uint64_t whash = kFnvOffset;
            for (size_t st = 0; st < weights.stages(); ++st) {
                for (int16_t w : weights.stage_weights(st)) {
                    whash = fnv16(whash, w);
                }
            }
            STELLO_CHECK_EQ(whash, parse_hex(fields[5]));
        } else {
            throw TestFailure("unknown patterns.txt line: " + fields[0]);
        }
    }
    STELLO_CHECK_EQ(checked_sets, size_t{3});
}

STELLO_TEST(parity_perft) {
    const std::array<uint64_t, 8> expected = {4ULL, 12ULL, 56ULL, 244ULL, 1396ULL, 8200ULL, 55092ULL, 390216ULL};
    for (uint32_t d = 1; d <= 8; ++d) {
        STELLO_CHECK_EQ_MSG(stello::perft(Board::initial(), Player::Black, d), expected[d - 1], std::to_string(d));
    }

    const auto lines = read_data_lines("perft.txt");
    STELLO_CHECK(lines.size() >= 8);
    for (const auto& line : lines) {
        const auto fields = split_char(line, ' ');
        const auto pos = parse_fixture(fields);
        const uint32_t depth = static_cast<uint32_t>(parse_i32(fields[4]));
        const uint64_t leaves = parse_u64(fields[5]);
        STELLO_CHECK_EQ_MSG(stello::perft(pos.board, pos.player, depth), leaves, pos.id);
    }
}

} // namespace stello::tests
