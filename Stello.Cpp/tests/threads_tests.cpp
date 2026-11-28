#include "tests/test_harness.hpp"

#include "cli/bench.hpp"
#include "core/bits.hpp"
#include "core/board.hpp"
#include "core/cpu.hpp"
#include "core/square.hpp"
#include "search/engine.hpp"
#include "search/limits.hpp"
#include "search/options.hpp"
#include "search/pool.hpp"
#include "search/table.hpp"
#include "search/ybwc.hpp"

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
                        STELLO_CHECK_EQ(entry->verify, table.verify_key(own, opp, tag));
                        STELLO_CHECK_EQ(static_cast<int32_t>(entry->tag), tag);
                        STELLO_CHECK_EQ(static_cast<int32_t>(entry->depth), expected.depth);
                        if (expected.bound == search::Bound::Exact) {
                            STELLO_CHECK_EQ(static_cast<int32_t>(entry->lower), expected.value);
                            STELLO_CHECK_EQ(static_cast<int32_t>(entry->upper), expected.value);
                        } else if (expected.bound == search::Bound::Lower) {
                            STELLO_CHECK_EQ(static_cast<int32_t>(entry->lower), expected.value);
                            STELLO_CHECK_EQ(static_cast<int32_t>(entry->upper), static_cast<int32_t>(search::TT_SCORE_MAX));
                        } else {
                            STELLO_CHECK_EQ(static_cast<int32_t>(entry->lower), static_cast<int32_t>(search::TT_SCORE_MIN));
                            STELLO_CHECK_EQ(static_cast<int32_t>(entry->upper), expected.value);
                        }
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
        opts, probcut.get(), nullptr, engine.midgame_table(), engine.endgame_table(), &search::system_clock, 64);

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

STELLO_TEST(phase2_root_split_brother_cut_and_split_heartbeats) {
    // Root splitting and brother cut produce exact scores on FFO #40-#44 and random 19-21 empty positions.
    const auto ffo = load_bench_file("ffo-40-59.txt");
    {
        search::SearchEngine engine(19);
        for (const auto& pos : ffo) {
            if (pos.id != "40" && pos.id != "41" && pos.id != "42" && pos.id != "43" && pos.id != "44") {
                continue;
            }
            engine.clear_hash();
            const auto outcome = engine.search(
                pos.board, pos.player, with_threads(SearchLimits::solve(), 8));
            STELLO_CHECK_MSG(outcome.ok, pos.id);
            STELLO_CHECK(pos.score.has_value());
            STELLO_CHECK_EQ_MSG(outcome.result.score, *pos.score, pos.id);
            STELLO_CHECK(outcome.result.kind == ScoreKind::Exact);
            STELLO_CHECK(outcome.result.mv.square.has_value());
            const Board after = pos.board.play_unchecked(pos.player, outcome.result.mv.square->index());
            STELLO_CHECK_EQ_MSG(score_for(after, pos.player), *pos.score, pos.id);
        }
    }

    // Brother cut recovers cleanly without surfacing SearchError on 19-21 empty solves with 8 threads.
    {
        search::SearchEngine single(18);
        search::SearchEngine threaded(18);
        uint64_t seed = 0x6A09'E667'F3BC'C909ULL;
        uint32_t checked = 0;
        while (checked < 8) {
            const auto pos = random_position(seed, 19U + (checked % 3U));
            if (!pos.has_value()) {
                continue;
            }
            single.clear_hash();
            threaded.clear_hash();
            const SearchResult expected = single.search_simple(pos->first, pos->second, SearchLimits::solve());
            const auto outcome = threaded.search(pos->first, pos->second, with_threads(SearchLimits::solve(), 8));
            STELLO_CHECK(outcome.ok);
            STELLO_CHECK_EQ(outcome.result.score, expected.score);
            STELLO_CHECK(outcome.result.kind == expected.kind);
            ++checked;
        }
    }

    // Waiting at a root split continues emitting heartbeat progress callbacks at ~100 ms intervals.
    {
        const auto midgame = load_bench_file("midgame.txt");
        search::SearchEngine engine(18);
        (void)engine.search_simple(Board::initial(), Player::Black, with_threads(SearchLimits::fixed_depth(4), 8));

        const uint64_t root_legal = midgame[0].board.legal_moves(midgame[0].player);
        std::vector<int64_t> report_times_ms;
        const auto outcome = engine.search(
            midgame[0].board,
            midgame[0].player,
            with_threads(SearchLimits::time_per_move_ms(550), 8),
            [&](const SearchInfo& info) {
                if (info.best_move.has_value()) {
                    STELLO_CHECK_MSG((root_legal & info.best_move->bit()) != 0ULL, "heartbeat best_move must be root-legal");
                }
                report_times_ms.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(info.elapsed).count());
            });
        STELLO_CHECK(outcome.ok);
        const int64_t total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(outcome.result.elapsed).count();
        STELLO_CHECK(total_ms >= 350);
        STELLO_CHECK_MSG(report_times_ms.size() >= 3U, "reports=" + std::to_string(report_times_ms.size()));
        const int64_t max_gap_ms = STELLO_TESTS_ASAN ? 600 : 250;
        for (size_t i = 1; i < report_times_ms.size(); ++i) {
            const int64_t gap = report_times_ms[i] - report_times_ms[i - 1];
            STELLO_CHECK_MSG(gap <= max_gap_ms, "gap=" + std::to_string(gap) + "ms");
        }
    }
}

