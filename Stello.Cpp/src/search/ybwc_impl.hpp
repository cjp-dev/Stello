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

/// RAII guard decrementing `joining` if a helper exits before claiming work from the split node.
struct JoinGuard {
    std::atomic<size_t>& joining;
    bool active;

    JoinGuard(std::atomic<size_t>& j, bool a) noexcept : joining(j), active(a) {}
    ~JoinGuard() noexcept { release(); }

    void release() noexcept {
        if (active) {
            joining.fetch_sub(1, std::memory_order_acq_rel);
            active = false;
        }
    }

    JoinGuard(const JoinGuard&) = delete;
    JoinGuard& operator=(const JoinGuard&) = delete;
};

/// Restores `Searcher::split_point` and `Searcher::current_split_entry` on unwind.
struct RestoreSplitContext {
    Searcher& s;
    const SplitPoint* prev_sp;
    SplitEntry* prev_en;

    RestoreSplitContext(Searcher& searcher, const SplitPoint* sp, SplitEntry* en) noexcept
        : s(searcher), prev_sp(sp), prev_en(en) {}
    ~RestoreSplitContext() noexcept {
        s.split_point = prev_sp;
        s.current_split_entry = prev_en;
    }

    RestoreSplitContext(const RestoreSplitContext&) = delete;
    RestoreSplitContext& operator=(const RestoreSplitContext&) = delete;
};

} // namespace detail::ybwc_impl

template <cpu::CpuTier Tier>
AbortResult<int32_t> Kernel<Tier>::child_null_window(
    Searcher& searcher,
    const MidgameChild& child,
    int32_t alpha
) {
    eval::PatternState child_eval;
    if (child.parent_eval != nullptr) {
        advance_eval_state(
            searcher.pattern->set(),
            child_eval,
            *child.parent_eval,
            child.parent_is_even,
            static_cast<uint32_t>(child.mv),
            child.move_flips);
    } else {
        child_eval = eval_state_of_node(
            searcher.pattern->set(),
            child.board.discs(child.player),
            child.board.discs(opponent(child.player)),
            64U - child.board.empty_count());
    }
    STELLO_TRY_ASSIGN_I32(
        const ScoreMove res,
        search_inc(
            searcher,
            child.board,
            child.player,
            child.look,
            child.ply,
            child.mv,
            -alpha - 1,
            -alpha,
            child_eval,
            child.known_legal));
    searcher.ordering->record_response(child.player, child.mv, res.mv, -res.score <= alpha);
    STELLO_RETURN_OK_R(int32_t, -res.score);
}

template <cpu::CpuTier Tier>
AbortResult<int32_t> Kernel<Tier>::child_window(
    Searcher& searcher,
    const MidgameChild& child,
    int32_t alpha,
    int32_t beta,
    int32_t /*null_score*/
) {
    // Principal variation re-search matches single-threaded `Searcher::search`: window starts from `alpha`.
    eval::PatternState child_eval;
    if (child.parent_eval != nullptr) {
        advance_eval_state(
            searcher.pattern->set(),
            child_eval,
            *child.parent_eval,
            child.parent_is_even,
            static_cast<uint32_t>(child.mv),
            child.move_flips);
    } else {
        child_eval = eval_state_of_node(
            searcher.pattern->set(),
            child.board.discs(child.player),
            child.board.discs(opponent(child.player)),
            64U - child.board.empty_count());
    }
    STELLO_TRY_ASSIGN_I32(
        const ScoreMove res,
        search_inc(
            searcher,
            child.board,
            child.player,
            child.look,
            child.ply,
            child.mv,
            -beta,
            -alpha,
            child_eval,
            child.known_legal));
    searcher.ordering->record_response(child.player, child.mv, res.mv, -res.score <= alpha);
    STELLO_RETURN_OK_R(int32_t, -res.score);
}

template <cpu::CpuTier Tier>
AbortResult<int32_t> Kernel<Tier>::child_null_window(
    Searcher& searcher,
    const EndgameChild& child,
    int32_t alpha
) {
    STELLO_TRY_ASSIGN_I32(const int32_t sc, end_null(searcher, child.own, child.opponent, -alpha - 1, child.empties, child.replies));
    STELLO_RETURN_OK_R(int32_t, -sc);
}

