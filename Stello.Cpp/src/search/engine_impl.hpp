#pragma once

#include "eval/eval.hpp"
#include "search/kernel.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <limits>
#include <optional>

namespace stello::search {

namespace detail::midgame {

inline constexpr int32_t ROOT_WINDOW = 32664;
inline constexpr int32_t ENDGAME_DISTANCE = 7;
inline constexpr int32_t PROBCUT_SCORE_LIMIT = 32000;
inline constexpr int32_t PROBCUT_MIN_LOOK = 2;
inline constexpr int32_t PROBCUT_PRE_CHECK_OFFSET = 4 * eval::PATTERN_UNITS_PER_DISC;
inline constexpr int32_t EVALUATION_ORDER_MIN_LOOK = 4;
inline constexpr int32_t MIDGAME_ETC_MIN_LOOK = 4;
inline constexpr size_t SOLVE_HELPERS = 3;

inline constexpr std::array<uint64_t, 5> STATIC_CELL_PRIORITY = {
    0x8100'0000'0000'0081ULL, // corners (4)
    0x0000'3C3C'3C3C'0000ULL, // center 4x4 box (16)
    0x3C3C'C3C3'C3C3'3C3CULL, // edges & outer-inner non-C/X (32)
    0x4281'0000'0000'8142ULL, // C squares (8)
    0x0042'0000'0000'4200ULL, // X squares (4)
};
static_assert((STATIC_CELL_PRIORITY[0] | STATIC_CELL_PRIORITY[1] | STATIC_CELL_PRIORITY[2] | STATIC_CELL_PRIORITY[3] | STATIC_CELL_PRIORITY[4]) == ~uint64_t{0});
static_assert(std::popcount(STATIC_CELL_PRIORITY[0]) + std::popcount(STATIC_CELL_PRIORITY[1]) + std::popcount(STATIC_CELL_PRIORITY[2]) + std::popcount(STATIC_CELL_PRIORITY[3]) + std::popcount(STATIC_CELL_PRIORITY[4]) == 64);

template <bool Enable>
struct SiblingBaselineSlot {};

template <>
struct SiblingBaselineSlot<true> {
    eval::SiblingBaseline data;
};

inline size_t lazy_smp_helpers(size_t workers) noexcept {
    return (workers + 2U) / 4U;
}

inline void apply_solve_best(ScoreKind kind, int32_t empties, int32_t mv, int32_t score, Best& best) noexcept {
    if (kind == ScoreKind::WinLossDraw && score > 0) {
        best = Best{mv, score, kind, empties};
    } else if (kind == ScoreKind::WinLossDraw && score == 0) {
        best = Best{mv, score, ScoreKind::Heuristic, best.depth};
    } else if (kind == ScoreKind::Exact && best.kind == ScoreKind::WinLossDraw) {
        // Keep WinLossDraw/empties until all moves finish, while retaining any strictly higher Exact move.
        best.mv = mv;
    }
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

struct ResetEndMpcStage {
    int32_t& stage;
    ~ResetEndMpcStage() noexcept { stage = 0; }
};

inline uint64_t current_total_nodes(const Searcher& s) noexcept {
    return s.nodes + (s.node_source != nullptr ? s.node_source->load(std::memory_order_relaxed) : 0ULL);
}

} // namespace detail::midgame

template <cpu::CpuTier Tier>
SearchOutcome Kernel<Tier>::run(
    Searcher& s,
    const Board& board,
    Player player,
    const SearchLimits& limits,
    std::optional<uint64_t> only_moves
) {
    uint64_t moves = legal_moves(board.discs(player), board.discs(opponent(player)));
    if (only_moves.has_value()) {
        moves &= *only_moves;
        if (moves == 0) {
            return {false, {}, SearchError::NoLegalOnlyMove};
        }
    }

    if (moves == 0) {
        return {true, s.make_result(Best{-1, 0, ScoreKind::None, 0}), SearchError::Cancelled};
    }

    if (std::popcount(moves) == 1 && limits.mode() != TimeControlMode::Solve && !only_moves.has_value()) {
        return {
            true,
            s.make_result(Best{static_cast<int32_t>(std::countr_zero(moves)), 0, ScoreKind::None, 0}),
            SearchError::Cancelled,
        };
    }

    s.ordering->clear_responses();
    std::array<int32_t, MAX_MOVES> root{};
    const size_t count = fill_moves(root, moves);
    std::span<int32_t> root_moves(root.data(), count);
    s.ordering->order(root_moves, -1, -1, player, board);

    Best best{root_moves[0], 0, ScoreKind::None, 0};

#if STELLO_ABORT_EXCEPTIONS
    try {
        iterate(s, board, player, limits, root_moves, best);
        return {true, s.make_result(best), SearchError::Cancelled};
    } catch (Abort a) {
        if (a == Abort::Cancelled) {
            return {false, {}, SearchError::Cancelled};
        }
        return {true, s.make_result(best), SearchError::Cancelled};
    }
#else
    const AbortStatus st = iterate(s, board, player, limits, root_moves, best);
    if (st.is_err() && st.abort_reason() == Abort::Cancelled) {
        return {false, {}, SearchError::Cancelled};
    }
    return {true, s.make_result(best), SearchError::Cancelled};
#endif
}

template <cpu::CpuTier Tier>
AbortVoid Kernel<Tier>::iterate(
    Searcher& s,
    const Board& board,
    Player player,
    const SearchLimits& limits,
    std::span<int32_t> root_moves,
    Best& best
) {
    using namespace detail::midgame;

    const int32_t empties = static_cast<int32_t>(board.empty_count());
    time::TimeBudget budget = time::for_midgame(limits, empties).with_soft_percent(s.soft_time_percent, 0);
    s.hard_limit_ms = budget.hard_ms;
    s.end_mpc_stage = 0;

    if (limits.mode() != TimeControlMode::Solve) {
        bool done = false;
        if (s.helpers.has_value()) {
            const Helpers active_helpers = *s.helpers;
            STELLO_TRY_ASSIGN_IN_STATUS(
                done,
                lazy_smp_midgame(s, active_helpers, board, player, limits, budget, root_moves, best));
        } else {
            STELLO_TRY_ASSIGN_IN_STATUS(
                done,
                midgame_iterations(s, board, player, limits, budget, root_moves, best, nullptr));
        }
        if (done) {
            STELLO_RETURN_OK_VOID();
        }
    } else {
        int32_t look = 0;
        while (look < SOLVE_ORDERING_LOOKS && empties - 1 - look > ENDGAME_DISTANCE) {
            s.hash_get_height = look - 1;
            s.hash_put_height = look;
            STELLO_TRY_VOID_IN_STATUS(search_root(s, board, player, look, root_moves, best));
            ++look;
        }
    }

    const int64_t mid_elapsed_ms = (std::max<int64_t>)(s.elapsed_ms(), 1);
    const uint64_t mid_total_nodes = current_total_nodes(s);

    if (limits.mode() != TimeControlMode::Solve
        && limits.mode() != TimeControlMode::FixedDepth
        && s.end_probcut_errors != nullptr
        && empties >= END_MPC_MIN_EMPTIES)
    {
        static constexpr std::array<int32_t, 3> kSelectiveStages = {1, 2, 4};
        ResetEndMpcStage reset_stage{s.end_mpc_stage};
        bool finished_all_stages = true;
        for (const int32_t stage : kSelectiveStages) {
            if (s.stop_iterating(limits, budget, 0)) {
                finished_all_stages = false;
                break;
            }
            s.end_mpc_stage = stage;
            s.hash_get_height = 64;
            s.hash_put_height = 64;
            Best stage_best = best;
            STELLO_TRY_ASSIGN_IN_STATUS(
                const ScoreMove stage_res,
                solve_iteration(s, board, player, -64, 64, ScoreKind::Heuristic, root_moves, stage_best));
            best = Best{
                stage_res.mv,
                stage_res.score * eval::PATTERN_UNITS_PER_DISC,
                ScoreKind::Heuristic,
                empties,
            };
        }
        s.end_mpc_stage = 0;

        if (!finished_all_stages) {
            STELLO_RETURN_OK_VOID();
        }
        if (limits.mode() == TimeControlMode::TimePerGame && best.depth > 0) {
            const time::TimeBudget est_budget = time::for_solver(limits, budget, s.elapsed_ms(), s.soft_time_percent);
            const int64_t rem_solve_ms = est_budget.hard_ms - s.elapsed_ms();
            if (rem_solve_ms <= 0) {
                STELLO_RETURN_OK_VOID();
            }
            const int64_t max_budget = (budget.hard_ms <= (std::numeric_limits<int64_t>::max)() / 4)
                ? (budget.hard_ms * 4)
                : (std::numeric_limits<int64_t>::max)();
            const int64_t capped_rem_ms = (std::min)(rem_solve_ms, max_budget);
            if (!time::should_admit_wld(
                    empties,
                    empties - 8,
                    s.last_iter_nodes,
                    mid_elapsed_ms,
                    mid_total_nodes,
                    capped_rem_ms))
            {
                STELLO_RETURN_OK_VOID();
            }
        }
    }

    budget = time::for_solver(limits, budget, s.elapsed_ms(), s.soft_time_percent);
    s.hard_limit_ms = budget.hard_ms;

    STELLO_TRY_ASSIGN_IN_STATUS(
        const ScoreMove wld,
        solve_iteration(s, board, player, -1, 1, ScoreKind::WinLossDraw, root_moves, best));
    const bool keep_midgame_move = (wld.score < 0 && best.kind == ScoreKind::Heuristic);
    best = Best{
        keep_midgame_move ? best.mv : wld.mv,
        wld.score,
        (wld.score == 0) ? ScoreKind::Exact : ScoreKind::WinLossDraw,
        empties,
    };

    if (wld.score == 0 || s.stop_iterating(limits, budget, 0)) {
        STELLO_RETURN_OK_VOID();
    }

    if (keep_midgame_move && best.mv >= 0) {
        const std::array<int32_t, 1> front{best.mv};
        detail::move_to_front(root_moves, front);
    }

    const int32_t alpha = (wld.score > 0) ? (wld.score - 1) : -65;
    const int32_t beta = (wld.score > 0) ? 65 : (wld.score + 1);
    STELLO_TRY_ASSIGN_IN_STATUS(
        const ScoreMove exact,
        solve_iteration(s, board, player, alpha, beta, ScoreKind::Exact, root_moves, best));
    best = Best{exact.mv, exact.score, ScoreKind::Exact, empties};
    STELLO_RETURN_OK_VOID();
}

template <cpu::CpuTier Tier>
AbortResult<ScoreMove> Kernel<Tier>::solve_iteration(
    Searcher& s,
    const Board& board,
    Player player,
    int32_t alpha,
    int32_t beta,
    ScoreKind kind,
    std::span<int32_t> root_moves,
    Best& best
) {
    using namespace detail::midgame;

    const size_t helper_count =
        (s.helpers.has_value() && !s.ybwc) ? (std::min)(s.helpers->count, SOLVE_HELPERS) : 0;
    if (helper_count == 0) {
        return solve_root(s, board, player, alpha, beta, kind, root_moves, best);
    }

    const Helpers active_helpers = *s.helpers;
    std::atomic<bool> stop_flag{false};
    const Board board_copy = board;
    const size_t count = root_moves.size();
    const Best start_best = best;
    const int32_t end_mpc_stage = s.end_mpc_stage;
    const int32_t hash_get_height = s.hash_get_height;
    const int32_t hash_put_height = s.hash_put_height;
    std::array<int32_t, MAX_MOVES> order{};
    std::copy_n(root_moves.data(), count, order.data());

    return active_helpers.pool->scope([&](const Scope<ThreadState>& scope) -> AbortResult<ScoreMove> {
        StopHelpers stop_helpers(stop_flag);
        for (size_t i = 0; i < helper_count; ++i) {
            const bool started = scope.try_spawn(
                [active_helpers, &stop_flag, board_copy, player, alpha, beta, kind, order, count, start_best,
                 end_mpc_stage, hash_get_height, hash_put_height, i](
                    ThreadState& state
                ) {
                    FlagStop flag(stop_flag);
                    Searcher helper = active_helpers.searcher(state, flag);
                    helper.end_mpc_stage = end_mpc_stage;
                    helper.hash_get_height = hash_get_height;
                    helper.hash_put_height = hash_put_height;
                    auto moves = order;
                    Best local_best = start_best;
                    std::rotate(
                        moves.begin(),
                        moves.begin() + static_cast<std::ptrdiff_t>((i + 1) % count),
                        moves.begin() + static_cast<std::ptrdiff_t>(count));
#if STELLO_ABORT_EXCEPTIONS
                    try {
                        (void)Kernel<Tier>::solve_root(
                            helper,
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
                    (void)Kernel<Tier>::solve_root(
                        helper,
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

        return solve_root(s, board_copy, player, alpha, beta, kind, root_moves, best);
    });
}

template <cpu::CpuTier Tier>
AbortResult<bool> Kernel<Tier>::lazy_smp_midgame(
    Searcher& s,
    const Helpers& smp_helpers,
    const Board& board,
    Player player,
    const SearchLimits& limits,
    time::TimeBudget budget,
    std::span<int32_t> root_moves,
    Best& best
) {
    using namespace detail::midgame;

    const int32_t last_look = static_cast<int32_t>(board.empty_count()) - 2 - ENDGAME_DISTANCE;
    if (last_look < 0) {
        return midgame_iterations(s, board, player, limits, budget, root_moves, best, nullptr);
    }

    std::atomic<bool> stop_flag{false};
    std::atomic<int32_t> main_look{0};
    const Board board_copy = board;
    const size_t count = root_moves.size();
    const Best start_best = best;
    std::array<int32_t, MAX_MOVES> order{};
    std::copy_n(root_moves.data(), count, order.data());

    return smp_helpers.pool->scope([&](const Scope<ThreadState>& scope) -> AbortResult<bool> {
        StopHelpers stop_helpers(stop_flag);
        const size_t lazy = s.ybwc ? lazy_smp_helpers(smp_helpers.count) : smp_helpers.count;
        for (size_t i = 0; i < lazy; ++i) {
            const int32_t ahead = 1 + static_cast<int32_t>(std::countr_zero(static_cast<uint32_t>(i) + 1U));
            const bool started = scope.try_spawn(
                [smp_helpers, &stop_flag, &main_look, board_copy, player, order, count, start_best, last_look, ahead, i](
                    ThreadState& state
                ) {
                    FlagStop flag(stop_flag);
                    Searcher helper = smp_helpers.searcher(state, flag);
                    auto moves = order;
                    if (count > 2) {
                        const size_t rot = (i + 1U) % (count - 1U);
                        if (rot > 0) {
                            std::rotate(
                                moves.begin() + 1,
                                moves.begin() + 1 + static_cast<std::ptrdiff_t>(rot),
                                moves.begin() + static_cast<std::ptrdiff_t>(count));
                        }
                    }
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
                            Kernel<Tier>::search_root(
                                helper,
                                board_copy,
                                player,
                                look,
                                std::span<int32_t>(moves.data(), count),
                                local_best);
                            if (look == last_look) {
                                break;
                            }
                            look += (((look + static_cast<int32_t>(i)) & 1) != 0) ? 2 : 1;
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
                        const AbortStatus st = Kernel<Tier>::search_root(
                            helper,
                            board_copy,
                            player,
                            look,
                            std::span<int32_t>(moves.data(), count),
                            local_best);
                        if (st.is_err() || look == last_look) {
                            break;
                        }
                        look += (((look + static_cast<int32_t>(i)) & 1) != 0) ? 2 : 1;
                    }
#endif
                    smp_helpers.finish(helper);
                });
            if (!started) {
                break;
            }
        }

        return midgame_iterations(s, board_copy, player, limits, budget, root_moves, best, &main_look);
    });
}

template <cpu::CpuTier Tier>
AbortResult<bool> Kernel<Tier>::midgame_iterations(
    Searcher& s,
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
    while (empties - 1 - look > detail::midgame::ENDGAME_DISTANCE) {
        if (main_look != nullptr) {
            main_look->store(look, std::memory_order_relaxed);
        }
        const uint64_t iter_nodes_start = detail::midgame::current_total_nodes(s);
        s.hash_get_height = look - 1;
        s.hash_put_height = look;
        STELLO_TRY_VOID_R(bool, search_root(s, board, player, look, root_moves, best));
        s.last_iter_nodes = detail::midgame::current_total_nodes(s) - iter_nodes_start;
        if (s.iteration_done != nullptr && *s.iteration_done) {
            (*s.iteration_done)(look + 1, best.score);
        }
        if (s.stop_iterating(limits, budget, look + 1)) {
            STELLO_RETURN_OK_R(bool, true);
        }
        ++look;
    }
    STELLO_RETURN_OK_R(bool, false);
}

template <cpu::CpuTier Tier>
AbortVoid Kernel<Tier>::search_root(
    Searcher& s,
    const Board& board,
    Player player,
    int32_t look,
    std::span<int32_t> root_moves,
    Best& best
) {
    const Player opp = opponent(player);
    const auto& set = s.pattern->set();
    const uint32_t root_discs = 64U - board.empty_count();
    const bool root_is_even = (root_discs & 1U) == 0U;
    const eval::PatternState root_eval = eval_state_of_node(set, board.discs(player), board.discs(opp), root_discs);
    const uint32_t child_discs = root_discs + 1U;
    const int32_t* child_w = nullptr;
    if (look == 0) {
        child_w = s.pattern->packed_weights_for_discs(child_discs);
    }

    int32_t alpha = -detail::midgame::ROOT_WINDOW;
    const int32_t beta = detail::midgame::ROOT_WINDOW;
    int32_t best_score = -INFINITY_SCORE;
    int32_t best_move = -1;
    int32_t skip_thresh = (std::numeric_limits<int32_t>::max)() / 2;
    std::array<int32_t, MAX_MOVES> improvements;
    size_t improved = 0;

    for (size_t i = 0; i < root_moves.size(); ++i) {
        if (i >= 1 && look >= YBWC_MID_MIN_DEPTH && s.may_split(root_moves.size() - i)) {
            const std::span<const int32_t> tail = root_moves.subspan(i);
            std::array<MidgameChild, MAX_MOVES> children;
            for (size_t c = 0; c < tail.size(); ++c) {
                const int32_t cmv = tail[c];
                const uint64_t cf = flips(board.discs(player), board.discs(opp), static_cast<uint32_t>(cmv));
                children[c] = MidgameChild{
                    board.play_flips(player, static_cast<uint32_t>(cmv), cf),
                    opp,
                    look - 1,
                    1,
                    cmv,
                    &root_eval,
                    cf,
                    endgame::bits::LEGAL_UNDEFINED,
                    root_is_even,
                };
            }

            s.report_depth = look + 1;
            s.report_move = tail[0];
            s.report_best_move = best_move;
            s.report_score = best_score;
            s.report_kind = ScoreKind::Heuristic;

            const int32_t score_before_split = best_score;
            struct CommitMidBest {
                Best& best;
                int32_t& best_move;
                int32_t& best_score;
                int32_t threshold;
                int32_t depth;
                ~CommitMidBest() noexcept {
                    if (best_score > threshold && best_move >= 0) {
                        best = Best{best_move, best_score, ScoreKind::Heuristic, depth};
                    }
                }
            } commit{best, best_move, best_score, score_before_split, look + 1};

            STELLO_TRY_VOID_IN_STATUS(
                s.split(std::span<const MidgameChild>(children.data(), tail.size()), alpha, beta, best_score, best_move, true));
            if (best_score > score_before_split && best_move >= 0) {
                improvements[improved] = best_move;
                ++improved;
            }
            s.report(look + 1, tail.back(), best_move, best_score, ScoreKind::Heuristic);
            break;
        }

        const int32_t mv = root_moves[i];
        s.report_depth = look + 1;
        s.report_move = mv;
        s.report_best_move = (best_move >= 0) ? best_move : best.mv;
        s.report_score = (best_move >= 0) ? best_score : best.score;
        s.report_kind = ScoreKind::Heuristic;

        const uint64_t f = flips(board.discs(player), board.discs(opp), static_cast<uint32_t>(mv));
        int32_t score = 0;

        if (look == 0) {
            STELLO_TRY_VOID_IN_STATUS(s.count_node());
            s.evaluations += 1;
            score = eval_after_move_score_bounded(
                *s.pattern, root_eval, root_is_even, child_discs, child_w,
                skip_thresh, best_score,
                board.discs(player), board.discs(opp), static_cast<uint32_t>(mv), f);
        } else {
            const Board child = board.play_flips(player, static_cast<uint32_t>(mv), f);
            eval::PatternState child_eval;
            advance_eval_state(set, child_eval, root_eval, root_is_even, static_cast<uint32_t>(mv), f);
            const int32_t child_look = look - 1;
            int32_t reply = -1;

            if (i == 0 || look <= 2) {
                STELLO_TRY_ASSIGN_IN_STATUS(
                    const ScoreMove res,
                    search_inc(s, child, opp, child_look, 1, mv, -beta, -alpha, child_eval));
                score = -res.score;
                reply = res.mv;
            } else {
                STELLO_TRY_ASSIGN_IN_STATUS(
                    const ScoreMove res,
                    search_inc(s, child, opp, child_look, 1, mv, -alpha - 1, -alpha, child_eval));
                if (-res.score > alpha && -res.score < beta) {
                    STELLO_TRY_ASSIGN_IN_STATUS(
                        const ScoreMove full,
                        search_inc(s, child, opp, child_look, 1, mv, -beta, -alpha, child_eval));
                    score = -full.score;
                    reply = full.mv;
                } else {
                    score = -res.score;
                    reply = res.mv;
                }
            }

            s.ordering->record_response(opp, mv, reply, score <= alpha);
        }

        if (score > best_score) {
            best_score = score;
            best_move = mv;
            if (look == 0) {
                skip_thresh = eval::raw_skip_threshold(best_score, s.pattern->units_per_disc());
            }
            if (score > alpha) {
                alpha = score;
            }
            improvements[improved] = mv;
            ++improved;
            best = Best{mv, score, ScoreKind::Heuristic, look + 1};
        }

        s.report(look + 1, mv, best_move, best_score, ScoreKind::Heuristic);
        if (best_score >= beta) {
            break;
        }
    }

    detail::move_to_front(root_moves, std::span<const int32_t>(improvements.data(), improved));
    STELLO_RETURN_OK_VOID();
}

template <cpu::CpuTier Tier>
AbortResult<ScoreMove> Kernel<Tier>::search(
    Searcher& s,
    const Board& board,
    Player player,
    int32_t look,
    int32_t ply,
    int32_t previous_move,
    int32_t alpha,
    int32_t beta
) {
    const eval::PatternState eval_state = eval_state_of_node(
        s.pattern->set(), board.discs(player), board.discs(opponent(player)), 64U - board.empty_count());
    return search_inc(s, board, player, look, ply, previous_move, alpha, beta, eval_state);
}

template <cpu::CpuTier Tier>
AbortResult<ScoreMove> Kernel<Tier>::search_inc(
    Searcher& s,
    const Board& board,
    Player player,
    int32_t look,
    int32_t ply,
    int32_t previous_move,
    int32_t alpha,
    int32_t beta,
    const eval::PatternState& eval_state,
    uint64_t known_legal
) {
    using namespace detail::midgame;

    if (beta - alpha == 1 && look <= 1) {
        if (look == 0) {
            return eval1_nws(s, board, player, ply, alpha, eval_state, known_legal);
        }
        return eval2_nws(s, board, player, ply, alpha, eval_state, known_legal);
    }

    const uint64_t nodes_start = s.nodes;
    STELLO_TRY_VOID_R(ScoreMove, s.count_node());

    const Player opp = opponent(player);
    const uint64_t own = board.discs(player);
    const uint64_t other = board.discs(opp);
    const uint32_t node_discs = 64U - board.empty_count();
    const uint64_t moves = (known_legal != endgame::bits::LEGAL_UNDEFINED)
        ? known_legal
        : legal_moves(own, other);

    if (moves == 0) {
        if (legal_moves(other, own) == 0) {
            s.evaluations += 1;
            STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{game_over_score(own, other), -1}));
        }

        const eval::PatternState pass_eval = eval_state_of_node(s.pattern->set(), other, own, node_discs);
        STELLO_TRY_ASSIGN_R(
            ScoreMove,
            const ScoreMove passed,
            search_inc(s, board, opp, look, ply, -1, -beta, -alpha, pass_eval));
        STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{-passed.score, -1}));
    }

    int32_t hash_move = -1;
    const bool hash_get = ply <= s.hash_get_height;
    const bool hash_put = ply <= s.hash_put_height;
    const int32_t tag = static_cast<int32_t>(player_index(player));
    if (hash_put && !hash_get) {
        s.midgame_table->prefetch(own, other, tag);
    }

    if (hash_get) {
        const uint8_t req_sel = (s.probcut_errors == nullptr) ? SEL_EXACT : SEL_MIDGAME_MPC;
        if (const auto entry = s.midgame_table->get_at_root(own, other, tag, s.root_empties)) {
            if (entry->mv >= 0 && entry->mv < 64 && (moves & (uint64_t{1} << entry->mv)) != 0) {
                if (static_cast<int32_t>(entry->depth) >= look && entry->selectivity >= req_sel) {
                    const int32_t lo = static_cast<int32_t>(entry->lower);
                    const int32_t hi = static_cast<int32_t>(entry->upper);
                    if (lo > TT_SCORE_MIN && lo >= beta) {
                        STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{lo, static_cast<int32_t>(entry->mv)}));
                    }
                    if (hi < TT_SCORE_MAX && hi <= alpha) {
                        STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{hi, static_cast<int32_t>(entry->mv)}));
                    }
                    if (lo == hi) {
                        STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{lo, static_cast<int32_t>(entry->mv)}));
                    }
                }
                hash_move = static_cast<int32_t>(entry->mv);
            }
        }
    }

    if (s.probcut_errors != nullptr && look >= PROBCUT_MIN_LOOK && !s.in_probcut) {
        STELLO_TRY_ASSIGN_R(
            ScoreMove,
            const auto cut,
            try_probcut_inc(s, board, player, look, ply, previous_move, alpha, beta, eval_state));
        if (cut.has_value()) {
            STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{*cut, -1}));
        }
    }

    if (look >= EVALUATION_ORDER_MIN_LOOK) {
        return search_inc_deep(s, board, player, look, ply, alpha, beta, eval_state, moves, hash_move, hash_put);
    }

    if (look == 2 && beta - alpha == 1) {
        const auto& set = s.pattern->set();
        const bool node_is_even = (node_discs & 1U) == 0U;
        int32_t best = -INFINITY_SCORE;
        int32_t best_move = -1;
        uint64_t rem = moves;

        if (hash_move >= 0) {
            const uint32_t hmv = static_cast<uint32_t>(hash_move);
            const uint64_t f = flips(own, other, hmv);
            const Board child = board.play_flips(player, hmv, f);
            eval::PatternState child_eval;
            advance_eval_state(set, child_eval, eval_state, node_is_even, hmv, f);
            STELLO_TRY_ASSIGN_R(
                ScoreMove,
                const ScoreMove res,
                eval2_nws(s, child, opp, ply + 1, -alpha - 1, child_eval));
            best = -res.score;
            best_move = hash_move;
            rem &= ~(uint64_t{1} << hmv);
        }

        for (size_t mi = 0; mi < STATIC_CELL_PRIORITY.size() && best <= alpha; ++mi) {
            uint64_t sub = rem & STATIC_CELL_PRIORITY[mi];
            while (sub != 0) {
                const uint32_t mv = static_cast<uint32_t>(std::countr_zero(sub));
                sub &= (sub - 1ULL);
                const uint64_t f = flips(own, other, mv);
                const Board child = board.play_flips(player, mv, f);
                eval::PatternState child_eval;
                advance_eval_state(set, child_eval, eval_state, node_is_even, mv, f);
                STELLO_TRY_ASSIGN_R(
                    ScoreMove,
                    const ScoreMove res,
                    eval2_nws(s, child, opp, ply + 1, -alpha - 1, child_eval));
                const int32_t score = -res.score;
                if (score > best) {
                    best = score;
                    best_move = static_cast<int32_t>(mv);
                    if (best > alpha) {
                        break;
                    }
                }
            }
        }

        if (hash_put) {
            const uint8_t req_sel = (s.probcut_errors == nullptr) ? SEL_EXACT : SEL_MIDGAME_MPC;
            s.midgame_table->store_bounds_at_root(
                own, other, tag, 2, req_sel, alpha, beta, best, best_move, s.root_empties,
                search_cost(s.nodes - nodes_start));
        }
        STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{best, best_move}));
    }

    const int32_t stored_look = look;
    const size_t move_count = static_cast<size_t>(std::popcount(moves));

    if (move_count == 1 && look == 0) {
        look = 1;
    }

    std::array<int32_t, MAX_MOVES> buffer;
    fill_moves(buffer, moves);
    std::span<int32_t> list(buffer.data(), move_count);

    s.ordering->order(list, hash_move, previous_move, player, board);

    const auto& set = s.pattern->set();
    const bool node_is_even = (node_discs & 1U) == 0U;
    const uint32_t child_discs = node_discs + 1U;
    const int32_t* child_w = nullptr;
    if (look == 0) {
        child_w = s.pattern->packed_weights_for_discs(child_discs);
    }

    int32_t best = -INFINITY_SCORE;
    int32_t best_move = -1;
    int32_t skip_thresh = (std::numeric_limits<int32_t>::max)() / 2;
    const int32_t alpha_start = alpha;

    for (size_t index = 0; index < list.size(); ++index) {
        if (index >= 1 && look >= YBWC_MID_MIN_DEPTH && s.may_split(list.size() - index)) {
            STELLO_TRY_VOID_R(
                ScoreMove,
                split_midgame(
                    s, board, player, look, ply, list.subspan(index), alpha, beta, best, best_move,
                    eval_state, node_is_even, {}, {}));
            break;
        }

        const int32_t mv = list[index];
        const uint64_t f = flips(own, other, static_cast<uint32_t>(mv));
        int32_t score = 0;

        if (look == 0) {
            STELLO_TRY_VOID_R(ScoreMove, s.count_node());
            s.evaluations += 1;
            score = eval_after_move_score_bounded(
                *s.pattern, eval_state, node_is_even, child_discs, child_w,
                skip_thresh, best,
                own, other, static_cast<uint32_t>(mv), f);
        } else {
            const Board child = board.play_flips(player, static_cast<uint32_t>(mv), f);
            eval::PatternState child_eval;
            advance_eval_state(set, child_eval, eval_state, node_is_even, static_cast<uint32_t>(mv), f);
            if (best == -INFINITY_SCORE || beta - alpha == 1) {
                STELLO_TRY_ASSIGN_R(
                    ScoreMove,
                    const ScoreMove res,
                    search_inc(s, child, opp, look - 1, ply + 1, mv, -beta, -alpha, child_eval));
                score = -res.score;
                s.ordering->record_response(opp, mv, res.mv, score <= alpha);
            } else {
                STELLO_TRY_ASSIGN_R(
                    ScoreMove,
                    ScoreMove res,
                    search_inc(s, child, opp, look - 1, ply + 1, mv, -alpha - 1, -alpha, child_eval));
                if (-res.score > alpha && -res.score < beta) {
                    STELLO_TRY_ASSIGN_EXISTING_R(
                        ScoreMove,
                        res,
                        search_inc(s, child, opp, look - 1, ply + 1, mv, -beta, -alpha, child_eval));
                }
                score = -res.score;
                s.ordering->record_response(opp, mv, res.mv, score <= alpha);
            }
        }

        if (score > best) {
            if (score > alpha) {
                alpha = score;
            }
            best = score;
            best_move = mv;
            if (look == 0) {
                skip_thresh = eval::raw_skip_threshold(best, s.pattern->units_per_disc());
            }
            if (best >= beta) {
                break;
            }
        }
    }

    if (hash_put) {
        const uint8_t req_sel = (s.probcut_errors == nullptr) ? SEL_EXACT : SEL_MIDGAME_MPC;
        s.midgame_table->store_bounds_at_root(
            own, other, tag, stored_look, req_sel, alpha_start, beta, best, best_move, s.root_empties,
            search_cost(s.nodes - nodes_start));
    }

    STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{best, best_move}));
}