STELLO_TEST(split_point_flags_pool_rollback_tt_wrap_and_cpu_detect_are_safe) {
    // C1: SplitPoint STOP_BIT and CUT_BIT transition atomically without losing either flag.
    search::SplitPoint parent{};
    search::SplitPoint child{};
    child.parent = &parent;
    STELLO_CHECK(!child.is_stopped());
    STELLO_CHECK(!child.is_cut());
    STELLO_CHECK(!child.stopped());

    child.request_stop();
    STELLO_CHECK(child.is_stopped());
    STELLO_CHECK(!child.is_cut());
    STELLO_CHECK(child.stopped());

    child.request_cut();
    STELLO_CHECK(child.is_stopped());
    STELLO_CHECK(child.is_cut());

    search::SplitPoint grandchild{};
    grandchild.parent = &parent;
    parent.request_stop();
    STELLO_CHECK(grandchild.stopped());
    STELLO_CHECK(!grandchild.is_cut());

    // C4: Scope recovers cleanly when a spawned job throws inside the worker.
    {
        search::ThreadPool<uint32_t> pool(2, &search::std_spawner, []() { return uint32_t{0}; });
        bool threw = false;
        try {
            pool.scope([](const search::Scope<uint32_t>& scope) {
                (void)scope.try_spawn([](uint32_t&) { throw std::runtime_error("worker boom"); });
            });
        } catch (const std::runtime_error&) {
            threw = true;
        }
        STELLO_CHECK(threw);
        const uint32_t ok = pool.scope([](const search::Scope<uint32_t>& scope) -> uint32_t {
            (void)scope.try_spawn([](uint32_t& s) { s = 7; });
            return 1U;
        });
        STELLO_CHECK_EQ(ok, 1U);
    }

    // 1C.1: Wrapping TranspositionTable generation clears slots safely using atomics.
    {
        search::TranspositionTable tt(4);
        tt.store(1ULL, 2ULL, 0, 4, search::Bound::Exact, 12, 19);
        for (uint32_t i = 0; i < 65536U; ++i) {
            tt.clear();
        }
        STELLO_CHECK(!tt.get(1ULL, 2ULL, 0).has_value());
    }

    // 1C.6: Concurrent CPU feature queries synchronize via CAS without races.
    {
        std::vector<std::thread> readers;
        readers.reserve(4);
        for (int i = 0; i < 4; ++i) {
            readers.emplace_back([]() {
                for (int k = 0; k < 200; ++k) {
                    (void)cpu::fast_pext();
                    (void)cpu::has_avx2_bmi2();
                }
            });
        }
        for (auto& t : readers) {
            t.join();
        }
    }
}

