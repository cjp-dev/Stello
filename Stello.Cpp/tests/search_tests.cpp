#include "tests/test_harness.hpp"

#include "cli/bench.hpp"
#include "core/bits.hpp"
#include "core/board.hpp"
#include "core/square.hpp"
#include "endgame/bits.hpp"
#include "endgame/local.hpp"
#include "endgame/stable.hpp"
#include "eval/eval.hpp"
#include "eval/weights.hpp"
#include "search/abort.hpp"
#include "search/engine.hpp"
#include "search/kernel.hpp"
#include "search/limits.hpp"
#include "search/microbench.hpp"
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
    STELLO_CHECK_EQ(static_cast<int32_t>(hit->lower), 100);
    STELLO_CHECK_EQ(static_cast<int32_t>(hit->upper), 100);
    STELLO_CHECK_EQ(static_cast<int32_t>(hit->depth), 5);
    STELLO_CHECK_EQ(static_cast<int32_t>(hit->mv), 7);
    STELLO_CHECK(!table.get(1, 2, 1).has_value());

    table.store(1, 2, 0, 3, search::Bound::Lower, 50, 8);
    const auto still_deep = table.get(1, 2, 0);
    STELLO_CHECK(still_deep.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(still_deep->lower), 100);
    STELLO_CHECK_EQ(static_cast<int32_t>(still_deep->upper), 100);

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
    STELLO_CHECK_EQ(static_cast<int32_t>(hit->lower), 50);
    STELLO_CHECK_EQ(static_cast<int32_t>(hit->upper), static_cast<int32_t>(search::TT_SCORE_MAX));
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
    for (uint64_t seed = 1; seed <= 1'000'000 && others.size() < search::TranspositionTable::WAYS; ++seed) {
        const uint64_t cand = seed * 0x9E37'79B9'7F4A'7C15ULL;
        if (std::popcount(cand) >= 4 && table.slot_index(cand, 0, 0) == target_slot) {
            others.push_back(cand);
        }
    }
    STELLO_CHECK_EQ(others.size(), search::TranspositionTable::WAYS);

    table.store_at_root(old_own, old_opp, 0, 9, search::Bound::Exact, 100, 7, 64);
    for (size_t i = 0; i < search::TranspositionTable::WAYS - 1; ++i) {
        table.store_at_root(others[i], 0, 0, static_cast<int32_t>(2 + i), search::Bound::Lower, static_cast<int32_t>(40 + i), static_cast<int32_t>(i), 64);
    }
    STELLO_CHECK(table.get_at_root(old_own, old_opp, 0, 64).has_value());
    STELLO_CHECK(!table.get_at_root(old_own, old_opp, 0, 60).has_value());

    table.store_at_root(others[3], 0, 0, 1, search::Bound::Lower, 50, 8, 60);
    const auto hit_last = table.get_at_root(others[3], 0, 0, 60);
    STELLO_CHECK(hit_last.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(hit_last->depth), 1);
    for (size_t i = 0; i < search::TranspositionTable::WAYS - 1; ++i) {
        const auto hit = table.get_at_root(others[i], 0, 0, 60);
        STELLO_CHECK(hit.has_value());
        STELLO_CHECK_EQ(static_cast<int32_t>(hit->depth), static_cast<int32_t>(2 + i));
    }
    STELLO_CHECK(!table.get_at_root(old_own, old_opp, 0, 64).has_value());
}

STELLO_TEST(table_packing_keeps_every_field) {
    struct Case {
        int16_t lower;
        int16_t upper;
        int8_t mv;
        uint8_t depth;
        uint8_t selectivity;
        uint8_t tag;
        uint8_t cost;
        uint8_t date;
        uint8_t generation;
    };
    const std::array<Case, 4> cases = {{
        {-32768, 32767, -1, 63, 7, 3, 31, 3, 127},
        {32767, -32768, 63, 0, 0, 0, 0, 0, 1},
        {-150, 420, 0, 28, 4, 2, 19, 2, 64},
        {0, 0, -1, 60, 1, 1, 1, 1, 2},
    }};
    for (const auto& c : cases) {
        search::Entry e;
        e.verify = (uint64_t{40} << search::detail::VERIFY_EMPTIES_SHIFT) | 0x0123'4567'89AB'CDEFULL;
        e.lower = c.lower;
        e.upper = c.upper;
        e.mv = c.mv;
        e.depth = c.depth;
        e.selectivity = c.selectivity;
        e.tag = c.tag;
        e.cost = c.cost;
        e.date = c.date;
        e.generation = c.generation;
        const auto unpacked = search::Entry::unpack(e.verify, e.pack());
        STELLO_CHECK(unpacked == e);
        STELLO_CHECK_EQ(
            search::detail::writeable_level_packed(e.pack(), c.date),
            search::detail::writeable_level(e, c.date));
        const uint8_t other_date = static_cast<uint8_t>((c.date + 1U) & 3U);
        STELLO_CHECK_EQ(
            search::detail::writeable_level_packed(e.pack(), other_date),
            search::detail::writeable_level(e, other_date));
    }
    STELLO_CHECK_EQ(search::Entry{}.pack(), uint64_t{0});
    STELLO_CHECK(search::Entry::unpack(0, 0) == search::Entry{});
}