template <cpu::CpuTier Tier>
AbortResult<ScoreMove> Kernel<Tier>::eval1_nws(
    Searcher& s,
    const Board& board,
    Player player,
    int32_t ply,
    int32_t alpha,
    const eval::PatternState& eval_state,
    uint64_t known_legal
) {
    using namespace detail::midgame;

    STELLO_TRY_VOID_R(ScoreMove, s.count_node());

    const Player opp = opponent(player);
    const uint64_t own = board.discs(player);
    const uint64_t other = board.discs(opp);
    const uint32_t node_discs = 64U - board.empty_count();
    const uint64_t moves = (known_legal != endgame::bits::LEGAL_UNDEFINED)
        ? known_legal
        : legal_moves(own, other);

    if (moves == 0) {
        if (legal_moves(other, own) == 0) {
            s.evaluations += 1;
            STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{game_over_score(own, other), -1}));
        }
        const eval::PatternState pass_eval = eval_state_of_node(s.pattern->set(), other, own, node_discs);
        STELLO_TRY_ASSIGN_R(
            ScoreMove,
            const ScoreMove passed,
            eval1_nws(s, board, opp, ply, -alpha - 1, pass_eval));
        STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{-passed.score, -1}));
    }

    int32_t hash_move = -1;
    const bool hash_get = ply <= s.hash_get_height;
    const int32_t tag = static_cast<int32_t>(player_index(player));
    const uint8_t req_sel = (s.probcut_errors == nullptr) ? SEL_EXACT : SEL_MIDGAME_MPC;
    if (hash_get) {
        if (const auto entry = s.midgame_table->get_at_root(own, other, tag, s.root_empties)) {
            if (entry->mv >= 0 && entry->mv < 64 && (moves & (uint64_t{1} << entry->mv)) != 0) {
                if (entry->selectivity >= req_sel) {
                    const int32_t lo = static_cast<int32_t>(entry->lower);
                    const int32_t hi = static_cast<int32_t>(entry->upper);
                    if ((lo > TT_SCORE_MIN && lo > alpha) || (hi < TT_SCORE_MAX && hi <= alpha) || (lo == hi)) {
                        const int32_t cut_v = (lo > TT_SCORE_MIN && lo > alpha) ? lo : ((hi < TT_SCORE_MAX && hi <= alpha) ? hi : lo);
                        STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{cut_v, static_cast<int32_t>(entry->mv)}));
                    }
                }
                hash_move = static_cast<int32_t>(entry->mv);
            }
        }
    }

    const auto& set = s.pattern->set();
    const bool node_is_even = (node_discs & 1U) == 0U;

    // Preserve the 1-move forced extension at look == 0.
    if ((moves & (moves - 1ULL)) == 0ULL) {
        const uint32_t mv = static_cast<uint32_t>(std::countr_zero(moves));
        const uint64_t f = flips(own, other, mv);
        const Board child = board.play_flips(player, mv, f);
        eval::PatternState child_eval;
        advance_eval_state(set, child_eval, eval_state, node_is_even, mv, f);
        STELLO_TRY_ASSIGN_R(
            ScoreMove,
            const ScoreMove res,
            eval1_nws(s, child, opp, ply + 1, -alpha - 1, child_eval));
        STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{-res.score, static_cast<int32_t>(mv)}));
    }

    const uint32_t child_discs = node_discs + 1U;
    const int32_t* const child_w = s.pattern->packed_weights_for_discs(child_discs);
    int32_t best = -INFINITY_SCORE;
    int32_t best_move = -1;
    int32_t skip_thresh = (std::numeric_limits<int32_t>::max)() / 2;
    uint64_t rem = moves;

    if (hash_move >= 0) {
        const uint32_t hmv = static_cast<uint32_t>(hash_move);
        const uint64_t f = flips(own, other, hmv);
        STELLO_TRY_VOID_R(ScoreMove, s.count_node());
        s.evaluations += 1;
        const int32_t score = eval_after_move_score_bounded(
            *s.pattern, eval_state, node_is_even, child_discs, child_w,
            skip_thresh, best, own, other, hmv, f);
        best = score;
        best_move = hash_move;
        if (best > alpha) {
            STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{best, best_move}));
        }
        skip_thresh = eval::raw_skip_threshold(best, s.pattern->units_per_disc());
        rem &= ~(uint64_t{1} << hmv);
    }

    for (size_t mi = 0; mi < STATIC_CELL_PRIORITY.size() && best <= alpha; ++mi) {
        uint64_t sub = rem & STATIC_CELL_PRIORITY[mi];
        while (sub != 0) {
            const uint32_t mv = static_cast<uint32_t>(std::countr_zero(sub));
            sub &= (sub - 1ULL);
            const uint64_t f = flips(own, other, mv);
            STELLO_TRY_VOID_R(ScoreMove, s.count_node());
            s.evaluations += 1;
            const int32_t score = eval_after_move_score_bounded(
                *s.pattern, eval_state, node_is_even, child_discs, child_w,
                skip_thresh, best, own, other, mv, f);
            if (score > best) {
                best = score;
                best_move = static_cast<int32_t>(mv);
                if (best > alpha) {
                    break;
                }
                skip_thresh = eval::raw_skip_threshold(best, s.pattern->units_per_disc());
            }
        }
    }

    STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{best, best_move}));
}