STELLO_TEST(phase3_helpful_master_transitions_and_rapid_stop_stress) {
    // Helpful Master + deep nested splits at 16t with pool thread-count transitions and 1t parity afterward.
    {
        const auto ffo = load_bench_file("ffo-40-59.txt");
        search::SearchEngine single(19);
        search::SearchEngine engine(19);

        for (const auto& pos : ffo) {
            if (pos.id != "40" && pos.id != "42" && pos.id != "44") {
                continue;
            }
            engine.clear_hash();
            const auto outcome = engine.search(
                pos.board, pos.player, with_threads(SearchLimits::solve(), 16));
            STELLO_CHECK_MSG(outcome.ok, pos.id + " 16t");
            STELLO_CHECK(pos.score.has_value());
            STELLO_CHECK_EQ_MSG(outcome.result.score, *pos.score, pos.id + " 16t");
            STELLO_CHECK(outcome.result.kind == ScoreKind::Exact);
        }

        // Random 20-22 empty solves at 16t exercising dynamic SplitNode + Helpful Master work-stealing.
        uint64_t seed = 0x9E37'79B9'7F4A'7C15ULL;
        uint32_t checked = 0;
        while (checked < 6) {
            const auto pos = random_position(seed, 20U + (checked % 3U));
            if (!pos.has_value()) {
                continue;
            }
            single.clear_hash();
            engine.clear_hash();
            const SearchResult expected = single.search_simple(pos->first, pos->second, SearchLimits::solve());
            const auto outcome = engine.search(pos->first, pos->second, with_threads(SearchLimits::solve(), 16));
            STELLO_CHECK(outcome.ok);
            STELLO_CHECK_EQ(outcome.result.score, expected.score);
            STELLO_CHECK(outcome.result.kind == expected.kind);
            ++checked;
        }

        // Pool transitions 2 -> 8 -> 16 -> 1 -> 8 followed by exact 1t parity check.
        const auto midgame = load_bench_file("midgame.txt");
        for (const uint32_t tcount : {2U, 8U, 16U, 1U, 8U}) {
            engine.clear_hash();
            const SearchResult res = engine.search_simple(
                midgame[0].board, midgame[0].player, with_threads(SearchLimits::fixed_depth(9), tcount));
            STELLO_CHECK_EQ(res.depth, 9);
            STELLO_CHECK(res.mv.square.has_value());
            STELLO_CHECK((midgame[0].board.legal_moves(midgame[0].player) & res.mv.square->bit()) != 0ULL);
        }

        engine.clear_hash();
        const SearchResult after = engine.search_simple(
            midgame[3].board, midgame[3].player, SearchLimits::fixed_depth(10));
        const SearchResult fresh = search::SearchEngine(19).search_simple(
            midgame[3].board, midgame[3].player, SearchLimits::fixed_depth(10));
        STELLO_CHECK(after.mv == fresh.mv);
        STELLO_CHECK_EQ(after.score, fresh.score);
        STELLO_CHECK_EQ(after.nodes, fresh.nodes);
        STELLO_CHECK_EQ(after.evaluations, fresh.evaluations);
    }

    // Rapid repeated Move Now / Cancel stress at 16t on FFO #49.
    {
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
        (void)engine.search_simple(Board::initial(), Player::Black, with_threads(SearchLimits::fixed_depth(4), 16));

        const int64_t max_lag_ms = STELLO_TESTS_ASAN ? 500 : 200;
        for (int iter = 0; iter < 6; ++iter) {
            const bool move_now = (iter % 2 == 0);
            const int delay_ms = 15 + (iter * 5);
            search::SearchSignals signals;
            std::chrono::steady_clock::time_point signalled{};
            std::thread setter([&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                signalled = std::chrono::steady_clock::now();
                if (move_now) {
                    signals.move_now();
                } else {
                    signals.cancel();
                }
            });

            engine.clear_hash();
            const auto outcome = engine.search(
                ffo49->board, ffo49->player, with_threads(SearchLimits::solve(), 16), {}, signals);
            const auto returned = std::chrono::steady_clock::now();
            setter.join();
            const auto lag = std::chrono::duration_cast<std::chrono::milliseconds>(returned - signalled);
            STELLO_CHECK_MSG(lag.count() < max_lag_ms, "16t lag=" + std::to_string(lag.count()) + "ms");
            if (move_now) {
                STELLO_CHECK(outcome.ok);
                STELLO_CHECK(outcome.result.mv.square.has_value());
                STELLO_CHECK((ffo49->board.legal_moves(ffo49->player) & outcome.result.mv.square->bit()) != 0ULL);
            } else {
                STELLO_CHECK(!outcome.ok);
                STELLO_CHECK(outcome.error == search::SearchError::Cancelled);
            }
        }
    }

    // High-frequency nested SplitNode lifetime drain stress at 16t (shallow midgame & solves).
    {
        const auto midgame = load_bench_file("midgame.txt");
        search::SearchEngine engine(16);
        for (size_t i = 0; i < 20; ++i) {
            const SearchResult res = engine.search_simple(
                midgame[i].board, midgame[i].player, with_threads(SearchLimits::fixed_depth(10), 16));
            STELLO_CHECK_EQ(res.depth, 10);
            STELLO_CHECK(res.mv.square.has_value());
            STELLO_CHECK((midgame[i].board.legal_moves(midgame[i].player) & res.mv.square->bit()) != 0ULL);
        }
    }
}