STELLO_TEST(table_dual_bounds_and_composite_level) {
    search::TranspositionTable table(10);
    const uint64_t own = 0x1111'2222'3333'4444ULL;
    const size_t target_slot = table.slot_index(own, 0, search::ENDGAME_TT_TAG);

    // 1. Same-level Lower + Upper merges into [lower, upper] preserving valid move.
    table.store_bounds_at_root(own, 0, search::ENDGAME_TT_TAG, 16, 7, 40, 50, 50, 12, 64, 5);
    table.store_bounds_at_root(own, 0, search::ENDGAME_TT_TAG, 16, 7, 80, 90, 80, -1, 64, 9);
    auto h = table.get_at_root(own, 0, search::ENDGAME_TT_TAG, 64);
    STELLO_CHECK(h.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(h->lower), 50);
    STELLO_CHECK_EQ(static_cast<int32_t>(h->upper), 80);
    STELLO_CHECK_EQ(static_cast<int32_t>(h->mv), 12);
    STELLO_CHECK_EQ(static_cast<int32_t>(h->cost), 9);

    // 2. Meeting bounds ([50, 80] + fail-low at 50) narrows to exact [50, 50] & keeps Cut move;
    // contradiction resets cleanly; same-level fail-low updates mv only when prior lower == TT_SCORE_MIN.
    table.store_bounds_at_root(own, 0, search::ENDGAME_TT_TAG, 16, 7, 50, 51, 50, 15, 64, 3);
    h = table.get_at_root(own, 0, search::ENDGAME_TT_TAG, 64);
    STELLO_CHECK(h.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(h->lower), 50);
    STELLO_CHECK_EQ(static_cast<int32_t>(h->upper), 50);
    STELLO_CHECK_EQ(static_cast<int32_t>(h->mv), 12);
    table.store_bounds_at_root(own, 0, search::ENDGAME_TT_TAG, 16, 7, 30, 40, 30, 17, 64, 3);
    h = table.get_at_root(own, 0, search::ENDGAME_TT_TAG, 64);
    STELLO_CHECK(h.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(h->lower), static_cast<int32_t>(search::TT_SCORE_MIN));
    STELLO_CHECK_EQ(static_cast<int32_t>(h->upper), 30);
    STELLO_CHECK_EQ(static_cast<int32_t>(h->mv), 12);
    table.store_bounds_at_root(own, 0, search::ENDGAME_TT_TAG, 16, 7, 25, 35, 25, 19, 64, 3);
    h = table.get_at_root(own, 0, search::ENDGAME_TT_TAG, 64);
    STELLO_CHECK(h.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(h->mv), 19);

    // 3. Bidirectional same-level lower-bound move preservation & upgrade:
    // - A weaker same-level fail-high must NEVER steal the move of an existing lower bound.
    // - A stronger same-level fail-high or fresh exact search with a valid move MUST adopt the new move.
    const uint64_t mv_own = 0x3333'4444'5555'6666ULL;
    const int8_t sq_c4 = static_cast<int8_t>(Square::parse("c4")->index());
    const int8_t sq_h3 = static_cast<int8_t>(Square::parse("h3")->index());
    const int8_t sq_d3 = static_cast<int8_t>(Square::parse("d3")->index());
    table.store_bounds_at_root(mv_own, 0, 0, 8, 7, -32665, 32665, -193, sq_c4, 48, 2);
    table.store_bounds_at_root(mv_own, 0, 0, 8, 7, -6401, -6400, -1131, sq_h3, 48, 1);
    h = table.get_at_root(mv_own, 0, 0, 48);
    STELLO_CHECK(h.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(h->lower), -193);
    STELLO_CHECK_EQ(static_cast<int32_t>(h->upper), -193);
    STELLO_CHECK_EQ(static_cast<int32_t>(h->mv), static_cast<int32_t>(sq_c4));

    const uint64_t mv_up_own = 0x4444'5555'6666'7777ULL;
    table.store_bounds_at_root(mv_up_own, 0, 0, 8, 7, -6401, -6400, -1131, sq_h3, 48, 1);
    table.store_bounds_at_root(mv_up_own, 0, 0, 8, 7, -200, -193, -193, sq_c4, 48, 2);
    h = table.get_at_root(mv_up_own, 0, 0, 48);
    STELLO_CHECK(h.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(h->lower), -193);
    STELLO_CHECK_EQ(static_cast<int32_t>(h->mv), static_cast<int32_t>(sq_c4));
    table.store_bounds_at_root(mv_up_own, 0, 0, 8, 7, -500, 0, -250, sq_d3, 48, 3);
    h = table.get_at_root(mv_up_own, 0, 0, 48);
    STELLO_CHECK(h.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(h->lower), -250);
    STELLO_CHECK_EQ(static_cast<int32_t>(h->upper), -250);
    STELLO_CHECK_EQ(static_cast<int32_t>(h->mv), static_cast<int32_t>(sq_d3));

    // 4. Upgrade (new_level > old_level) resets stale bounds while preserving mv if mv == -1, or adopting valid deeper mv even on fail-low.
    const uint64_t up_own = 0x2222'3333'4444'5555ULL;
    table.store_bounds_at_root(up_own, 0, search::ENDGAME_TT_TAG, 16, 4, 0, 10, 10, 22, 64, 15);
    table.store_bounds_at_root(up_own, 0, search::ENDGAME_TT_TAG, 16, 7, -20, -10, -20, -1, 64, 4);
    h = table.get_at_root(up_own, 0, search::ENDGAME_TT_TAG, 64);
    STELLO_CHECK(h.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(h->selectivity), 7);
    STELLO_CHECK_EQ(static_cast<int32_t>(h->lower), static_cast<int32_t>(search::TT_SCORE_MIN));
    STELLO_CHECK_EQ(static_cast<int32_t>(h->upper), -20);
    STELLO_CHECK_EQ(static_cast<int32_t>(h->mv), 22);
    STELLO_CHECK_EQ(static_cast<int32_t>(h->cost), 15);
    table.store_bounds_at_root(up_own, 0, search::ENDGAME_TT_TAG, 18, 7, -15, -5, -15, 27, 64, 4);
    h = table.get_at_root(up_own, 0, search::ENDGAME_TT_TAG, 64);
    STELLO_CHECK(h.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(h->mv), 27);

    // 5. Composite level ordering: empties=28 sel=4 (level=228) outlives empties=14 sel=7 (level=119).
    table.clear();
    std::vector<uint64_t> col;
    for (uint64_t seed = 1; seed <= 1'000'000 && col.size() < 5; ++seed) {
        const uint64_t cand = seed * 0x9E37'79B9'7F4A'7C15ULL;
        if (std::popcount(cand) >= 4 && table.slot_index(cand, 0, search::ENDGAME_TT_TAG) == target_slot) {
            col.push_back(cand);
        }
    }
    STELLO_CHECK_EQ(col.size(), size_t{5});
    table.store_bounds_at_root(col[0], 0, search::ENDGAME_TT_TAG, 28, 4, 0, 2, 2, 1, 64, 10);
    table.store_bounds_at_root(col[1], 0, search::ENDGAME_TT_TAG, 14, 7, 0, 2, 2, 2, 64, 25);
    table.store_bounds_at_root(col[2], 0, search::ENDGAME_TT_TAG, 20, 7, 0, 2, 2, 3, 64, 10);
    table.store_bounds_at_root(col[3], 0, search::ENDGAME_TT_TAG, 22, 7, 0, 2, 2, 4, 64, 10);
    table.store_bounds_at_root(col[4], 0, search::ENDGAME_TT_TAG, 18, 7, 0, 2, 2, 5, 64, 10);
    STELLO_CHECK(table.get(col[0], 0, search::ENDGAME_TT_TAG).has_value());
    STELLO_CHECK(!table.get(col[1], 0, search::ENDGAME_TT_TAG).has_value());
}

