#include "search/kernel.hpp"

#include "endgame/stable.hpp"
#include "eval/eval.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>

namespace stello::search {

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

std::unique_ptr<ProbCutErrors> make_probcut_errors(double t, bool egaroucid16, bool has_mobility) {
    auto errors = std::make_unique<ProbCutErrors>();
    for (size_t discs = 0; discs < 65; ++discs) {
        const double x = static_cast<double>(discs) / 64.0;
        for (int32_t depth = 1; depth <= 60; ++depth) {
            const int32_t shallow = probcut_shallow_depth(depth);
            const double s = static_cast<double>(shallow);
            const double d = static_cast<double>(depth);
            double sigma;
            if (!egaroucid16) {
                sigma = 1.8871 + 2.9543 * x + 0.0660 * x * x - 0.6749 * s + 0.2778 * d;
            } else if (!has_mobility) {
                sigma = 0.9172 + 5.0552 * x - 2.4951 * x * x - 0.5569 * s + 0.2567 * d;
            } else {
                sigma = 0.9972 + 5.1252 * x - 2.8746 * x * x - 0.5219 * s + 0.2280 * d;
            }
            (*errors)[discs][static_cast<size_t>(depth)] = static_cast<int32_t>(
                std::ceil(t * (std::max)(sigma, 0.5) * static_cast<double>(eval::PATTERN_UNITS_PER_DISC)));
        }
    }
    return errors;
}

std::unique_ptr<EndProbCutErrors> make_end_probcut_errors(bool egaroucid16, bool has_mobility) {
    static constexpr std::array<double, END_MPC_STAGES + 1> kSelectivityT = {
        0.0, 1.13, 1.55, 1.81, 2.32, 2.57, 3.29,
    };
    auto errors = std::make_unique<EndProbCutErrors>();
    for (size_t stage = 1; stage <= END_MPC_STAGES; ++stage) {
        const double t = kSelectivityT[stage];
        for (int32_t empties = 0; empties <= 64; ++empties) {
            const int32_t shallow = ((empties / 3) & ~1) + (empties & 1);
            const double x = static_cast<double>(64 - empties) / 64.0;
            const double s = static_cast<double>(shallow);
            double raw_sigma;
            if (!egaroucid16) {
                raw_sigma = 1.0376 + 5.4960 * x - 0.0378 * s;
            } else if (!has_mobility) {
                raw_sigma = 6.5937 - 0.2985 * x - 0.2975 * s;
            } else {
                raw_sigma = 8.0571 - 1.8599 * x - 0.3890 * s;
            }
            const double sigma = (std::max)(2.0, raw_sigma);
            (*errors)[stage][static_cast<size_t>(empties)] = static_cast<int32_t>(
                std::ceil(t * sigma * static_cast<double>(eval::PATTERN_UNITS_PER_DISC)));
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
    const EndProbCutErrors* end_probcut_errors,
    const TranspositionTable& midgame_table,
    const TranspositionTable& endgame_table,
    Clock clock,
    uint32_t root_empties
) {
    SearchContext ctx;
    ctx.pattern = options.pattern ? options.pattern.get() : &eval::EvalWeights::built_in();
    ctx.probcut_errors = probcut_errors;
    ctx.end_probcut_errors = end_probcut_errors;
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
                    static_cast<double>(options_.probcut_percent.value_or(DEFAULT_PROBCUT_PERCENT)) / 100.0,
                    (options_.pattern ? *options_.pattern : eval::EvalWeights::built_in()).set().name() == "egaroucid16",
                    (options_.pattern ? *options_.pattern : eval::EvalWeights::built_in()).has_mobility())
              : nullptr),
      end_probcut_errors_(
          options_.selective
              ? detail::make_end_probcut_errors(
                    (options_.pattern ? *options_.pattern : eval::EvalWeights::built_in()).set().name() == "egaroucid16",
                    (options_.pattern ? *options_.pattern : eval::EvalWeights::built_in()).has_mobility())
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
        options_, probcut_errors_.get(), end_probcut_errors_.get(), midgame_table_, endgame_table_, clock_, root_empties);
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
        options_, probcut_errors_.get(), end_probcut_errors_.get(), midgame_table_, endgame_table_, clock_, root_empties);
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
      end_probcut_errors(context.end_probcut_errors),
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
    STELLO_TIER_DISPATCH(run, *this, board, player, limits, only_moves);
}

AbortVoid Searcher::iterate(
    const Board& board,
    Player player,
    const SearchLimits& limits,
    std::span<int32_t> root_moves,
    Best& best
) {
    STELLO_TIER_DISPATCH(iterate, *this, board, player, limits, root_moves, best);
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
    STELLO_TIER_DISPATCH(solve_iteration, *this, board, player, alpha, beta, kind, root_moves, best);
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
    STELLO_TIER_DISPATCH(lazy_smp_midgame, *this, smp_helpers, board, player, limits, budget, root_moves, best);
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
    STELLO_TIER_DISPATCH(midgame_iterations, *this, board, player, limits, budget, root_moves, best, main_look);
}

AbortVoid Searcher::search_root(
    const Board& board,
    Player player,
    int32_t look,
    std::span<int32_t> root_moves,
    Best& best
) {
    STELLO_TIER_DISPATCH(search_root, *this, board, player, look, root_moves, best);
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
    STELLO_TIER_DISPATCH(search, *this, board, player, look, ply, previous_move, alpha, beta);
}

AbortVoid Searcher::order_by_evaluation(
    std::span<int32_t> list,
    int32_t hash_move,
    const Board& board,
    Player player
) {
    STELLO_TIER_DISPATCH(order_by_evaluation, *this, list, hash_move, board, player);
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
    STELLO_TIER_DISPATCH(split_midgame, *this, board, player, look, ply, tail, alpha, beta, best, best_move);
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
    STELLO_TIER_DISPATCH(try_probcut, *this, board, player, look, ply, previous_move, alpha, beta);
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
    STELLO_TIER_DISPATCH(probcut_searches, *this, board, player, shallow_look, ply, previous_move, alpha, beta, error, error0, static0);
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
    STELLO_TIER_DISPATCH(solve_root, *this, board, player, alpha, beta, kind, root_moves, best);
}

AbortResult<int32_t> Searcher::evaluate_leaf(const Board& board, Player player) {
    STELLO_TIER_DISPATCH(evaluate_leaf, *this, board, player);
}

bool Searcher::stop_iterating(const SearchLimits& limits, time::TimeBudget budget, int32_t next_look) const noexcept {
    switch (limits.mode()) {
        case TimeControlMode::FixedDepth: return next_look >= limits.depth();
        case TimeControlMode::Solve: return false;
        default: return elapsed_ms() > budget.soft_ms;
    }
}

void Searcher::maybe_report_heartbeat() {
    if (progress && report_move >= 0 && elapsed_ms() - last_heartbeat_ms >= 100) {
        report(report_depth, report_move, report_best_move, report_score, report_kind);
    }
}

STELLO_NOINLINE AbortVoid Searcher::check_abort() {
    flush_nodes();
    maybe_report_heartbeat();

    if (stop->is_cancelled()) {
        STELLO_RETURN_ABORT_IN_STATUS(Abort::Cancelled);
    }

    if (stop->is_move_now()
        || (split_point != nullptr && split_point->stopped())
        || elapsed_ms() >= hard_limit_ms)
    {
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