namespace {

struct BorrowedSplitStop final : search::StopSignal {
    const search::SplitPoint* point{nullptr};
    explicit BorrowedSplitStop(const search::SplitPoint* p) noexcept : point(p) {}
    bool is_cancelled() const noexcept override { return false; }
    bool is_move_now() const noexcept override { return point->stopped(); }
};

std::array<search::EndgameChild, 2> make_fast_and_heavy_endgame_children() {
    uint64_t seed = 0x1357'9BDF'2468'ACE0ULL;
    std::optional<std::pair<Board, Player>> trivial;
    while (!trivial.has_value()) {
        trivial = random_position(seed, 1U);
    }
    const auto ffo = load_bench_file("ffo-40-59.txt");
    const cli::BenchPosition* ffo55 = nullptr;
    for (const auto& p : ffo) {
        if (p.id == "55") {
            ffo55 = &p;
            break;
        }
    }
    STELLO_CHECK(ffo55 != nullptr);

    const uint32_t mv0 = static_cast<uint32_t>(std::countr_zero(trivial->first.legal_moves(trivial->second)));
    const uint64_t heavy_legal = ffo55->board.legal_moves(ffo55->player);
    STELLO_CHECK(heavy_legal != 0ULL);
    const uint32_t mv1 = static_cast<uint32_t>(std::countr_zero(heavy_legal));
    const Board heavy_after = ffo55->board.play_unchecked(ffo55->player, mv1);
    const Player heavy_opp = opponent(ffo55->player);

    return {
        search::EndgameChild{
            trivial->first.discs(trivial->second),
            trivial->first.discs(opponent(trivial->second)),
            static_cast<int32_t>(trivial->first.empty_count()),
            endgame::bits::LEGAL_UNDEFINED,
            static_cast<int32_t>(mv0),
        },
        search::EndgameChild{
            heavy_after.discs(heavy_opp),
            heavy_after.discs(ffo55->player),
            static_cast<int32_t>(heavy_after.empty_count()),
            endgame::bits::LEGAL_UNDEFINED,
            static_cast<int32_t>(mv1),
        },
    };
}

} // namespace

