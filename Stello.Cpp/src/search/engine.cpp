#include "search/engine.hpp"

#include "endgame/stable.hpp"
#include "eval/eval.hpp"
#include "search/ybwc.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace stello::search {

namespace {

constexpr int32_t ROOT_WINDOW = 32664;
constexpr int32_t ENDGAME_DISTANCE = 7;
constexpr int32_t PROBCUT_SCORE_LIMIT = 32000;
constexpr int32_t PROBCUT_MIN_LOOK = 2;
constexpr int32_t PROBCUT_PRE_CHECK_OFFSET = 4 * eval::PATTERN_UNITS_PER_DISC;
constexpr int32_t EVALUATION_ORDER_MIN_LOOK = 4;
constexpr int32_t MIDGAME_ETC_MIN_LOOK = 4;
constexpr size_t SOLVE_HELPERS = 3;

size_t lazy_smp_helpers(size_t workers) noexcept {
    return (workers + 1U) / 2U;
}

class FlagStop final : public StopSignal {
public:
    explicit FlagStop(const std::atomic<bool>& flag) noexcept : flag_(flag) {}

    bool is_cancelled() const noexcept override { return false; }
    bool is_move_now() const noexcept override { return flag_.load(std::memory_order_relaxed); }

private:
    const std::atomic<bool>& flag_;
};

struct StopHelpers {
    std::atomic<bool>& stop;

    explicit StopHelpers(std::atomic<bool>& s) noexcept : stop(s) {}
    ~StopHelpers() noexcept { stop.store(true, std::memory_order_relaxed); }

    StopHelpers(const StopHelpers&) = delete;
    StopHelpers& operator=(const StopHelpers&) = delete;
};

} // namespace

Searcher Helpers::searcher(ThreadState& state, const StopSignal& stop_signal) const {
    Searcher s(context, state, stop_signal);
    s.node_sink = nodes;
    return s;
}

void Helpers::finish(Searcher& helper) const noexcept {
    helper.flush_nodes();
    evaluations->fetch_add(helper.evaluations, std::memory_order_relaxed);
}

Duration system_clock() {
    static const auto origin = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<Duration>(std::chrono::steady_clock::now() - origin);
}

namespace detail {

int32_t probcut_shallow_depth(int32_t depth) noexcept {
    return ((depth / 2) & ~1) + (depth & 1);
}

std::unique_ptr<ProbCutErrors> make_probcut_errors(double t) {
    auto errors = std::make_unique<ProbCutErrors>();
    for (size_t discs = 0; discs < 65; ++discs) {
        const double x = static_cast<double>(discs) / 64.0;
        for (int32_t depth = 1; depth <= 60; ++depth) {
            const int32_t shallow = probcut_shallow_depth(depth);
            const double sigma = 1.8871 + 2.9543 * x + 0.0660 * x * x
                               - 0.6749 * static_cast<double>(shallow)
                               + 0.2778 * static_cast<double>(depth);
            (*errors)[discs][static_cast<size_t>(depth)] = static_cast<int32_t>(
                std::ceil(t * (std::max)(sigma, 0.5) * static_cast<double>(eval::PATTERN_UNITS_PER_DISC)));
        }
    }
    return errors;
}

void move_to_front(std::span<int32_t> moves, std::span<const int32_t> improvements) noexcept {
    for (const int32_t mv : improvements) {
        size_t index = 0;
        while (index < moves.size() && moves[index] != mv) {
            ++index;
        }
        assert(index < moves.size() && "an improvement is a root move");
        std::rotate(moves.begin(), moves.begin() + static_cast<std::ptrdiff_t>(index),
                    moves.begin() + static_cast<std::ptrdiff_t>(index + 1));
    }
}

} // namespace detail

SearchContext SearchContext::create(
    const SearchOptions& options,
    const ProbCutErrors* probcut_errors,
    const TranspositionTable& midgame_table,
    const TranspositionTable& endgame_table,
    Clock clock,
    uint32_t root_empties
) {
    SearchContext ctx;
    ctx.pattern = options.pattern ? options.pattern.get() : &eval::EvalWeights::built_in();
    ctx.probcut_errors = probcut_errors;
    ctx.midgame_table = &midgame_table;
    ctx.endgame_table = &endgame_table;
    ctx.clock = clock;
    ctx.soft_time_percent = options.soft_time_percent;
    ctx.ybwc = options.ybwc;
    ctx.root_empties = root_empties;
    return ctx;
}

SearchEngine::SearchEngine(uint32_t hash_bits, SearchOptions options)
    : hash_bits_(hash_bits),
      options_(std::move(options)),
      probcut_errors_(
          options_.selective
              ? detail::make_probcut_errors(
                    static_cast<double>(options_.probcut_percent.value_or(DEFAULT_PROBCUT_PERCENT)) / 100.0)
              : nullptr),
      midgame_table_(hash_bits),
      endgame_table_(hash_bits) {
    assert(hash_bits >= 10 && hash_bits <= 26 && "hash bits must be 10-26");
    endgame::stable::init();
}