STELLO_TEST(table_same_level_weaker_fail_high_preserves_exact_move) {
    const auto [board, player] = find_bench_pos("midgame-deep.txt", "g01e48");
    const auto c4 = Square::parse("c4");
    const auto h3 = Square::parse("h3");
    STELLO_CHECK(c4.has_value() && h3.has_value());

    SearchOptions opts;
    opts.selective = false;
    search::TranspositionTable table(12);
    search::ThreadState thread;
    const auto ctx = search::SearchContext::create(
        opts, nullptr, nullptr, table, table, search::system_clock, board.empty_count());
    search::Searcher searcher(ctx, thread, search::no_stop());
    searcher.hash_put_height = 1;

    auto unwrap = [](search::AbortResult<search::ScoreMove> r) -> search::ScoreMove {
#if STELLO_ABORT_EXCEPTIONS
        return r;
#else
        STELLO_CHECK(r.is_ok());
        return r.value;
#endif
    };

    // 1. Exact full-window 1-ply interior search stores c4 = -193.
    searcher.hash_get_height = 0;
    const search::ScoreMove step1 = unwrap(searcher.search(board, player, 0, 1, -1, -32665, 32665));
    STELLO_CHECK_EQ(step1.score, -193);
    STELLO_CHECK_EQ(step1.mv, static_cast<int32_t>(c4->index()));

    // 2. Weaker narrow fail-high search (h3 = -1131) must NOT overwrite c4 in the TT.
    searcher.hash_get_height = 0;
    const search::ScoreMove step2 = unwrap(searcher.search(board, player, 0, 1, -1, -6401, -6399));
    STELLO_CHECK_EQ(step2.score, -1131);
    STELLO_CHECK_EQ(step2.mv, static_cast<int32_t>(h3->index()));

    // 3. Exact TT hit must return c4 = -193 in 1 node.
    searcher.hash_get_height = 1;
    const uint64_t nodes_before = searcher.nodes;
    const search::ScoreMove step3 = unwrap(searcher.search(board, player, 0, 1, -1, -32665, 32665));
    STELLO_CHECK_EQ(searcher.nodes - nodes_before, uint64_t{1});
    STELLO_CHECK_EQ(step3.score, -193);
    STELLO_CHECK_EQ(step3.mv, static_cast<int32_t>(c4->index()));

    // 4. Confirm h3 alone scores -1131 at depth 1.
    search::SearchEngine engine(12, opts);
    const auto step4 = engine.search(board, player, SearchLimits::fixed_depth(1), {}, search::no_stop(), h3->bit());
    STELLO_CHECK(step4.ok);
    STELLO_CHECK(step4.result.mv == Move(*h3));
    STELLO_CHECK_EQ(step4.result.score, -1131);
}