STELLO_TEST(phase3_nested_helpful_master_wait_forwards_cancel_and_move_now) {
    search::SearchEngine engine(18);
    const auto children = make_fast_and_heavy_endgame_children();
    SearchOptions opts;
    const auto context = search::SearchContext::create(
        opts, nullptr, nullptr, engine.midgame_table(), engine.endgame_table(), &search::system_clock, 64);
    search::ThreadPool<search::ThreadState> pool(1, &search::std_spawner, []() {
        return search::ThreadState{};
    });
    const int64_t max_lag_ms = STELLO_TESTS_ASAN ? 500 : 200;

    for (const bool wide_window : {false, true}) {
        for (const bool move_now : {false, true}) {
            search::SearchSignals owner_signals;
            search::ThreadState owner_state;
            search::ThreadState help_state;
            std::atomic<uint64_t> nodes{0};
            std::atomic<uint64_t> evaluations{0};
            const search::Helpers search_helpers{&pool, context, 1, &nodes, &evaluations};

            search::Searcher owner(context, owner_state, owner_signals);
            owner.heartbeat_owner = &owner;
            owner.helpers = search_helpers;

            search::SplitPoint outer_point{};
            BorrowedSplitStop borrowed_stop(&outer_point);
            search::Searcher helper_task(context, help_state, borrowed_stop);
            const search::TaskSetup setup{64, 64, false, 0, owner.start, owner.hard_limit_ms};
            helper_task.split_task(search_helpers, &outer_point, setup, nullptr);
            helper_task.heartbeat_owner = &owner;

            std::chrono::steady_clock::time_point signalled{};
            std::thread setter([&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                signalled = std::chrono::steady_clock::now();
                if (move_now) {
                    owner_signals.move_now();
                } else {
                    owner_signals.cancel();
                }
            });

            int32_t best = -search::INFINITY_SCORE;
            int32_t best_move = -1;
            bool got_abort = false;
            search::Abort abort_reason = search::Abort::Stop;
            const int32_t alpha = wide_window ? -64 : -1;
            const int32_t beta = wide_window ? 64 : 0;

#if STELLO_ABORT_EXCEPTIONS
            try {
                helper_task.split<search::EndgameChild>(children, alpha, beta, best, best_move, false);
            } catch (search::Abort a) {
                got_abort = true;
                abort_reason = a;
            }
#else
            const search::AbortStatus st = helper_task.split<search::EndgameChild>(children, alpha, beta, best, best_move, false);
            if (st.is_err()) {
                got_abort = true;
                abort_reason = st.abort_reason();
            }
#endif

            const auto returned = std::chrono::steady_clock::now();
            setter.join();
            const auto lag = std::chrono::duration_cast<std::chrono::milliseconds>(returned - signalled);
            STELLO_CHECK(got_abort);
            STELLO_CHECK(abort_reason == (move_now ? search::Abort::Stop : search::Abort::Cancelled));
            STELLO_CHECK_MSG(lag.count() < max_lag_ms, "nested wait lag=" + std::to_string(lag.count()) + "ms");
        }
    }
}