template <cpu::CpuTier Tier>
AbortResult<ScoreMove> Kernel<Tier>::eval2_nws(
    Searcher& s,
    const Board& board,
    Player player,
    int32_t ply,
    int32_t alpha,
    const eval::PatternState& eval_state,
    uint64_t known_legal
) {
    using namespace detail::midgame;

    const uint64_t nodes_start = s.nodes;
    STELLO_TRY_VOID_R(ScoreMove, s.count_node());

    const Player opp = opponent(player);
    const uint64_t own = board.discs(player);
    const uint64_t other = board.discs(opp);
    const uint32_t node_discs = 64U - board.empty_count();
    const uint64_t moves = (known_legal != endgame::bits::LEGAL_UNDEFINED)
        ? known_legal
        : legal_moves(own, other);

    if (moves == 0) {
        if (legal_moves(other, own) == 0) {
            s.evaluations += 1;
            STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{game_over_score(own, other), -1}));
        }
        const eval::PatternState pass_eval = eval_state_of_node(s.pattern->set(), other, own, node_discs);
        STELLO_TRY_ASSIGN_R(
            ScoreMove,
            const ScoreMove passed,
            eval2_nws(s, board, opp, ply, -alpha - 1, pass_eval));
        STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{-passed.score, -1}));
    }

    int32_t hash_move = -1;
    const bool hash_get = ply <= s.hash_get_height;
    const bool hash_put = ply <= s.hash_put_height;
    const int32_t tag = static_cast<int32_t>(player_index(player));
    const uint8_t req_sel = (s.probcut_errors == nullptr) ? SEL_EXACT : SEL_MIDGAME_MPC;
    if (hash_put && !hash_get) {
        s.midgame_table->prefetch(own, other, tag);
    }
    if (hash_get) {
        if (const auto entry = s.midgame_table->get_at_root(own, other, tag, s.root_empties)) {
            if (entry->mv >= 0 && entry->mv < 64 && (moves & (uint64_t{1} << entry->mv)) != 0) {
                if (static_cast<int32_t>(entry->depth) >= 1 && entry->selectivity >= req_sel) {
                    const int32_t lo = static_cast<int32_t>(entry->lower);
                    const int32_t hi = static_cast<int32_t>(entry->upper);
                    if ((lo > TT_SCORE_MIN && lo > alpha) || (hi < TT_SCORE_MAX && hi <= alpha) || (lo == hi)) {
                        const int32_t cut_v = (lo > TT_SCORE_MIN && lo > alpha) ? lo : ((hi < TT_SCORE_MAX && hi <= alpha) ? hi : lo);
                        STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{cut_v, static_cast<int32_t>(entry->mv)}));
                    }
                }
                hash_move = static_cast<int32_t>(entry->mv);
            }
        }
    }

    const auto& set = s.pattern->set();
    const bool node_is_even = (node_discs & 1U) == 0U;
    int32_t best = -INFINITY_SCORE;
    int32_t best_move = -1;
    uint64_t rem = moves;

    if (hash_move >= 0) {
        const uint32_t hmv = static_cast<uint32_t>(hash_move);
        const uint64_t f = flips(own, other, hmv);
        const Board child = board.play_flips(player, hmv, f);
        eval::PatternState child_eval;
        advance_eval_state(set, child_eval, eval_state, node_is_even, hmv, f);
        STELLO_TRY_ASSIGN_R(
            ScoreMove,
            const ScoreMove res,
            eval1_nws(s, child, opp, ply + 1, -alpha - 1, child_eval));
        best = -res.score;
        best_move = hash_move;
        if (best > alpha) {
            if (hash_put) {
                s.midgame_table->store_bounds_at_root(
                    own, other, tag, 1, req_sel, alpha, alpha + 1, best, best_move, s.root_empties,
                    search_cost(s.nodes - nodes_start));
            }
            STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{best, best_move}));
        }
        rem &= ~(uint64_t{1} << hmv);
    }

    for (size_t mi = 0; mi < STATIC_CELL_PRIORITY.size() && best <= alpha; ++mi) {
        uint64_t sub = rem & STATIC_CELL_PRIORITY[mi];
        while (sub != 0) {
            const uint32_t mv = static_cast<uint32_t>(std::countr_zero(sub));
            sub &= (sub - 1ULL);
            const uint64_t f = flips(own, other, mv);
            const Board child = board.play_flips(player, mv, f);
            eval::PatternState child_eval;
            advance_eval_state(set, child_eval, eval_state, node_is_even, mv, f);
            STELLO_TRY_ASSIGN_R(
                ScoreMove,
                const ScoreMove res,
                eval1_nws(s, child, opp, ply + 1, -alpha - 1, child_eval));
            const int32_t score = -res.score;
            if (score > best) {
                best = score;
                best_move = static_cast<int32_t>(mv);
                if (best > alpha) {
                    break;
                }
            }
        }
    }

    if (hash_put) {
        s.midgame_table->store_bounds_at_root(
            own, other, tag, 1, req_sel, alpha, alpha + 1, best, best_move, s.root_empties,
            search_cost(s.nodes - nodes_start));
    }
    STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{best, best_move}));
}