void SearchEngine::clear_hash() {
    midgame_table_.clear();
    endgame_table_.clear();
    main_.endgame.clear();
    if (pool_) {
        pool_->for_each_state([](ThreadState& state) { state.endgame.clear(); });
    }
}

Searcher SearchEngine::searcher(
    ProgressCallback progress,
    const StopSignal& stop,
    uint32_t root_empties
) {
    const SearchContext ctx = SearchContext::create(
        options_, probcut_errors_.get(), midgame_table_, endgame_table_, clock_, root_empties);
    Searcher s(ctx, main_, stop);
    s.progress = std::move(progress);
    s.iteration_done = iteration_done_ ? &iteration_done_ : nullptr;
    return s;
}

SearchOutcome SearchEngine::search(
    const Board& board,
    Player player,
    const SearchLimits& limits,
    ProgressCallback progress,
    const StopSignal& stop,
    std::optional<uint64_t> only_moves
) {
    if (stop.is_cancelled()) {
        return {false, {}, SearchError::Cancelled};
    }

    const uint32_t root_empties = board.empty_count();
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    const uint32_t threads = 1;
#else
    const uint32_t threads = limits.threads();
#endif
    if (threads == 1) {
        return searcher(std::move(progress), stop, root_empties).run(board, player, limits, only_moves);
    }

    if (!pool_ || pool_threads_ != threads) {
        // End old workers first so no more than `threads` threads are alive.
        pool_.reset();
        pool_ = std::make_unique<ThreadPool<ThreadState>>(
            static_cast<size_t>(threads - 1),
            spawner_,
            []() { return ThreadState{}; });
        pool_threads_ = threads;
    }

    const SearchContext context = SearchContext::create(
        options_, probcut_errors_.get(), midgame_table_, endgame_table_, clock_, root_empties);
    pool_->for_each_state([](ThreadState& state) { state.ordering.clear_responses(); });

    std::atomic<uint64_t> nodes{0};
    std::atomic<uint64_t> evaluations{0};
    const Helpers search_helpers{pool_.get(), context, static_cast<size_t>(threads - 1), &nodes, &evaluations};

    Searcher s(context, main_, stop);
    s.progress = std::move(progress);
    s.iteration_done = iteration_done_ ? &iteration_done_ : nullptr;
    s.helpers = search_helpers;
    s.node_source = &nodes;
    SearchOutcome outcome = s.run(board, player, limits, only_moves);
    if (outcome.ok) {
        outcome.result.nodes += nodes.load(std::memory_order_relaxed);
        outcome.result.evaluations += evaluations.load(std::memory_order_relaxed);
    }
    return outcome;
}

SearchResult SearchEngine::search_simple(const Board& board, Player player, const SearchLimits& limits) {
    const SearchOutcome outcome = search(board, player, limits, {}, no_stop(), std::nullopt);
    assert(outcome.ok && "a search without stop signals finishes");
    return outcome.result;
}

Searcher::Searcher(const SearchContext& context, ThreadState& thread, const StopSignal& stop_signal)
    : pattern(context.pattern),
      probcut_errors(context.probcut_errors),
      midgame_table(context.midgame_table),
      endgame_table(context.endgame_table),
      endgame(&thread.endgame),
      ordering(&thread.ordering),
      stop(&stop_signal),
      clock(context.clock),
      start(context.clock()),
      soft_time_percent(context.soft_time_percent),
      ybwc(context.ybwc),
      root_empties(context.root_empties) {}

SearchOutcome Searcher::run(
    const Board& board,
    Player player,
    const SearchLimits& limits,
    std::optional<uint64_t> only_moves
) {
    uint64_t moves = board.legal_moves(player);
    if (only_moves.has_value()) {
        moves &= *only_moves;
        if (moves == 0) {
            return {false, {}, SearchError::NoLegalOnlyMove};
        }
    }

    if (moves == 0) {
        return {true, make_result(Best{-1, 0, ScoreKind::None, 0}), SearchError::Cancelled};
    }

    if (std::popcount(moves) == 1 && limits.mode() != TimeControlMode::Solve && !only_moves.has_value()) {
        return {
            true,
            make_result(Best{static_cast<int32_t>(std::countr_zero(moves)), 0, ScoreKind::None, 0}),
            SearchError::Cancelled,
        };
    }

    ordering->clear_responses();
    std::array<int32_t, MAX_MOVES> root{};
    const size_t count = fill_moves(root, moves);
    std::span<int32_t> root_moves(root.data(), count);
    ordering->order(root_moves, -1, -1, player, board);

    Best best{root_moves[0], 0, ScoreKind::None, 0};

#if STELLO_ABORT_EXCEPTIONS
    try {
        iterate(board, player, limits, root_moves, best);
        return {true, make_result(best), SearchError::Cancelled};
    } catch (Abort a) {
        if (a == Abort::Cancelled) {
            return {false, {}, SearchError::Cancelled};
        }
        return {true, make_result(best), SearchError::Cancelled};
    }
#else
    const AbortStatus st = iterate(board, player, limits, root_moves, best);
    if (st.is_err() && st.abort_reason() == Abort::Cancelled) {
        return {false, {}, SearchError::Cancelled};
    }
    return {true, make_result(best), SearchError::Cancelled};
#endif
}

