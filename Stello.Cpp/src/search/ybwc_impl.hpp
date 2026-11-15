#pragma once

#include "search/kernel.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <limits>
#include <optional>

namespace stello::search {

namespace detail::ybwc_impl {

inline constexpr int32_t NO_RESULT = (std::numeric_limits<int32_t>::min)();

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

} // namespace detail::ybwc_impl

template <bool UseAvx2>
AbortResult<int32_t> Kernel<UseAvx2>::child_null_window(
    Searcher& searcher,
    const MidgameChild& child,
    int32_t alpha
) {
    const eval::PatternState child_eval = searcher.pattern->set().state_of(child.board.black(), child.board.white());
    STELLO_TRY_ASSIGN_I32(
        const ScoreMove res,
        search_inc(searcher, child.board, child.player, child.look, child.ply, child.mv, -alpha - 1, -alpha, child_eval));
    searcher.ordering->record_response(child.player, child.mv, res.mv, -res.score <= alpha);
    STELLO_RETURN_OK_R(int32_t, -res.score);
}

template <bool UseAvx2>
AbortResult<int32_t> Kernel<UseAvx2>::child_window(
    Searcher& searcher,
    const MidgameChild& child,
    int32_t alpha,
    int32_t beta,
    int32_t /*null_score*/
) {
    // Principal variation re-search matches single-threaded `Searcher::search`: window starts from `alpha`.
    const eval::PatternState child_eval = searcher.pattern->set().state_of(child.board.black(), child.board.white());
    STELLO_TRY_ASSIGN_I32(
        const ScoreMove res,
        search_inc(searcher, child.board, child.player, child.look, child.ply, child.mv, -beta, -alpha, child_eval));
    searcher.ordering->record_response(child.player, child.mv, res.mv, -res.score <= alpha);
    STELLO_RETURN_OK_R(int32_t, -res.score);
}

template <bool UseAvx2>
AbortResult<int32_t> Kernel<UseAvx2>::child_null_window(
    Searcher& searcher,
    const EndgameChild& child,
    int32_t alpha
) {
    STELLO_TRY_ASSIGN_I32(const int32_t sc, end_null(searcher, child.own, child.opponent, -alpha - 1, child.empties, child.replies));
    STELLO_RETURN_OK_R(int32_t, -sc);
}

template <bool UseAvx2>
AbortResult<int32_t> Kernel<UseAvx2>::child_window(
    Searcher& searcher,
    const EndgameChild& child,
    int32_t alpha,
    int32_t beta,
    int32_t null_score
) {
    // Matches single-threaded `Searcher::end_window`: re-search starts from the null-window lower bound.
    const int32_t bound_alpha = (std::max)(alpha, null_score);
    STELLO_TRY_ASSIGN_I32(const int32_t sc, end_window(searcher, child.own, child.opponent, -beta, -bound_alpha, child.empties, false));
    STELLO_RETURN_OK_R(int32_t, -sc);
}

template <bool UseAvx2>
template <typename ChildT>
AbortVoid Kernel<UseAvx2>::split(
    Searcher& s,
    std::span<const ChildT> children,
    int32_t alpha,
    int32_t beta,
    int32_t& best,
    int32_t& best_move,
    bool is_root
) {
    using namespace detail::ybwc_impl;

    assert(s.helpers.has_value() && "only a search with helpers splits");
    assert(children.size() <= MAX_MOVES);

    const auto update_best = [&](int32_t score, int32_t mv) noexcept {
        if (score > best) {
            best = score;
            best_move = mv;
            if (is_root) {
                s.report_best_move = best_move;
                s.report_score = best;
            }
        }
    };

    const Helpers split_helpers = *s.helpers;
    uint64_t pending = (children.size() == 64) ? ~uint64_t{0} : ((uint64_t{1} << children.size()) - 1ULL);
    std::array<int32_t, MAX_MOVES> scores{};

    while (pending != 0) {
        alpha = (std::max)(alpha, best);
        const int32_t window_alpha = alpha;

        SplitPoint point;
        const SplitPoint* const prev_split = s.split_point;
        point.parent = prev_split;

        struct RestoreSplit {
            Searcher& s;
            const SplitPoint* prev;
            ~RestoreSplit() noexcept { s.split_point = prev; }
        };

        std::array<std::atomic<int32_t>, MAX_MOVES> results{};
        for (size_t i = 0; i < children.size(); ++i) {
            results[i].store(NO_RESULT, std::memory_order_relaxed);
        }

        const TaskSetup setup{
            s.hash_get_height,
            s.hash_put_height,
            s.in_probcut,
            s.start,
            s.hard_limit_ms,
        };
        const StopSignal* caller_stop = s.stop;
        const Clock caller_clock = s.clock;

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
                                const int32_t score = Kernel<UseAvx2>::child_null_window(task, child, window_alpha);
                                results[i].store(score, std::memory_order_relaxed);
                                if (score > window_alpha) {
                                    point.cut.store(true, std::memory_order_relaxed);
                                    point.stop.store(true, std::memory_order_relaxed);
                                }
                            } catch (Abort) {
                            }
#else
                            const AbortResult<int32_t> res = Kernel<UseAvx2>::child_null_window(task, child, window_alpha);
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

                int32_t score = 0;
#if STELLO_ABORT_EXCEPTIONS
                std::optional<Abort> abort_caught;
                try {
                    RestoreSplit restore{s, prev_split};
                    s.split_point = &point;
                    score = child_null_window(s, child, window_alpha);
                } catch (Abort a) {
                    const bool external_stop = (a == Abort::Cancelled)
                        || caller_stop->is_cancelled()
                        || caller_stop->is_move_now()
                        || (prev_split != nullptr && prev_split->stopped())
                        || (s.elapsed_ms() >= setup.hard_limit_ms);
                    if (!external_stop && point.cut.load(std::memory_order_relaxed)) {
                        break;
                    }
                    abort_caught = a;
                }
                if (abort_caught.has_value()) {
                    STELLO_RETURN_ABORT_IN_STATUS(*abort_caught);
                }
#else
                AbortResult<int32_t> local_res{};
                {
                    RestoreSplit restore{s, prev_split};
                    s.split_point = &point;
                    local_res = child_null_window(s, child, window_alpha);
                }
                if (local_res.is_err()) {
                    const Abort a = local_res.abort_reason();
                    const bool external_stop = (a == Abort::Cancelled)
                        || caller_stop->is_cancelled()
                        || caller_stop->is_move_now()
                        || (prev_split != nullptr && prev_split->stopped())
                        || (s.elapsed_ms() >= setup.hard_limit_ms);
                    if (!external_stop && point.cut.load(std::memory_order_relaxed)) {
                        break;
                    }
                    return make_abort<AbortStatus>(a);
                }
                score = local_res.value;
#endif
                pending &= ~bit;
                if (score > window_alpha) {
                    fail_highs |= bit;
                    scores[i] = score;
                    point.cut.store(true, std::memory_order_relaxed);
                    point.stop.store(true, std::memory_order_relaxed);
                    break;
                }

                update_best(score, child.square());
            }

