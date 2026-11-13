#pragma once

#include "eval/eval.hpp"
#include "search/kernel.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
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

inline size_t lazy_smp_helpers(size_t workers) noexcept {
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

} // namespace detail::midgame

template <bool UseAvx2>
SearchOutcome Kernel<UseAvx2>::run(
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

template <bool UseAvx2>
AbortVoid Kernel<UseAvx2>::iterate(
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

    const int32_t alpha = (wld.score > 0) ? (wld.score - 1) : -65;
    const int32_t beta = (wld.score > 0) ? 65 : (wld.score + 1);
    STELLO_TRY_ASSIGN_IN_STATUS(
        const ScoreMove exact,
        solve_iteration(s, board, player, alpha, beta, ScoreKind::Exact, root_moves, best));
    best = Best{exact.mv, exact.score, ScoreKind::Exact, empties};
    STELLO_RETURN_OK_VOID();
}

template <bool UseAvx2>
AbortResult<ScoreMove> Kernel<UseAvx2>::solve_iteration(
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

    if (!s.helpers.has_value() || s.ybwc) {
        return solve_root(s, board, player, alpha, beta, kind, root_moves, best);
    }

    const Helpers active_helpers = *s.helpers;
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
                        (void)Kernel<UseAvx2>::solve_root(
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
                    (void)Kernel<UseAvx2>::solve_root(
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

        StopHelpers stop_helpers(stop_flag);
        return solve_root(s, board_copy, player, alpha, beta, kind, root_moves, best);
    });
}

template <bool UseAvx2>
AbortResult<bool> Kernel<UseAvx2>::lazy_smp_midgame(
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
        const size_t lazy = s.ybwc ? lazy_smp_helpers(smp_helpers.count) : smp_helpers.count;
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
                            Kernel<UseAvx2>::search_root(
                                helper,
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
                        const AbortStatus st = Kernel<UseAvx2>::search_root(
                            helper,
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
        return midgame_iterations(s, board_copy, player, limits, budget, root_moves, best, &main_look);
    });
}

template <bool UseAvx2>
AbortResult<bool> Kernel<UseAvx2>::midgame_iterations(
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
        s.hash_get_height = look - 1;
        s.hash_put_height = look;
        STELLO_TRY_VOID_R(bool, search_root(s, board, player, look, root_moves, best));
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

template <bool UseAvx2>
AbortVoid Kernel<UseAvx2>::search_root(
    Searcher& s,
    const Board& board,
    Player player,
    int32_t look,
    std::span<int32_t> root_moves,
    Best& best
) {
    const Player opp = opponent(player);
    int32_t alpha = -detail::midgame::ROOT_WINDOW;
    const int32_t beta = detail::midgame::ROOT_WINDOW;
    int32_t best_score = -INFINITY_SCORE;
    int32_t best_move = -1;
    std::array<int32_t, MAX_MOVES> improvements{};
    size_t improved = 0;

    for (size_t i = 0; i < root_moves.size(); ++i) {
        const int32_t mv = root_moves[i];
        s.report_depth = look + 1;
        s.report_move = mv;
        s.report_best_move = (best_move >= 0) ? best_move : best.mv;
        s.report_score = (best_move >= 0) ? best_score : best.score;
        s.report_kind = ScoreKind::Heuristic;

        const Board child = play_unchecked(board, player, static_cast<uint32_t>(mv));
        int32_t score = 0;

        if (look == 0) {
            STELLO_TRY_ASSIGN_IN_STATUS(const int32_t leaf, evaluate_leaf(s, child, opp));
            score = -leaf;
        } else {
            const int32_t child_look = look - 1;
            int32_t reply = -1;

            if (i == 0 || look <= 2) {
                STELLO_TRY_ASSIGN_IN_STATUS(const ScoreMove res, search(s, child, opp, child_look, 1, mv, -beta, -alpha));
                score = -res.score;
                reply = res.mv;
            } else {
                STELLO_TRY_ASSIGN_IN_STATUS(const ScoreMove res, search(s, child, opp, child_look, 1, mv, -alpha - 1, -alpha));
                if (-res.score > alpha && -res.score < beta) {
                    STELLO_TRY_ASSIGN_IN_STATUS(const ScoreMove full, search(s, child, opp, child_look, 1, mv, -beta, -alpha));
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
            if (score > alpha) {
                alpha = score;
            }
            improvements[improved] = mv;
            ++improved;
            best = Best{mv, score, ScoreKind::Heuristic, look + 1};
        }

        s.report(look + 1, mv, best_move, best_score, ScoreKind::Heuristic);
    }

    detail::move_to_front(root_moves, std::span<const int32_t>(improvements.data(), improved));
    STELLO_RETURN_OK_VOID();
}

template <bool UseAvx2>
AbortResult<ScoreMove> Kernel<UseAvx2>::search(
    Searcher& s,
    const Board& board,
    Player player,
    int32_t look,
    int32_t ply,
    int32_t previous_move,
    int32_t alpha,
    int32_t beta
) {
    using namespace detail::midgame;

    STELLO_TRY_VOID_R(ScoreMove, s.count_node());

    const Player opp = opponent(player);
    const uint64_t own = board.discs(player);
    const uint64_t other = board.discs(opp);
    const uint64_t moves = legal_moves(own, other);

    if (moves == 0) {
        if (legal_moves(other, own) == 0) {
            s.evaluations += 1;
            STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{game_over_score(own, other), -1}));
        }

        STELLO_TRY_ASSIGN_R(ScoreMove, const ScoreMove passed, search(s, board, opp, look, ply, -1, -beta, -alpha));
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
        if (const auto entry = s.midgame_table->get_at_root(own, other, tag, s.root_empties)) {
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

    if (s.probcut_errors != nullptr && look >= PROBCUT_MIN_LOOK && !s.in_probcut) {
        STELLO_TRY_ASSIGN_R(ScoreMove, const auto cut, try_probcut(s, board, player, look, ply, previous_move, alpha, beta));
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

    if (look >= MIDGAME_ETC_MIN_LOOK && ply < s.hash_get_height) {
        for (const int32_t mv : list) {
            const Board child = play_unchecked(board, player, static_cast<uint32_t>(mv));
            if (const auto entry = s.midgame_table->get_at_root(
                    child.discs(opp), child.discs(player), static_cast<int32_t>(player_index(opp)), s.root_empties))
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
        STELLO_TRY_VOID_R(ScoreMove, order_by_evaluation(s, list, hash_move, board, player));
    } else {
        s.ordering->order(list, hash_move, previous_move, player, board);
    }

    int32_t best = -INFINITY_SCORE;
    int32_t best_move = -1;
    Bound bound = Bound::Lower;
    const int32_t alpha_start = alpha;

    for (size_t index = 0; index < list.size(); ++index) {
        if (index == 1 && look >= YBWC_MID_MIN_DEPTH && s.may_split()) {
            STELLO_TRY_VOID_R(
                ScoreMove,
                split_midgame(s, board, player, look, ply, list.subspan(1), alpha, beta, best, best_move));
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
        const Board child = play_unchecked(board, player, static_cast<uint32_t>(mv));
        int32_t score = 0;

        if (look == 0) {
            STELLO_TRY_ASSIGN_R(ScoreMove, const int32_t leaf, evaluate_leaf(s, child, opp));
            score = -leaf;
        } else if (best == -INFINITY_SCORE || beta - alpha == 1) {
            STELLO_TRY_ASSIGN_R(ScoreMove, const ScoreMove res, search(s, child, opp, look - 1, ply + 1, mv, -beta, -alpha));
            score = -res.score;
            s.ordering->record_response(opp, mv, res.mv, score <= alpha);
        } else {
            STELLO_TRY_ASSIGN_R(ScoreMove, ScoreMove res, search(s, child, opp, look - 1, ply + 1, mv, -alpha - 1, -alpha));
            if (-res.score > alpha && -res.score < beta) {
                STELLO_TRY_ASSIGN_EXISTING_R(ScoreMove, res, search(s, child, opp, look - 1, ply + 1, mv, -beta, -alpha));
            }
            score = -res.score;
            s.ordering->record_response(opp, mv, res.mv, score <= alpha);
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
        s.midgame_table->store_at_root(own, other, tag, stored_look, bound, best, best_move, s.root_empties);
    }

    STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{best, best_move}));
}

template <bool UseAvx2>
AbortVoid Kernel<UseAvx2>::order_by_evaluation(
    Searcher& s,
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

        const Board child = play_unchecked(board, player, static_cast<uint32_t>(mv));
        const uint64_t child_own = child.discs(opp);
        const uint64_t child_other = child.discs(player);
        const int32_t replies = endgame::bits::weighted_moves(legal_moves(child_own, child_other));
        const int32_t potential = endgame::bits::potential_mobility(child_other, empty & ~(uint64_t{1} << mv));
        STELLO_TRY_ASSIGN_IN_STATUS(const int32_t value, evaluate_leaf(s, child, opp));
        keys[idx] = (ORDER_OFFSET - replies) * ORDER_MOBILITY
                  + (ORDER_OFFSET - potential) * ORDER_POTENTIAL_MOBILITY
                  - value * ORDER_VALUE / eval::PATTERN_UNITS_PER_DISC;
    }

    stello::search::sort_descending(list, std::span<int32_t>(keys.data(), list.size()));
    STELLO_RETURN_OK_VOID();
}

template <bool UseAvx2>
AbortVoid Kernel<UseAvx2>::split_midgame(
    Searcher& s,
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
            play_unchecked(board, player, static_cast<uint32_t>(child_mv)),
            opp,
            look - 1,
            ply + 1,
            child_mv,
        };
    }
    return split(s, std::span<const MidgameChild>(children.data(), tail.size()), alpha, beta, best, best_move);
}

template <bool UseAvx2>
AbortResult<std::optional<int32_t>> Kernel<UseAvx2>::try_probcut(
    Searcher& s,
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
    assert(s.probcut_errors != nullptr && "Multi-ProbCut is on");
    const int32_t error = (*s.probcut_errors)[64U - board.empty_count()][static_cast<size_t>(depth)];
    const int32_t error0 = (std::max)(eval::PATTERN_UNITS_PER_DISC, error - detail::midgame::PROBCUT_PRE_CHECK_OFFSET);
    STELLO_TRY_ASSIGN_R(std::optional<int32_t>, const int32_t static0, evaluate_leaf(s, board, player));

    ProbCutGuard guard(s.in_probcut);
    STELLO_TRY_ASSIGN_R(
        std::optional<int32_t>,
        const auto cut,
        probcut_searches(s, board, player, shallow_look, ply, previous_move, alpha, beta, error, error0, static0));
    STELLO_RETURN_OK_R(std::optional<int32_t>, cut);
}

template <bool UseAvx2>
AbortResult<std::optional<int32_t>> Kernel<UseAvx2>::probcut_searches(
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
    using detail::midgame::PROBCUT_SCORE_LIMIT;

    if (static0 >= beta + error0 && beta + error < PROBCUT_SCORE_LIMIT) {
        const int32_t pc_beta = beta + error;
        STELLO_TRY_ASSIGN_R(
            std::optional<int32_t>,
            const ScoreMove high,
            search(s, board, player, shallow_look, ply, previous_move, pc_beta - 1, pc_beta));
        if (high.score >= pc_beta) {
            STELLO_RETURN_OK_R(std::optional<int32_t>, std::optional<int32_t>(beta));
        }
    }

    if (static0 <= alpha - error0 && alpha - error > -PROBCUT_SCORE_LIMIT) {
        const int32_t pc_alpha = alpha - error;
        STELLO_TRY_ASSIGN_R(
            std::optional<int32_t>,
            const ScoreMove low,
            search(s, board, player, shallow_look, ply, previous_move, pc_alpha, pc_alpha + 1));
        if (low.score <= pc_alpha) {
            STELLO_RETURN_OK_R(std::optional<int32_t>, std::optional<int32_t>(alpha));
        }
    }

    STELLO_RETURN_OK_R(std::optional<int32_t>, std::nullopt);
}

template <bool UseAvx2>
AbortResult<ScoreMove> Kernel<UseAvx2>::solve_root(
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
    int32_t best_score = -INFINITY_SCORE;
    int32_t best_move = -1;
    std::array<int32_t, MAX_MOVES> improvements{};
    size_t improved = 0;

    for (size_t i = 0; i < root_moves.size(); ++i) {
        const int32_t mv = root_moves[i];
        const bool has_solved_move = (best_move >= 0 || best.kind == ScoreKind::WinLossDraw || best.kind == ScoreKind::Exact);
        s.report_depth = has_solved_move ? empties : best.depth;
        s.report_move = mv;
        s.report_best_move = (best_move >= 0) ? best_move : best.mv;
        s.report_score = (best_move >= 0) ? best_score : best.score;
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

            if (kind == ScoreKind::WinLossDraw && score > 0) {
                best = Best{mv, score, kind, empties};
            } else if (kind == ScoreKind::WinLossDraw && score == 0) {
                best = Best{mv, score, ScoreKind::Heuristic, best.depth};
            } else if (kind == ScoreKind::Exact && best.kind == ScoreKind::WinLossDraw) {
                // Keep WinLossDraw/empties until all moves finish, while retaining any strictly higher Exact move.
                best.mv = mv;
            }
        }

        s.report(empties, mv, best_move, best_score, kind);
        if (best_score >= beta) {
            break;
        }
    }

    detail::move_to_front(root_moves, std::span<const int32_t>(improvements.data(), improved));
    STELLO_RETURN_OK_R(ScoreMove, (ScoreMove{best_score, best_move}));
}

template <bool UseAvx2>
AbortResult<int32_t> Kernel<UseAvx2>::evaluate_leaf(Searcher& s, const Board& board, Player player) {
    STELLO_TRY_VOID_I32(s.count_node());
    s.evaluations += 1;
    STELLO_RETURN_OK_R(int32_t, eval::pattern_score(*s.pattern, board.discs(player), board.discs(opponent(player))));
}

} // namespace stello::search