AbortVoid Searcher::iterate(
    const Board& board,
    Player player,
    const SearchLimits& limits,
    std::span<int32_t> root_moves,
    Best& best
) {
    const int32_t empties = static_cast<int32_t>(board.empty_count());
    time::TimeBudget budget = time::for_midgame(limits, empties).with_soft_percent(soft_time_percent, 0);
    hard_limit_ms = budget.hard_ms;

    if (limits.mode() != TimeControlMode::Solve) {
        bool done = false;
        if (helpers.has_value()) {
            const Helpers active_helpers = *helpers;
            STELLO_TRY_ASSIGN_IN_STATUS(
                done,
                lazy_smp_midgame(active_helpers, board, player, limits, budget, root_moves, best));
        } else {
            STELLO_TRY_ASSIGN_IN_STATUS(
                done,
                midgame_iterations(board, player, limits, budget, root_moves, best, nullptr));
        }
        if (done) {
            STELLO_RETURN_OK_VOID();
        }
    } else {
        int32_t look = 0;
        while (look < SOLVE_ORDERING_LOOKS && empties - 1 - look > ENDGAME_DISTANCE) {
            hash_get_height = look - 1;
            hash_put_height = look;
            STELLO_TRY_VOID_IN_STATUS(search_root(board, player, look, root_moves, best));
            ++look;
        }
    }

    budget = time::for_solver(limits, budget, elapsed_ms(), soft_time_percent);
    hard_limit_ms = budget.hard_ms;

    STELLO_TRY_ASSIGN_IN_STATUS(
        const ScoreMove wld,
        solve_iteration(board, player, -1, 1, ScoreKind::WinLossDraw, root_moves, best));
    const bool keep_midgame_move = (wld.score < 0 && best.kind == ScoreKind::Heuristic);
    best = Best{
        keep_midgame_move ? best.mv : wld.mv,
        wld.score,
        (wld.score == 0) ? ScoreKind::Exact : ScoreKind::WinLossDraw,
        empties,
    };

    if (wld.score == 0 || stop_iterating(limits, budget, 0)) {
        STELLO_RETURN_OK_VOID();
    }

    const int32_t alpha = (wld.score > 0) ? (wld.score - 1) : -65;
    const int32_t beta = (wld.score > 0) ? 65 : (wld.score + 1);
    STELLO_TRY_ASSIGN_IN_STATUS(
        const ScoreMove exact,
        solve_iteration(board, player, alpha, beta, ScoreKind::Exact, root_moves, best));
    best = Best{exact.mv, exact.score, ScoreKind::Exact, empties};
    STELLO_RETURN_OK_VOID();
}

AbortResult<ScoreMove> Searcher::solve_iteration(
    const Board& board,
    Player player,
    int32_t alpha,
    int32_t beta,
    ScoreKind kind,
    std::span<int32_t> root_moves,
    Best& best
) {
    if (!helpers.has_value() || ybwc) {
        return solve_root(board, player, alpha, beta, kind, root_moves, best);
    }

    const Helpers active_helpers = *helpers;
    std::atomic<bool> stop_flag{false};
    const Board board_copy = board;
    const size_t count = root_moves.size();
    const Best start_best = best;
    std::array<int32_t, MAX_MOVES> order{};
    std::copy_n(root_moves.data(), count, order.data());

    return active_helpers.pool->scope([&](const Scope<ThreadState>& scope) -> AbortResult<ScoreMove> {
        const size_t helper_count = (std::min)(active_helpers.count, SOLVE_HELPERS);
        for (size_t i = 0; i < helper_count; ++i) {
            const bool started = scope.try_spawn(
                [active_helpers, &stop_flag, board_copy, player, alpha, beta, kind, order, count, start_best, i](
                    ThreadState& state
                ) {
                    FlagStop flag(stop_flag);
                    Searcher helper = active_helpers.searcher(state, flag);
                    auto moves = order;
                    Best local_best = start_best;
                    std::rotate(
                        moves.begin(),
                        moves.begin() + static_cast<std::ptrdiff_t>((i + 1) % count),
                        moves.begin() + static_cast<std::ptrdiff_t>(count));
#if STELLO_ABORT_EXCEPTIONS
                    try {
                        (void)helper.solve_root(
                            board_copy,
                            player,
                            alpha,
                            beta,
                            kind,
                            std::span<int32_t>(moves.data(), count),
                            local_best);
                    } catch (Abort) {
                    }
#else
                    (void)helper.solve_root(
                        board_copy,
                        player,
                        alpha,
                        beta,
                        kind,
                        std::span<int32_t>(moves.data(), count),
                        local_best);
#endif
                    active_helpers.finish(helper);
                });
            if (!started) {
                break;
            }
        }

        StopHelpers stop_helpers(stop_flag);
        return solve_root(board_copy, player, alpha, beta, kind, root_moves, best);
    });
}