            guard.finish();
            STELLO_RETURN_OK_VOID();
        };

        const auto poll = [&]() {
            if (!point.stop.load(std::memory_order_relaxed)) {
                const Duration now = caller_clock();
                const Duration el = (now > setup.start) ? (now - setup.start) : Duration::zero();
                const int64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(el).count();
                if (caller_stop->is_cancelled()
                    || caller_stop->is_move_now()
                    || (prev_split != nullptr && prev_split->stopped())
                    || elapsed_ms >= setup.hard_limit_ms)
                {
                    point.stop.store(true, std::memory_order_relaxed);
                }
            }
            s.maybe_report_heartbeat();
        };

#if STELLO_ABORT_EXCEPTIONS
        split_helpers.pool->scope_polling(body, poll);
#else
        STELLO_TRY_VOID_IN_STATUS(split_helpers.pool->scope_polling(body, poll));
#endif

        if (caller_stop->is_cancelled()) {
            STELLO_RETURN_ABORT_IN_STATUS(Abort::Cancelled);
        }

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
            } else {
                update_best(score, children[i].square());
            }
        }

        // Brothers stopped without a cutoff here: an ancestor split point, signal, or hard limit stopped the search.
        if (missing && !point.cut.load(std::memory_order_relaxed)) {
            STELLO_RETURN_ABORT_IN_STATUS(s.stop->is_cancelled() ? Abort::Cancelled : Abort::Stop);
        }

        // A brother reaching beta cuts off the node immediately.
        uint64_t highs = fail_highs;
        while (highs != 0) {
            const size_t i = static_cast<size_t>(std::countr_zero(highs));
            highs &= (highs - 1ULL);
            if (scores[i] >= beta) {
                update_best(scores[i], children[i].square());
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
                child_window(s, children[i], (std::max)(alpha, best), beta, scores[i]));
            update_best(score, children[i].square());
            if (best >= beta) {
                STELLO_RETURN_OK_VOID();
            }
        }
    }

    STELLO_RETURN_OK_VOID();
}

} // namespace stello::search