STELLO_TEST(phase3_throwing_progress_callback_propagates_from_splits) {
    const auto ffo = load_bench_file("ffo-40-59.txt");
    const cli::BenchPosition* ffo40 = nullptr;
    const cli::BenchPosition* ffo55 = nullptr;
    for (const auto& p : ffo) {
        if (p.id == "40") {
            ffo40 = &p;
        } else if (p.id == "55") {
            ffo55 = &p;
        }
    }
    STELLO_CHECK(ffo40 != nullptr && ffo55 != nullptr);

    // 1. Serial baseline: throwing ProgressCallback propagates out of search() once.
    {
        search::SearchEngine engine(18);
        std::atomic<int> calls{0};
        bool caught = false;
        try {
            (void)engine.search(
                ffo55->board,
                ffo55->player,
                SearchLimits::solve(),
                [&](const SearchInfo&) {
                    calls.fetch_add(1, std::memory_order_relaxed);
                    throw std::runtime_error("serial progress boom");
                });
        } catch (const std::runtime_error& e) {
            caught = (std::string(e.what()) == "serial progress boom");
        }
        STELLO_CHECK(caught);
        STELLO_CHECK_EQ(calls.load(std::memory_order_relaxed), 1);
    }

    // 2. Controlled SplitNode (null window) and WaveSplitNode (wide window): poll() throws without std::terminate.
    {
        search::SearchEngine engine(18);
        const auto children = make_fast_and_heavy_endgame_children();
        SearchOptions opts;
        const auto context = search::SearchContext::create(
            opts, nullptr, nullptr, engine.midgame_table(), engine.endgame_table(), &search::system_clock, 64);
        search::ThreadPool<search::ThreadState> pool(1, &search::std_spawner, []() {
            return search::ThreadState{};
        });

        for (const bool wide_window : {false, true}) {
            search::SearchSignals signals;
            search::ThreadState state;
            std::atomic<uint64_t> nodes{0};
            std::atomic<uint64_t> evaluations{0};
            std::atomic<int> calls{0};

            search::Searcher s(context, state, signals);
            s.heartbeat_owner = &s;
            s.helpers = search::Helpers{&pool, context, 1, &nodes, &evaluations};
            s.node_source = &nodes;
            s.start = search::system_clock() - std::chrono::milliseconds(500);
            s.last_heartbeat_ms = -1000;
            s.report_move = 0;
            s.progress = [&](const SearchInfo&) {
                calls.fetch_add(1, std::memory_order_relaxed);
                throw std::runtime_error("split poll boom");
            };

            int32_t best = -search::INFINITY_SCORE;
            int32_t best_move = -1;
            bool caught = false;
            try {
                const int32_t alpha = wide_window ? -64 : -1;
                const int32_t beta = wide_window ? 64 : 0;
                (void)s.split<search::EndgameChild>(children, alpha, beta, best, best_move, true);
            } catch (const std::runtime_error& e) {
                caught = (std::string(e.what()) == "split poll boom");
            }
            STELLO_CHECK(caught);
            STELLO_CHECK_EQ(calls.load(std::memory_order_relaxed), 1);
        }
    }

    // 3. End-to-end 8-thread search: callback throwing mid-search propagates cleanly and engine remains reusable.
    {
        search::SearchEngine engine(18);
        std::atomic<int> calls{0};
        bool caught = false;
        try {
            (void)engine.search(
                ffo55->board,
                ffo55->player,
                with_threads(SearchLimits::solve(), 8),
                [&](const SearchInfo&) {
                    const int n = calls.fetch_add(1, std::memory_order_relaxed) + 1;
                    if (n == 2) {
                        throw std::runtime_error("mt progress boom");
                    }
                });
        } catch (const std::runtime_error& e) {
            caught = (std::string(e.what()) == "mt progress boom");
        }
        STELLO_CHECK(caught);
        STELLO_CHECK_EQ(calls.load(std::memory_order_relaxed), 2);

        engine.clear_hash();
        const auto after = engine.search(ffo40->board, ffo40->player, with_threads(SearchLimits::solve(), 8));
        STELLO_CHECK(after.ok);
        STELLO_CHECK_EQ(after.result.score, *ffo40->score);
    }
}

STELLO_TEST(phase3_atomic_live_best_snapshot_has_no_torn_pairs) {
    struct Case {
        int32_t mv;
        int32_t score;
    };
    const std::array<Case, 5> cases = {{
        {-1, -search::INFINITY_SCORE},
        {0, -64},
        {63, 64},
        {27, -12345},
        {19, 12345},
    }};
    for (const auto& c : cases) {
        const uint64_t packed = search::pack_live_best(c.mv, c.score);
        int32_t out_mv = 0;
        int32_t out_score = 0;
        search::unpack_live_best(packed, out_mv, out_score);
        STELLO_CHECK_EQ(out_mv, c.mv);
        STELLO_CHECK_EQ(out_score, c.score);
    }

    const auto midgame = load_bench_file("midgame.txt");
    search::SearchEngine engine(18);
    const uint64_t root_legal = midgame[0].board.legal_moves(midgame[0].player);
    const auto outcome = engine.search(
        midgame[0].board,
        midgame[0].player,
        with_threads(SearchLimits::time_per_move_ms(300), 8),
        [&](const SearchInfo& info) {
            if (info.best_move.has_value()) {
                STELLO_CHECK((root_legal & info.best_move->bit()) != 0ULL);
                STELLO_CHECK(info.score > -search::INFINITY_SCORE && info.score < search::INFINITY_SCORE);
            }
        });
    STELLO_CHECK(outcome.ok);
}