AbortResult<bool> Searcher::lazy_smp_midgame(
    const Helpers& smp_helpers,
    const Board& board,
    Player player,
    const SearchLimits& limits,
    time::TimeBudget budget,
    std::span<int32_t> root_moves,
    Best& best
) {
    const int32_t last_look = static_cast<int32_t>(board.empty_count()) - 2 - ENDGAME_DISTANCE;
    if (last_look < 0) {
        return midgame_iterations(board, player, limits, budget, root_moves, best, nullptr);
    }

    std::atomic<bool> stop_flag{false};
    std::atomic<int32_t> main_look{0};
    const Board board_copy = board;
    const size_t count = root_moves.size();
    const Best start_best = best;
    std::array<int32_t, MAX_MOVES> order{};
    std::copy_n(root_moves.data(), count, order.data());

    return smp_helpers.pool->scope([&](const Scope<ThreadState>& scope) -> AbortResult<bool> {
        const size_t lazy = ybwc ? lazy_smp_helpers(smp_helpers.count) : smp_helpers.count;
        for (size_t i = 0; i < lazy; ++i) {
            const int32_t ahead = 1 + static_cast<int32_t>(std::countr_zero(static_cast<uint32_t>(i) + 1U));
            const bool started = scope.try_spawn(
                [smp_helpers, &stop_flag, &main_look, board_copy, player, order, count, start_best, last_look, ahead](
                    ThreadState& state
                ) {
                    FlagStop flag(stop_flag);
                    Searcher helper = smp_helpers.searcher(state, flag);
                    auto moves = order;
                    Best local_best = start_best;
                    int32_t look = 0;

#if STELLO_ABORT_EXCEPTIONS
                    try {
                        for (;;) {
                            look = (std::min)(
                                (std::max)(look, main_look.load(std::memory_order_relaxed) + ahead),
                                last_look);
                            helper.hash_get_height = look - 1;
                            helper.hash_put_height = look;
                            helper.search_root(
                                board_copy,
                                player,
                                look,
                                std::span<int32_t>(moves.data(), count),
                                local_best);
                            if (look == last_look) {
                                break;
                            }
                            ++look;
                        }
                    } catch (Abort) {
                    }
#else
                    for (;;) {
                        look = (std::min)(
                            (std::max)(look, main_look.load(std::memory_order_relaxed) + ahead),
                            last_look);
                        helper.hash_get_height = look - 1;
                        helper.hash_put_height = look;
                        const AbortStatus st = helper.search_root(
                            board_copy,
                            player,
                            look,
                            std::span<int32_t>(moves.data(), count),
                            local_best);
                        if (st.is_err() || look == last_look) {
                            break;
                        }
                        ++look;
                    }
#endif
                    smp_helpers.finish(helper);
                });
            if (!started) {
                break;
            }
        }

        StopHelpers stop_helpers(stop_flag);
        return midgame_iterations(board_copy, player, limits, budget, root_moves, best, &main_look);
    });
}

AbortResult<bool> Searcher::midgame_iterations(
    const Board& board,
    Player player,
    const SearchLimits& limits,
    time::TimeBudget budget,
    std::span<int32_t> root_moves,
    Best& best,
    std::atomic<int32_t>* main_look
) {
    const int32_t empties = static_cast<int32_t>(board.empty_count());
    int32_t look = 0;
    while (empties - 1 - look > ENDGAME_DISTANCE) {
        if (main_look != nullptr) {
            main_look->store(look, std::memory_order_relaxed);
        }
        hash_get_height = look - 1;
        hash_put_height = look;
        STELLO_TRY_VOID_R(bool, search_root(board, player, look, root_moves, best));
        if (iteration_done != nullptr && *iteration_done) {
            (*iteration_done)(look + 1, best.score);
        }
        if (stop_iterating(limits, budget, look + 1)) {
            STELLO_RETURN_OK_R(bool, true);
        }
        ++look;
    }
    STELLO_RETURN_OK_R(bool, false);
}

