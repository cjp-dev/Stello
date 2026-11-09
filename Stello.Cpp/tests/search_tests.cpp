#include "tests/test_harness.hpp"

#include "cli/bench.hpp"
#include "core/bits.hpp"
#include "core/board.hpp"
#include "core/square.hpp"
#include "endgame/bits.hpp"
#include "endgame/local.hpp"
#include "endgame/stable.hpp"
#include "eval/weights.hpp"
#include "search/abort.hpp"
#include "search/engine.hpp"
#include "search/limits.hpp"
#include "search/options.hpp"
#include "search/ordering.hpp"
#include "search/table.hpp"
#include "search/time.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace stello::tests {

namespace {

std::string read_repo_text(const std::filesystem::path& rel) {
    const auto full = repo_root() / rel;
    std::ifstream file(full, std::ios::binary);
    STELLO_CHECK_MSG(static_cast<bool>(file), full.string());
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        out.push_back(std::move(line));
    }
    return out;
}

std::vector<std::string> split_char(std::string_view sv, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        const size_t pos = sv.find(sep, start);
        if (pos == std::string_view::npos) {
            out.emplace_back(sv.substr(start));
            break;
        }
        out.emplace_back(sv.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

std::vector<cli::BenchPosition> load_bench_file(const char* name) {
    std::vector<cli::BenchPosition> positions;
    std::string err;
    const auto path = repo_root() / "Stello.Net" / "tools" / "Stello.Benchmark" / "Data" / name;
    const bool ok = cli::load_positions(path, positions, err);
    STELLO_CHECK_MSG(ok, err);
    return positions;
}

std::pair<Board, Player> find_bench_pos(const char* file, std::string_view id) {
    const auto all = load_bench_file(file);
    for (const auto& p : all) {
        if (p.id == id) {
            return {p.board, p.player};
        }
    }
    STELLO_CHECK_MSG(false, std::string(file) + " missing " + std::string(id));
    return {Board::initial(), Player::Black};
}

int32_t exact_minimax(const Board& board, Player player, bool passed) {
    const uint64_t moves = board.legal_moves(player);
    if (moves == 0) {
        if (passed) {
            return search::final_score(board.discs(player), board.discs(opponent(player)));
        }
        return -exact_minimax(board, opponent(player), true);
    }

    int32_t best = -(std::numeric_limits<int32_t>::max)();
    for (const Square sq : Square::in_mask(moves)) {
        const int32_t s = -exact_minimax(board.play_unchecked(player, sq.index()), opponent(player), false);
        if (s > best) {
            best = s;
        }
    }
    return best;
}

int32_t exact_bits_minimax(uint64_t own, uint64_t opp, bool passed) {
    uint64_t moves = bits::legal_moves(own, opp);
    if (moves == 0) {
        if (passed) {
            return search::final_score(own, opp);
        }
        return -exact_bits_minimax(opp, own, true);
    }
    int32_t best = -(std::numeric_limits<int32_t>::max)();
    while (moves != 0) {
        const uint32_t sq = static_cast<uint32_t>(std::countr_zero(moves));
        moves &= (moves - 1ULL);
        const uint64_t f = bits::flips(own, opp, sq);
        const int32_t s = -exact_bits_minimax(opp & ~f, own | f | (uint64_t{1} << sq), false);
        if (s > best) {
            best = s;
        }
    }
    return best;
}

} // namespace

// ---- TranspositionTable unit tests ----

STELLO_TEST(table_keeps_deeper_entry_and_replaces_other) {
    search::TranspositionTable table(10);
    STELLO_CHECK_EQ(reinterpret_cast<uintptr_t>(table.data()) & 63ULL, uint64_t{0});

    table.store(1, 2, 0, 5, search::Bound::Exact, 100, 7);
    const auto hit = table.get(1, 2, 0);
    STELLO_CHECK(hit.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(hit->value), 100);
    STELLO_CHECK_EQ(static_cast<int32_t>(hit->depth), 5);
    STELLO_CHECK_EQ(static_cast<int32_t>(hit->mv), 7);
    STELLO_CHECK(!table.get(1, 2, 1).has_value());

    table.store(1, 2, 0, 3, search::Bound::Lower, 50, 8);
    const auto still_deep = table.get(1, 2, 0);
    STELLO_CHECK(still_deep.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(still_deep->value), 100);

    table.clear();
    STELLO_CHECK(!table.get(1, 2, 0).has_value());
}

STELLO_TEST(table_entries_before_clear_are_empty_for_replacement) {
    search::TranspositionTable table(10);
    table.store(1, 2, 0, 9, search::Bound::Exact, 100, 7);
    table.clear();
    table.store(1, 2, 0, 3, search::Bound::Lower, 50, 8);
    const auto hit = table.get(1, 2, 0);
    STELLO_CHECK(hit.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(hit->value), 50);
    STELLO_CHECK_EQ(static_cast<int32_t>(hit->depth), 3);
    STELLO_CHECK_EQ(static_cast<int32_t>(hit->mv), 8);

    for (int i = 0; i < 70'000; ++i) {
        table.clear();
        STELLO_CHECK(!table.get(1, 2, 0).has_value());
        table.store(1, 2, 0, 3, search::Bound::Lower, 50, 8);
    }
    STELLO_CHECK(table.get(1, 2, 0).has_value());
}

STELLO_TEST(table_entries_from_older_root_are_ignored_and_replaced) {
    search::TranspositionTable table(10);
    const uint64_t old_own = 1;
    const uint64_t old_opp = 2;
    const size_t target_slot = table.slot_index(old_own, old_opp, 0);

    std::vector<uint64_t> others;
    for (uint64_t seed = 1; seed <= 1'000'000 && others.size() < 2; ++seed) {
        const uint64_t cand = seed * 0x9E37'79B9'7F4A'7C15ULL;
        if (std::popcount(cand) >= 4 && table.slot_index(cand, 0, 0) == target_slot) {
            others.push_back(cand);
        }
    }
    STELLO_CHECK_EQ(others.size(), size_t{2});

    table.store_at_root(old_own, old_opp, 0, 9, search::Bound::Exact, 100, 7, 64);
    table.store_at_root(others[0], 0, 0, 2, search::Bound::Lower, 40, 6, 64);
    STELLO_CHECK(table.get_at_root(old_own, old_opp, 0, 64).has_value());
    STELLO_CHECK(!table.get_at_root(old_own, old_opp, 0, 60).has_value());

    table.store_at_root(others[1], 0, 0, 1, search::Bound::Lower, 50, 8, 60);
    const auto hit1 = table.get_at_root(others[1], 0, 0, 60);
    const auto hit0 = table.get_at_root(others[0], 0, 0, 60);
    STELLO_CHECK(hit1.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(hit1->depth), 1);
    STELLO_CHECK(hit0.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(hit0->depth), 2);
    STELLO_CHECK(!table.get_at_root(old_own, old_opp, 0, 64).has_value());
}

STELLO_TEST(table_packing_keeps_every_field) {
    struct Case {
        int16_t value;
        int8_t depth;
        search::Bound bound;
        int8_t mv;
        uint8_t tag;
    };
    const std::array<Case, 4> cases = {{
        {-32768, -128, search::Bound::Upper, -1, 255},
        {32767, 127, search::Bound::Exact, 63, 0},
        {-1, 0, search::Bound::Lower, 0, 1},
        {0, 60, search::Bound::None, -1, 0},
    }};
    for (const auto& c : cases) {
        search::Entry e;
        e.own = 0x8000'0000'0000'0001ULL;
        e.opponent = 42;
        e.value = c.value;
        e.depth = c.depth;
        e.bound = c.bound;
        e.mv = c.mv;
        e.tag = c.tag;
        e.generation = 65535;
        const auto unpacked = search::Entry::unpack(e.own, e.opponent, e.pack());
        STELLO_CHECK(unpacked == e);
    }
    STELLO_CHECK_EQ(search::Entry{}.pack(), uint64_t{0});
}

// ---- MoveOrdering & Time unit tests ----

STELLO_TEST(ordering_sort_and_responses) {
    std::array<int32_t, 5> moves = {1, 2, 3, 4, 5};
    std::array<int32_t, 5> keys = {0, 5, 0, 5, 7};
    search::sort_descending(moves, keys);
    STELLO_CHECK((moves == std::array<int32_t, 5>{5, 2, 4, 1, 3}));
    STELLO_CHECK((keys == std::array<int32_t, 5>{7, 5, 5, 0, 0}));

    search::MoveOrdering ordering;
    const Board board = Board::initial();
    ordering.record_response(Player::White, 19, 34, false);
    ordering.record_response(Player::White, 19, 20, true);
    std::array<int32_t, 4> o_moves = {18, 20, 34, 44};
    ordering.order(o_moves, 44, 19, Player::White, board);
    STELLO_CHECK((o_moves == std::array<int32_t, 4>{44, 20, 34, 18}));
}

STELLO_TEST(time_budget_and_for_solver_tests) {
    const auto b_per_move = time::for_midgame(SearchLimits::time_per_move_ms(900), 40);
    STELLO_CHECK(b_per_move == (time::TimeBudget{600, 900}));
    STELLO_CHECK(b_per_move.with_soft_percent(50, 0) == (time::TimeBudget{450, 900}));
    STELLO_CHECK(time::TimeBudget::unlimited().with_soft_percent(50, 0) == time::TimeBudget::unlimited());
    STELLO_CHECK(time::for_midgame(SearchLimits::fixed_depth(5), 40) == time::TimeBudget::unlimited());

    STELLO_CHECK(time::for_midgame(SearchLimits::time_per_game_ms(60'000), 40) == (time::TimeBudget{1414, 2122}));
    STELLO_CHECK(time::for_midgame(SearchLimits::time_per_game_ms(-5), 6) == (time::TimeBudget{6, 9}));
    const time::TimeBudget mid{1, 2};
    STELLO_CHECK(time::for_endgame(SearchLimits::time_per_move_ms(100), mid, 50) == mid);
    STELLO_CHECK(time::for_endgame(SearchLimits::time_per_game_ms(10'000), mid, 1'000) == (time::TimeBudget{4000, 5500}));

    STELLO_CHECK(
        time::for_solver(SearchLimits::time_per_game_ms(10'000), time::TimeBudget::unlimited(), 1'000, std::nullopt)
        == (time::TimeBudget{4000, 5500}));
    STELLO_CHECK(
        time::for_solver(SearchLimits::time_per_game_ms(10'000), time::TimeBudget::unlimited(), 1'000, 1)
        == (time::TimeBudget{1045, 5500}));
    STELLO_CHECK(
        time::for_solver(SearchLimits::time_per_game_ms(10'000), time::TimeBudget::unlimited(), 1'000, 50)
        == (time::TimeBudget{3250, 5500}));
    STELLO_CHECK(
        time::for_solver(SearchLimits::time_per_game_ms(10'000), time::TimeBudget::unlimited(), 1'000, 100)
        == (time::TimeBudget{5500, 5500}));

    const auto pm = SearchLimits::time_per_move_ms(900);
    const auto pm_b = time::for_midgame(pm, 40);
    STELLO_CHECK(time::for_solver(pm, pm_b, 300, std::nullopt) == pm_b);
    STELLO_CHECK(time::for_solver(pm, pm_b, 300, 50) == (time::TimeBudget{450, 900}));
    STELLO_CHECK(time::for_solver(SearchLimits::fixed_depth(5), time::TimeBudget::unlimited(), 300, 50) == time::TimeBudget::unlimited());
    STELLO_CHECK(time::for_solver(SearchLimits::solve(), time::TimeBudget::unlimited(), 300, 50) == time::TimeBudget::unlimited());
}

// ---- Stable & Last4..1 unit tests ----

STELLO_TEST(stable_discs_unit_test) {
    STELLO_CHECK_EQ(endgame::stable::of(1, 0), uint64_t{1});
    STELLO_CHECK_EQ(endgame::stable::of(2, 0), uint64_t{0});
    STELLO_CHECK_EQ(endgame::stable::of(0xFFULL, 0), uint64_t{0xFFULL});
    STELLO_CHECK_EQ(endgame::stable::of(0x0000'0008'1000'0000ULL, 0x0000'0010'0800'0000ULL), uint64_t{0});
    STELLO_CHECK_EQ(
        endgame::stable::of(0x5555'5555'5555'5555ULL, 0xAAAA'AAAA'AAAA'AAAAULL),
        uint64_t{0x5555'5555'5555'5555ULL});
}

STELLO_TEST(last_flip_counts_equal_flips_and_abort_paths) {
    uint64_t state = 0x9E37'79B9'7F4A'7C15ULL;
    auto next = [&]() noexcept {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    };

    for (int round = 0; round < 2'000; ++round) {
        uint64_t own = 0;
        switch (round % 4) {
            case 0: own = next(); break;
            case 1: own = next() & next(); break;
            case 2: own = next() | next(); break;
            default: own = next() & next() & next(); break;
        }
        for (uint32_t sq = 0; sq < 64; ++sq) {
            const uint64_t bit = uint64_t{1} << sq;
            const int32_t expected = static_cast<int32_t>(std::popcount(endgame::bits::flips(own & ~bit, ~own & ~bit, sq)));
            STELLO_CHECK_EQ(search::detail::last_flip_count_for_test(own, sq), expected);
        }
    }

    search::SearchEngine engine(10);
    search::SearchSignals stop;
    stop.cancel();
    search::Searcher searcher = engine.searcher({}, stop, 4);

    auto check_cancelled = [&](auto fn) {
#if STELLO_ABORT_EXCEPTIONS
        bool caught = false;
        try {
            (void)fn();
        } catch (search::Abort a) {
            caught = (a == search::Abort::Cancelled);
        }
        STELLO_CHECK(caught);
#else
        const auto res = fn();
        STELLO_CHECK(res.is_err() && res.abort_reason() == search::Abort::Cancelled);
#endif
    };

    searcher.nodes = 1022;
    searcher.next_abort_check = 1024;
    check_cancelled([&]() { return searcher.last4(0, ~uint64_t{0b1111}, 0); });

    searcher.nodes = 1022;
    searcher.next_abort_check = 1024;
    check_cancelled([&]() { return searcher.last3(0, ~uint64_t{0b111}, 0, 0, 1, 2, 0b111); });

    searcher.nodes = 1022;
    searcher.next_abort_check = 1024;
    check_cancelled([&]() { return searcher.last2(0, ~uint64_t{0b11}, 0, 0, 1); });

    searcher.nodes = 1022;
    searcher.next_abort_check = 1024;
    check_cancelled([&]() { return searcher.last1(0, 0, 0); });
}

// ---- Endgame search unit tests ----

STELLO_TEST(endgame_shallow_and_unit_regressions) {
    search::SearchEngine engine(10);
    search::Searcher searcher = engine.searcher({}, search::no_stop(), 64);

    uint64_t seed = 0x2545'F491'4F6C'DD1DULL;
    int checked = 0;
    for (int game = 0; game < 300; ++game) {
        Board board = Board::initial();
        Player player = Player::Black;
        while (board.empty_count() > 6 && !board.is_game_over()) {
            const uint64_t moves = board.legal_moves(player);
            if (moves != 0) {
                seed ^= seed << 13;
                seed ^= seed >> 7;
                seed ^= seed << 17;
                uint32_t pick = static_cast<uint32_t>(seed % static_cast<uint64_t>(std::popcount(moves)));
                uint64_t rest = moves;
                while (pick > 0) {
                    rest &= (rest - 1ULL);
                    --pick;
                }
                board = board.play_unchecked(player, static_cast<uint32_t>(std::countr_zero(rest)));
            }
            player = opponent(player);
        }

        const uint64_t own = board.discs(player);
        const uint64_t opp = board.discs(opponent(player));
        const int32_t empties = static_cast<int32_t>(board.empty_count());
        if (empties == 0) {
            continue;
        }

        const int32_t score = exact_bits_minimax(own, opp, false);
        const auto unwrap = [](auto res) {
#if STELLO_ABORT_EXCEPTIONS
            return res;
#else
            STELLO_CHECK(res.is_ok());
            return res.value;
#endif
        };
        STELLO_CHECK_EQ(unwrap(searcher.solve_shallow(own, opp, -64, 64, empties, false)), score);
        for (const int32_t alpha : {-10, -1, 0, 4}) {
            const int32_t b = unwrap(searcher.solve_shallow(own, opp, alpha, alpha + 1, empties, false));
            STELLO_CHECK_EQ(b > alpha, score > alpha);
        }
        ++checked;
    }
    STELLO_CHECK(checked > 200);
    STELLO_CHECK(searcher.nodes > 0 && searcher.evaluations > 0);

    // ordering_value terminal evaluations & abort check paths
    {
        search::SearchEngine e2(10);
        search::Searcher s2 = e2.searcher({}, search::no_stop(), 64);
#if STELLO_ABORT_EXCEPTIONS
        (void)s2.ordering_value(0, ~uint64_t{0}, 0, -64, 64);
        STELLO_CHECK_EQ(s2.nodes, uint64_t{1});
        STELLO_CHECK_EQ(s2.evaluations, uint64_t{1});
        STELLO_CHECK_EQ(s2.ordering_value(0, ~uint64_t{0}, 1, -64, 64), search::final_score(0, ~uint64_t{0}));
        STELLO_CHECK_EQ(s2.nodes, uint64_t{2});
        STELLO_CHECK_EQ(s2.evaluations, uint64_t{2});
#else
        STELLO_CHECK(s2.ordering_value(0, ~uint64_t{0}, 0, -64, 64).is_ok());
        STELLO_CHECK_EQ(s2.nodes, uint64_t{1});
        STELLO_CHECK_EQ(s2.evaluations, uint64_t{1});
        const auto ov1 = s2.ordering_value(0, ~uint64_t{0}, 1, -64, 64);
        STELLO_CHECK(ov1.is_ok() && ov1.value == search::final_score(0, ~uint64_t{0}));
        STELLO_CHECK_EQ(s2.nodes, uint64_t{2});
        STELLO_CHECK_EQ(s2.evaluations, uint64_t{2});
#endif
    }

    // undone_first_keeps_decided_moves_out_of_the_sort
    {
        std::array<search::EndMove, 4> moves = {{
            {1, 0, -5, 0},
            {2, 0, 100, 0},
            {3, 0, 5, 0},
            {4, 0, 200, 0},
        }};
        const size_t order_count = search::detail::undone_first_for_test(moves, (uint64_t{1} << 2) | (uint64_t{1} << 4));
        STELLO_CHECK_EQ(order_count, size_t{2});
        STELLO_CHECK_EQ(moves[0].square, uint32_t{1});
        STELLO_CHECK_EQ(moves[1].square, uint32_t{3});
        for (size_t i = 0; i < order_count; ++i) {
            search::detail::swap_next_best_for_test(std::span<search::EndMove>(moves.data(), order_count), i);
        }
        STELLO_CHECK_EQ(moves[0].square, uint32_t{3});
        STELLO_CHECK_EQ(moves[1].square, uint32_t{1});
    }

    // end_deep_stores_the_move_with_the_best_etc_fail_low_value
    {
        uint64_t s = 0x2545'F491'4F6C'DD1DULL;
        Board board;
        Player player = Player::Black;
        uint64_t legal = 0;
        for (;;) {
            board = Board::initial();
            player = Player::Black;
            while (board.empty_count() > 14) {
                uint64_t mv = board.legal_moves(player);
                if (mv == 0) {
                    player = opponent(player);
                    mv = board.legal_moves(player);
                }
                if (mv == 0) {
                    break;
                }
                s ^= s << 13;
                s ^= s >> 7;
                s ^= s << 17;
                uint32_t pick = static_cast<uint32_t>(s % static_cast<uint64_t>(std::popcount(mv)));
                while (pick > 0) {
                    mv &= (mv - 1ULL);
                    --pick;
                }
                board = board.play_unchecked(player, static_cast<uint32_t>(std::countr_zero(mv)));
                player = opponent(player);
            }
            if (board.empty_count() != 14) {
                continue;
            }
            legal = board.legal_moves(player);
            if (legal == 0) {
                player = opponent(player);
                legal = board.legal_moves(player);
            }
            if (legal == 0) {
                continue;
            }
            const size_t count = static_cast<size_t>(std::popcount(legal));
            std::array<search::EndMove, 64> buf{};
            if (!search::detail::fill_end_moves_for_test(
                    board.discs(player), board.discs(opponent(player)), legal, std::span<search::EndMove>(buf.data(), count)))
            {
                break;
            }
        }

        search::SearchEngine e3(12);
        const uint32_t root_empties = board.empty_count();
        const uint64_t own = board.discs(player);
        const uint64_t opp = board.discs(opponent(player));
        search::Searcher s3 = e3.searcher({}, search::no_stop(), root_empties);
        int32_t best_move = -1;
        int32_t best_value = -search::END_INFINITY;
        int32_t index = 0;
        uint64_t remaining = legal;
        while (remaining != 0) {
            const uint32_t sq = static_cast<uint32_t>(std::countr_zero(remaining));
            remaining &= (remaining - 1ULL);
            const uint64_t f = bits::flips(own, opp, sq);
            const uint64_t child_own = opp ^ f;
            const uint64_t child_opp = own ^ f ^ (uint64_t{1} << sq);
            const int32_t val = 10 + index;
            if (val > best_value) {
                best_value = val;
                best_move = static_cast<int32_t>(sq);
            }
            s3.endgame_table->store_at_root(
                child_own, child_opp, 0, static_cast<int32_t>(root_empties) - 1,
                search::Bound::Exact, -val, -1, root_empties);
            ++index;
        }
#if STELLO_ABORT_EXCEPTIONS
        STELLO_CHECK_EQ(s3.end_deep(own, opp, 63, 14, true, legal), best_value);
#else
        const auto deep_res = s3.end_deep(own, opp, 63, 14, true, legal);
        STELLO_CHECK(deep_res.is_ok() && deep_res.value == best_value);
#endif
        const auto hit = s3.endgame_table->get_at_root(own, opp, 0, root_empties);
        STELLO_CHECK(hit.has_value());
        STELLO_CHECK_EQ(static_cast<int32_t>(hit->mv), best_move);
    }
}

// ---- Midgame SearchEngine unit regressions ----

STELLO_TEST(midgame_hash_cutoffs_and_model_unit_tests) {
    const int32_t mv = static_cast<int32_t>(std::countr_zero(Board::initial().legal_moves(Player::Black)));
    auto check_cached = [&](search::Bound bound, int32_t value, int32_t depth, int32_t stored_move, bool expected_cut) {
        SearchOptions opts;
        opts.selective = false;
        search::SearchEngine engine(10, opts);
        const Board board = Board::initial();
        const Player player = Player::Black;
        search::Searcher s = engine.searcher({}, search::no_stop(), 64);
        s.midgame_table->store(
            board.discs(player), board.discs(opponent(player)), static_cast<int32_t>(player_index(player)),
            depth, bound, value, stored_move);
        s.hash_get_height = 0;
        s.hash_put_height = 0;
#if STELLO_ABORT_EXCEPTIONS
        const auto res = s.search(board, player, 2, 0, -1, 0, 1);
#else
        const auto boxed = s.search(board, player, 2, 0, -1, 0, 1);
        STELLO_CHECK(boxed.is_ok());
        const auto res = boxed.value;
#endif
        STELLO_CHECK_EQ(s.nodes == 1, expected_cut);
        if (expected_cut && bound == search::Bound::Upper && value == 0 && stored_move == mv && depth == 2) {
            STELLO_CHECK_EQ(res.score, 0);
            STELLO_CHECK_EQ(res.mv, mv);
        }
    };

    check_cached(search::Bound::Upper, 0, 2, mv, true);
    check_cached(search::Bound::Upper, -1, 2, mv, true);
    check_cached(search::Bound::Upper, 1, 2, mv, false);
    check_cached(search::Bound::Upper, 0, 1, mv, false);
    check_cached(search::Bound::Upper, 0, 2, -1, false);
    check_cached(search::Bound::Upper, 0, 2, 0, false);
    check_cached(search::Bound::Lower, 1, 2, mv, true);
    check_cached(search::Bound::Lower, 0, 2, mv, false);
    check_cached(search::Bound::Exact, 0, 2, mv, true);

    // shallow_depth_keeps_the_parity
    const std::array<int32_t, 10> expected_shallow = {1, 0, 1, 2, 3, 2, 3, 4, 5, 4};
    for (int32_t d = 1; d <= 10; ++d) {
        STELLO_CHECK_EQ(search::detail::probcut_shallow_depth(d), expected_shallow[static_cast<size_t>(d - 1)]);
    }

    // errors_follow_the_fitted_model
    const auto errors = search::detail::make_probcut_errors(1.0);
    STELLO_CHECK_EQ((*errors)[30][6], 361);
    for (size_t discs = 0; discs < 65; ++discs) {
        for (size_t d = 1; d <= 60; ++d) {
            STELLO_CHECK((*errors)[discs][d] >= 50);
        }
    }

    // move_to_front
    std::array<int32_t, 4> mtf = {1, 2, 3, 4};
    const std::array<int32_t, 2> imp = {3, 4};
    search::detail::move_to_front(mtf, imp);
    STELLO_CHECK((mtf == std::array<int32_t, 4>{4, 3, 1, 2}));

    // final_scores
    STELLO_CHECK_EQ(search::final_score(0b111ULL, 0b1ULL), 2 + 60);
    STELLO_CHECK_EQ(search::final_score(0b1ULL, 0b111ULL), -2 - 60);
    STELLO_CHECK_EQ(search::game_over_score(0b11ULL, 0b1100ULL), 0);
    STELLO_CHECK_EQ(search::game_over_score(~uint64_t{0}, 0), 64 + search::WIN_SCORE);
}

STELLO_TEST(persistent_engine_ages_full_table_across_game_positions) {
    search::SearchEngine engine(12);
    Board board = Board::initial();
    Player player = Player::Black;
    uint64_t total_nodes = 0;

    for (uint32_t ply = 0; ply < 34; ++ply) {
        uint64_t moves = board.legal_moves(player);
        if (moves == 0) {
            player = opponent(player);
            moves = board.legal_moves(player);
        }
        STELLO_CHECK(moves != 0);
        const SearchResult res = engine.search_simple(board, player, SearchLimits::fixed_depth(12));
        STELLO_CHECK_EQ(res.depth, 12);
        total_nodes += res.nodes;

        const uint32_t steps = (ply * 7U) % static_cast<uint32_t>(std::popcount(moves));
        for (uint32_t k = 0; k < steps; ++k) {
            moves &= (moves - 1ULL);
        }
        board = board.play_unchecked(player, static_cast<uint32_t>(std::countr_zero(moves)));
        player = opponent(player);
    }

    STELLO_CHECK_EQ(total_nodes, uint64_t{2'156'127});
}

// ---- Full 105-signature baseline integration test (Stello 3.1) ----

STELLO_TEST(stello_v3_all_105_signatures_match_baseline) {
    const auto lines = split_lines(read_repo_text("Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt"));
    const auto mid_positions = load_bench_file("midgame.txt");
    const auto ffo_positions = load_bench_file("ffo-40-59.txt");

    search::SearchEngine engine(19);
    size_t mid_count = 0;
    size_t ffo_count = 0;

    for (const auto& line : lines) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto fields = split_char(line, ' ');
        STELLO_CHECK_EQ(fields.size(), size_t{8});

        const auto& catalog = (fields[0] == "midgame.txt") ? mid_positions : ffo_positions;
        const auto it = std::find_if(catalog.begin(), catalog.end(), [&](const auto& p) { return p.id == fields[1]; });
        STELLO_CHECK_MSG(it != catalog.end(), line);

        SearchLimits limits = SearchLimits::solve();
        if (fields[2] != "solve") {
            STELLO_CHECK(fields[2].rfind("depth=", 0) == 0);
            const int32_t depth = std::stoi(fields[2].substr(6));
            limits = SearchLimits::fixed_depth(depth);
            ++mid_count;
        } else {
            ++ffo_count;
        }

        engine.clear_hash();
        const SearchResult res = engine.search_simple(it->board, it->player, limits);
        const std::string actual = fields[0] + " " + fields[1] + " " + fields[2] + " "
                                 + res.mv.to_string() + " " + std::to_string(res.score) + " "
                                 + std::string(score_kind_name(res.kind)) + " "
                                 + std::to_string(res.depth) + " " + std::to_string(res.nodes);
        STELLO_CHECK_EQ_MSG(actual, line, fields[1]);
    }

    STELLO_CHECK_EQ(mid_count, size_t{100});
    STELLO_CHECK_EQ(ffo_count, size_t{5});
}

// ---- Random endings & interactive/time tests ----

STELLO_TEST(solver_finds_exact_score_of_random_endings) {
    search::SearchEngine engine(14);
    uint64_t seed = 0x9E37'79B9'7F4A'7C15ULL;
    auto random_idx = [&](uint32_t n) noexcept -> uint32_t {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        return static_cast<uint32_t>(seed % static_cast<uint64_t>(n));
    };

    int checked = 0;
    for (int game = 0; game < 120; ++game) {
        const uint32_t target = 2U + static_cast<uint32_t>(game % 10); // 2-11 empties
        Board board = Board::initial();
        Player player = Player::Black;
        while (board.empty_count() > target && !board.is_game_over()) {
            const uint64_t moves = board.legal_moves(player);
            if (moves != 0) {
                std::vector<Square> sqs;
                for (const Square sq : Square::in_mask(moves)) {
                    sqs.push_back(sq);
                }
                board = board.play_unchecked(player, sqs[random_idx(static_cast<uint32_t>(sqs.size()))].index());
            }
            player = opponent(player);
        }

        if (board.legal_moves(player) == 0) {
            continue;
        }

        engine.clear_hash();
        const SearchResult res = engine.search_simple(board, player, SearchLimits::solve());
        const int32_t score = exact_minimax(board, player, false);
        STELLO_CHECK_EQ(res.score, score);
        STELLO_CHECK(res.kind == ScoreKind::Exact);
        STELLO_CHECK(res.mv.square.has_value());
        const Board after = board.play_unchecked(player, res.mv.square->index());
        STELLO_CHECK_EQ(-exact_minimax(after, opponent(player), false), score);
        ++checked;
    }

    STELLO_CHECK(checked > 100);
}

STELLO_TEST(move_now_and_cancel_stop_a_solve) {
    const auto [board, player] = find_bench_pos("ffo-40-59.txt", "49");
    (void)eval::EvalWeights::built_in();

    for (const bool do_move_now : {true, false}) {
        search::SearchSignals signals;
        const auto t0 = std::chrono::steady_clock::now();
        std::thread setter([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            if (do_move_now) {
                signals.move_now();
            } else {
                signals.cancel();
            }
        });

        search::SearchEngine engine(16);
        uint64_t nodes = 0;
        const auto outcome = engine.search(
            board,
            player,
            SearchLimits::solve(),
            [&](const SearchInfo& info) { nodes = info.nodes; },
            signals);
        const auto elapsed = std::chrono::steady_clock::now() - t0;
        setter.join();

        STELLO_CHECK(elapsed >= std::chrono::milliseconds(290));
        STELLO_CHECK(elapsed < std::chrono::seconds(5));
        STELLO_CHECK(nodes > 100'000);
        if (do_move_now) {
            STELLO_CHECK(outcome.ok);
            STELLO_CHECK(outcome.result.mv.square.has_value());
            STELLO_CHECK((board.legal_moves(player) & outcome.result.mv.square->bit()) != 0);
            STELLO_CHECK(outcome.result.nodes > 0);
        } else {
            STELLO_CHECK(!outcome.ok);
            STELLO_CHECK(outcome.error == search::SearchError::Cancelled);
        }
    }
}

namespace {

template <typename Pred>
SearchResult solve_with_move_now_at(const Board& board, Player player, Pred stop_at) {
    search::SearchSignals signals;
    search::SearchEngine engine(16);
    bool stopped = false;
    const auto outcome = engine.search(
        board,
        player,
        SearchLimits::solve(),
        [&](const SearchInfo& info) {
            if (!stopped && stop_at(info)) {
                stopped = true;
                signals.move_now();
            }
        },
        signals);
    STELLO_CHECK(outcome.ok);
    STELLO_CHECK(stopped);
    return outcome.result;
}

} // namespace

STELLO_TEST(stopped_solve_score_kinds) {
    {
        const auto [board, player] = find_bench_pos("ffo-40-59.txt", "40");
        const int32_t empties = static_cast<int32_t>(board.empty_count());
        const SearchResult res = solve_with_move_now_at(
            board, player, [&](const SearchInfo& info) { return info.depth == empties && info.kind == ScoreKind::Exact; });
        STELLO_CHECK(res.kind == ScoreKind::WinLossDraw);
        STELLO_CHECK(res.score > 0);
        STELLO_CHECK_EQ(res.depth, empties);
        STELLO_CHECK(res.mv.square.has_value());
        STELLO_CHECK((board.legal_moves(player) & res.mv.square->bit()) != 0);
    }

    {
        const auto [board, player] = find_bench_pos("ffo-40-59.txt", "41");
        const int32_t empties = static_cast<int32_t>(board.empty_count());
        const SearchResult res = solve_with_move_now_at(
            board, player, [&](const SearchInfo& info) {
                return info.depth == empties && info.kind == ScoreKind::WinLossDraw && info.score == 0;
            });
        STELLO_CHECK(res.kind != ScoreKind::Exact);
        STELLO_CHECK(res.kind != ScoreKind::WinLossDraw);
        STELLO_CHECK(res.mv.square.has_value());
        STELLO_CHECK((board.legal_moves(player) & res.mv.square->bit()) != 0);
    }

    {
        const auto board = Board::parse("XXXXXXXO--OOOXXXOOOOXOOXXOXXXOOX-XOXOX-XOOXOOO---XOXOOO-XOOOOOOO");
        STELLO_CHECK(board.has_value());
        const Player player = Player::Black;
        const SearchResult res = solve_with_move_now_at(
            *board, player, [&](const SearchInfo& info) {
                return info.depth == 8 && info.kind == ScoreKind::WinLossDraw && info.score < 0;
            });
        STELLO_CHECK(res.kind == ScoreKind::None);
        STELLO_CHECK(res.mv.square.has_value());
        STELLO_CHECK((board->legal_moves(player) & res.mv.square->bit()) != 0);
    }
}

STELLO_TEST(trivial_positions_only_moves_and_selective) {
    search::SearchEngine engine(10);
    const std::string text = "XO" + std::string(62, '-');
    const auto trivial = Board::parse(text);
    STELLO_CHECK(trivial.has_value());

    const SearchResult single = engine.search_simple(*trivial, Player::Black, SearchLimits::fixed_depth(8));
    STELLO_CHECK_EQ(single.mv.to_string(), std::string("c1"));
    STELLO_CHECK(single.kind == ScoreKind::None);
    STELLO_CHECK_EQ(single.nodes, uint64_t{0});

    const SearchResult pass = engine.search_simple(*trivial, Player::White, SearchLimits::fixed_depth(8));
    STELLO_CHECK(pass.mv.is_pass());
    STELLO_CHECK(pass.kind == ScoreKind::None);

    // only_moves
    search::SearchEngine e2(12);
    const auto [board, player] = find_bench_pos("midgame.txt", "g01e48");
    const uint64_t legal = board.legal_moves(player);
    const uint64_t only = uint64_t{1} << std::countr_zero(legal);
    const SearchLimits d6 = SearchLimits::fixed_depth(6);

    const auto ok_res = e2.search(board, player, d6, {}, search::no_stop(), only);
    STELLO_CHECK(ok_res.ok);
    STELLO_CHECK(ok_res.result.mv.square.has_value());
    STELLO_CHECK_EQ(ok_res.result.mv.square->bit(), only);
    STELLO_CHECK(ok_res.result.kind == ScoreKind::Heuristic);
    STELLO_CHECK_EQ(ok_res.result.depth, 6);
    STELLO_CHECK(ok_res.result.nodes > 0);

    const auto bad_res = e2.search(board, player, d6, {}, search::no_stop(), ~legal);
    STELLO_CHECK(!bad_res.ok);
    STELLO_CHECK(bad_res.error == search::SearchError::NoLegalOnlyMove);

    // selective_search_cuts_nodes
    const auto [b42, p42] = find_bench_pos("midgame.txt", "g01e42");
    const SearchLimits d8 = SearchLimits::fixed_depth(8);
    const SearchResult selective = search::SearchEngine(16).search_simple(b42, p42, d8);
    SearchOptions plain_opts;
    plain_opts.selective = false;
    const SearchResult plain = search::SearchEngine(16, plain_opts).search_simple(b42, p42, d8);
    STELLO_CHECK(plain.nodes > selective.nodes);

    // select_by_id_range FFO #50-#59
    const auto selected = cli::select_by_id_range(load_bench_file("ffo-40-59.txt"), 50, 59);
    STELLO_CHECK_EQ(selected.size(), size_t{10});
    for (size_t i = 0; i < 10; ++i) {
        STELLO_CHECK_EQ(selected[i].id, std::to_string(50 + i));
    }
}

namespace {

std::atomic<uint64_t> g_jumping_ms{0};
Duration jumping_clock() {
    return std::chrono::milliseconds(g_jumping_ms.fetch_add(200, std::memory_order_relaxed));
}

std::atomic<uint64_t> g_manual_ms{0};
Duration manual_clock() {
    return std::chrono::milliseconds(g_manual_ms.load(std::memory_order_relaxed));
}

} // namespace

STELLO_TEST(progress_and_time_limit_tests) {
    // progress_reports_every_root_move_and_heartbeats
    {
        g_jumping_ms.store(0, std::memory_order_relaxed);
        search::SearchEngine engine(16);
        engine.set_clock(&jumping_clock);
        const auto [board, player] = find_bench_pos("midgame.txt", "g01e36");
        const size_t root_moves = static_cast<size_t>(std::popcount(board.legal_moves(player)));
        std::vector<SearchInfo> reports;
        const auto outcome = engine.search(
            board, player, SearchLimits::fixed_depth(8),
            [&](const SearchInfo& info) { reports.push_back(info); });
        STELLO_CHECK(outcome.ok);

        size_t depth1 = 0;
        for (const auto& r : reports) {
            if (r.depth == 1) {
                ++depth1;
            }
            STELLO_CHECK((board.legal_moves(player) & r.current_move.bit()) != 0);
        }
        STELLO_CHECK_EQ(depth1, root_moves);

        size_t heartbeats = 0;
        for (size_t i = 1; i < reports.size(); ++i) {
            STELLO_CHECK(reports[i - 1].depth <= reports[i].depth);
            STELLO_CHECK(reports[i - 1].nodes <= reports[i].nodes);
            if (reports[i - 1].depth == reports[i].depth
                && reports[i - 1].current_move == reports[i].current_move
                && reports[i - 1].best_move == reports[i].best_move)
            {
                ++heartbeats;
            }
        }
        STELLO_CHECK(heartbeats > 0);
        STELLO_CHECK_EQ(reports.back().depth, 8);
        STELLO_CHECK(reports.back().best_move == outcome.result.mv.square);
        STELLO_CHECK_EQ(reports.back().score, outcome.result.score);
        STELLO_CHECK(reports.back().kind == ScoreKind::Heuristic);
    }

    // soft_time_rule_stops_before_a_new_iteration
    {
        const auto [board, player] = find_bench_pos("midgame.txt", "g02e42");
        const SearchLimits limits = SearchLimits::time_per_move_ms(3000);
        struct Case {
            std::optional<int32_t> pct;
            uint64_t (*time_after)(int32_t);
            int32_t expected;
        };
        const std::array<Case, 3> cases = {{
            {std::nullopt, [](int32_t d) -> uint64_t { return d >= 3 ? 2001 : 0; }, 3},
            {std::nullopt, [](int32_t d) -> uint64_t { return (d == 3 || d == 4) ? 2000 : (d >= 5 ? 2500 : 0); }, 5},
            {50, [](int32_t d) -> uint64_t { return d >= 2 ? 1600 : 0; }, 2},
        }};

        for (const auto& c : cases) {
            g_manual_ms.store(0, std::memory_order_relaxed);
            SearchOptions opts;
            opts.soft_time_percent = c.pct;
            search::SearchEngine engine(16, opts);
            engine.set_clock(&manual_clock);
            engine.set_iteration_done([&](int32_t d, int32_t) {
                g_manual_ms.store(c.time_after(d), std::memory_order_relaxed);
            });
            int32_t deepest = 0;
            const auto outcome = engine.search(
                board, player, limits,
                [&](const SearchInfo& info) { deepest = (std::max)(deepest, info.depth); });
            STELLO_CHECK(outcome.ok);
            STELLO_CHECK_EQ(outcome.result.depth, c.expected);
            STELLO_CHECK(outcome.result.kind == ScoreKind::Heuristic);
            STELLO_CHECK_EQ(deepest, c.expected);
        }
    }

    // hard_time_limit_stops_within_an_iteration
    {
        g_manual_ms.store(0, std::memory_order_relaxed);
        search::SearchEngine engine(16);
        engine.set_clock(&manual_clock);
        const auto [board, player] = find_bench_pos("midgame.txt", "g01e42");
        std::vector<SearchInfo> reports;
        const auto outcome = engine.search(
            board, player, SearchLimits::time_per_move_ms(1000),
            [&](const SearchInfo& info) {
                if (info.depth == 7) {
                    g_manual_ms.store(1000, std::memory_order_relaxed);
                }
                reports.push_back(info);
            });
        STELLO_CHECK(outcome.ok);
        STELLO_CHECK_EQ(outcome.result.depth, 7);
        STELLO_CHECK(outcome.result.kind == ScoreKind::Heuristic);
        std::vector<SearchInfo> sevens;
        for (const auto& r : reports) {
            if (r.depth == 7) {
                sevens.push_back(r);
            }
        }
        STELLO_CHECK(!sevens.empty());
        STELLO_CHECK(sevens.size() < static_cast<size_t>(std::popcount(board.legal_moves(player))));
        STELLO_CHECK(outcome.result.mv.square == sevens.back().best_move);
    }

    // real clock time_per_move
    {
        search::SearchEngine engine(16);
        (void)eval::EvalWeights::built_in();
        for (const int64_t limit : {int64_t{30}, int64_t{200}}) {
            engine.clear_hash();
            const auto t0 = std::chrono::steady_clock::now();
            const SearchResult res = engine.search_simple(Board::initial(), Player::Black, SearchLimits::time_per_move_ms(limit));
            const auto elapsed = std::chrono::steady_clock::now() - t0;
            STELLO_CHECK(elapsed < std::chrono::milliseconds(limit + 30));
            STELLO_CHECK(res.elapsed <= elapsed);
            STELLO_CHECK(res.kind == ScoreKind::Heuristic);
            STELLO_CHECK(res.depth >= 1);
            STELLO_CHECK(res.mv.square.has_value());
            STELLO_CHECK((Board::initial().legal_moves(Player::Black) & res.mv.square->bit()) != 0);
        }
    }

    // cancel & move_now from another thread on midgame
    {
        const auto [board, player] = find_bench_pos("midgame.txt", "g03e48");
        {
            search::SearchSignals signals;
            const auto t0 = std::chrono::steady_clock::now();
            std::thread setter([&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                signals.cancel();
            });
            search::SearchEngine engine(16);
            const auto outcome = engine.search(board, player, SearchLimits::fixed_depth(40), {}, signals);
            const auto elapsed = std::chrono::steady_clock::now() - t0;
            setter.join();
            STELLO_CHECK(!outcome.ok);
            STELLO_CHECK(outcome.error == search::SearchError::Cancelled);
            STELLO_CHECK(elapsed >= std::chrono::milliseconds(90) && elapsed < std::chrono::seconds(5));

            search::SearchSignals pre;
            pre.cancel();
            search::SearchEngine small_e(10);
            const auto pre_out = small_e.search(Board::initial(), Player::Black, SearchLimits::fixed_depth(4), {}, pre);
            STELLO_CHECK(!pre_out.ok);
            STELLO_CHECK(pre_out.error == search::SearchError::Cancelled);
            pre.reset();
            STELLO_CHECK(small_e.search(Board::initial(), Player::Black, SearchLimits::fixed_depth(4), {}, pre).ok);
        }

        {
            search::SearchSignals signals;
            const auto t0 = std::chrono::steady_clock::now();
            std::thread setter([&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                signals.move_now();
            });
            search::SearchEngine engine(16);
            const auto outcome = engine.search(board, player, SearchLimits::fixed_depth(40), {}, signals);
            const auto elapsed = std::chrono::steady_clock::now() - t0;
            setter.join();
            STELLO_CHECK(outcome.ok);
            STELLO_CHECK(elapsed >= std::chrono::milliseconds(90) && elapsed < std::chrono::seconds(5));
            STELLO_CHECK(outcome.result.kind == ScoreKind::Heuristic);
            STELLO_CHECK(outcome.result.depth >= 2 && outcome.result.depth < 40);
            STELLO_CHECK(outcome.result.mv.square.has_value());
            STELLO_CHECK((board.legal_moves(player) & outcome.result.mv.square->bit()) != 0);
        }
    }
}

} // namespace stello::tests