template <cpu::CpuTier Tier>
AbortResult<ScoreMove> Kernel<Tier>::search_inc_deep(
    Searcher& s,
    const Board& board,
    Player player,
    int32_t look,
    int32_t ply,
    int32_t alpha,
    int32_t beta,
    const eval::PatternState& eval_state,
    uint64_t moves,
    int32_t hash_move,
    bool hash_put
) {
    using namespace detail::midgame;

    const uint64_t nodes_start = s.nodes;
    const Player opp = opponent(player);
    const uint64_t own = board.discs(player);
    const uint64_t other = board.discs(opp);
    const uint32_t node_discs = 64U - board.empty_count();
    const int32_t tag = static_cast<int32_t>(player_index(player));
    const uint8_t req_sel = (s.probcut_errors == nullptr) ? SEL_EXACT : SEL_MIDGAME_MPC;
    const size_t move_count = static_cast<size_t>(std::popcount(moves));

    std::array<int32_t, MAX_MOVES> buffer;
    fill_moves(buffer, moves);
    std::span<int32_t> list(buffer.data(), move_count);

    std::array<uint64_t, MAX_MOVES> cached_flips;
    const bool did_etc = (ply < s.hash_get_height);
    if (did_etc) {
        const int32_t opp_tag = static_cast<int32_t>(player_index(opp));
        for (size_t idx = 0; idx < move_count; ++idx) {
            const int32_t mv = list[idx];
            const uint64_t f = flips(own, other, static_cast<uint32_t>(mv));
            cached_flips[idx] = f;
            const uint64_t child_own = other & ~f;
            const uint64_t child_other = own | f | (uint64_t{1} << mv);
            if (const auto entry = s.midgame_table->get_at_root(child_own, child_other, opp_tag, s.root_empties)) {
                if (static_cast<int32_t>(entry->depth) >= look - 1
                    && entry->selectivity >= req_sel
                    && entry->upper < TT_SCORE_MAX
                    && -static_cast<int32_t>(entry->upper) >= beta)
                {
                    STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{-static_cast<int32_t>(entry->upper), mv}));
                }
            }
        }
    }

    const auto& set = s.pattern->set();
    const bool node_is_even = (node_discs & 1U) == 0U;
    const uint32_t child_discs = node_discs + 1U;
    const int32_t* const child_w = s.pattern->packed_weights_for_discs(child_discs);

    int32_t best = -INFINITY_SCORE;
    int32_t best_move = -1;
    const int32_t alpha_start = alpha;
    bool searched_first = false;
    size_t loop_count = 0;

    std::array<int32_t, MAX_MOVES> sorted_moves;
    std::array<uint64_t, MAX_MOVES> sorted_flips;
    std::array<uint64_t, MAX_MOVES> sorted_replies;

    const auto eval_and_sort = [&](size_t eval_count) -> AbortVoid {
#ifndef __wasm_simd128__
        constexpr bool allow_sibling_deep = (Tier == cpu::CpuTier::Portable);
        [[maybe_unused]] detail::midgame::SiblingBaselineSlot<allow_sibling_deep> sibling_slot;
        [[maybe_unused]] bool use_sibling_delta = false;
        if constexpr (allow_sibling_deep) {
            use_sibling_delta = (eval_count >= size_t{3});
            if (use_sibling_delta) {
                set.init_sibling_baseline(eval_state, child_w, node_is_even, sibling_slot.data);
            }
        }
#endif

        std::array<int32_t, MAX_MOVES> keys;
        std::array<uint64_t, MAX_MOVES> cached_replies;
        for (size_t idx = 0; idx < eval_count; ++idx) {
            const int32_t mv = list[idx];
            const uint64_t f = did_etc
                ? cached_flips[idx]
                : (cached_flips[idx] = flips(own, other, static_cast<uint32_t>(mv)));
            const uint64_t child_own = other & ~f;
            const uint64_t child_other = own | f | (uint64_t{1} << mv);
            uint64_t replies_bb = 0;
            uint64_t opp_moves_bb = 0;
            int32_t potential = 0;
            if (s.pattern->has_mobility()) {
                legal_and_potential(child_own, child_other, replies_bb, potential);
                opp_moves_bb = legal_moves(child_other, child_own);
            } else {
                legal_and_potential(child_own, child_other, replies_bb, potential);
            }
            cached_replies[idx] = replies_bb;
            const int32_t replies = endgame::bits::weighted_moves(replies_bb);
            STELLO_TRY_VOID_IN_STATUS(s.count_node());
            s.evaluations += 1;
            int32_t raw;
#ifndef __wasm_simd128__
            if constexpr (allow_sibling_deep) {
                raw = use_sibling_delta
                    ? set.sum_weights_sibling_delta(
                        sibling_slot.data, eval_state, node_is_even, static_cast<uint32_t>(mv), f, child_w, node_is_even)
                    : sum_eval_after_move(
                        set, eval_state, node_is_even, static_cast<uint32_t>(mv), f, child_w, node_is_even, s.pattern->packed_vpaddw_safe());
            } else
#endif
            {
                raw = sum_eval_after_move(
                    set, eval_state, node_is_even, static_cast<uint32_t>(mv), f, child_w, node_is_even, s.pattern->packed_vpaddw_safe());
            }
            if (s.pattern->has_mobility()) {
                const uint32_t m_own = static_cast<uint32_t>(std::popcount(replies_bb));
                const uint32_t m_opp = static_cast<uint32_t>(std::popcount(opp_moves_bb));
                raw += s.pattern->mobility_bonus_for_discs(child_discs, m_own, m_opp);
            }
            const int32_t value = eval::clamp_pattern_score(*s.pattern, raw);
            keys[idx] = (ORDER_OFFSET - replies) * ORDER_MOBILITY
                      + (ORDER_OFFSET - potential) * ORDER_POTENTIAL_MOBILITY
                      - value * ORDER_VALUE / eval::PATTERN_UNITS_PER_DISC;
        }

        for (size_t i = 0; i < eval_count; ++i) {
            const int32_t key = keys[i];
            size_t before = 0;
            for (size_t j = 0; j < i; ++j) {
                before += static_cast<size_t>(keys[j] >= key);
            }
            size_t after = 0;
            for (size_t j = i + 1; j < eval_count; ++j) {
                after += static_cast<size_t>(keys[j] > key);
            }
            const size_t pos = before + after;
            sorted_moves[pos] = list[i];
            sorted_flips[pos] = cached_flips[i];
            sorted_replies[pos] = cached_replies[i];
        }
        STELLO_RETURN_OK_VOID();
    };

    if (move_count == 1) {
        const int32_t mv = list[0];
        sorted_moves[0] = mv;
        sorted_flips[0] = did_etc ? cached_flips[0] : flips(own, other, static_cast<uint32_t>(mv));
        sorted_replies[0] = endgame::bits::LEGAL_UNDEFINED;
        loop_count = 1;
    } else if (hash_move >= 0) {
        size_t hash_idx = 0;
        while (hash_idx < move_count && list[hash_idx] != hash_move) {
            ++hash_idx;
        }
        assert(hash_idx < move_count);
        const uint64_t hf = did_etc ? cached_flips[hash_idx] : flips(own, other, static_cast<uint32_t>(hash_move));
        const Board child = board.play_flips(player, static_cast<uint32_t>(hash_move), hf);
        eval::PatternState child_eval;
        advance_eval_state(set, child_eval, eval_state, node_is_even, static_cast<uint32_t>(hash_move), hf);
        STELLO_TRY_ASSIGN_R(
            ScoreMove,
            const ScoreMove res,
            search_inc(
                s, child, opp, look - 1, ply + 1, hash_move, -beta, -alpha, child_eval,
                endgame::bits::LEGAL_UNDEFINED));
        const int32_t score = -res.score;
        s.ordering->record_response(opp, hash_move, res.mv, score <= alpha);
        if (score > alpha) {
            alpha = score;
        }
        best = score;
        best_move = hash_move;
        if (best >= beta) {
            if (hash_put) {
                s.midgame_table->store_bounds_at_root(
                    own, other, tag, look, req_sel, alpha_start, beta, best, best_move, s.root_empties,
                    search_cost(s.nodes - nodes_start));
            }
            STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{best, best_move}));
        }
        searched_first = true;
        if (move_count == 2) {
            const size_t other_idx = 1U - hash_idx;
            const int32_t omv = list[other_idx];
            sorted_moves[0] = omv;
            sorted_flips[0] = did_etc ? cached_flips[other_idx] : flips(own, other, static_cast<uint32_t>(omv));
            sorted_replies[0] = endgame::bits::LEGAL_UNDEFINED;
            loop_count = 1;
        } else {
            size_t dst = 0;
            for (size_t idx = 0; idx < move_count; ++idx) {
                if (idx != hash_idx) {
                    list[dst] = list[idx];
                    if (did_etc) {
                        cached_flips[dst] = cached_flips[idx];
                    }
                    ++dst;
                }
            }
            STELLO_TRY_VOID_R(ScoreMove, eval_and_sort(dst));
            loop_count = dst;
        }
    } else {
        STELLO_TRY_VOID_R(ScoreMove, eval_and_sort(move_count));
        loop_count = move_count;
    }

    for (size_t index = 0; index < loop_count; ++index) {
        if ((searched_first || index >= 1) && look >= YBWC_MID_MIN_DEPTH && s.may_split(loop_count - index)) {
            STELLO_TRY_VOID_R(
                ScoreMove,
                split_midgame(
                    s, board, player, look, ply,
                    std::span<const int32_t>(sorted_moves.data() + index, loop_count - index),
                    alpha, beta, best, best_move, eval_state, node_is_even,
                    std::span<const uint64_t>(sorted_flips.data() + index, loop_count - index),
                    std::span<const uint64_t>(sorted_replies.data() + index, loop_count - index)));
            break;
        }

        const int32_t mv = sorted_moves[index];
        const Board child = board.play_flips(player, static_cast<uint32_t>(mv), sorted_flips[index]);
        eval::PatternState child_eval;
        advance_eval_state(set, child_eval, eval_state, node_is_even, static_cast<uint32_t>(mv), sorted_flips[index]);

        int32_t score = 0;
        if (best == -INFINITY_SCORE || beta - alpha == 1) {
            STELLO_TRY_ASSIGN_R(
                ScoreMove,
                const ScoreMove res,
                search_inc(s, child, opp, look - 1, ply + 1, mv, -beta, -alpha, child_eval, sorted_replies[index]));
            score = -res.score;
            s.ordering->record_response(opp, mv, res.mv, score <= alpha);
        } else {
            STELLO_TRY_ASSIGN_R(
                ScoreMove,
                ScoreMove res,
                search_inc(s, child, opp, look - 1, ply + 1, mv, -alpha - 1, -alpha, child_eval, sorted_replies[index]));
            if (-res.score > alpha && -res.score < beta) {
                STELLO_TRY_ASSIGN_EXISTING_R(
                    ScoreMove,
                    res,
                    search_inc(s, child, opp, look - 1, ply + 1, mv, -beta, -alpha, child_eval, sorted_replies[index]));
            }
            score = -res.score;
            s.ordering->record_response(opp, mv, res.mv, score <= alpha);
        }

        if (score > best) {
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
        s.midgame_table->store_bounds_at_root(
            own, other, tag, look, req_sel, alpha_start, beta, best, best_move, s.root_empties,
            search_cost(s.nodes - nodes_start));
    }

    STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{best, best_move}));
}