AbortVoid Searcher::search_root(
    const Board& board,
    Player player,
    int32_t look,
    std::span<int32_t> root_moves,
    Best& best
) {
    const Player opp = opponent(player);
    int32_t alpha = -ROOT_WINDOW;
    const int32_t beta = ROOT_WINDOW;
    int32_t best_score = -INFINITY_SCORE;
    int32_t best_move = -1;
    std::array<int32_t, MAX_MOVES> improvements{};
    size_t improved = 0;

    for (size_t i = 0; i < root_moves.size(); ++i) {
        const int32_t mv = root_moves[i];
        report_depth = look + 1;
        report_move = mv;
        report_best_move = (best_move >= 0) ? best_move : best.mv;
        report_score = (best_move >= 0) ? best_score : best.score;
        report_kind = ScoreKind::Heuristic;

        const Board child = board.play_unchecked(player, static_cast<uint32_t>(mv));
        int32_t score = 0;

        if (look == 0) {
            STELLO_TRY_ASSIGN_IN_STATUS(const int32_t leaf, evaluate_leaf(child, opp));
            score = -leaf;
        } else {
            const int32_t child_look = look - 1;
            int32_t reply = -1;

            if (i == 0 || look <= 2) {
                STELLO_TRY_ASSIGN_IN_STATUS(const ScoreMove res, search(child, opp, child_look, 1, mv, -beta, -alpha));
                score = -res.score;
                reply = res.mv;
            } else {
                STELLO_TRY_ASSIGN_IN_STATUS(const ScoreMove res, search(child, opp, child_look, 1, mv, -alpha - 1, -alpha));
                if (-res.score > alpha && -res.score < beta) {
                    STELLO_TRY_ASSIGN_IN_STATUS(const ScoreMove full, search(child, opp, child_look, 1, mv, -beta, -alpha));
                    score = -full.score;
                    reply = full.mv;
                } else {
                    score = -res.score;
                    reply = res.mv;
                }
            }

            ordering->record_response(opp, mv, reply, score <= alpha);
        }

        if (score > best_score) {
            best_score = score;
            best_move = mv;
            if (score > alpha) {
                alpha = score;
            }
            improvements[improved] = mv;
            ++improved;
            best = Best{mv, score, ScoreKind::Heuristic, look + 1};
        }

        report(look + 1, mv, best_move, best_score, ScoreKind::Heuristic);
    }

    detail::move_to_front(root_moves, std::span<const int32_t>(improvements.data(), improved));
    STELLO_RETURN_OK_VOID();
}