namespace {

std::thread::id g_ybwc_master_tid{};
std::atomic<uint64_t>* g_ybwc_worker_nodes{nullptr};

Duration ybwc_yield_until_worker_claims_clock() {
    if (std::this_thread::get_id() == g_ybwc_master_tid && g_ybwc_worker_nodes != nullptr) {
        while (g_ybwc_worker_nodes->load(std::memory_order_acquire) == 0) {
            std::this_thread::yield();
        }
    }
    return Duration::zero();
}

} // namespace

STELLO_TEST(phase4_ybwc_split_propagates_nonzero_probcut_level) {
    const auto midgame = load_bench_file("midgame.txt");
    const Board board = midgame[0].board;
    const Player player = midgame[0].player;
    const Player child_player = opponent(player);
    const uint64_t own = board.discs(player);
    const uint64_t legal = board.legal_moves(player);
    STELLO_CHECK(legal != 0ULL);

    const int32_t mv = static_cast<int32_t>(std::countr_zero(legal));
    Board child_board = board;
    STELLO_CHECK(child_board.play(player, Square(static_cast<uint8_t>(mv))));
    const uint64_t f = (child_board.discs(player) ^ own) & ~(uint64_t{1} << mv);
    const int32_t child_look = 6;
    const int32_t split_alpha = 6'000; // Fail-low across all identical children (no early cutoff races).

    const auto probcut = search::detail::make_probcut_errors(1.0);
    search::TranspositionTable table(14);
    SearchOptions opts;
    opts.selective = true;
    const auto context = search::SearchContext::create(
        opts, probcut.get(), nullptr, table, table, &ybwc_yield_until_worker_claims_clock, board.empty_count());

    struct ResetClockGuard {
        ~ResetClockGuard() noexcept {
            g_ybwc_master_tid = std::thread::id{};
            g_ybwc_worker_nodes = nullptr;
        }
    } clock_guard;

    // 1. Direct worker receipt check (split_task).
    {
        search::ThreadPool<search::ThreadState> pool(1, &search::std_spawner, []() {
            return search::ThreadState{};
        });
        std::atomic<uint64_t> nodes{0};
        std::atomic<uint64_t> evaluations{0};
        const search::Helpers helpers{&pool, context, 1, &nodes, &evaluations};
        search::SplitPoint sp{};
        for (const uint8_t lvl : {uint8_t{0}, uint8_t{1}, uint8_t{2}}) {
            search::ThreadState worker_state;
            search::Searcher worker(context, worker_state, search::no_stop());
            const search::TaskSetup setup{-1, -1, lvl, 0, Duration::zero(), 0};
            worker.split_task(helpers, &sp, setup, nullptr);
            STELLO_CHECK_EQ(worker.probcut_level, lvl);
        }
    }

    // 2. Measure sequential per-child node counts at probcut_level = 0, 1, 2 (TT disabled -> exact level sensitivity).
    auto measure_one_child = [&](int32_t look, uint8_t lvl) -> uint64_t {
        g_ybwc_master_tid = std::thread::id{};
        g_ybwc_worker_nodes = nullptr;
        search::ThreadState st;
        search::Searcher s(context, st, search::no_stop());
        s.hash_get_height = -1;
        s.hash_put_height = -1;
        s.probcut_level = lvl;
        const search::MidgameChild c{child_board, child_player, look, 1, mv, nullptr, f, endgame::bits::LEGAL_UNDEFINED, true};
#if STELLO_ABORT_EXCEPTIONS
        (void)c.null_window(s, split_alpha);
#else
        STELLO_CHECK(c.null_window(s, split_alpha).is_ok());
#endif
        STELLO_CHECK_EQ(s.probcut_level, lvl);
        return s.nodes;
    };

    const uint64_t seq0 = measure_one_child(child_look, 0);
    const uint64_t seq1 = measure_one_child(child_look, 1);
    const uint64_t seq2 = measure_one_child(child_look, 2);
    STELLO_CHECK_MSG(seq0 < seq1 && seq1 < seq2,
        "seq0=" + std::to_string(seq0) + " seq1=" + std::to_string(seq1) + " seq2=" + std::to_string(seq2));

    // 3. Coordinate master & helper deterministically across NarrowSplitNode and WaveSplitNode (zero retries/sleeps).
    constexpr size_t K = 6;
    search::ThreadPool<search::ThreadState> pool(1, &search::std_spawner, []() {
        return search::ThreadState{};
    });

    for (const bool wide_window : {false, true}) {
        for (const uint8_t start_level : {uint8_t{1}, uint8_t{2}}) {
            std::array<search::MidgameChild, K> children{};
            for (size_t i = 0; i < K; ++i) {
                children[i] = search::MidgameChild{
                    child_board, child_player, child_look, 1, mv, nullptr, f, endgame::bits::LEGAL_UNDEFINED, true};
            }

            const uint64_t expected_one = (start_level == 1) ? seq1 : seq2;
            const uint64_t expected_worker_nodes = static_cast<uint64_t>(K - 1) * expected_one;
            const uint64_t zeroed_worker_nodes = static_cast<uint64_t>(K - 1) * seq0;

            search::ThreadState owner_state;
            std::atomic<uint64_t> worker_nodes{0};
            std::atomic<uint64_t> evaluations{0};
            const search::Helpers search_helpers{&pool, context, 1, &worker_nodes, &evaluations};
            g_ybwc_worker_nodes = nullptr;
            search::Searcher owner(context, owner_state, search::no_stop());
            owner.heartbeat_owner = &owner;
            owner.helpers = search_helpers;
            owner.ybwc = true;
            owner.hash_get_height = -1;
            owner.hash_put_height = -1;
            owner.probcut_level = start_level;
            owner.next_abort_check = 1;
            owner.start = Duration::zero();
            owner.hard_limit_ms = 1'000'000;

            g_ybwc_master_tid = std::this_thread::get_id();
            g_ybwc_worker_nodes = &worker_nodes;

            int32_t best = -search::INFINITY_SCORE;
            int32_t best_move = -1;
            const int32_t beta = wide_window ? 32'000 : (split_alpha + 1);
#if STELLO_ABORT_EXCEPTIONS
            owner.split<search::MidgameChild>(children, split_alpha, beta, best, best_move, false);
#else
            const auto st = owner.split<search::MidgameChild>(children, split_alpha, beta, best, best_move, false);
            STELLO_CHECK(st.is_ok());
#endif
            g_ybwc_master_tid = std::thread::id{};
            g_ybwc_worker_nodes = nullptr;

            STELLO_CHECK_EQ(owner.probcut_level, start_level);
            const uint64_t wnodes = worker_nodes.load(std::memory_order_acquire);
            STELLO_CHECK_MSG(
                wnodes > zeroed_worker_nodes && wnodes >= (expected_worker_nodes * 90ULL) / 100ULL,
                "wide=" + std::to_string(wide_window ? 1 : 0)
                    + " lvl=" + std::to_string(start_level)
                    + " owner=" + std::to_string(owner.nodes)
                    + " worker=" + std::to_string(wnodes)
                    + " expected_worker=" + std::to_string(expected_worker_nodes)
                    + " zeroed_worker=" + std::to_string(zeroed_worker_nodes));
        }
    }
}

} // namespace stello::tests
