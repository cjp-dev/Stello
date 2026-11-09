#include "tests/test_harness.hpp"

#include "cli/bench.hpp"
#include "core/bits.hpp"
#include "core/board.hpp"
#include "core/square.hpp"
#include "search/engine.hpp"
#include "search/limits.hpp"
#include "search/options.hpp"
#include "search/pool.hpp"
#include "search/table.hpp"

#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define STELLO_TESTS_ASAN 1
#endif
#endif
#ifndef STELLO_TESTS_ASAN
#define STELLO_TESTS_ASAN 0
#endif

namespace stello::tests {

namespace {

std::vector<cli::BenchPosition> load_bench_file(const char* name) {
    std::vector<cli::BenchPosition> positions;
    std::string err;
    const auto path = repo_root() / "Stello.Net" / "tools" / "Stello.Benchmark" / "Data" / name;
    const bool ok = cli::load_positions(path, positions, err);
    STELLO_CHECK_MSG(ok, err);
    return positions;
}

SearchLimits with_threads(SearchLimits base, uint32_t count) {
    const auto lim = base.with_threads(count);
    STELLO_CHECK(lim.has_value());
    return *lim;
}

std::optional<std::pair<Board, Player>> random_position(uint64_t& seed, uint32_t empties) {
    auto next_rand = [&](uint32_t n) -> size_t {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        return static_cast<size_t>(seed % static_cast<uint64_t>(n));
    };

    Board board = Board::initial();
    Player player = Player::Black;
    while (board.empty_count() > empties && !board.is_game_over()) {
        const uint64_t mask = board.legal_moves(player);
        std::vector<Square> moves;
        for (const Square sq : Square::in_mask(mask)) {
            moves.push_back(sq);
        }
        if (!moves.empty()) {
            board = board.play_unchecked(player, moves[next_rand(static_cast<uint32_t>(moves.size()))].index());
        }
        player = opponent(player);
    }

    if (board.empty_count() == empties && board.legal_moves(player) != 0) {
        return std::make_pair(board, player);
    }
    return std::nullopt;
}

int32_t score_for(const Board& board, Player player) {
    search::SearchEngine engine(16);
    if (board.legal_moves(opponent(player)) != 0) {
        return -engine.search_simple(board, opponent(player), SearchLimits::solve()).score;
    }
    STELLO_CHECK(board.legal_moves(player) != 0);
    return engine.search_simple(board, player, SearchLimits::solve()).score;
}

struct FieldTuple {
    int32_t value;
    int32_t depth;
    search::Bound bound;
    int32_t mv;
};

FieldTuple seqlock_fields(uint64_t own, uint64_t opponent) noexcept {
    const uint64_t hash = (own ^ std::rotl(opponent, 17)) * 0x9E37'79B9'7F4A'7C15ULL;
    constexpr std::array<search::Bound, 3> bounds = {
        search::Bound::Exact,
        search::Bound::Lower,
        search::Bound::Upper,
    };
    return {
        static_cast<int32_t>(static_cast<int16_t>(hash >> 16)),
        static_cast<int32_t>((hash >> 40) & 63ULL),
        bounds[static_cast<size_t>(hash >> 7) % 3U],
        static_cast<int32_t>((hash >> 50) & 63ULL),
    };
}

bool fails_after_one_spawner(size_t index, std::function<void()> body) {
    if (index == 0) {
        return search::std_spawner(index, std::move(body));
    }
    return false;
}

} // namespace

STELLO_TEST(seqlock_many_threads_never_read_a_torn_entry) {
    constexpr uint64_t THREADS = 8;
    constexpr uint64_t ROUNDS = 1'000'000;
    constexpr uint64_t KEYS = 2048;

    search::TranspositionTable table(10);
    std::atomic<uint64_t> hits{0};
    {
        std::vector<std::thread> workers;
        workers.reserve(THREADS);
        for (uint64_t thread = 0; thread < THREADS; ++thread) {
            workers.emplace_back([thread, &table, &hits]() {
                uint64_t seed = 0x2545'F491'4F6C'DD1DULL ^ (thread * 0x9E37'79B9'7F4A'7C15ULL);
                for (uint64_t r = 0; r < ROUNDS; ++r) {
                    seed ^= seed << 13;
                    seed ^= seed >> 7;
                    seed ^= seed << 17;
                    const uint64_t key = seed % KEYS;
                    const uint64_t own = key * 0x0101'0101'0101'0101ULL;
                    const uint64_t opp = (key << 1) | (uint64_t{1} << 63);
                    const int32_t tag = static_cast<int32_t>((seed >> 20) & 1ULL);
                    const FieldTuple expected = seqlock_fields(own, opp ^ static_cast<uint64_t>(tag));
                    if (((seed >> 33) & 1ULL) == 0) {
                        table.store(own, opp, tag, expected.depth, expected.bound, expected.value, expected.mv);
                    } else if (const auto entry = table.get(own, opp, tag)) {
                        STELLO_CHECK_EQ(entry->own, own);
                        STELLO_CHECK_EQ(entry->opponent, opp);
                        STELLO_CHECK_EQ(static_cast<int32_t>(entry->tag), tag);
                        STELLO_CHECK_EQ(static_cast<int32_t>(entry->value), expected.value);
                        STELLO_CHECK_EQ(static_cast<int32_t>(entry->depth), expected.depth);
                        STELLO_CHECK(entry->bound == expected.bound);
                        STELLO_CHECK_EQ(static_cast<int32_t>(entry->mv), expected.mv);
                        hits.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }
        for (auto& w : workers) {
            w.join();
        }
    }

    const uint64_t hit_count = hits.load(std::memory_order_relaxed);
    STELLO_CHECK_MSG(hit_count > (THREADS * ROUNDS) / 20ULL, std::to_string(hit_count) + " hits");
}

STELLO_TEST(thread_pool_unit_tests) {
    // a_pool_without_workers_runs_nothing_else
    {
        search::ThreadPool<uint32_t> pool(0, &search::std_spawner, []() { return uint32_t{0}; });
        STELLO_CHECK_EQ(pool.workers(), size_t{0});
        const bool ran = pool.scope([](const search::Scope<uint32_t>& scope) {
            return scope.try_spawn([](uint32_t&) { STELLO_CHECK_MSG(false, "no worker"); });
        });
        STELLO_CHECK(!ran);
    }

    // tasks_borrow_from_outside_and_finish_before_the_scope_returns
    {
        search::ThreadPool<uint64_t> pool(3, &search::std_spawner, []() { return uint64_t{0}; });
        while (pool.idle() < 3) {
            std::this_thread::yield();
        }

        std::atomic<uint64_t> total{0};
        std::atomic<bool> release{false};
        const size_t started = pool.scope([&](const search::Scope<uint64_t>& scope) {
            size_t count = 0;
            for (uint64_t i = 1; i <= 3; ++i) {
                if (scope.try_spawn([&total, &release, i](uint64_t& worker_count) {
                        while (!release.load(std::memory_order_acquire)) {
                            std::this_thread::yield();
                        }
                        worker_count += 1;
                        total.fetch_add(i, std::memory_order_relaxed);
                    }))
                {
                    ++count;
                }
            }
            STELLO_CHECK(!scope.try_spawn([](uint64_t&) { STELLO_CHECK_MSG(false, "no idle worker"); }));
            release.store(true, std::memory_order_release);
            return count;
        });

        STELLO_CHECK_EQ(started, size_t{3});
        STELLO_CHECK_EQ(total.load(std::memory_order_relaxed), uint64_t{6});
        STELLO_CHECK_EQ(pool.idle(), size_t{3});

        uint64_t tasks = 0;
        pool.for_each_state([&tasks](uint64_t& count) { tasks += count; });
        STELLO_CHECK_EQ(tasks, uint64_t{3});
    }

    // long_tasks_stop_through_a_flag_they_borrow
    {
        search::ThreadPool<int> pool(2, &search::std_spawner, []() { return 0; });
        std::atomic<bool> stop{false};
        std::atomic<uint64_t> spins{0};
        pool.scope([&](const search::Scope<int>& scope) {
            for (int i = 0; i < 2; ++i) {
                STELLO_CHECK(scope.try_spawn([&](int&) {
                    while (!stop.load(std::memory_order_relaxed)) {
                        spins.fetch_add(1, std::memory_order_relaxed);
                        std::this_thread::yield();
                    }
                }));
            }
            while (spins.load(std::memory_order_relaxed) < 1000) {
                std::this_thread::yield();
            }
            stop.store(true, std::memory_order_relaxed);
        });
        STELLO_CHECK(spins.load(std::memory_order_relaxed) >= 1000);
    }

    // a_polling_scope_polls_while_it_waits_and_can_stop_its_tasks
    {
        search::ThreadPool<int> pool(1, &search::std_spawner, []() { return 0; });
        std::atomic<bool> stop{false};
        size_t polls = 0;
        pool.scope_polling(
            [&](const search::Scope<int>& scope) {
                STELLO_CHECK(scope.try_spawn([&](int&) {
                    while (!stop.load(std::memory_order_relaxed)) {
                        std::this_thread::yield();
                    }
                }));
            },
            [&]() {
                ++polls;
                if (polls == 3) {
                    stop.store(true, std::memory_order_relaxed);
                }
            });
        STELLO_CHECK(polls >= 3);
    }

    // an_exception_in_a_task_reaches_the_scope_and_the_pool_lives_on
    {
        search::ThreadPool<int> pool(1, &search::std_spawner, []() { return 0; });
        bool caught = false;
        try {
            pool.scope([](const search::Scope<int>& scope) {
                STELLO_CHECK(scope.try_spawn([](int&) { throw std::runtime_error("task failed"); }));
            });
        } catch (const std::runtime_error&) {
            caught = true;
        }
        STELLO_CHECK(caught);

        std::atomic<bool> ran{false};
        pool.scope([&](const search::Scope<int>& scope) {
            STELLO_CHECK(scope.try_spawn([&](int&) { ran.store(true, std::memory_order_relaxed); }));
        });
        STELLO_CHECK(ran.load(std::memory_order_relaxed));
    }

    // a_spawner_that_fails_leaves_the_workers_that_started
    {
        search::ThreadPool<int> pool(4, &fails_after_one_spawner, []() { return 0; });
        STELLO_CHECK_EQ(pool.workers(), size_t{1});
    }
}

STELLO_TEST(threads_solving_different_positions_on_one_table_get_exact_scores) {
    std::vector<std::pair<Board, Player>> endings;
    uint64_t seed = 0x9E37'79B9'7F4A'7C15ULL;
    while (endings.size() < 48) {
        const uint32_t empties = 12U + static_cast<uint32_t>(endings.size() % 7U);
        Board board = Board::initial();
        Player player = Player::Black;
        while (board.empty_count() > empties && !board.is_game_over()) {
            uint64_t moves = board.legal_moves(player);
            if (moves != 0) {
                seed ^= seed << 13;
                seed ^= seed >> 7;
                seed ^= seed << 17;
                const uint64_t pick = seed % static_cast<uint64_t>(std::popcount(moves));
                for (uint64_t k = 0; k < pick; ++k) {
                    moves &= (moves - 1ULL);
                }
                board = board.play_unchecked(player, static_cast<uint32_t>(std::countr_zero(moves)));
            }
            player = opponent(player);
        }
        if (board.empty_count() == empties && board.legal_moves(player) != 0) {
            endings.emplace_back(board, player);
        }
    }

    std::vector<int32_t> expected;
    expected.reserve(endings.size());
    for (const auto& [board, player] : endings) {
        expected.push_back(search::SearchEngine(16).search_simple(board, player, SearchLimits::solve()).score);
    }

    constexpr size_t THREADS = 8;
    SearchOptions opts;
    search::SearchEngine engine(10, opts);
    search::ThreadPool<search::ThreadState> pool(THREADS - 1, &search::std_spawner, []() {
        return search::ThreadState{};
    });
    const auto probcut = search::detail::make_probcut_errors(static_cast<double>(search::DEFAULT_PROBCUT_PERCENT) / 100.0);
    const search::SearchContext context = search::SearchContext::create(
        opts, probcut.get(), engine.midgame_table(), engine.endgame_table(), &search::system_clock, 64);

    auto solve_bucket = [&](size_t thread_idx, search::ThreadState& state) {
        std::vector<std::pair<size_t, int32_t>> scores;
        for (int pass = 0; pass < 2; ++pass) {
            for (size_t i = thread_idx; i < endings.size(); i += THREADS) {
                search::Searcher s(context, state, search::no_stop());
                const auto outcome = s.run(endings[i].first, endings[i].second, SearchLimits::solve());
                STELLO_CHECK(outcome.ok);
                scores.emplace_back(i, outcome.result.score);
            }
        }
        return scores;
    };

    std::mutex results_mutex;
    std::vector<std::pair<size_t, int32_t>> results;
    search::ThreadState main_state;
    pool.scope([&](const search::Scope<search::ThreadState>& scope) {
        for (size_t t = 1; t < THREADS; ++t) {
            STELLO_CHECK(scope.try_spawn([&, t](search::ThreadState& state) {
                auto scores = solve_bucket(t, state);
                std::lock_guard<std::mutex> lk(results_mutex);
                results.insert(results.end(), scores.begin(), scores.end());
            }));
        }
        auto main_scores = solve_bucket(0, main_state);
        std::lock_guard<std::mutex> lk(results_mutex);
        results.insert(results.end(), main_scores.begin(), main_scores.end());
    });

    STELLO_CHECK_EQ(results.size(), 2U * endings.size());
    for (const auto& [idx, score] : results) {
        STELLO_CHECK_EQ_MSG(score, expected[idx], endings[idx].first.to_string());
    }
}

STELLO_TEST(several_threads_solve_ffo_exactly) {
    const auto positions = load_bench_file("ffo-40-59.txt");
    search::SearchEngine engine(19);
    for (const uint32_t count : {2U, 4U, 8U, 16U}) {
        for (const auto& pos : positions) {
            if (pos.id != "40" && pos.id != "41" && pos.id != "42" && pos.id != "43" && pos.id != "44") {
                continue;
            }
            engine.clear_hash();
            const SearchResult result = engine.search_simple(
                pos.board, pos.player, with_threads(SearchLimits::solve(), count));
            STELLO_CHECK(pos.score.has_value());
            STELLO_CHECK_EQ_MSG(result.score, *pos.score, pos.id + " threads=" + std::to_string(count));
            STELLO_CHECK(result.kind == ScoreKind::Exact);
            STELLO_CHECK(result.mv.square.has_value());
            const Board after = pos.board.play_unchecked(pos.player, result.mv.square->index());
            STELLO_CHECK_EQ_MSG(score_for(after, pos.player), *pos.score, pos.id + " mv=" + result.mv.to_string());
        }
    }
}

STELLO_TEST(several_threads_solve_random_endings_like_one) {
    search::SearchEngine threaded(16);
    search::SearchEngine single(16);
    uint64_t seed = 0x2545'F491'4F6C'DD1DULL;
    uint32_t checked = 0;
    while (checked < 40) {
        const auto pos = random_position(seed, 14U + (checked % 7U));
        if (!pos.has_value()) {
            continue;
        }
        single.clear_hash();
        const SearchResult expected = single.search_simple(pos->first, pos->second, SearchLimits::solve());
        const SearchResult result = threaded.search_simple(
            pos->first, pos->second, with_threads(SearchLimits::solve(), 4));
        STELLO_CHECK_EQ_MSG(result.score, expected.score, pos->first.to_string());
        STELLO_CHECK(result.kind == expected.kind);
        STELLO_CHECK_EQ(result.depth, expected.depth);
        ++checked;
    }
}

STELLO_TEST(several_threads_search_midgame_and_preserve_single_thread_parity) {
    const auto positions = load_bench_file("midgame.txt");

    // several_threads_search_the_midgame
    {
        search::SearchEngine engine(18);
        for (size_t i = 0; i < positions.size(); i += 4) {
            const uint32_t count = ((i / 4) % 2 == 0) ? 3U : 6U;
            const SearchResult result = engine.search_simple(
                positions[i].board,
                positions[i].player,
                with_threads(SearchLimits::fixed_depth(9), count));
            STELLO_CHECK(result.kind == ScoreKind::Heuristic);
            STELLO_CHECK_EQ(result.depth, 9);
            STELLO_CHECK(result.mv.square.has_value());
            STELLO_CHECK((positions[i].board.legal_moves(positions[i].player) & result.mv.square->bit()) != 0);
            STELLO_CHECK(result.score > -(search::WIN_SCORE + 64) && result.score < (search::WIN_SCORE + 64));
        }
    }

    // several_threads_count_the_nodes_of_all
    {
        search::SearchEngine engine(18);
        const SearchResult one = engine.search_simple(positions[0].board, positions[0].player, SearchLimits::fixed_depth(10));
        engine.clear_hash();
        const SearchResult four = engine.search_simple(
            positions[0].board, positions[0].player, with_threads(SearchLimits::fixed_depth(10), 4));
        STELLO_CHECK(four.nodes > one.nodes);
    }

    // one_thread_after_several_is_unchanged
    {
        search::SearchEngine engine(16);
        for (size_t i = 0; i < positions.size(); i += 10) {
            (void)engine.search_simple(
                positions[i].board, positions[i].player, with_threads(SearchLimits::fixed_depth(8), 4));
            engine.clear_hash();
            const SearchResult after = engine.search_simple(
                positions[i].board, positions[i].player, SearchLimits::fixed_depth(8));
            const SearchResult fresh = search::SearchEngine(16).search_simple(
                positions[i].board, positions[i].player, SearchLimits::fixed_depth(8));
            STELLO_CHECK(after.mv == fresh.mv);
            STELLO_CHECK_EQ(after.score, fresh.score);
            STELLO_CHECK_EQ_MSG(after.nodes, fresh.nodes, positions[i].id);
            STELLO_CHECK_EQ(after.evaluations, fresh.evaluations);
        }
    }
}

STELLO_TEST(stop_signals_and_time_per_move_with_eight_threads) {
    // stop_signals_stop_all_threads on FFO #49
    const auto ffo = load_bench_file("ffo-40-59.txt");
    const auto* ffo49 = [&]() -> const cli::BenchPosition* {
        for (const auto& p : ffo) {
            if (p.id == "49") {
                return &p;
            }
        }
        return nullptr;
    }();
    STELLO_CHECK(ffo49 != nullptr);

    search::SearchEngine engine(18);
    // Warm up the 8-thread pool before measuring stop latency.
    (void)engine.search_simple(Board::initial(), Player::Black, with_threads(SearchLimits::fixed_depth(4), 8));

    for (const bool move_now : {true, false}) {
        search::SearchSignals signals;
        std::chrono::steady_clock::time_point signalled{};
        std::thread setter([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            signalled = std::chrono::steady_clock::now();
            if (move_now) {
                signals.move_now();
            } else {
                signals.cancel();
            }
        });

        engine.clear_hash();
        const auto outcome = engine.search(
            ffo49->board, ffo49->player, with_threads(SearchLimits::solve(), 8), {}, signals);
        const auto returned = std::chrono::steady_clock::now();
        setter.join();
        const auto lag = std::chrono::duration_cast<std::chrono::milliseconds>(returned - signalled);
        const int64_t max_lag_ms = STELLO_TESTS_ASAN ? 500 : 200;
        STELLO_CHECK_MSG(lag.count() < max_lag_ms, "lag=" + std::to_string(lag.count()) + "ms");
        if (move_now) {
            STELLO_CHECK(outcome.ok);
            STELLO_CHECK(outcome.result.mv.square.has_value());
            STELLO_CHECK((ffo49->board.legal_moves(ffo49->player) & outcome.result.mv.square->bit()) != 0);
        } else {
            STELLO_CHECK(!outcome.ok);
            STELLO_CHECK(outcome.error == search::SearchError::Cancelled);
        }
    }

    const SearchResult after = engine.search_simple(
        Board::initial(), Player::Black, with_threads(SearchLimits::fixed_depth(6), 8));
    STELLO_CHECK_EQ(after.depth, 6);

    // several_threads_keep_a_time_per_move
    const auto midgame = load_bench_file("midgame.txt");
    for (size_t i = 0; i < midgame.size(); i += 25) {
        const auto t0 = std::chrono::steady_clock::now();
        const SearchResult res = engine.search_simple(
            midgame[i].board, midgame[i].player, with_threads(SearchLimits::time_per_move_ms(150), 8));
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0);
        const int64_t limit_ms = STELLO_TESTS_ASAN ? (150 + 150) : (150 + 40);
        STELLO_CHECK_MSG(elapsed.count() < limit_ms, midgame[i].id + " elapsed=" + std::to_string(elapsed.count()) + "ms");
        STELLO_CHECK(res.depth >= 4);
        STELLO_CHECK(res.mv.square.has_value());
        STELLO_CHECK((midgame[i].board.legal_moves(midgame[i].player) & res.mv.square->bit()) != 0);
    }
}

STELLO_TEST(several_threads_handover_few_empties_splits_and_lazy_smp) {
    // several_threads_hand_over_to_the_solver_like_one
    {
        const auto positions = load_bench_file("midgame-handover.txt");
        for (const bool selective : {true, false}) {
            SearchOptions opts;
            opts.selective = selective;
            search::SearchEngine single(16, opts);
            search::SearchEngine threaded(16, opts);
            int32_t solved = 0;
            size_t taken = 0;
            for (const auto& pos : positions) {
                if (pos.board.empty_count() > 20) {
                    continue;
                }
                if (taken >= 24) {
                    break;
                }
                ++taken;
                single.clear_hash();
                threaded.clear_hash();
                const SearchResult one = single.search_simple(pos.board, pos.player, SearchLimits::fixed_depth(12));
                const SearchResult four = threaded.search_simple(
                    pos.board, pos.player, with_threads(SearchLimits::fixed_depth(12), 4));
                STELLO_CHECK_MSG(one.kind == four.kind, pos.id);
                if (one.kind == ScoreKind::Exact) {
                    STELLO_CHECK_EQ_MSG(four.score, one.score, pos.id);
                    ++solved;
                } else if (one.kind == ScoreKind::WinLossDraw) {
                    const int s_one = (one.score > 0) - (one.score < 0);
                    const int s_four = (four.score > 0) - (four.score < 0);
                    STELLO_CHECK_EQ_MSG(s_four, s_one, pos.id);
                    ++solved;
                }
            }
            STELLO_CHECK(solved >= 6);
        }
    }

    // several_threads_with_few_empty_squares_solve_like_one
    {
        uint64_t seed = 0x2545'F491'4F6C'DD1DULL;
        search::SearchEngine single(14);
        search::SearchEngine threaded(14);
        for (uint32_t empties = 1; empties <= 10; ++empties) {
            for (int rep = 0; rep < 4; ++rep) {
                const auto pos = random_position(seed, empties);
                if (!pos.has_value()) {
                    continue;
                }
                for (const SearchLimits limits : {SearchLimits::fixed_depth(12), SearchLimits::time_per_move_ms(100)}) {
                    const SearchResult one = single.search_simple(pos->first, pos->second, limits);
                    const SearchResult eight = threaded.search_simple(
                        pos->first, pos->second, with_threads(limits, 8));
                    STELLO_CHECK_EQ(eight.score, one.score);
                    STELLO_CHECK(eight.kind == one.kind);
                }
            }
        }
    }

    // split_solves_give_the_exact_score
    {
        uint64_t seed = 0x5DEE'CE66'D1CE'4E5BULL;
        search::SearchEngine single(18);
        uint32_t checked = 0;
        while (checked < 8) {
            const auto pos = random_position(seed, 19U + (checked % 3U));
            if (!pos.has_value()) {
                continue;
            }
            single.clear_hash();
            const SearchResult expected = single.search_simple(pos->first, pos->second, SearchLimits::solve());
            struct Case {
                uint32_t count;
                bool selective;
            };
            const std::array<Case, 3> cases = {{{3, true}, {8, false}, {16, true}}};
            for (const auto& c : cases) {
                SearchOptions opts;
                opts.selective = c.selective;
                search::SearchEngine threaded(12, opts);
                const SearchResult result = threaded.search_simple(
                    pos->first, pos->second, with_threads(SearchLimits::solve(), c.count));
                STELLO_CHECK_EQ_MSG(result.score, expected.score, pos->first.to_string() + " threads=" + std::to_string(c.count));
                STELLO_CHECK(result.kind == expected.kind);
                STELLO_CHECK(result.mv.square.has_value());
                const Board after = pos->first.play_unchecked(pos->second, result.mv.square->index());
                STELLO_CHECK_EQ(score_for(after, pos->second), expected.score);
            }
            ++checked;
        }
    }

    // lazy_smp_alone_solves_exactly
    {
        const auto positions = load_bench_file("ffo-40-59.txt");
        SearchOptions opts;
        opts.ybwc = false;
        search::SearchEngine engine(18, opts);
        for (const auto& pos : positions) {
            if (pos.id != "40" && pos.id != "41" && pos.id != "42") {
                continue;
            }
            engine.clear_hash();
            const SearchResult result = engine.search_simple(
                pos.board, pos.player, with_threads(SearchLimits::solve(), 6));
            STELLO_CHECK(pos.score.has_value());
            STELLO_CHECK_EQ_MSG(result.score, *pos.score, pos.id);
            STELLO_CHECK(result.kind == ScoreKind::Exact);
            const SearchResult midgame = engine.search_simple(
                pos.board, pos.player, with_threads(SearchLimits::fixed_depth(8), 6));
            STELLO_CHECK(midgame.mv.square.has_value());
            STELLO_CHECK((pos.board.legal_moves(pos.player) & midgame.mv.square->bit()) != 0);
        }
    }
}

} // namespace stello::tests