AbortResult<ScoreMove> Searcher::search(
    const Board& board,
    Player player,
    int32_t look,
    int32_t ply,
    int32_t previous_move,
    int32_t alpha,
    int32_t beta
) {
    STELLO_TRY_VOID_R(ScoreMove, count_node());

    const Player opp = opponent(player);
    const uint64_t own = board.discs(player);
    const uint64_t other = board.discs(opp);
    const uint64_t moves = bits::legal_moves(own, other);

    if (moves == 0) {
        if (bits::legal_moves(other, own) == 0) {
            evaluations += 1;
            STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{game_over_score(own, other), -1}));
        }

        STELLO_TRY_ASSIGN_R(ScoreMove, const ScoreMove passed, search(board, opp, look, ply, -1, -beta, -alpha));
        STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{-passed.score, -1}));
    }

    int32_t hash_move = -1;
    const bool hash_get = ply <= hash_get_height;
    const bool hash_put = ply <= hash_put_height;
    const int32_t tag = static_cast<int32_t>(player_index(player));
    if (hash_put && !hash_get) {
        midgame_table->prefetch(own, other, tag);
    }

    if (hash_get) {
        if (const auto entry = midgame_table->get_at_root(own, other, tag, root_empties)) {
            if (entry->mv >= 0 && entry->mv < 64 && (moves & (uint64_t{1} << entry->mv)) != 0) {
                const int32_t value = static_cast<int32_t>(entry->value);
                if (static_cast<int32_t>(entry->depth) >= look) {
                    bool cut = false;
                    switch (entry->bound) {
                        case Bound::Exact: cut = true; break;
                        case Bound::Upper: cut = (value <= alpha); break;
                        case Bound::Lower: cut = (value >= beta); break;
                        case Bound::None: cut = false; break;
                    }
                    if (cut) {
                        STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{value, static_cast<int32_t>(entry->mv)}));
                    }
                }
                hash_move = static_cast<int32_t>(entry->mv);
            }
        }
    }

    if (probcut_errors != nullptr && look >= PROBCUT_MIN_LOOK && !in_probcut) {
        STELLO_TRY_ASSIGN_R(ScoreMove, const auto cut, try_probcut(board, player, look, ply, previous_move, alpha, beta));
        if (cut.has_value()) {
            STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{*cut, -1}));
        }
    }

    const int32_t stored_look = look;
    const size_t move_count = static_cast<size_t>(std::popcount(moves));

    if (move_count == 1 && look == 0) {
        look = 1;
    }

    std::array<int32_t, MAX_MOVES> buffer{};
    fill_moves(buffer, moves);
    std::span<int32_t> list(buffer.data(), move_count);

    if (look >= MIDGAME_ETC_MIN_LOOK && ply < hash_get_height) {
        for (const int32_t mv : list) {
            const Board child = board.play_unchecked(player, static_cast<uint32_t>(mv));
            if (const auto entry = midgame_table->get_at_root(
                    child.discs(opp), child.discs(player), static_cast<int32_t>(player_index(opp)), root_empties))
            {
                if (static_cast<int32_t>(entry->depth) >= look - 1
                    && entry->bound != Bound::Lower
                    && -static_cast<int32_t>(entry->value) >= beta)
                {
                    STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{-static_cast<int32_t>(entry->value), mv}));
                }
            }
        }
    }

    if (look >= EVALUATION_ORDER_MIN_LOOK) {
        STELLO_TRY_VOID_R(ScoreMove, order_by_evaluation(list, hash_move, board, player));
    } else {
        ordering->order(list, hash_move, previous_move, player, board);
    }

    int32_t best = -INFINITY_SCORE;
    int32_t best_move = -1;
    Bound bound = Bound::Lower;
    const int32_t alpha_start = alpha;

    for (size_t index = 0; index < list.size(); ++index) {
        if (index == 1 && look >= YBWC_MID_MIN_DEPTH && may_split()) {
            STELLO_TRY_VOID_R(
                ScoreMove,
                split_midgame(board, player, look, ply, list.subspan(1), alpha, beta, best, best_move));
            if (best >= beta) {
                bound = Bound::Lower;
            } else if (best > alpha_start) {
                bound = Bound::Exact;
            } else {
                bound = Bound::Upper;
            }
            break;
        }

        const int32_t mv = list[index];
        const Board child = board.play_unchecked(player, static_cast<uint32_t>(mv));
        int32_t score = 0;

        if (look == 0) {
            STELLO_TRY_ASSIGN_R(ScoreMove, const int32_t leaf, evaluate_leaf(child, opp));
            score = -leaf;
        } else if (best == -INFINITY_SCORE || beta - alpha == 1) {
            STELLO_TRY_ASSIGN_R(ScoreMove, const ScoreMove res, search(child, opp, look - 1, ply + 1, mv, -beta, -alpha));
            score = -res.score;
            ordering->record_response(opp, mv, res.mv, score <= alpha);
        } else {
            STELLO_TRY_ASSIGN_R(ScoreMove, ScoreMove res, search(child, opp, look - 1, ply + 1, mv, -alpha - 1, -alpha));
            if (-res.score > alpha && -res.score < beta) {
                STELLO_TRY_ASSIGN_EXISTING_R(ScoreMove, res, search(child, opp, look - 1, ply + 1, mv, -beta, -alpha));
            }
            score = -res.score;
            ordering->record_response(opp, mv, res.mv, score <= alpha);
        }

        if (score > best) {
            if (score >= beta) {
                bound = Bound::Lower;
            } else if (score > alpha) {
                bound = Bound::Exact;
            } else {
                bound = Bound::Upper;
            }
            if (score > alpha) {
                alpha = score;
            }
            best = score;
            best_move = mv;
            if (best >= beta) {
                break;
            }
        }
    }

    if (hash_put) {
        midgame_table->store_at_root(own, other, tag, stored_look, bound, best, best_move, root_empties);
    }

    STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{best, best_move}));
}

AbortVoid Searcher::order_by_evaluation(
    std::span<int32_t> list,
    int32_t hash_move,
    const Board& board,
    Player player
) {
    const Player opp = opponent(player);
    const uint64_t empty = ~(board.discs(player) | board.discs(opp));
    std::array<int32_t, MAX_MOVES> keys{};

    for (size_t idx = 0; idx < list.size(); ++idx) {
        const int32_t mv = list[idx];
        if (mv == hash_move) {
            keys[idx] = (std::numeric_limits<int32_t>::max)();
            continue;
        }

        const Board child = board.play_unchecked(player, static_cast<uint32_t>(mv));
        const uint64_t child_own = child.discs(opp);
        const uint64_t child_other = child.discs(player);
        const int32_t replies = endgame::bits::weighted_moves(bits::legal_moves(child_own, child_other));
        const int32_t potential = endgame::bits::potential_mobility(child_other, empty & ~(uint64_t{1} << mv));
        STELLO_TRY_ASSIGN_IN_STATUS(const int32_t value, evaluate_leaf(child, opp));
        keys[idx] = (ORDER_OFFSET - replies) * ORDER_MOBILITY
                  + (ORDER_OFFSET - potential) * ORDER_POTENTIAL_MOBILITY
                  - value * ORDER_VALUE / eval::PATTERN_UNITS_PER_DISC;
    }

    stello::search::sort_descending(list, std::span<int32_t>(keys.data(), list.size()));
    STELLO_RETURN_OK_VOID();
}

