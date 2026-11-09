#include "search/ybwc.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <limits>

namespace stello::search {

namespace {

constexpr int32_t NO_RESULT = (std::numeric_limits<int32_t>::min)();

/// Stop signal for a YBWC split child: requests Move Now once `point` or any ancestor split point is stopped.
class SplitStop final : public StopSignal {
public:
    explicit SplitStop(const SplitPoint* point) noexcept : point_(point) {}

    bool is_cancelled() const noexcept override { return false; }
    bool is_move_now() const noexcept override { return point_->stopped(); }

private:
    const SplitPoint* point_;
};

/// Marks a split point stopped on early unwind/return before `finish()` is called.
struct StopUnlessFinished {
    std::atomic<bool>& stop;
    bool finished{false};

    explicit StopUnlessFinished(std::atomic<bool>& s) noexcept : stop(s) {}
    ~StopUnlessFinished() noexcept {
        if (!finished) {
            stop.store(true, std::memory_order_relaxed);
        }
    }

    void finish() noexcept { finished = true; }

    StopUnlessFinished(const StopUnlessFinished&) = delete;
    StopUnlessFinished& operator=(const StopUnlessFinished&) = delete;
};

} // namespace

AbortResult<int32_t> MidgameChild::null_window(Searcher& searcher, int32_t alpha) const {
    STELLO_TRY_ASSIGN_I32(const ScoreMove res, searcher.search(board, player, look, ply, mv, -alpha - 1, -alpha));
    searcher.ordering->record_response(player, mv, res.mv, -res.score <= alpha);
    STELLO_RETURN_OK_R(int32_t, -res.score);
}

AbortResult<int32_t> MidgameChild::window(
    Searcher& searcher,
    int32_t alpha,
    int32_t beta,
    int32_t /*null_score*/
) const {
    // Principal variation re-search matches single-threaded `Searcher::search`: window starts from `alpha`.
    STELLO_TRY_ASSIGN_I32(const ScoreMove res, searcher.search(board, player, look, ply, mv, -beta, -alpha));
    searcher.ordering->record_response(player, mv, res.mv, -res.score <= alpha);
    STELLO_RETURN_OK_R(int32_t, -res.score);
}

AbortResult<int32_t> EndgameChild::null_window(Searcher& searcher, int32_t alpha) const {
    STELLO_TRY_ASSIGN_I32(const int32_t s, searcher.end_null(own, opponent, -alpha - 1, empties, replies));
    STELLO_RETURN_OK_R(int32_t, -s);
}

AbortResult<int32_t> EndgameChild::window(
    Searcher& searcher,
    int32_t alpha,
    int32_t beta,
    int32_t null_score
) const {
    // Matches single-threaded `Searcher::end_window`: re-search starts from the null-window lower bound.
    const int32_t bound_alpha = (std::max)(alpha, null_score);
    STELLO_TRY_ASSIGN_I32(const int32_t s, searcher.end_window(own, opponent, -beta, -bound_alpha, empties, false));
    STELLO_RETURN_OK_R(int32_t, -s);
}

void Searcher::split_task(const Helpers& h, const SplitPoint* point, const TaskSetup& setup) noexcept {
    helpers = h;
    split_point = point;
    hash_get_height = setup.hash_get_height;
    hash_put_height = setup.hash_put_height;
    in_probcut = setup.in_probcut;
    start = setup.start;
    hard_limit_ms = setup.hard_limit_ms;
}

template <typename ChildT>
AbortVoid Searcher::split(
    std::span<const ChildT> children,
    int32_t alpha,
    int32_t beta,
    int32_t& best,
    int32_t& best_move
) {
    assert(helpers.has_value() && "only a search with helpers splits");
    assert(children.size() <= MAX_MOVES);

    const Helpers split_helpers = *helpers;
    uint64_t pending = (children.size() == 64) ? ~uint64_t{0} : ((uint64_t{1} << children.size()) - 1ULL);
    std::array<int32_t, MAX_MOVES> scores{};

    while (pending != 0) {
        alpha = (std::max)(alpha, best);
        const int32_t window_alpha = alpha;

        SplitPoint point;
        point.parent = split_point;

        std::array<std::atomic<int32_t>, MAX_MOVES> results{};
        for (size_t i = 0; i < children.size(); ++i) {
            results[i].store(NO_RESULT, std::memory_order_relaxed);
        }

        const TaskSetup setup{
            hash_get_height,
            hash_put_height,
            in_probcut,
            start,
            hard_limit_ms,
        };
        const StopSignal* caller_stop = stop;
        const Clock caller_clock = clock;

        uint64_t spawned = 0;
        uint64_t fail_highs = 0;

        const auto body = [&](const Scope<ThreadState>& scope) -> AbortVoid {
            StopUnlessFinished guard(point.stop);
            uint64_t rest = pending;

            while (rest != 0) {
                if (point.stop.load(std::memory_order_relaxed)) {
                    break;
                }

                const size_t i = static_cast<size_t>(std::countr_zero(rest));
                const uint64_t bit = uint64_t{1} << i;
                rest &= ~bit;
                const ChildT child = children[i];

                // Keep the last brother on this thread rather than waiting immediately.
                if (rest != 0 && split_helpers.pool->idle() > 0) {
                    const bool started = scope.try_spawn(
                        [split_helpers, &point, &results, setup, child, window_alpha, i](ThreadState& state) {
                            SplitStop flag(&point);
                            Searcher task = split_helpers.searcher(state, flag);
                            task.split_task(split_helpers, &point, setup);

#if STELLO_ABORT_EXCEPTIONS
                            try {
                                const int32_t score = child.null_window(task, window_alpha);
                                results[i].store(score, std::memory_order_relaxed);
                                if (score > window_alpha) {
                                    point.cut.store(true, std::memory_order_relaxed);
                                    point.stop.store(true, std::memory_order_relaxed);
                                }
                            } catch (Abort) {
                            }
#else
                            const AbortResult<int32_t> res = child.null_window(task, window_alpha);
                            if (res.is_ok()) {
                                results[i].store(res.value, std::memory_order_relaxed);
                                if (res.value > window_alpha) {
                                    point.cut.store(true, std::memory_order_relaxed);
                                    point.stop.store(true, std::memory_order_relaxed);
                                }
                            }
#endif
                            split_helpers.finish(task);
                        });
                    if (started) {
                        spawned |= bit;
                        continue;
                    }
                }

                STELLO_TRY_ASSIGN_IN_STATUS(const int32_t score, child.null_window(*this, window_alpha));
                pending &= ~bit;
                if (score > window_alpha) {
                    fail_highs |= bit;
                    scores[i] = score;
                    point.cut.store(true, std::memory_order_relaxed);
                    point.stop.store(true, std::memory_order_relaxed);
                    break;
                }

                if (score > best) {
                    best = score;
                    best_move = child.square();
                }
            }

            guard.finish();
            STELLO_RETURN_OK_VOID();
        };

        const auto poll = [&]() noexcept {
            if (!point.stop.load(std::memory_order_relaxed)) {
                const Duration now = caller_clock();
                const Duration el = (now > setup.start) ? (now - setup.start) : Duration::zero();
                const int64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(el).count();
                if (caller_stop->is_cancelled() || caller_stop->is_move_now() || elapsed_ms >= setup.hard_limit_ms) {
                    point.stop.store(true, std::memory_order_relaxed);
                }
            }
        };

#if STELLO_ABORT_EXCEPTIONS
        split_helpers.pool->scope_polling(body, poll);
#else
        STELLO_TRY_VOID_IN_STATUS(split_helpers.pool->scope_polling(body, poll));
#endif

        bool missing = false;
        uint64_t tasks = spawned;
        while (tasks != 0) {
            const size_t i = static_cast<size_t>(std::countr_zero(tasks));
            tasks &= (tasks - 1ULL);
            const int32_t score = results[i].load(std::memory_order_relaxed);
            if (score == NO_RESULT) {
                missing = true;
                continue;
            }

            pending &= ~(uint64_t{1} << i);
            if (score > window_alpha) {
                fail_highs |= (uint64_t{1} << i);
                scores[i] = score;
            } else if (score > best) {
                best = score;
                best_move = children[i].square();
            }
        }

        // Brothers stopped without a cutoff here: an ancestor split point, signal, or hard limit stopped the search.
        if (missing && !point.cut.load(std::memory_order_relaxed)) {
            STELLO_RETURN_ABORT_IN_STATUS(stop->is_cancelled() ? Abort::Cancelled : Abort::Stop);
        }

        // A brother reaching beta cuts off the node immediately.
        uint64_t highs = fail_highs;
        while (highs != 0) {
            const size_t i = static_cast<size_t>(std::countr_zero(highs));
            highs &= (highs - 1ULL);
            if (scores[i] >= beta) {
                if (scores[i] > best) {
                    best = scores[i];
                    best_move = children[i].square();
                }
                STELLO_RETURN_OK_VOID();
            }
        }

        // Other fail-highs re-search sequentially with the full window.
        highs = fail_highs;
        while (highs != 0) {
            const size_t i = static_cast<size_t>(std::countr_zero(highs));
            highs &= (highs - 1ULL);
            STELLO_TRY_ASSIGN_IN_STATUS(
                const int32_t score,
                children[i].window(*this, (std::max)(alpha, best), beta, scores[i]));
            if (score > best) {
                best = score;
                best_move = children[i].square();
                if (best >= beta) {
                    STELLO_RETURN_OK_VOID();
                }
            }
        }
    }

    STELLO_RETURN_OK_VOID();
}

template AbortVoid Searcher::split<MidgameChild>(
    std::span<const MidgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&
);

template AbortVoid Searcher::split<EndgameChild>(
    std::span<const EndgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&
);

} // namespace stello::search