template <cpu::CpuTier Tier>
AbortVoid Kernel<Tier>::order_by_evaluation(
    Searcher& s,
    std::span<int32_t> list,
    int32_t hash_move,
    const Board& board,
    Player player
) {
    const eval::PatternState eval_state = eval_state_of_node(
        s.pattern->set(), board.discs(player), board.discs(opponent(player)), 64U - board.empty_count());
    return order_by_evaluation(s, list, hash_move, board, player, eval_state);
}

template <cpu::CpuTier Tier>
AbortVoid Kernel<Tier>::order_by_evaluation(
    Searcher& s,
    std::span<int32_t> list,
    int32_t hash_move,
    const Board& board,
    Player player,
    const eval::PatternState& eval_state
) {
    const Player opp = opponent(player);
    const uint64_t own = board.discs(player);
    const uint64_t other = board.discs(opp);
    const auto& set = s.pattern->set();
    const uint32_t node_discs = 64U - board.empty_count();
    const bool node_is_even = (node_discs & 1U) == 0U;
    const uint32_t child_discs = node_discs + 1U;
    const int32_t* const child_w = s.pattern->packed_weights_for_discs(child_discs);
    std::array<int32_t, MAX_MOVES> keys;

    for (size_t idx = 0; idx < list.size(); ++idx) {
        const int32_t mv = list[idx];
        if (mv == hash_move) {
            keys[idx] = (std::numeric_limits<int32_t>::max)();
            continue;
        }

        const uint64_t f = flips(own, other, static_cast<uint32_t>(mv));
        const uint64_t child_own = other & ~f;
        const uint64_t child_other = own | f | (uint64_t{1} << mv);
        uint64_t replies_bb = 0;
        uint64_t opp_moves_bb = 0;
        int32_t potential = 0;
        legal_and_potential(child_own, child_other, replies_bb, potential);
        if (s.pattern->has_mobility()) {
            opp_moves_bb = legal_moves(child_other, child_own);
        }
        const int32_t replies = endgame::bits::weighted_moves(replies_bb);
        STELLO_TRY_VOID_IN_STATUS(s.count_node());
        s.evaluations += 1;
        int32_t raw = sum_eval_after_move(set, eval_state, node_is_even, static_cast<uint32_t>(mv), f, child_w, node_is_even, s.pattern->packed_vpaddw_safe());
        if (s.pattern->has_mobility()) {
            const uint32_t m_own = static_cast<uint32_t>(std::popcount(replies_bb));
            const uint32_t m_opp = static_cast<uint32_t>(std::popcount(opp_moves_bb));
            raw += s.pattern->mobility_bonus_for_discs(child_discs, m_own, m_opp);
        }
        const int32_t value = eval::clamp_pattern_score(*s.pattern, raw);
        keys[idx] = (ORDER_OFFSET - replies) * ORDER_MOBILITY
                  + (ORDER_OFFSET - potential) * ORDER_POTENTIAL_MOBILITY
                  - value * ORDER_VALUE / eval::PATTERN_UNITS_PER_DISC;
    }

    stello::search::sort_descending(list, std::span<int32_t>(keys.data(), list.size()));
    STELLO_RETURN_OK_VOID();
}