AbortVoid Searcher::split_midgame(
    const Board& board,
    Player player,
    int32_t look,
    int32_t ply,
    std::span<const int32_t> tail,
    int32_t alpha,
    int32_t beta,
    int32_t& best,
    int32_t& best_move
) {
    const Player opp = opponent(player);
    std::array<MidgameChild, MAX_MOVES> children{};
    for (size_t c = 0; c < tail.size(); ++c) {
        const int32_t child_mv = tail[c];
        children[c] = MidgameChild{
            board.play_unchecked(player, static_cast<uint32_t>(child_mv)),
            opp,
            look - 1,
            ply + 1,
            child_mv,
        };
    }
    return split(std::span<const MidgameChild>(children.data(), tail.size()), alpha, beta, best, best_move);
}

AbortResult<std::optional<int32_t>> Searcher::try_probcut(
    const Board& board,
    Player player,
    int32_t look,
    int32_t ply,
    int32_t previous_move,
    int32_t alpha,
    int32_t beta
) {
    const int32_t depth = look + 1;
    const int32_t shallow_look = detail::probcut_shallow_depth(depth) - 1;
    assert(probcut_errors != nullptr && "Multi-ProbCut is on");
    const int32_t error = (*probcut_errors)[64U - board.empty_count()][static_cast<size_t>(depth)];
    const int32_t error0 = (std::max)(eval::PATTERN_UNITS_PER_DISC, error - PROBCUT_PRE_CHECK_OFFSET);
    STELLO_TRY_ASSIGN_R(std::optional<int32_t>, const int32_t static0, evaluate_leaf(board, player));

    ProbCutGuard guard(in_probcut);
    STELLO_TRY_ASSIGN_R(
        std::optional<int32_t>,
        const auto cut,
        probcut_searches(board, player, shallow_look, ply, previous_move, alpha, beta, error, error0, static0));
    STELLO_RETURN_OK_R(std::optional<int32_t>, cut);
}

AbortResult<std::optional<int32_t>> Searcher::probcut_searches(
    const Board& board,
    Player player,
    int32_t shallow_look,
    int32_t ply,
    int32_t previous_move,
    int32_t alpha,
    int32_t beta,
    int32_t error,
    int32_t error0,
    int32_t static0
) {
    if (static0 >= beta + error0 && beta + error < PROBCUT_SCORE_LIMIT) {
        const int32_t pc_beta = beta + error;
        STELLO_TRY_ASSIGN_R(
            std::optional<int32_t>,
            const ScoreMove high,
            search(board, player, shallow_look, ply, previous_move, pc_beta - 1, pc_beta));
        if (high.score >= pc_beta) {
            STELLO_RETURN_OK_R(std::optional<int32_t>, std::optional<int32_t>(beta));
        }
    }

    if (static0 <= alpha - error0 && alpha - error > -PROBCUT_SCORE_LIMIT) {
        const int32_t pc_alpha = alpha - error;
        STELLO_TRY_ASSIGN_R(
            std::optional<int32_t>,
            const ScoreMove low,
            search(board, player, shallow_look, ply, previous_move, pc_alpha, pc_alpha + 1));
        if (low.score <= pc_alpha) {
            STELLO_RETURN_OK_R(std::optional<int32_t>, std::optional<int32_t>(alpha));
        }
    }

    STELLO_RETURN_OK_R(std::optional<int32_t>, std::nullopt);
}