STELLO_TEST(table_4way_replacement_and_domain_isolation) {
    search::TranspositionTable table(10);
    const uint64_t base_own = 0x1111'2222'3333'4444ULL;
    const size_t target_slot = table.slot_index(base_own, 0, 0);

    constexpr size_t ways = search::TranspositionTable::WAYS;
    std::vector<uint64_t> colliding;
    for (uint64_t seed = 1; seed <= 1'000'000 && colliding.size() < ways + 1; ++seed) {
        const uint64_t cand = seed * 0x9E37'79B9'7F4A'7C15ULL;
        if (cand != base_own && std::popcount(cand) >= 4 && table.slot_index(cand, 0, 0) == target_slot) {
            colliding.push_back(cand);
        }
    }
    STELLO_CHECK_EQ(colliding.size(), ways + 1);

    // Advance date for colliding[0] (even with higher depth/cost, stale date gets evicted once overwritten by a current entry).
    table.store(colliding[0], 0, 0, 12, search::Bound::Exact, 100, 1, 30);
    table.advance_date();
    for (size_t i = 1; i < ways; ++i) {
        table.store(colliding[i], 0, 0, static_cast<int32_t>(4 + i), search::Bound::Lower, static_cast<int32_t>(i), 2, 10);
    }
    for (size_t i = 0; i < ways; ++i) {
        STELLO_CHECK(table.get(colliding[i], 0, 0).has_value());
    }

    // Extra colliding entry must evict the stale-date entry colliding[0], keeping all current-date entries.
    table.store(colliding[ways], 0, 0, 3, search::Bound::Upper, -5, 3, 2);
    STELLO_CHECK(!table.get(colliding[0], 0, 0).has_value());
    for (size_t i = 1; i <= ways; ++i) {
        STELLO_CHECK(table.get(colliding[i], 0, 0).has_value());
    }

    // In-place update: deeper overwrites, equal preserves max(cost), shallower does not overwrite.
    table.store(colliding[ways], 0, 0, 3, search::Bound::Exact, -4, 4, 8);
    auto updated = table.get(colliding[ways], 0, 0);
    STELLO_CHECK(updated.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(updated->lower), -4);
    STELLO_CHECK_EQ(static_cast<int32_t>(updated->upper), -4);
    STELLO_CHECK_EQ(static_cast<int32_t>(updated->cost), 8);
    table.store(colliding[ways], 0, 0, 2, search::Bound::Lower, 99, 5, 1);
    updated = table.get(colliding[ways], 0, 0);
    STELLO_CHECK(updated.has_value());
    STELLO_CHECK_EQ(static_cast<int32_t>(updated->depth), 3);
    STELLO_CHECK_EQ(static_cast<int32_t>(updated->lower), -4);
    STELLO_CHECK_EQ(static_cast<int32_t>(updated->upper), -4);

    // Domain isolation: tags 0, 1 (midgame Black/White) and ENDGAME_TT_TAG (2) never collide or alias.
    table.clear();
    table.store(base_own, 42, 0, 6, search::Bound::Exact, 111, 10, 5);
    table.store(base_own, 42, 1, 7, search::Bound::Lower, 222, 11, 6);
    table.store(base_own, 42, search::ENDGAME_TT_TAG, 18, search::Bound::Exact, -14, 12, 7);
    const auto h0 = table.get(base_own, 42, 0);
    const auto h1 = table.get(base_own, 42, 1);
    const auto h2 = table.get(base_own, 42, search::ENDGAME_TT_TAG);
    STELLO_CHECK(h0.has_value() && h0->lower == 111 && h0->upper == 111 && h0->mv == 10);
    STELLO_CHECK(h1.has_value() && h1->lower == 222 && h1->upper == search::TT_SCORE_MAX && h1->mv == 11);
    STELLO_CHECK(h2.has_value() && h2->lower == -14 && h2->upper == -14 && h2->mv == 12);
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

STELLO_TEST(wld_admission_decision_tests) {
    STELLO_CHECK_EQ(time::estimate_wld_ms(24, 16, 100'000, 100, 1'000'000), int64_t{240});
    STELLO_CHECK(!time::should_admit_wld(24, 16, 100'000, 100, 1'000'000, 200));
    STELLO_CHECK(time::should_admit_wld(24, 16, 100'000, 100, 1'000'000, 300));
    STELLO_CHECK(!time::should_admit_wld(24, 16, 100'000, 100, 1'000'000, 0));
    STELLO_CHECK(time::should_admit_wld(14, 6, 100'000, 100, 1'000'000, 1));
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
            {1, -5, 0, 0},
            {2, 100, 0, 0},
            {3, 5, 0, 0},
            {4, 200, 0, 0},
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
                child_own, child_opp, search::ENDGAME_TT_TAG, static_cast<int32_t>(root_empties) - 1,
                search::Bound::Exact, -val, -1, root_empties);
            ++index;
        }
#if STELLO_ABORT_EXCEPTIONS
        STELLO_CHECK_EQ(s3.end_deep(own, opp, 63, 14, true, legal), best_value);
#else
        const auto deep_res = s3.end_deep(own, opp, 63, 14, true, legal);
        STELLO_CHECK(deep_res.is_ok() && deep_res.value == best_value);
#endif
        const auto hit = s3.endgame_table->get_at_root(own, opp, search::ENDGAME_TT_TAG, root_empties);
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

    STELLO_CHECK_EQ(total_nodes, uint64_t{1'698'407});
}

// ---- Full 105-signature baseline integration test (Stello 3.1) ----

STELLO_TEST(stello_v3_all_105_signatures_match_baseline) {
    const auto lines = split_lines(read_repo_text("Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt"));
    const auto mid_positions = load_bench_file("midgame.txt");
    const auto ffo_positions = load_bench_file("ffo-40-59.txt");

    const cpu::CpuTier saved_tier = cpu::active_tier();
    struct RestoreTier {
        cpu::CpuTier tier;
        ~RestoreTier() { (void)cpu::set_forced_tier(tier); }
    } restore_tier{saved_tier};

    std::vector<cpu::CpuTier> tiers = {cpu::CpuTier::Portable};
    if (cpu::has_avx2_bmi2()) {
        tiers.push_back(cpu::CpuTier::V3);
    }
    if (cpu::has_avx512_v4()) {
        tiers.push_back(cpu::CpuTier::V4);
    }

    for (const cpu::CpuTier tier : tiers) {
        STELLO_CHECK(cpu::set_forced_tier(tier));
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
        const auto [board, player] = find_bench_pos("ffo-40-59.txt", "40");
        const int32_t empties = static_cast<int32_t>(board.empty_count());
        const SearchResult res = solve_with_move_now_at(
            board,
            player,
            [&](const SearchInfo& info) {
                return info.depth == empties
                    && info.kind == ScoreKind::Exact
                    && info.best_move.has_value()
                    && info.best_move->to_string() == "a2";
            });
        STELLO_CHECK(res.kind == ScoreKind::WinLossDraw);
        STELLO_CHECK(res.score > 0);
        STELLO_CHECK_EQ(res.depth, empties);
        STELLO_CHECK_EQ(res.mv.to_string(), std::string("a2"));
    }

    {
        const auto [board, player] = find_bench_pos("ffo-40-59.txt", "41");
        const int32_t empties = static_cast<int32_t>(board.empty_count());
        const SearchResult res = solve_with_move_now_at(
            board, player, [&](const SearchInfo& info) {
                return info.depth == empties && info.kind == ScoreKind::WinLossDraw && info.score == 0;
            });
        STELLO_CHECK(res.kind == ScoreKind::Heuristic);
        STELLO_CHECK_EQ(res.score, 0);
        STELLO_CHECK_EQ(res.depth, 4);
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
                if (info.depth == 9) {
                    g_manual_ms.store(1000, std::memory_order_relaxed);
                }
                reports.push_back(info);
            });
        STELLO_CHECK(outcome.ok);
        STELLO_CHECK_EQ(outcome.result.depth, 9);
        STELLO_CHECK(outcome.result.kind == ScoreKind::Heuristic);
        std::vector<SearchInfo> nines;
        for (const auto& r : reports) {
            if (r.depth == 9) {
                nines.push_back(r);
            }
        }
        STELLO_CHECK(!nines.empty());
        STELLO_CHECK(nines.size() < static_cast<size_t>(std::popcount(board.legal_moves(player))));
        STELLO_CHECK(outcome.result.mv.square == nines.back().best_move);
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

STELLO_TEST(selective_endgame_progress_and_time_per_game_tests) {
    // Selective endgame stages in TimePerMove report Heuristic scores scaled by PATTERN_UNITS_PER_DISC (100).
    {
        const auto [board, player] = find_bench_pos("ffo-40-59.txt", "40");
        const int32_t empties = static_cast<int32_t>(board.empty_count());
        for (const uint32_t threads : {uint32_t{1}, uint32_t{4}}) {
            search::SearchEngine engine(16);
            std::vector<SearchInfo> end_reports;
            const auto limits = *SearchLimits::time_per_move_ms(2000).with_threads(threads);
            const auto outcome = engine.search(board, player, limits, [&](const SearchInfo& info) {
                if (info.depth == empties && info.kind == ScoreKind::Heuristic) {
                    end_reports.push_back(info);
                }
            });
            STELLO_CHECK(outcome.ok);
            STELLO_CHECK(!end_reports.empty());
            bool saw_nonzero_scaled = false;
            for (const auto& info : end_reports) {
                STELLO_CHECK_EQ(info.score % eval::PATTERN_UNITS_PER_DISC, 0);
                STELLO_CHECK(info.score >= -64 * eval::PATTERN_UNITS_PER_DISC && info.score <= 64 * eval::PATTERN_UNITS_PER_DISC);
                if (info.score != 0) {
                    saw_nonzero_scaled = true;
                    STELLO_CHECK(std::abs(info.score) >= eval::PATTERN_UNITS_PER_DISC);
                }
            }
            STELLO_CHECK(saw_nonzero_scaled);
        }
    }

    // TimePerGame (2 min remaining) at 30 empties respects move budget instead of burning rest/2 (60 s).
    {
        const auto [board, player] = find_bench_pos("midgame.txt", "g01e30");
        search::SearchEngine engine(16);
        const auto outcome = engine.search(board, player, SearchLimits::time_per_game_ms(120'000));
        STELLO_CHECK(outcome.ok);
        STELLO_CHECK(outcome.result.elapsed < std::chrono::seconds(20));
        STELLO_CHECK(outcome.result.mv.square.has_value());
        STELLO_CHECK((board.legal_moves(player) & outcome.result.mv.square->bit()) != 0);
    }
}

STELLO_TEST(microbench_portable_and_v3_parity_tests) {
    search::MicrobenchOptions opts{};
    opts.warmup = 8;
    opts.repeats = 64;
    opts.verbose = true;

    const auto expected_names = search::microbench_names();
    const auto portable = search::run_microbench(opts, cpu::CpuTier::Portable);
    STELLO_CHECK_EQ(portable.size(), expected_names.size());
    for (size_t i = 0; i < portable.size(); ++i) {
        const auto& s = portable[i];
        STELLO_CHECK_EQ(s.name, expected_names[i]);
        STELLO_CHECK(std::isfinite(s.min_cycles) && s.min_cycles >= 0.0);
        STELLO_CHECK(std::isfinite(s.mean_cycles) && s.mean_cycles >= s.min_cycles);
        STELLO_CHECK(std::isfinite(s.stddev_cycles) && s.stddev_cycles >= 0.0);
        STELLO_CHECK(std::isfinite(s.max_cycles) && s.max_cycles >= s.mean_cycles);
        STELLO_CHECK_EQ(s.per_sample.size(), size_t{64});
    }

    auto check_tier = [&](cpu::CpuTier tier) {
        const auto res = search::run_microbench(opts, tier);
        STELLO_CHECK_EQ(res.size(), portable.size());
        for (size_t i = 0; i < res.size(); ++i) {
            STELLO_CHECK_EQ(res[i].name, portable[i].name);
            if (res[i].name != "hash_board") {
                STELLO_CHECK_EQ_MSG(res[i].checksum, portable[i].checksum, std::string(res[i].name));
            }
            STELLO_CHECK(std::isfinite(res[i].mean_cycles) && res[i].mean_cycles >= 0.0);
        }
    };

    if (cpu::has_avx2_bmi2()) {
        check_tier(cpu::CpuTier::V3);
    }
    if (cpu::has_avx512_v4()) {
        check_tier(cpu::CpuTier::V4);
    }
}

STELLO_TEST(phase1_c2_c3_c5_search_and_time_regression_tests) {
    // C2: ybwc=false multi-threaded solve helpers inherit end_mpc_stage & hash heights cleanly on losing/winning FFOs.
    {
        SearchOptions opts;
        opts.ybwc = false;
        search::SearchEngine engine(18, opts);
        for (const char* id : {"40", "41", "43"}) {
            const auto [board, player] = find_bench_pos("ffo-40-59.txt", id);
            engine.clear_hash();
            const auto res = engine.search_simple(board, player, *SearchLimits::solve().with_threads(4));
            STELLO_CHECK(res.kind == ScoreKind::Exact);
            STELLO_CHECK(res.mv.square.has_value());
            STELLO_CHECK((board.legal_moves(player) & res.mv.square->bit()) != 0ULL);
        }
    }

    // C3: Losing endgame (FFO #43 = -12, WLD < 0) keeps heuristic move order / exact solve on 1t and 4t.
    {
        const auto [board, player] = find_bench_pos("ffo-40-59.txt", "43");
        for (const uint32_t threads : {uint32_t{1}, uint32_t{4}}) {
            search::SearchEngine engine(18);
            const auto res = engine.search_simple(board, player, *SearchLimits::solve().with_threads(threads));
            STELLO_CHECK_EQ(res.score, -12);
            STELLO_CHECK(res.kind == ScoreKind::Exact);
            STELLO_CHECK(res.mv.square.has_value());
            STELLO_CHECK((board.legal_moves(player) & res.mv.square->bit()) != 0ULL);
        }
    }

    // C5: Huge and negative time bounds never trigger signed overflow or out-of-order soft/hard budgets.
    {
        constexpr int64_t huge = (std::numeric_limits<int64_t>::max)();
        const time::TimeBudget g_huge = time::for_game(huge, 60);
        STELLO_CHECK(g_huge.hard_ms > 0 && g_huge.hard_ms <= time::MAX_TIME_MS);
        STELLO_CHECK(g_huge.soft_ms >= 0 && g_huge.soft_ms <= g_huge.hard_ms);

        const time::TimeBudget g_neg = time::for_game(-5000, 20);
        STELLO_CHECK(g_neg.hard_ms >= 0 && g_neg.soft_ms >= 0 && g_neg.soft_ms <= g_neg.hard_ms);
        STELLO_CHECK(time::for_game(-5000, 6).hard_ms > 0);

        const auto half_max_limits = SearchLimits::time_per_move_ms(huge / 2);
        const time::TimeBudget m_huge = time::for_midgame(half_max_limits, 60);
        STELLO_CHECK(m_huge.hard_ms > 0 && m_huge.hard_ms <= time::MAX_TIME_MS);
        STELLO_CHECK(m_huge.soft_ms >= 0 && m_huge.soft_ms <= m_huge.hard_ms);

        const auto overtime_limits = SearchLimits::time_per_game_ms(100);
        const time::TimeBudget e_overtime = time::for_endgame(overtime_limits, m_huge, huge / 2);
        STELLO_CHECK(e_overtime.soft_ms >= 0 && e_overtime.soft_ms <= e_overtime.hard_ms);
        STELLO_CHECK(e_overtime.hard_ms <= time::MAX_TIME_MS);

        const time::TimeBudget soft_clamped =
            time::TimeBudget{50, 1000}.with_soft_percent(std::optional<int32_t>{150}, 200);
        STELLO_CHECK_EQ(soft_clamped.soft_ms, 1000);
        STELLO_CHECK_EQ(soft_clamped.hard_ms, 1000);
    }
}

STELLO_TEST(phase4_1d_leaf_wipeout_recognition) {
    const cpu::CpuTier saved_tier = cpu::active_tier();
    struct RestoreTier {
        cpu::CpuTier tier;
        ~RestoreTier() { (void)cpu::set_forced_tier(tier); }
    } restore_tier{saved_tier};

    std::vector<cpu::CpuTier> tiers = {cpu::CpuTier::Portable};
    if (cpu::has_avx2_bmi2()) {
        tiers.push_back(cpu::CpuTier::V3);
    }
    if (cpu::has_avx512_v4()) {
        tiers.push_back(cpu::CpuTier::V4);
    }

    Board before_c5 = Board::initial();
    Player side = Player::Black;
    for (const char* mv : {"f5", "f6", "d3", "e3", "f3", "f4", "f7"}) {
        const auto sq = Square::parse(mv);
        STELLO_CHECK(sq.has_value());
        before_c5 = before_c5.play_unchecked(side, sq->index());
        side = opponent(side);
    }
    STELLO_CHECK(side == Player::White);
    const auto c5 = Square::parse("c5");
    const auto b5 = Square::parse("b5");
    STELLO_CHECK(c5.has_value() && b5.has_value());
    const Board wipe_board = before_c5.play_unchecked(Player::White, c5->index());
    STELLO_CHECK_EQ(wipe_board.discs(Player::Black), uint64_t{0x0020202038380000ULL});
    STELLO_CHECK_EQ(wipe_board.discs(Player::White), uint64_t{0x0000001c00000000ULL});

    const Board swapped(wipe_board.discs(Player::White), wipe_board.discs(Player::Black));
    // Black: c1 (2), White: b1 (1) | d1 (3) | h8 (63).
    // Playing a1 (0) flips b1 only: White survives at d1 & h8 with 0 legal moves, while Black can still play e1 (4).
    const Board pass_board(uint64_t{1} << 2, (uint64_t{1} << 1) | (uint64_t{1} << 3) | (uint64_t{1} << 63));
    const Square sq_a1(0);
    const Square sq_e1(4);

    for (const cpu::CpuTier tier : tiers) {
        STELLO_CHECK(cpu::set_forced_tier(tier));
        for (const bool selective : {false, true}) {
            SearchOptions opts;
            opts.selective = selective;
            search::SearchEngine engine(14, opts);

            // 1. Root look == 0 wipeout (f5 f6 d3 e3 f3 f4 f7 c5 -> b5).
            engine.clear_hash();
            const SearchResult r1 = engine.search_simple(wipe_board, Player::Black, SearchLimits::fixed_depth(1));
            STELLO_CHECK(r1.mv == Move(*b5));
            STELLO_CHECK_EQ(r1.score, search::WIN_SCORE + 64);
            STELLO_CHECK(r1.kind == ScoreKind::Heuristic);
            STELLO_CHECK_EQ(r1.depth, 1);

            // 2. Color-swapped equivalent (White to move).
            engine.clear_hash();
            const SearchResult r2 = engine.search_simple(swapped, Player::White, SearchLimits::fixed_depth(1));
            STELLO_CHECK(r2.mv == Move(*b5));
            STELLO_CHECK_EQ(r2.score, search::WIN_SCORE + 64);
            STELLO_CHECK(r2.kind == ScoreKind::Heuristic);
            STELLO_CHECK_EQ(r2.depth, 1);

            // 3. only_moves excluding b5 keeps legal move set & non-win heuristic score.
            const uint64_t all_moves = wipe_board.legal_moves(Player::Black);
            const uint64_t without_b5 = all_moves & ~b5->bit();
            STELLO_CHECK(without_b5 != 0ULL);
            engine.clear_hash();
            const auto r3 = engine.search(
                wipe_board, Player::Black, SearchLimits::fixed_depth(1), {}, search::no_stop(), without_b5);
            STELLO_CHECK(r3.ok);
            STELLO_CHECK(r3.result.mv != Move(*b5));
            STELLO_CHECK((without_b5 & r3.result.mv.square->bit()) != 0ULL);
            STELLO_CHECK(r3.result.score < search::WIN_SCORE);

            // 4. Interior search_inc look == 0 wipeout (playing c5 from before_c5 at depth 2).
            engine.clear_hash();
            const auto r4 = engine.search(
                before_c5, Player::White, SearchLimits::fixed_depth(2), {}, search::no_stop(), c5->bit());
            STELLO_CHECK(r4.ok);
            STELLO_CHECK(r4.result.mv == Move(*c5));
            STELLO_CHECK_EQ(r4.result.score, -(search::WIN_SCORE + 64));

            // 5. Non-wipeout pass at depth 1 (only_moves = a1 bypasses the 1-move shortcut and evaluates the depth-1 leaf).
            const Board after_a1 = pass_board.play_unchecked(Player::Black, sq_a1.index());
            STELLO_CHECK(after_a1.discs(Player::White) != 0ULL);
            STELLO_CHECK_EQ(after_a1.legal_moves(Player::White), uint64_t{0});
            STELLO_CHECK_EQ(after_a1.legal_moves(Player::Black), sq_e1.bit());
            engine.clear_hash();
            const auto r5 = engine.search(
                pass_board, Player::Black, SearchLimits::fixed_depth(1), {}, search::no_stop(), sq_a1.bit());
            STELLO_CHECK(r5.ok);
            STELLO_CHECK(r5.result.mv == Move(sq_a1));
            STELLO_CHECK(r5.result.kind == ScoreKind::Heuristic);
            STELLO_CHECK_EQ(r5.result.depth, 1);
            STELLO_CHECK(r5.result.nodes >= uint64_t{1});
            STELLO_CHECK(r5.result.score > -search::WIN_SCORE && r5.result.score < search::WIN_SCORE);
        }
    }
}

STELLO_TEST(phase4_3a1_nws_kernels_parity) {
    const cpu::CpuTier saved_tier = cpu::active_tier();
    struct RestoreTier {
        cpu::CpuTier tier;
        ~RestoreTier() { (void)cpu::set_forced_tier(tier); }
    } restore_tier{saved_tier};

    std::vector<cpu::CpuTier> tiers = {cpu::CpuTier::Portable};
    if (cpu::has_avx2_bmi2()) {
        tiers.push_back(cpu::CpuTier::V3);
    }
    if (cpu::has_avx512_v4()) {
        tiers.push_back(cpu::CpuTier::V4);
    }

    const auto [mid_board, mid_player] = find_bench_pos("midgame.txt", "g01e48");
    const auto c4_sq = Square::parse("c4");
    const auto h3_sq = Square::parse("h3");
    STELLO_CHECK(c4_sq.has_value() && h3_sq.has_value());
    const int32_t c4_idx = static_cast<int32_t>(c4_sq->index());
    const int32_t h3_idx = static_cast<int32_t>(h3_sq->index());

    // Pass position: Black c1, White b1 | d1 | h8 -> play a1 leaves White with 0 legal moves and Black with e1 only.
    const Board pass_board(uint64_t{1} << 2, (uint64_t{1} << 1) | (uint64_t{1} << 3) | (uint64_t{1} << 63));
    const Board after_a1 = pass_board.play_unchecked(Player::Black, 0U);
    STELLO_CHECK_EQ(after_a1.legal_moves(Player::White), uint64_t{0});
    STELLO_CHECK_EQ(after_a1.legal_moves(Player::Black), uint64_t{1} << 4);

    // Double-pass (game over) position: 62 Black discs, 0 White discs -> 0 legal moves on both sides.
    const Board terminal_board(0xFFFF'FFFF'FFFF'FFFCULL, 0ULL);
    STELLO_CHECK_EQ(terminal_board.legal_moves(Player::Black), uint64_t{0});
    STELLO_CHECK_EQ(terminal_board.legal_moves(Player::White), uint64_t{0});

    // Wipeout position (f5 f6 d3 e3 f3 f4 f7 c5 -> b5).
    Board before_c5 = Board::initial();
    Player side = Player::Black;
    for (const char* mv : {"f5", "f6", "d3", "e3", "f3", "f4", "f7"}) {
        const auto sq = Square::parse(mv);
        STELLO_CHECK(sq.has_value());
        before_c5 = before_c5.play_unchecked(side, sq->index());
        side = opponent(side);
    }
    const auto c5 = Square::parse("c5");
    const auto b5 = Square::parse("b5");
    STELLO_CHECK(c5.has_value() && b5.has_value());
    const Board wipe_board = before_c5.play_unchecked(Player::White, c5->index());

    auto unwrap = [](search::AbortResult<search::ScoreMove> r) -> search::ScoreMove {
#if STELLO_ABORT_EXCEPTIONS
        return r;
#else
        STELLO_CHECK(r.is_ok());
        return r.value;
#endif
    };

    SearchOptions opts;
    opts.selective = false;

    for (const cpu::CpuTier tier : tiers) {
        STELLO_CHECK(cpu::set_forced_tier(tier));
        search::TranspositionTable table(12);
        search::ThreadState thread;

        auto make_ctx = [&](const Board& b) {
            return search::SearchContext::create(
                opts, nullptr, nullptr, table, table, search::system_clock, b.empty_count());
        };
        auto score_move_at_horizon = [&](const search::SearchContext& ctx, const Board& b, Player p, int32_t look, int32_t mv) -> int32_t {
            const Player opp = opponent(p);
            const uint64_t own = b.discs(p);
            const uint64_t other = b.discs(opp);
            const uint64_t f = endgame::bits::flips(own, other, static_cast<uint32_t>(mv));
            const Board child = b.play_flips(p, static_cast<uint32_t>(mv), f);
            if (look == 0 && std::popcount(b.legal_moves(p)) > 1) {
                if (f == other) {
                    return search::WIN_SCORE + 64;
                }
                return -eval::pattern_score(eval::EvalWeights::built_in(), child.discs(opp), child.discs(p));
            }
            search::Searcher s_chk(ctx, thread, search::no_stop());
            s_chk.hash_get_height = 0;
            s_chk.hash_put_height = 0;
            return -unwrap(s_chk.search(child, opp, (look == 0 ? 0 : look - 1), 2, mv, -32665, 32665)).score;
        };
        auto ref_full_window = [&](const search::SearchContext& ctx, const Board& b, Player p, int32_t look) -> int32_t {
            search::Searcher s_ref(ctx, thread, search::no_stop());
            s_ref.hash_get_height = 0;
            s_ref.hash_put_height = 0;
            return unwrap(s_ref.search(b, p, look, 1, -1, -32665, 32665)).score;
        };
        auto check_scout = [&](const search::SearchContext& ctx, const Board& b, Player p, int32_t look,
                               int32_t hash_get_h, int32_t hash_put_h, int32_t alpha, int32_t v, bool expect_descend) {
            search::Searcher s(ctx, thread, search::no_stop());
            s.hash_get_height = hash_get_h;
            s.hash_put_height = hash_put_h;
            const search::ScoreMove nws = unwrap(s.search(b, p, look, 1, -1, alpha, alpha + 1));
            const uint64_t legal = b.legal_moves(p);
            if (v > alpha) {
                STELLO_CHECK(nws.score > alpha && nws.score <= v);
                if (legal != 0ULL) {
                    STELLO_CHECK(nws.mv >= 0 && nws.mv < 64 && (legal & (uint64_t{1} << nws.mv)) != 0ULL);
                    STELLO_CHECK(score_move_at_horizon(ctx, b, p, look, nws.mv) >= nws.score);
                }
            } else {
                STELLO_CHECK(nws.score >= v && nws.score <= alpha);
            }
            if (expect_descend) {
                STELLO_CHECK(s.nodes > uint64_t{1});
                STELLO_CHECK(s.evaluations > uint64_t{0});
            }
            return std::pair<search::ScoreMove, search::Searcher*>{nws, nullptr};
        };

        const auto ctx_mid = make_ctx(mid_board);
        const auto ctx_a1 = make_ctx(after_a1);
        const auto ctx_term = make_ctx(terminal_board);
        const auto ctx_wipe = make_ctx(wipe_board);
        const auto ctx_bc5 = make_ctx(before_c5);

        const uint64_t mid_own = mid_board.discs(mid_player);
        const uint64_t mid_opp = mid_board.discs(opponent(mid_player));
        const int32_t mid_tag = static_cast<int32_t>(player_index(mid_player));
        const uint32_t mid_empties = mid_board.empty_count();

        for (const int32_t look : {0, 1, 2}) {
            // 1. Standard midgame: full-window reference computed with TT reads and writes disabled.
            const int32_t v_mid = ref_full_window(ctx_mid, mid_board, mid_player, look);

            // 1a. Cold empty table: scouts must actually descend and satisfy strict fail-soft bounds & move soundness.
            for (const int32_t alpha : {v_mid - 200, v_mid - 1, v_mid, v_mid + 1, v_mid + 200}) {
                table.clear();
                (void)check_scout(ctx_mid, mid_board, mid_player, look, 4, 4, alpha, v_mid, true);
            }

            // 1b. Compatible one-sided TT bounds that cannot cut: must descend rather than returning seeded value.
            table.clear();
            table.store_at_root(
                mid_own, mid_opp, mid_tag, look, search::Bound::Upper, v_mid + 20, h3_idx, mid_empties, 0, search::SEL_EXACT);
            (void)check_scout(ctx_mid, mid_board, mid_player, look, 4, 4, v_mid - 1, v_mid, true);

            table.clear();
            table.store_at_root(
                mid_own, mid_opp, mid_tag, look, search::Bound::Lower, v_mid - 20, h3_idx, mid_empties, 0, search::SEL_EXACT);
            (void)check_scout(ctx_mid, mid_board, mid_player, look, 4, 4, v_mid, v_mid, true);

            // 1c. Legal move hint whose exact score cannot cut (insufficient depth for look=1,2; insufficient sel for look=0).
            table.clear();
            if (look == 0) {
                table.store_at_root(
                    mid_own, mid_opp, mid_tag, 0, search::Bound::Exact, v_mid + 500, h3_idx, mid_empties, 0,
                    search::SEL_MIDGAME_MPC);
            } else {
                table.store_at_root(
                    mid_own, mid_opp, mid_tag, look - 1, search::Bound::Exact, v_mid + 500, h3_idx, mid_empties, 0,
                    search::SEL_EXACT);
            }
            (void)check_scout(ctx_mid, mid_board, mid_player, look, 4, 4, v_mid, v_mid, true);

            // 1d. Separate 1-node exact TT hit test.
            for (const int32_t alpha : {v_mid - 1, v_mid}) {
                table.clear();
                table.store_at_root(
                    mid_own, mid_opp, mid_tag, look, search::Bound::Exact, v_mid, c4_idx, mid_empties, 0, search::SEL_EXACT);
                search::Searcher s_hit(ctx_mid, thread, search::no_stop());
                s_hit.hash_get_height = 4;
                s_hit.hash_put_height = 4;
                const search::ScoreMove hit = unwrap(s_hit.search(mid_board, mid_player, look, 1, -1, alpha, alpha + 1));
                STELLO_CHECK_EQ(s_hit.nodes, uint64_t{1});
                STELLO_CHECK_EQ(s_hit.evaluations, uint64_t{0});
                STELLO_CHECK_EQ(hit.score, v_mid);
                STELLO_CHECK_EQ(hit.mv, c4_idx);
            }

            // 2. Pass position (White passes to Black e1 only).
            const int32_t v_pass = ref_full_window(ctx_a1, after_a1, Player::White, look);
            for (const int32_t alpha : {v_pass - 1, v_pass}) {
                table.clear();
                (void)check_scout(ctx_a1, after_a1, Player::White, look, 4, 4, alpha, v_pass, false);
            }

            // 3. Double-pass / game-over terminal score.
            const int32_t v_term = ref_full_window(ctx_term, terminal_board, Player::Black, look);
            STELLO_CHECK(v_term > 0);
            for (const int32_t alpha : {v_term - 1, v_term}) {
                table.clear();
                (void)check_scout(ctx_term, terminal_board, Player::Black, look, 4, 4, alpha, v_term, false);
            }

            // 4. Forced 1-move extension at look == 0 (after_a1, Black e1 only).
            const int32_t v_1mv = ref_full_window(ctx_a1, after_a1, Player::Black, look);
            for (const int32_t alpha : {v_1mv - 1, v_1mv}) {
                table.clear();
                (void)check_scout(ctx_a1, after_a1, Player::Black, look, 4, 4, alpha, v_1mv, false);
            }
        }

        // Concrete g01e48 look=0 ply=1 regression: c4=-193, h3=-1131; seed Upper(-173, hint=h3) and scout [-194, -193].
        {
            const int32_t v0 = ref_full_window(ctx_mid, mid_board, mid_player, 0);
            STELLO_CHECK_EQ(v0, -193);
            table.clear();
            table.store_at_root(
                mid_own, mid_opp, mid_tag, 0, search::Bound::Upper, -173, h3_idx, mid_empties, 0, search::SEL_EXACT);
            search::Searcher s_reg(ctx_mid, thread, search::no_stop());
            s_reg.hash_get_height = 4;
            s_reg.hash_put_height = 4;
            const search::ScoreMove reg = unwrap(s_reg.search(mid_board, mid_player, 0, 1, -1, -194, -193));
            STELLO_CHECK(s_reg.nodes > uint64_t{1});
            STELLO_CHECK(s_reg.evaluations > uint64_t{0});
            STELLO_CHECK_EQ(reg.score, -193);
            const uint64_t legal = mid_board.legal_moves(mid_player);
            STELLO_CHECK(reg.mv >= 0 && reg.mv < 64 && (legal & (uint64_t{1} << reg.mv)) != 0ULL);
            STELLO_CHECK(score_move_at_horizon(ctx_mid, mid_board, mid_player, 0, reg.mv) >= -193);
        }

        // 5. Wipeout recognition in eval1_nws (look == 0) and propagation in eval2_nws / search_inc (look == 1, 2).
        table.clear();
        search::Searcher s_wipe(ctx_wipe, thread, search::no_stop());
        s_wipe.hash_get_height = 4;
        s_wipe.hash_put_height = 4;
        const search::ScoreMove w0 = unwrap(s_wipe.search(wipe_board, Player::Black, 0, 1, -1, search::WIN_SCORE, search::WIN_SCORE + 1));
        STELLO_CHECK_EQ(w0.score, search::WIN_SCORE + 64);
        STELLO_CHECK_EQ(w0.mv, static_cast<int32_t>(b5->index()));

        for (const int32_t look : {1, 2}) {
            const int32_t v_wb = ref_full_window(ctx_bc5, before_c5, Player::White, look);
            for (const int32_t alpha : {v_wb - 1, v_wb}) {
                table.clear();
                (void)check_scout(ctx_bc5, before_c5, Player::White, look, 4, 4, alpha, v_wb, false);
            }
        }
    }
}

} // namespace stello::tests