template <cpu::CpuTier Tier>
AbortVoid Kernel<Tier>::split_midgame(
    Searcher& s,
    const Board& board,
    Player player,
    int32_t look,
    int32_t ply,
    std::span<const int32_t> tail,
    int32_t alpha,
    int32_t beta,
    int32_t& best,
    int32_t& best_move,
    const eval::PatternState& eval_state,
    bool node_is_even,
    std::span<const uint64_t> tail_flips,
    std::span<const uint64_t> tail_replies
) {
    const Player opp = opponent(player);
    const uint64_t own = board.discs(player);
    const uint64_t opp_discs = board.discs(opp);
    std::array<MidgameChild, MAX_MOVES> children;
    for (size_t c = 0; c < tail.size(); ++c) {
        const int32_t child_mv = tail[c];
        const uint64_t cf = (!tail_flips.empty() && tail_flips[c] != 0)
            ? tail_flips[c]
            : flips(own, opp_discs, static_cast<uint32_t>(child_mv));
        const uint64_t creplies = !tail_replies.empty()
            ? tail_replies[c]
            : endgame::bits::LEGAL_UNDEFINED;
        children[c] = MidgameChild{
            board.play_flips(player, static_cast<uint32_t>(child_mv), cf),
            opp,
            look - 1,
            ply + 1,
            child_mv,
            &eval_state,
            cf,
            creplies,
            node_is_even,
        };
    }
    return split(s, std::span<const MidgameChild>(children.data(), tail.size()), alpha, beta, best, best_move);
}