AbortResult<ScoreMove> Searcher::solve_root(
    const Board& board,
    Player player,
    int32_t alpha,
    int32_t beta,
    ScoreKind kind,
    std::span<int32_t> root_moves,
    Best& best
) {
    const uint64_t own = board.discs(player);
    const uint64_t opp = board.discs(opponent(player));
    const int32_t empties = static_cast<int32_t>(board.empty_count());
    int32_t best_score = -INFINITY_SCORE;
    int32_t best_move = -1;
    std::array<int32_t, MAX_MOVES> improvements{};
    size_t improved = 0;

    for (size_t i = 0; i < root_moves.size(); ++i) {
        const int32_t mv = root_moves[i];
        const bool has_solved_move = (best_move >= 0 || best.kind == ScoreKind::WinLossDraw || best.kind == ScoreKind::Exact);
        report_depth = has_solved_move ? empties : best.depth;
        report_move = mv;
        report_best_move = (best_move >= 0) ? best_move : best.mv;
        report_score = (best_move >= 0) ? best_score : best.score;
        report_kind = (best_move >= 0) ? kind : best.kind;

        const uint64_t f = bits::flips(own, opp, static_cast<uint32_t>(mv));
        const uint64_t child_own = own | f | (uint64_t{1} << mv);
        const uint64_t child_opp = opp & ~f;

        int32_t score = 0;
        if (i == 0) {
            STELLO_TRY_ASSIGN_R(ScoreMove, const int32_t s, solve_child(child_opp, child_own, -beta, -alpha, empties - 1));
            score = -s;
        } else {
            STELLO_TRY_ASSIGN_R(ScoreMove, const int32_t s_null, solve_child(child_opp, child_own, -alpha - 1, -alpha, empties - 1));
            const int32_t value = -s_null;
            if (value > alpha && value < beta) {
                STELLO_TRY_ASSIGN_R(ScoreMove, const int32_t s_full, solve_child(child_opp, child_own, -beta, -alpha, empties - 1));
                score = -s_full;
            } else {
                score = value;
            }
        }

        if (score > best_score) {
            best_score = score;
            best_move = mv;
            if (score > alpha) {
                alpha = score;
            }
            improvements[improved] = mv;
            ++improved;

            if (kind == ScoreKind::WinLossDraw && score > 0) {
                best = Best{mv, score, kind, empties};
            } else if (kind == ScoreKind::WinLossDraw && score == 0) {
                best = Best{mv, score, ScoreKind::Heuristic, best.depth};
            }
        }

        report(empties, mv, best_move, best_score, kind);
        if (best_score >= beta) {
            break;
        }
    }

    detail::move_to_front(root_moves, std::span<const int32_t>(improvements.data(), improved));
    STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{best_score, best_move}));
}

AbortResult<int32_t> Searcher::evaluate_leaf(const Board& board, Player player) {
    STELLO_TRY_VOID_I32(count_node());
    evaluations += 1;
    STELLO_RETURN_OK_R(int32_t, eval::pattern_score(*pattern, board.discs(player), board.discs(opponent(player))));
}

bool Searcher::stop_iterating(const SearchLimits& limits, time::TimeBudget budget, int32_t next_look) const noexcept {
    switch (limits.mode()) {
        case TimeControlMode::FixedDepth: return next_look >= limits.depth();
        case TimeControlMode::Solve: return false;
        default: return elapsed_ms() > budget.soft_ms;
    }
}

STELLO_NOINLINE AbortVoid Searcher::check_abort() {
    flush_nodes();
    if (progress && report_move >= 0 && elapsed_ms() - last_heartbeat_ms >= 100) {
        report(report_depth, report_move, report_best_move, report_score, report_kind);
    }

    if (stop->is_cancelled()) {
        STELLO_RETURN_ABORT_IN_STATUS(Abort::Cancelled);
    }

    if (stop->is_move_now() || elapsed_ms() >= hard_limit_ms) {
        STELLO_RETURN_ABORT_IN_STATUS(Abort::Stop);
    }

    STELLO_RETURN_OK_VOID();
}

void Searcher::flush_nodes() noexcept {
    if (node_sink != nullptr) {
        node_sink->fetch_add(nodes - flushed_nodes, std::memory_order_relaxed);
        flushed_nodes = nodes;
    }
}

void Searcher::report(int32_t depth, int32_t mv, int32_t best_move, int32_t score, ScoreKind kind) {
    if (!progress) {
        return;
    }
    const Duration el = elapsed();
    last_heartbeat_ms = std::chrono::duration_cast<std::chrono::milliseconds>(el).count();
    SearchInfo info;
    info.depth = depth;
    info.current_move = Square(static_cast<uint8_t>(mv));
    info.best_move = (best_move >= 0) ? std::optional<Square>(Square(static_cast<uint8_t>(best_move))) : std::nullopt;
    info.score = score;
    info.kind = kind;
    info.nodes = nodes + (node_source != nullptr ? node_source->load(std::memory_order_relaxed) : 0ULL);
    info.evaluations = evaluations;
    info.elapsed = el;
    progress(info);
}

Duration Searcher::elapsed() const {
    const Duration now = clock();
    return (now > start) ? (now - start) : Duration::zero();
}

int64_t Searcher::elapsed_ms() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed()).count();
}

SearchResult Searcher::make_result(const Best& best) const {
    SearchResult res;
    res.mv = (best.mv >= 0) ? Move(Square(static_cast<uint8_t>(best.mv))) : Move::pass();
    res.score = best.score;
    res.kind = best.kind;
    res.depth = best.depth;
    res.nodes = nodes;
    res.evaluations = evaluations;
    res.elapsed = elapsed();
    return res;
}

} // namespace stello::search