template <cpu::CpuTier Tier>
AbortResult<int32_t> Kernel<Tier>::child_window(
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

template <cpu::CpuTier Tier>
template <typename ChildT>
AbortVoid Kernel<Tier>::split(
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
    assert(s.thread_state != nullptr && "main searcher must have thread_state");
    assert(children.size() <= MAX_MOVES);

    if (children.empty()) {
        STELLO_RETURN_OK_VOID();
    }

    const SplitPoint* const prev_split = s.split_point;
    SplitEntry* const prev_entry = s.current_split_entry;
    const StopSignal* caller_stop = s.stop;
    const Clock caller_clock = s.clock;
    Searcher* const hb_owner = (s.heartbeat_owner != nullptr) ? s.heartbeat_owner : &s;

    const auto commit_score = [&](int32_t score, int32_t mv) noexcept {
        if (score > best) {
            best = score;
            best_move = mv;
            if (is_root) {
                s.report_best_move = best_move;
                s.report_score = (s.end_mpc_stage > 0) ? (best * eval::PATTERN_UNITS_PER_DISC) : best;
            }
        }
    };

    // Fast path: null-window search (`beta == alpha + 1`) uses a single-pass stack SplitNode with zero waves.
    if (beta - alpha == 1) {
        struct SplitNode final : SplitEntry {
            SplitPoint point{};
            std::span<const ChildT> children;
            Helpers split_helpers{};
            TaskSetup setup{};
            int32_t alpha{0};
            int32_t beta{0};
            bool is_root{false};

            std::atomic<int32_t> live_best_move{-1};
            std::atomic<int32_t> live_best_score{-INFINITY_SCORE};

            std::mutex data_mutex;
            size_t next_idx{0};
            size_t completed_moves{0};
            int32_t best{-INFINITY_SCORE};
            int32_t best_move{-1};

            static void execute_worker(SplitEntry* self, void* state_ptr, void* host_ctx) noexcept {
                auto* const node = static_cast<SplitNode*>(self);
                auto* const state = static_cast<ThreadState*>(state_ptr);
                SplitStop flag(&node->point);
                Searcher task = node->split_helpers.searcher(*state, flag);
                task.split_task(node->split_helpers, &node->point, node->setup, node);
                task.heartbeat_owner = static_cast<Searcher*>(host_ctx);
                (void)node->run_loop(task, false);
                node->split_helpers.finish(task);
            }

            AbortStatus run_loop(Searcher& task, bool is_master) noexcept {
                JoinGuard join_guard{joining, !is_master};

                for (;;) {
                    if (point.is_stopped()) {
                        return AbortStatus::ok();
                    }

                    size_t idx = 0;
                    {
                        std::lock_guard<std::mutex> dlk(data_mutex);
                        if (next_idx >= children.size() || best >= beta) {
                            open.store(false, std::memory_order_release);
                            return AbortStatus::ok();
                        }
                        idx = next_idx;
                        ++next_idx;
                        unclaimed.fetch_sub(1, std::memory_order_acq_rel);
                        join_guard.release();
                        if (next_idx >= children.size()) {
                            open.store(false, std::memory_order_release);
                        }
                    }

                    const ChildT& child = children[idx];
                    int32_t score = 0;

#if STELLO_ABORT_EXCEPTIONS
                    try {
                        score = Kernel<Tier>::child_null_window(task, child, alpha);
                    } catch (Abort a) {
                        if (is_master) {
                            return AbortStatus::err(a);
                        }
                        return AbortStatus::ok();
                    }
#else
                    const AbortResult<int32_t> nres = Kernel<Tier>::child_null_window(task, child, alpha);
                    if (nres.is_err()) {
                        if (is_master) {
                            return AbortStatus::err(nres.abort_reason());
                        }
                        return AbortStatus::ok();
                    }
                    score = nres.value;
#endif

                    std::lock_guard<std::mutex> dlk(data_mutex);
                    ++completed_moves;
                    if (score > best) {
                        best = score;
                        best_move = child.square();
                        if (is_root) {
                            live_best_move.store(best_move, std::memory_order_relaxed);
                            live_best_score.store(best, std::memory_order_relaxed);
                        }
                        if (best >= beta) {
                            point.request_cut();
                            open.store(false, std::memory_order_release);
                            return AbortStatus::ok();
                        }
                    }
                }
            }
        };

        SplitNode node;
        node.parent = prev_entry;
        node.priority = children[0].priority();
        node.max_slaves = is_root
            ? children.size()
            : (std::min)(children.size(), SPLIT_MAX_SLAVES);
        node.unclaimed.store(children.size(), std::memory_order_relaxed);
        node.execute = &SplitNode::execute_worker;
        node.point.parent = prev_split;
        node.children = children;
        node.split_helpers = *s.helpers;
        node.setup = TaskSetup{
            s.hash_get_height,
            s.hash_put_height,
            s.in_probcut,
            s.end_mpc_stage,
            s.start,
            s.hard_limit_ms,
        };
        node.alpha = alpha;
        node.beta = beta;
        node.is_root = is_root;
        node.best = best;
        node.best_move = best_move;
        node.live_best_move.store(best_move, std::memory_order_relaxed);
        node.live_best_score.store(best, std::memory_order_relaxed);

        const int32_t start_best_score = best;
        const auto poll = [&]() {
            if (!node.point.is_stopped()) {
                const Duration now = caller_clock();
                const Duration el = (now > node.setup.start) ? (now - node.setup.start) : Duration::zero();
                const int64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(el).count();
                if (caller_stop->is_cancelled()
                    || caller_stop->is_move_now()
                    || (prev_split != nullptr && prev_split->stopped())
                    || elapsed_ms >= node.setup.hard_limit_ms)
                {
                    node.point.request_stop();
                }
            }
            if (is_root) {
                const int32_t lm = node.live_best_move.load(std::memory_order_relaxed);
                const int32_t ls = node.live_best_score.load(std::memory_order_relaxed);
                if (lm >= 0 && ls > start_best_score) {
                    const int32_t scaled = (s.end_mpc_stage > 0) ? (ls * eval::PATTERN_UNITS_PER_DISC) : ls;
                    if (s.report_best_move < 0 || scaled > s.report_score) {
                        s.report_best_move = lm;
                        s.report_score = scaled;
                    }
                }
            }
            hb_owner->maybe_report_heartbeat();
        };

        const size_t max_wake = (children.size() > 1U)
            ? (std::min)(children.size() - 1U, node.max_slaves)
            : size_t{0};
        node.split_helpers.pool->publish_split(node, max_wake);

        bool has_master_abort = false;
        Abort master_abort = Abort::Stop;
        struct WaitOnExit {
            const ThreadPool<ThreadState>* pool;
            SplitNode& node;
            ThreadState& state;
            void* host_ctx;
            const decltype(poll)& poll_fn;
            bool waited{false};
            ~WaitOnExit() noexcept {
                if (!waited) {
                    pool->wait_split(node, state, host_ctx, poll_fn);
                }
            }
        } wait_guard{node.split_helpers.pool, node, *s.thread_state, hb_owner, poll, false};

        {
            RestoreSplitContext restore{s, prev_split, prev_entry};
            s.split_point = &node.point;
            s.current_split_entry = &node;

            const AbortStatus master_st = node.run_loop(s, true);
            if (master_st.is_err()) {
                const Abort a = master_st.abort_reason();
                const bool external_stop = (a == Abort::Cancelled)
                    || caller_stop->is_cancelled()
                    || caller_stop->is_move_now()
                    || (prev_split != nullptr && prev_split->stopped())
                    || (s.elapsed_ms() >= node.setup.hard_limit_ms);
                if (external_stop || !node.point.is_cut()) {
                    node.point.request_stop();
                    has_master_abort = true;
                    master_abort = a;
                }
            }
        }

        node.split_helpers.pool->wait_split(node, *s.thread_state, hb_owner, poll);
        wait_guard.waited = true;

        commit_score(node.best, node.best_move);

        if (has_master_abort) {
            STELLO_RETURN_ABORT_IN_STATUS(master_abort);
        }
        if (caller_stop->is_cancelled()) {
            STELLO_RETURN_ABORT_IN_STATUS(Abort::Cancelled);
        }
        if (!node.point.is_cut() && node.completed_moves < children.size()) {
            STELLO_RETURN_ABORT_IN_STATUS(caller_stop->is_move_now() ? Abort::Stop : (s.stop->is_cancelled() ? Abort::Cancelled : Abort::Stop));
        }

        STELLO_RETURN_OK_VOID();
    }

    // Wide-window path (`beta - alpha > 1`): cut siblings immediately on any fail-high, re-search PVs cleanly, and continue remaining waves with tightened alpha.
    uint64_t pending = (children.size() == 64) ? ~uint64_t{0} : ((uint64_t{1} << children.size()) - 1ULL);

    while (pending != 0) {
        alpha = (std::max)(alpha, best);
        if (alpha >= beta) {
            STELLO_RETURN_OK_VOID();
        }
        const int32_t window_alpha = alpha;

        std::array<uint8_t, MAX_MOVES> wave_indices{};
        size_t wave_count = 0;
        {
            uint64_t rem = pending;
            while (rem != 0) {
                const size_t idx = static_cast<size_t>(std::countr_zero(rem));
                rem &= (rem - 1ULL);
                wave_indices[wave_count++] = static_cast<uint8_t>(idx);
            }
        }

        if (wave_count == 1 || !s.may_split(wave_count)) {
            for (size_t w = 0; w < wave_count; ++w) {
                const size_t cidx = wave_indices[w];
                const ChildT& child = children[cidx];
                if (is_root) {
                    s.report_move = child.square();
                    s.report_best_move = best_move;
                    s.report_score = (s.end_mpc_stage > 0) ? (best * eval::PATTERN_UNITS_PER_DISC) : best;
                }
                const int32_t cur_alpha = (std::max)(alpha, best);
                STELLO_TRY_ASSIGN_IN_STATUS(const int32_t nscore, child_null_window(s, child, cur_alpha));
                int32_t score = nscore;
                if (score > cur_alpha && score < beta) {
                    STELLO_TRY_ASSIGN_IN_STATUS(const int32_t wscore, child_window(s, child, cur_alpha, beta, score));
                    score = wscore;
                }
                if (score > best) {
                    commit_score(score, child.square());
                    if (best > alpha) {
                        alpha = best;
                    }
                    if (best >= beta) {
                        STELLO_RETURN_OK_VOID();
                    }
                }
            }
            STELLO_RETURN_OK_VOID();
        }

        struct WaveSplitNode final : SplitEntry {
            SplitPoint point{};
            std::span<const ChildT> children;
            const uint8_t* wave_indices{nullptr};
            size_t wave_count{0};
            Helpers split_helpers{};
            TaskSetup setup{};
            int32_t window_alpha{0};
            bool is_root{false};

            std::atomic<int32_t> live_best_move{-1};
            std::atomic<int32_t> live_best_score{-INFINITY_SCORE};

            std::mutex data_mutex;
            size_t next_idx{0};
            size_t completed_count{0};
            uint64_t completed_mask{0};
            uint64_t fail_highs{0};
            std::array<int32_t, MAX_MOVES> fail_scores{};
            int32_t best{-INFINITY_SCORE};
            int32_t best_move{-1};

            static void execute_worker(SplitEntry* self, void* state_ptr, void* host_ctx) noexcept {
                auto* const node = static_cast<WaveSplitNode*>(self);
                auto* const state = static_cast<ThreadState*>(state_ptr);
                SplitStop flag(&node->point);
                Searcher task = node->split_helpers.searcher(*state, flag);
                task.split_task(node->split_helpers, &node->point, node->setup, node);
                task.heartbeat_owner = static_cast<Searcher*>(host_ctx);
                (void)node->run_loop(task, false);
                node->split_helpers.finish(task);
            }

            AbortStatus run_loop(Searcher& task, bool is_master) noexcept {
                JoinGuard join_guard{joining, !is_master};

                for (;;) {
                    if (point.is_stopped()) {
                        return AbortStatus::ok();
                    }

                    size_t wi = 0;
                    {
                        std::lock_guard<std::mutex> dlk(data_mutex);
                        if (next_idx >= wave_count) {
                            open.store(false, std::memory_order_release);
                            return AbortStatus::ok();
                        }
                        wi = next_idx;
                        ++next_idx;
                        unclaimed.fetch_sub(1, std::memory_order_acq_rel);
                        join_guard.release();
                        if (next_idx >= wave_count) {
                            open.store(false, std::memory_order_release);
                        }
                    }

                    const size_t cidx = wave_indices[wi];
                    const ChildT& child = children[cidx];
                    int32_t score = 0;

#if STELLO_ABORT_EXCEPTIONS
                    try {
                        score = Kernel<Tier>::child_null_window(task, child, window_alpha);
                    } catch (Abort a) {
                        if (is_master) {
                            return AbortStatus::err(a);
                        }
                        return AbortStatus::ok();
                    }
#else
                    const AbortResult<int32_t> nres = Kernel<Tier>::child_null_window(task, child, window_alpha);
                    if (nres.is_err()) {
                        if (is_master) {
                            return AbortStatus::err(nres.abort_reason());
                        }
                        return AbortStatus::ok();
                    }
                    score = nres.value;
#endif

                    const uint64_t bit = uint64_t{1} << cidx;
                    std::lock_guard<std::mutex> dlk(data_mutex);
                    completed_mask |= bit;
                    ++completed_count;
                    if (score > window_alpha) {
                        fail_highs |= bit;
                        fail_scores[cidx] = score;
                        point.request_cut();
                        open.store(false, std::memory_order_release);
                        return AbortStatus::ok();
                    }
                    if (score > best) {
                        best = score;
                        best_move = child.square();
                        if (is_root) {
                            live_best_move.store(best_move, std::memory_order_relaxed);
                            live_best_score.store(best, std::memory_order_relaxed);
                        }
                    }
                }
            }
        };

        WaveSplitNode node;
        node.parent = prev_entry;
        node.priority = children[wave_indices[0]].priority();
        node.max_slaves = is_root
            ? wave_count
            : (std::min)(wave_count, SPLIT_MAX_SLAVES);
        node.unclaimed.store(wave_count, std::memory_order_relaxed);
        node.execute = &WaveSplitNode::execute_worker;
        node.point.parent = prev_split;
        node.children = children;
        node.wave_indices = wave_indices.data();
        node.wave_count = wave_count;
        node.split_helpers = *s.helpers;
        node.setup = TaskSetup{
            s.hash_get_height,
            s.hash_put_height,
            s.in_probcut,
            s.end_mpc_stage,
            s.start,
            s.hard_limit_ms,
        };
        node.window_alpha = window_alpha;
        node.is_root = is_root;
        node.best = best;
        node.best_move = best_move;
        node.live_best_move.store(best_move, std::memory_order_relaxed);
        node.live_best_score.store(best, std::memory_order_relaxed);

        const int32_t start_best_score = best;
        const auto poll = [&]() {
            if (!node.point.is_stopped()) {
                const Duration now = caller_clock();
                const Duration el = (now > node.setup.start) ? (now - node.setup.start) : Duration::zero();
                const int64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(el).count();
                if (caller_stop->is_cancelled()
                    || caller_stop->is_move_now()
                    || (prev_split != nullptr && prev_split->stopped())
                    || elapsed_ms >= node.setup.hard_limit_ms)
                {
                    node.point.request_stop();
                }
            }
            if (is_root) {
                const int32_t lm = node.live_best_move.load(std::memory_order_relaxed);
                const int32_t ls = node.live_best_score.load(std::memory_order_relaxed);
                if (lm >= 0 && ls > start_best_score) {
                    const int32_t scaled = (s.end_mpc_stage > 0) ? (ls * eval::PATTERN_UNITS_PER_DISC) : ls;
                    if (s.report_best_move < 0 || scaled > s.report_score) {
                        s.report_best_move = lm;
                        s.report_score = scaled;
                    }
                }
            }
            hb_owner->maybe_report_heartbeat();
        };

        const size_t max_wake = (wave_count > 1U)
            ? (std::min)(wave_count - 1U, node.max_slaves)
            : size_t{0};
        node.split_helpers.pool->publish_split(node, max_wake);

        bool has_master_abort = false;
        Abort master_abort = Abort::Stop;
        struct WaitOnExit {
            const ThreadPool<ThreadState>* pool;
            WaveSplitNode& node;
            ThreadState& state;
            void* host_ctx;
            const decltype(poll)& poll_fn;
            bool waited{false};
            ~WaitOnExit() noexcept {
                if (!waited) {
                    pool->wait_split(node, state, host_ctx, poll_fn);
                }
            }
        } wait_guard{node.split_helpers.pool, node, *s.thread_state, hb_owner, poll, false};

        {
            RestoreSplitContext restore{s, prev_split, prev_entry};
            s.split_point = &node.point;
            s.current_split_entry = &node;

            const AbortStatus master_st = node.run_loop(s, true);
            if (master_st.is_err()) {
                const Abort a = master_st.abort_reason();
                const bool external_stop = (a == Abort::Cancelled)
                    || caller_stop->is_cancelled()
                    || caller_stop->is_move_now()
                    || (prev_split != nullptr && prev_split->stopped())
                    || (s.elapsed_ms() >= node.setup.hard_limit_ms);
                if (external_stop || !node.point.is_cut()) {
                    node.point.request_stop();
                    has_master_abort = true;
                    master_abort = a;
                }
            }
        }

        node.split_helpers.pool->wait_split(node, *s.thread_state, hb_owner, poll);
        wait_guard.waited = true;

        commit_score(node.best, node.best_move);

        if (has_master_abort) {
            STELLO_RETURN_ABORT_IN_STATUS(master_abort);
        }
        if (caller_stop->is_cancelled()) {
            STELLO_RETURN_ABORT_IN_STATUS(Abort::Cancelled);
        }
        if (!node.point.is_cut() && node.completed_count < wave_count) {
            STELLO_RETURN_ABORT_IN_STATUS(caller_stop->is_move_now() ? Abort::Stop : (s.stop->is_cancelled() ? Abort::Cancelled : Abort::Stop));
        }

        pending &= ~node.completed_mask;

        if (node.fail_highs != 0) {
            int32_t beta_score = -INFINITY_SCORE;
            int32_t beta_move = -1;
            uint64_t highs = node.fail_highs;
            while (highs != 0) {
                const size_t cidx = static_cast<size_t>(std::countr_zero(highs));
                highs &= (highs - 1ULL);
                if (node.fail_scores[cidx] >= beta && node.fail_scores[cidx] > beta_score) {
                    beta_score = node.fail_scores[cidx];
                    beta_move = children[cidx].square();
                }
            }
            if (beta_move >= 0) {
                commit_score(beta_score, beta_move);
                STELLO_RETURN_OK_VOID();
            }

            std::array<uint8_t, MAX_MOVES> fh_order{};
            size_t fh_count = 0;
            highs = node.fail_highs;
            while (highs != 0) {
                const size_t cidx = static_cast<size_t>(std::countr_zero(highs));
                highs &= (highs - 1ULL);
                fh_order[fh_count++] = static_cast<uint8_t>(cidx);
            }
            std::stable_sort(
                fh_order.begin(),
                fh_order.begin() + static_cast<std::ptrdiff_t>(fh_count),
                [&](uint8_t a, uint8_t b) { return node.fail_scores[a] > node.fail_scores[b]; });

            for (size_t k = 0; k < fh_count; ++k) {
                const size_t cidx = fh_order[k];
                const int32_t cur_alpha = (std::max)(alpha, best);
                int32_t probe = node.fail_scores[cidx];
                if (cur_alpha > window_alpha) {
                    STELLO_TRY_ASSIGN_IN_STATUS(const int32_t reprobe, child_null_window(s, children[cidx], cur_alpha));
                    if (reprobe <= cur_alpha) {
                        commit_score(reprobe, children[cidx].square());
                        continue;
                    }
                    probe = reprobe;
                }
                if (probe < beta) {
                    STELLO_TRY_ASSIGN_IN_STATUS(const int32_t full, child_window(s, children[cidx], cur_alpha, beta, probe));
                    probe = full;
                }
                commit_score(probe, children[cidx].square());
                if (best > alpha) {
                    alpha = best;
                }
                if (best >= beta) {
                    STELLO_RETURN_OK_VOID();
                }
            }
        }
    }

    STELLO_RETURN_OK_VOID();
}

} // namespace stello::search