template <cpu::CpuTier Tier>
AbortResult<std::optional<int32_t>> Kernel<Tier>::try_probcut(
    Searcher& s,
    const Board& board,
    Player player,
    int32_t look,
    int32_t ply,
    int32_t previous_move,
    int32_t alpha,
    int32_t beta
) {
    const eval::PatternState eval_state = eval_state_of_node(
        s.pattern->set(), board.discs(player), board.discs(opponent(player)), 64U - board.empty_count());
    return try_probcut_inc(s, board, player, look, ply, previous_move, alpha, beta, eval_state);
}

template <cpu::CpuTier Tier>
AbortResult<std::optional<int32_t>> Kernel<Tier>::try_probcut_inc(
    Searcher& s,
    const Board& board,
    Player player,
    int32_t look,
    int32_t ply,
    int32_t previous_move,
    int32_t alpha,
    int32_t beta,
    const eval::PatternState& eval_state
) {
    const int32_t depth = look + 1;
    const int32_t shallow_look = detail::probcut_shallow_depth(depth) - 1;
    assert(s.probcut_errors != nullptr && "Multi-ProbCut is on");
    const int32_t error = (*s.probcut_errors)[64U - board.empty_count()][static_cast<size_t>(depth)];
    const int32_t error0 = (std::max)(eval::PATTERN_UNITS_PER_DISC, error - detail::midgame::PROBCUT_PRE_CHECK_OFFSET);
    STELLO_TRY_VOID_R(std::optional<int32_t>, s.count_node());
    s.evaluations += 1;
    const uint32_t discs = 64U - board.empty_count();
    const int32_t* const w = s.pattern->packed_weights_for_discs(discs);
    const int32_t static0 = eval_state_score(
        *s.pattern, eval_state, board.discs(player), board.discs(opponent(player)), discs, w);

    ProbCutGuard guard(s.in_probcut);
    STELLO_TRY_ASSIGN_R(
        std::optional<int32_t>,
        const auto cut,
        probcut_searches_inc(s, board, player, shallow_look, ply, previous_move, alpha, beta, error, error0, static0, eval_state));
    STELLO_RETURN_OK_R(std::optional<int32_t>, cut);
}

template <cpu::CpuTier Tier>
AbortResult<std::optional<int32_t>> Kernel<Tier>::probcut_searches(
    Searcher& s,
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
    const eval::PatternState eval_state = eval_state_of_node(
        s.pattern->set(), board.discs(player), board.discs(opponent(player)), 64U - board.empty_count());
    return probcut_searches_inc(s, board, player, shallow_look, ply, previous_move, alpha, beta, error, error0, static0, eval_state);
}

template <cpu::CpuTier Tier>
AbortResult<std::optional<int32_t>> Kernel<Tier>::probcut_searches_inc(
    Searcher& s,
    const Board& board,
    Player player,
    int32_t shallow_look,
    int32_t ply,
    int32_t previous_move,
    int32_t alpha,
    int32_t beta,
    int32_t error,
    int32_t error0,
    int32_t static0,
    const eval::PatternState& eval_state
) {
    using detail::midgame::PROBCUT_SCORE_LIMIT;

    if (static0 >= beta + error0 && beta + error < PROBCUT_SCORE_LIMIT) {
        const int32_t pc_beta = beta + error;
        STELLO_TRY_ASSIGN_R(
            std::optional<int32_t>,
            const ScoreMove high,
            search_inc(s, board, player, shallow_look, ply, previous_move, pc_beta - 1, pc_beta, eval_state));
        if (high.score >= pc_beta) {
            STELLO_RETURN_OK_R(std::optional<int32_t>, std::optional<int32_t>(beta));
        }
    }

    if (static0 <= alpha - error0 && alpha - error > -PROBCUT_SCORE_LIMIT) {
        const int32_t pc_alpha = alpha - error;
        STELLO_TRY_ASSIGN_R(
            std::optional<int32_t>,
            const ScoreMove low,
            search_inc(s, board, player, shallow_look, ply, previous_move, pc_alpha, pc_alpha + 1, eval_state));
        if (low.score <= pc_alpha) {
            STELLO_RETURN_OK_R(std::optional<int32_t>, std::optional<int32_t>(alpha));
        }
    }

    STELLO_RETURN_OK_R(std::optional<int32_t>, std::nullopt);
}

template <cpu::CpuTier Tier>
AbortResult<ScoreMove> Kernel<Tier>::solve_root(
    Searcher& s,
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
    const int32_t report_scale = (kind == ScoreKind::Heuristic) ? eval::PATTERN_UNITS_PER_DISC : 1;
    int32_t best_score = -INFINITY_SCORE;
    int32_t best_move = -1;
    std::array<int32_t, MAX_MOVES> improvements;
    size_t improved = 0;

    for (size_t i = 0; i < root_moves.size(); ++i) {
        if (i >= 1 && empties > YBWC_END_MIN_EMPTIES && s.may_split(root_moves.size() - i)) {
            const std::span<const int32_t> tail = root_moves.subspan(i);
            std::array<EndgameChild, MAX_MOVES> children;
            for (size_t c = 0; c < tail.size(); ++c) {
                const int32_t cmv = tail[c];
                const uint64_t cf = flips(own, opp, static_cast<uint32_t>(cmv));
                children[c] = EndgameChild{
                    opp & ~cf,
                    own | cf | (uint64_t{1} << cmv),
                    empties - 1,
                    endgame::bits::LEGAL_UNDEFINED,
                    cmv,
                };
            }

            const bool has_solved_move = (best_move >= 0 || best.kind == ScoreKind::WinLossDraw || best.kind == ScoreKind::Exact);
            s.report_depth = has_solved_move ? empties : best.depth;
            s.report_move = tail[0];
            s.report_best_move = (best_move >= 0) ? best_move : best.mv;
            s.report_score = (best_move >= 0) ? (best_score * report_scale) : best.score;
            s.report_kind = (best_move >= 0) ? kind : best.kind;

            const int32_t score_before_split = best_score;
            struct CommitSolveBest {
                Best& best;
                ScoreKind kind;
                int32_t empties;
                int32_t& best_move;
                int32_t& best_score;
                int32_t threshold;
                ~CommitSolveBest() noexcept {
                    if (best_score > threshold && best_move >= 0) {
                        detail::midgame::apply_solve_best(kind, empties, best_move, best_score, best);
                    }
                }
            } commit{best, kind, empties, best_move, best_score, score_before_split};

            STELLO_TRY_VOID_R(
                ScoreMove,
                s.split(std::span<const EndgameChild>(children.data(), tail.size()), alpha, beta, best_score, best_move, true));
            if (best_score > score_before_split && best_move >= 0) {
                improvements[improved] = best_move;
                ++improved;
            }
            s.report(empties, tail.back(), best_move, best_score * report_scale, kind);
            break;
        }

        const int32_t mv = root_moves[i];
        const bool has_solved_move = (best_move >= 0 || best.kind == ScoreKind::WinLossDraw || best.kind == ScoreKind::Exact);
        s.report_depth = has_solved_move ? empties : best.depth;
        s.report_move = mv;
        s.report_best_move = (best_move >= 0) ? best_move : best.mv;
        s.report_score = (best_move >= 0) ? (best_score * report_scale) : best.score;
        s.report_kind = (best_move >= 0) ? kind : best.kind;

        const uint64_t f = flips(own, opp, static_cast<uint32_t>(mv));
        const uint64_t child_own = own | f | (uint64_t{1} << mv);
        const uint64_t child_opp = opp & ~f;

        int32_t score = 0;
        if (i == 0) {
            STELLO_TRY_ASSIGN_R(ScoreMove, const int32_t sc, solve_child(s, child_opp, child_own, -beta, -alpha, empties - 1));
            score = -sc;
        } else {
            STELLO_TRY_ASSIGN_R(ScoreMove, const int32_t s_null, solve_child(s, child_opp, child_own, -alpha - 1, -alpha, empties - 1));
            const int32_t value = -s_null;
            if (value > alpha && value < beta) {
                STELLO_TRY_ASSIGN_R(ScoreMove, const int32_t s_full, solve_child(s, child_opp, child_own, -beta, -alpha, empties - 1));
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
            detail::midgame::apply_solve_best(kind, empties, mv, score, best);
        }

        s.report(empties, mv, best_move, best_score * report_scale, kind);
        if (best_score >= beta) {
            break;
        }
    }

    detail::move_to_front(root_moves, std::span<const int32_t>(improvements.data(), improved));
    STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{best_score, best_move}));
}

template <cpu::CpuTier Tier>
AbortResult<int32_t> Kernel<Tier>::evaluate_leaf(Searcher& s, const Board& board, Player player) {
    STELLO_TRY_VOID_I32(s.count_node());
    s.evaluations += 1;
    STELLO_RETURN_OK_R(int32_t, eval::pattern_score(*s.pattern, board.discs(player), board.discs(opponent(player))));
}

} // namespace stello::search
