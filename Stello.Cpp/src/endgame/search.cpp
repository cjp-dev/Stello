#include "search/engine.hpp"

#include "endgame/bits.hpp"
#include "endgame/stable.hpp"
#include "eval/eval.hpp"
#include "search/ybwc.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <optional>
#include <utility>

namespace stello::search {

namespace {

constexpr std::array<uint64_t, 4> QUADRANTS = {
    0x0000'0000'0F0F'0F0FULL,
    0x0000'0000'F0F0'F0F0ULL,
    0x0F0F'0F0F'0000'0000ULL,
    0xF0F0'F0F0'0000'0000ULL,
};

constexpr size_t DEEP_MOVES = MAX_MOVES;
constexpr size_t LOCAL_MOVES = static_cast<size_t>(END_LOCAL_EMPTIES);

inline std::optional<int32_t> stability_cut(uint64_t own, uint64_t opponent, int32_t alpha, int32_t empties) {
    if (alpha >= endgame::bits::STABILITY_THRESHOLD_NWS[static_cast<size_t>(64 - empties)]) {
        const int32_t limit = 64 - 2 * static_cast<int32_t>(std::popcount(endgame::stable::of(opponent, own)));
        if (limit <= alpha) {
            return limit;
        }
    }
    return std::nullopt;
}

inline std::pair<uint64_t, uint64_t> child_pos(uint64_t own, uint64_t opponent, const EndMove& mv) noexcept {
    return {opponent ^ mv.flips, own ^ mv.flips ^ (uint64_t{1} << mv.square)};
}

inline bool fill_end_moves(uint64_t own, uint64_t opponent, uint64_t legal, std::span<EndMove> moves) noexcept {
    for (size_t idx = 0; idx < moves.size(); ++idx) {
        const uint32_t square = static_cast<uint32_t>(std::countr_zero(legal));
        legal &= (legal - 1ULL);
        const uint64_t flipped = endgame::bits::flips(own, opponent, square);
        if (flipped == opponent) {
            return true;
        }
        moves[idx] = EndMove{square, flipped, 0, endgame::bits::LEGAL_UNDEFINED};
    }
    return false;
}

inline void swap_next_best(std::span<EndMove> moves, size_t start) noexcept {
    size_t top = start;
    for (size_t i = start + 1; i < moves.size(); ++i) {
        if (moves[i].value > moves[top].value) {
            top = i;
        }
    }
    if (top != start) {
        std::swap(moves[start], moves[top]);
    }
}

inline size_t undone_first(std::span<EndMove> moves, uint64_t done) noexcept {
    size_t remaining = 0;
    for (size_t i = 0; i < moves.size(); ++i) {
        if ((done & (uint64_t{1} << moves[i].square)) == 0) {
            std::swap(moves[remaining], moves[i]);
            ++remaining;
        }
    }
    return remaining;
}

inline size_t make_end_children(
    uint64_t own,
    uint64_t opponent,
    int32_t empties,
    std::span<EndMove> moves,
    size_t from,
    uint64_t done,
    std::array<EndgameChild, DEEP_MOVES>& out
) noexcept {
    size_t child_count = 0;
    for (size_t i = from; i < moves.size(); ++i) {
        swap_next_best(moves, i);
        const EndMove mv = moves[i];
        if ((done & (uint64_t{1} << mv.square)) == 0) {
            const auto [child_own, child_opponent] = child_pos(own, opponent, mv);
            out[child_count] = EndgameChild{
                child_own,
                child_opponent,
                empties - 1,
                mv.replies,
                static_cast<int32_t>(mv.square),
            };
            ++child_count;
        }
    }
    return child_count;
}

} // namespace

namespace detail {

bool fill_end_moves_for_test(uint64_t own, uint64_t opponent, uint64_t legal, std::span<EndMove> moves) noexcept {
    return fill_end_moves(own, opponent, legal, moves);
}

size_t undone_first_for_test(std::span<EndMove> moves, uint64_t done) noexcept {
    return undone_first(moves, done);
}

void swap_next_best_for_test(std::span<EndMove> moves, size_t start) noexcept {
    swap_next_best(moves, start);
}

} // namespace detail

AbortResult<int32_t> Searcher::solve_child(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t beta,
    int32_t empties
) {
    if (beta - alpha == 1) {
        return end_null(own, opponent, alpha, empties, endgame::bits::LEGAL_UNDEFINED);
    }
    return end_window(own, opponent, alpha, beta, empties, false);
}

AbortResult<int32_t> Searcher::end_null(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t empties,
    uint64_t legal
) {
    if (empties <= 4) {
        return solve_shallow(own, opponent, alpha, alpha + 1, empties, false);
    }
    if (empties <= END_LOCAL_EMPTIES) {
        return end_local(own, opponent, alpha, false, legal, empties);
    }
    return end_deep(own, opponent, alpha, empties, false, legal);
}

AbortResult<int32_t> Searcher::end_window(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t beta,
    int32_t empties,
    bool passed
) {
    if (empties <= END_FAST_EMPTIES) {
        return solve_shallow(own, opponent, alpha, beta, empties, passed);
    }

    STELLO_TRY_VOID_I32(count_node());
    const uint64_t legal = endgame::bits::legal_moves(own, opponent);
    if (legal == 0) {
        if (passed) {
            evaluations += 1;
            STELLO_RETURN_OK_R(int32_t, final_score(own, opponent));
        }
        STELLO_TRY_ASSIGN_I32(const int32_t s, end_window(opponent, own, -beta, -alpha, empties, true));
        STELLO_RETURN_OK_R(int32_t, -s);
    }

    if (!passed && beta >= endgame::bits::STABILITY_THRESHOLD[static_cast<size_t>(64 - empties)]) {
        const int32_t limit = 64 - 2 * static_cast<int32_t>(std::popcount(endgame::stable::of(opponent, own)));
        if (limit <= alpha) {
            STELLO_RETURN_OK_R(int32_t, limit);
        }
        if (limit < beta) {
            beta = limit;
        }
    }

    int32_t hash_move = -1;
    if (const auto entry = endgame_table->get_at_root(own, opponent, 0, root_empties)) {
        const int32_t value = static_cast<int32_t>(entry->value);
        switch (entry->bound) {
            case Bound::Exact:
                STELLO_RETURN_OK_R(int32_t, value);
            case Bound::Lower:
                if (value > alpha) {
                    alpha = value;
                }
                break;
            default:
                if (value < beta) {
                    beta = value;
                }
                break;
        }

        if (alpha >= beta) {
            STELLO_RETURN_OK_R(int32_t, value);
        }
        hash_move = static_cast<int32_t>(entry->mv);
    }

    const size_t count = static_cast<size_t>(std::popcount(legal));
    std::array<EndMove, DEEP_MOVES> list{};
    std::span<EndMove> moves(list.data(), count);
    if (fill_end_moves(own, opponent, legal, moves)) {
        STELLO_RETURN_OK_R(int32_t, 64);
    }

    if (empties >= END_ETC_MIN_EMPTIES) {
        for (const auto& mv : moves) {
            const auto [child_own, child_opponent] = child_pos(own, opponent, mv);
            if (const auto entry = endgame_table->get_at_root(child_own, child_opponent, 0, root_empties)) {
                if (entry->bound != Bound::Lower && -static_cast<int32_t>(entry->value) >= beta) {
                    STELLO_RETURN_OK_R(int32_t, -static_cast<int32_t>(entry->value));
                }
            }
        }
    }

    if (count > 1) {
        const int32_t depth = (std::min)(empties >> 3, 2);
        const int32_t order_alpha = -(std::min)(beta + 8, 64);
        const int32_t order_beta = -(std::max)(alpha - 12, -64);
        const uint64_t empty = ~(own | opponent);

        for (auto& mv : moves) {
            if (static_cast<int32_t>(mv.square) == hash_move) {
                mv.value = ORDER_HASH_MOVE;
                continue;
            }

            const auto [child_own, child_opponent] = child_pos(own, opponent, mv);
            mv.replies = endgame::bits::legal_moves(child_own, child_opponent);
            const uint64_t child_empty = empty & ~(uint64_t{1} << mv.square);
            STELLO_TRY_ASSIGN_I32(const int32_t ov, ordering_value(child_own, child_opponent, depth, order_alpha, order_beta));
            mv.value = (ORDER_OFFSET - endgame::bits::weighted_moves(mv.replies)) * ORDER_MOBILITY
                     + (ORDER_OFFSET - endgame::bits::potential_mobility(child_opponent, child_empty)) * ORDER_POTENTIAL_MOBILITY
                     + (64 - ov) * (ORDER_VALUE + depth * ORDER_VALUE_DEEP);
        }
    }

    const int32_t alpha_start = alpha;
    int32_t best = -END_INFINITY;
    int32_t best_move = -1;

    for (size_t i = 0; i < count; ++i) {
        if (i == 1 && empties > YBWC_END_MIN_EMPTIES && may_split()) {
            std::array<EndgameChild, DEEP_MOVES> children{};
            const size_t child_count = make_end_children(own, opponent, empties, moves, i, 0, children);
            STELLO_TRY_VOID_I32(split(std::span<const EndgameChild>(children.data(), child_count), alpha, beta, best, best_move));
            break;
        }

        swap_next_best(moves, i);
        const EndMove mv = moves[i];
        const auto [child_own, child_opponent] = child_pos(own, opponent, mv);
        int32_t score = 0;

        if (i == 0) {
            STELLO_TRY_ASSIGN_I32(const int32_t s, end_window(child_own, child_opponent, -beta, -alpha, empties - 1, false));
            score = -s;
        } else {
            STELLO_TRY_ASSIGN_I32(const int32_t s_null, end_null(child_own, child_opponent, -alpha - 1, empties - 1, mv.replies));
            const int32_t probe = -s_null;
            if (probe > alpha && probe < beta) {
                STELLO_TRY_ASSIGN_I32(const int32_t s_full, end_window(child_own, child_opponent, -beta, -probe, empties - 1, false));
                score = -s_full;
            } else {
                score = probe;
            }
        }

        if (score > best) {
            best = score;
            best_move = static_cast<int32_t>(mv.square);
            if (best > alpha) {
                alpha = best;
                if (alpha >= beta) {
                    break;
                }
            }
        }
    }

    const Bound bound = (best >= beta) ? Bound::Lower : ((best > alpha_start) ? Bound::Exact : Bound::Upper);
    endgame_table->store_at_root(own, opponent, 0, empties, bound, best, best_move, root_empties);
    STELLO_RETURN_OK_R(int32_t, best);
}

AbortResult<int32_t> Searcher::end_deep(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t empties,
    bool passed,
    uint64_t legal
) {
    if (empties <= END_LOCAL_EMPTIES) {
        return end_local(own, opponent, alpha, passed, legal, empties);
    }

    STELLO_TRY_VOID_I32(count_node());
    if (legal == endgame::bits::LEGAL_UNDEFINED) {
        legal = endgame::bits::legal_moves(own, opponent);
    }

    if (legal == 0) {
        if (passed) {
            evaluations += 1;
            STELLO_RETURN_OK_R(int32_t, final_score(own, opponent));
        }
        STELLO_TRY_ASSIGN_I32(const int32_t s, end_deep(opponent, own, -alpha - 1, empties, true, endgame::bits::LEGAL_UNDEFINED));
        STELLO_RETURN_OK_R(int32_t, -s);
    }

    if (!passed) {
        if (const auto cut = stability_cut(own, opponent, alpha, empties)) {
            STELLO_RETURN_OK_R(int32_t, *cut);
        }
    }

    int32_t hash_move = -1;
    if (const auto entry = endgame_table->get_at_root(own, opponent, 0, root_empties)) {
        const int32_t value = static_cast<int32_t>(entry->value);
        if (entry->bound != Bound::Upper && value > alpha) {
            STELLO_RETURN_OK_R(int32_t, value);
        }
        if (entry->bound != Bound::Lower && value <= alpha) {
            STELLO_RETURN_OK_R(int32_t, value);
        }
        hash_move = static_cast<int32_t>(entry->mv);
    }

    const size_t count = static_cast<size_t>(std::popcount(legal));
    std::array<EndMove, DEEP_MOVES> list{};
    std::span<EndMove> moves(list.data(), count);
    if (fill_end_moves(own, opponent, legal, moves)) {
        STELLO_RETURN_OK_R(int32_t, 64);
    }

    int32_t best = -END_INFINITY;
    int32_t best_move = -1;
    uint64_t done = 0;
    bool hash_move_searched = false;

    if (empties >= END_ETC_MIN_EMPTIES) {
        for (const auto& mv : moves) {
            const auto [child_own, child_opponent] = child_pos(own, opponent, mv);
            const auto entry = endgame_table->get_at_root(child_own, child_opponent, 0, root_empties);
            if (!entry.has_value()) {
                continue;
            }
            const int32_t value = -static_cast<int32_t>(entry->value);
            if (entry->bound != Bound::Lower && value > alpha) {
                STELLO_RETURN_OK_R(int32_t, value);
            }
            if (entry->bound != Bound::Upper && value <= alpha) {
                if (value > best) {
                    best = value;
                    best_move = static_cast<int32_t>(mv.square);
                }
                done |= uint64_t{1} << mv.square;
            }
        }
    }

    if (hash_move >= 0 && hash_move < 64 && ((legal & ~done) & (uint64_t{1} << hash_move)) != 0) {
        for (const auto& mv : moves) {
            if (static_cast<int32_t>(mv.square) == hash_move) {
                const auto [child_own, child_opponent] = child_pos(own, opponent, mv);
                STELLO_TRY_ASSIGN_I32(
                    const int32_t s,
                    end_deep(child_own, child_opponent, -alpha - 1, empties - 1, false, endgame::bits::LEGAL_UNDEFINED));
                hash_move_searched = true;
                const int32_t score = -s;
                if (score > best) {
                    best = score;
                    best_move = hash_move;
                }
                done |= uint64_t{1} << hash_move;
                break;
            }
        }
    }

    if (best <= alpha) {
        if (count > 1) {
            const int32_t depth = empties >> 4;
            const int32_t order_alpha = -(std::min)(alpha + 6, 64);
            const int32_t order_beta = -(std::max)(alpha - 16, -64);

            for (auto& mv : moves) {
                if ((done & (uint64_t{1} << mv.square)) != 0) {
                    mv.value = (std::numeric_limits<int32_t>::min)();
                    continue;
                }

                const auto [child_own, child_opponent] = child_pos(own, opponent, mv);
                mv.replies = endgame::bits::legal_moves(child_own, child_opponent);
                STELLO_TRY_ASSIGN_I32(const int32_t ov, ordering_value(child_own, child_opponent, depth, order_alpha, order_beta));
                mv.value = (ORDER_OFFSET - endgame::bits::weighted_moves(mv.replies)) * ORDER_NULL_MOBILITY
                         + (64 - ov) * (ORDER_NULL_VALUE + depth * ORDER_NULL_VALUE_DEEP);
            }
        }

        bool searched = hash_move_searched;
        for (size_t i = 0; i < count; ++i) {
            if (searched && empties > YBWC_END_MIN_EMPTIES && may_split()) {
                std::array<EndgameChild, DEEP_MOVES> children{};
                const size_t child_count = make_end_children(own, opponent, empties, moves, i, done, children);
                if (child_count > 0) {
                    STELLO_TRY_VOID_I32(
                        split(std::span<const EndgameChild>(children.data(), child_count), alpha, alpha + 1, best, best_move));
                }
                break;
            }

            swap_next_best(moves, i);
            const EndMove mv = moves[i];
            if ((done & (uint64_t{1} << mv.square)) != 0) {
                continue;
            }

            searched = true;

            const auto [child_own, child_opponent] = child_pos(own, opponent, mv);
            STELLO_TRY_ASSIGN_I32(const int32_t s, end_deep(child_own, child_opponent, -alpha - 1, empties - 1, false, mv.replies));
            const int32_t score = -s;
            if (score > best) {
                best = score;
                best_move = static_cast<int32_t>(mv.square);
                if (best > alpha) {
                    break;
                }
            }
        }
    }

    const Bound bound = (best > alpha) ? Bound::Lower : Bound::Upper;
    endgame_table->store_at_root(own, opponent, 0, empties, bound, best, best_move, root_empties);
    STELLO_RETURN_OK_R(int32_t, best);
}

AbortResult<int32_t> Searcher::ordering_value(
    uint64_t own,
    uint64_t opponent,
    int32_t depth,
    int32_t alpha,
    int32_t beta
) {
    STELLO_TRY_VOID_I32(count_node());
    if (depth == 0) {
        evaluations += 1;
        STELLO_RETURN_OK_R(int32_t, eval::disc_score(*pattern, own, opponent));
    }

    uint64_t legal = endgame::bits::legal_moves(own, opponent);
    if (legal == 0) {
        if (endgame::bits::legal_moves(opponent, own) == 0) {
            evaluations += 1;
            STELLO_RETURN_OK_R(int32_t, final_score(own, opponent));
        }
        STELLO_TRY_ASSIGN_I32(const int32_t s, ordering_value(opponent, own, depth, -beta, -alpha));
        STELLO_RETURN_OK_R(int32_t, -s);
    }

    int32_t best = -END_INFINITY;
    while (legal != 0) {
        const uint32_t square = static_cast<uint32_t>(std::countr_zero(legal));
        legal &= (legal - 1ULL);
        const uint64_t flipped = endgame::bits::flips(own, opponent, square);
        STELLO_TRY_ASSIGN_I32(
            const int32_t s,
            ordering_value(opponent ^ flipped, own ^ flipped ^ (uint64_t{1} << square), depth - 1, -beta, -alpha));
        const int32_t score = -s;
        if (score > best) {
            best = score;
            if (best > alpha) {
                alpha = best;
                if (alpha >= beta) {
                    break;
                }
            }
        }
    }

    STELLO_RETURN_OK_R(int32_t, best);
}

AbortResult<int32_t> Searcher::end_local(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    bool passed,
    uint64_t legal,
    int32_t empties
) {
    if (empties <= END_SIMPLE_EMPTIES) {
        return end_simple(own, opponent, alpha, passed, legal, empties);
    }

    STELLO_TRY_VOID_I32(count_node());
    if (!passed) {
        if (const auto cut = stability_cut(own, opponent, alpha, empties)) {
            STELLO_RETURN_OK_R(int32_t, *cut);
        }
    }

    if (legal == endgame::bits::LEGAL_UNDEFINED) {
        legal = endgame::bits::legal_moves(own, opponent);
    }

    if (legal == 0) {
        if (passed) {
            evaluations += 1;
            STELLO_RETURN_OK_R(int32_t, final_score(own, opponent));
        }
        STELLO_TRY_ASSIGN_I32(const int32_t s, end_local(opponent, own, -alpha - 1, true, endgame::bits::LEGAL_UNDEFINED, empties));
        STELLO_RETURN_OK_R(int32_t, -s);
    }

    const size_t count = static_cast<size_t>(std::popcount(legal));
    std::array<EndMove, LOCAL_MOVES> list{};
    std::span<EndMove> moves(list.data(), count);
    if (fill_end_moves(own, opponent, legal, moves)) {
        STELLO_RETURN_OK_R(int32_t, 64);
    }

    int32_t best = -END_INFINITY;
    uint64_t done = 0;

    if (count > 1) {
        for (auto& mv : moves) {
            const auto [child_own, child_opponent] = child_pos(own, opponent, mv);
            const size_t slot = endgame->local.slot(child_own, child_opponent, empties - 1);
            const auto stored = endgame->local.get(slot);
            if (stored.matches(child_own, child_opponent)) {
                if (alpha < stored.lower) {
                    STELLO_RETURN_OK_R(int32_t, stored.lower);
                }
                if (stored.upper <= alpha) {
                    if (stored.upper > best) {
                        best = stored.upper;
                    }
                    done |= uint64_t{1} << mv.square;
                    continue;
                }
            }

            mv.replies = endgame::bits::legal_moves(child_own, child_opponent);
            const int32_t replies = endgame::bits::moves_corners_twice(mv.replies);
            if (replies <= 1) {
                STELLO_TRY_ASSIGN_I32(
                    const int32_t s,
                    end_local(child_own, child_opponent, -alpha - 1, false, mv.replies, empties - 1));
                const int32_t score = -s;
                if (score > best) {
                    best = score;
                    if (alpha < best) {
                        endgame->local.set(slot, child_own, child_opponent, best, 64);
                        STELLO_RETURN_OK_R(int32_t, best);
                    }
                }
                endgame->local.set(slot, child_own, child_opponent, -64, score);
                done |= uint64_t{1} << mv.square;
                continue;
            }

            mv.value = (ORDER_OFFSET - replies) * ORDER_LOCAL_MOBILITY;
        }
    }

    const size_t undone = undone_first(moves, done);
    std::span<EndMove> remaining = moves.subspan(0, undone);
    for (size_t i = 0; i < remaining.size(); ++i) {
        swap_next_best(remaining, i);
        const EndMove mv = remaining[i];
        const auto [child_own, child_opponent] = child_pos(own, opponent, mv);
        const size_t slot = endgame->local.slot(child_own, child_opponent, empties - 1);
        STELLO_TRY_ASSIGN_I32(
            const int32_t s,
            end_local(child_own, child_opponent, -alpha - 1, false, mv.replies, empties - 1));
        const int32_t score = -s;
        if (score > best) {
            best = score;
            if (alpha < best) {
                endgame->local.set(slot, child_own, child_opponent, best, 64);
                break;
            }
        }
        endgame->local.set(slot, child_own, child_opponent, -64, score);
    }

    STELLO_RETURN_OK_R(int32_t, best);
}

AbortResult<int32_t> Searcher::end_simple(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    bool passed,
    uint64_t legal,
    int32_t empties
) {
    if (empties <= END_FAST_EMPTIES) {
        return end_fast(own, opponent, alpha, passed, empties);
    }

    STELLO_TRY_VOID_I32(count_node());
    if (!passed) {
        if (const auto cut = stability_cut(own, opponent, alpha, empties)) {
            STELLO_RETURN_OK_R(int32_t, *cut);
        }
    }

    if (legal == endgame::bits::LEGAL_UNDEFINED) {
        legal = endgame::bits::legal_moves(own, opponent);
    }

    if (legal == 0) {
        if (passed) {
            evaluations += 1;
            STELLO_RETURN_OK_R(int32_t, final_score(own, opponent));
        }
        STELLO_TRY_ASSIGN_I32(const int32_t s, end_simple(opponent, own, -alpha - 1, true, endgame::bits::LEGAL_UNDEFINED, empties));
        STELLO_RETURN_OK_R(int32_t, -s);
    }

    const size_t count = static_cast<size_t>(std::popcount(legal));
    std::array<EndMove, LOCAL_MOVES> list{};
    std::span<EndMove> moves(list.data(), count);
    if (fill_end_moves(own, opponent, legal, moves)) {
        STELLO_RETURN_OK_R(int32_t, 64);
    }

    int32_t best = -END_INFINITY;
    uint64_t done = 0;

    if (count > 1) {
        const int32_t par = endgame::bits::parity(~(own | opponent));
        for (auto& mv : moves) {
            mv.value = ((par & endgame::bits::QUADRANT_BIT[mv.square]) != 0) ? ORDER_SIMPLE_PARITY : 0;
            const auto [child_own, child_opponent] = child_pos(own, opponent, mv);
            const size_t slot = endgame->local.slot(child_own, child_opponent, empties - 1);
            const auto stored = endgame->local.get(slot);
            if (stored.matches(child_own, child_opponent)) {
                if (alpha < stored.lower) {
                    STELLO_RETURN_OK_R(int32_t, stored.lower);
                }
                if (stored.upper <= alpha) {
                    if (stored.upper > best) {
                        best = stored.upper;
                    }
                    done |= uint64_t{1} << mv.square;
                    continue;
                }
            }

            mv.replies = endgame::bits::legal_moves(child_own, child_opponent);
            const int32_t replies = endgame::bits::moves_corners_twice(mv.replies);
            if (replies <= 1) {
                STELLO_TRY_ASSIGN_I32(
                    const int32_t s,
                    end_simple(child_own, child_opponent, -alpha - 1, false, mv.replies, empties - 1));
                const int32_t score = -s;
                if (score > best) {
                    best = score;
                    if (alpha < best) {
                        endgame->local.set(slot, child_own, child_opponent, best, 64);
                        STELLO_RETURN_OK_R(int32_t, best);
                    }
                }
                endgame->local.set(slot, child_own, child_opponent, -64, score);
                done |= uint64_t{1} << mv.square;
                continue;
            }

            mv.value += (ORDER_OFFSET - replies) * ORDER_SIMPLE_MOBILITY;
        }
    }

    const size_t undone = undone_first(moves, done);
    std::span<EndMove> remaining = moves.subspan(0, undone);
    for (size_t i = 0; i < remaining.size(); ++i) {
        swap_next_best(remaining, i);
        const EndMove mv = remaining[i];
        const auto [child_own, child_opponent] = child_pos(own, opponent, mv);
        const size_t slot = endgame->local.slot(child_own, child_opponent, empties - 1);
        STELLO_TRY_ASSIGN_I32(
            const int32_t s,
            end_simple(child_own, child_opponent, -alpha - 1, false, mv.replies, empties - 1));
        const int32_t score = -s;
        if (score > best) {
            best = score;
            if (alpha < best) {
                endgame->local.set(slot, child_own, child_opponent, best, 64);
                break;
            }
        }
        endgame->local.set(slot, child_own, child_opponent, -64, score);
    }

    STELLO_RETURN_OK_R(int32_t, best);
}

AbortResult<int32_t> Searcher::end_fast(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    bool passed,
    int32_t empties
) {
    STELLO_TRY_VOID_I32(count_node());
    if (!passed) {
        if (const auto cut = stability_cut(own, opponent, alpha, empties)) {
            STELLO_RETURN_OK_R(int32_t, *cut);
        }
    }

    uint64_t legal = endgame::bits::legal_moves(own, opponent);
    if (legal == 0) {
        if (passed) {
            evaluations += 1;
            STELLO_RETURN_OK_R(int32_t, final_score(own, opponent));
        }
        STELLO_TRY_ASSIGN_I32(const int32_t s, end_fast(opponent, own, -alpha - 1, true, empties));
        STELLO_RETURN_OK_R(int32_t, -s);
    }

    int32_t best = -END_INFINITY;
    uint64_t first = legal & endgame::bits::lone_empties(own, opponent);
    if (first == 0) {
        first = legal;
    }

    uint64_t batch = first;
    while (batch != 0) {
        legal ^= batch;
        while (batch != 0) {
            const uint32_t square = static_cast<uint32_t>(std::countr_zero(batch));
            batch &= (batch - 1ULL);
            const uint64_t flipped = endgame::bits::flips(own, opponent, square);
            const uint64_t child_own = opponent ^ flipped;
            const uint64_t child_opponent = own ^ flipped ^ (uint64_t{1} << square);
            int32_t score = 0;
            if (empties == 5) {
                STELLO_TRY_ASSIGN_EXISTING_I32(score, last4(child_own, child_opponent, alpha));
            } else {
                STELLO_TRY_ASSIGN_I32(const int32_t s, end_fast(child_own, child_opponent, -alpha - 1, false, empties - 1));
                score = -s;
            }
            if (score > alpha) {
                STELLO_RETURN_OK_R(int32_t, score);
            }
            if (score > best) {
                best = score;
            }
        }
        batch = legal;
    }

    STELLO_RETURN_OK_R(int32_t, best);
}

AbortResult<int32_t> Searcher::solve_shallow(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t beta,
    int32_t empties,
    bool passed
) {
    STELLO_TRY_VOID_I32(count_node());

    const uint64_t empty = ~(own | opponent);
    if (empties == 1) {
        STELLO_RETURN_OK_R(int32_t, solve_last(own, opponent, empty));
    }
    if (empties == 2) {
        return solve_last2(own, opponent, alpha, beta, empty, passed);
    }

    uint64_t odd = 0;
    for (const uint64_t quadrant : QUADRANTS) {
        if ((std::popcount(empty & quadrant) & 1) != 0) {
            odd |= quadrant;
        }
    }

    int32_t best = -INFINITY_SCORE;
    for (int pass = 0; pass < 2; ++pass) {
        uint64_t squares = empty & ((pass == 0) ? odd : ~odd);
        while (squares != 0) {
            const uint32_t square = static_cast<uint32_t>(std::countr_zero(squares));
            squares &= (squares - 1ULL);
            const uint64_t f = bits::flips(own, opponent, square);
            if (f == 0) {
                continue;
            }

            STELLO_TRY_ASSIGN_I32(
                const int32_t s,
                solve_shallow(opponent & ~f, own | f | (uint64_t{1} << square), -beta, -alpha, empties - 1, false));
            const int32_t score = -s;
            if (score > best) {
                best = score;
                if (best > alpha) {
                    alpha = best;
                    if (alpha >= beta) {
                        STELLO_RETURN_OK_R(int32_t, best);
                    }
                }
            }
        }
    }

    if (best > -INFINITY_SCORE) {
        STELLO_RETURN_OK_R(int32_t, best);
    }

    if (passed) {
        evaluations += 1;
        STELLO_RETURN_OK_R(int32_t, final_score(own, opponent));
    }

    STELLO_TRY_ASSIGN_I32(const int32_t s_pass, solve_shallow(opponent, own, -beta, -alpha, empties, true));
    STELLO_RETURN_OK_R(int32_t, -s_pass);
}

AbortResult<int32_t> Searcher::solve_last2(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t beta,
    uint64_t empty,
    bool passed
) {
    const uint32_t first = static_cast<uint32_t>(std::countr_zero(empty));
    const uint32_t second = static_cast<uint32_t>(std::countr_zero(empty & (empty - 1ULL)));
    int32_t best = -INFINITY_SCORE;

    const uint64_t f1 = bits::flips(own, opponent, first);
    if (f1 != 0) {
        STELLO_TRY_VOID_I32(count_node());
        best = -solve_last(opponent & ~f1, own | f1 | (uint64_t{1} << first), uint64_t{1} << second);
        if (best >= beta) {
            STELLO_RETURN_OK_R(int32_t, best);
        }
    }

    const uint64_t f2 = bits::flips(own, opponent, second);
    if (f2 != 0) {
        STELLO_TRY_VOID_I32(count_node());
        const int32_t s2 = -solve_last(opponent & ~f2, own | f2 | (uint64_t{1} << second), uint64_t{1} << first);
        if (s2 > best) {
            best = s2;
        }
    }

    if (best > -INFINITY_SCORE) {
        STELLO_RETURN_OK_R(int32_t, best);
    }

    if (passed) {
        evaluations += 1;
        STELLO_RETURN_OK_R(int32_t, final_score(own, opponent));
    }

    STELLO_TRY_VOID_I32(count_node());
    STELLO_TRY_ASSIGN_I32(const int32_t s_pass, solve_last2(opponent, own, -beta, -alpha, empty, true));
    STELLO_RETURN_OK_R(int32_t, -s_pass);
}

int32_t Searcher::solve_last(uint64_t own, uint64_t opponent, uint64_t empty) noexcept {
    evaluations += 1;
    const uint32_t square = static_cast<uint32_t>(std::countr_zero(empty));
    const uint64_t bit = uint64_t{1} << square;

    const uint64_t f1 = bits::flips(own, opponent, square);
    if (f1 != 0) {
        return final_score(own | f1 | bit, opponent & ~f1);
    }

    const uint64_t f2 = bits::flips(opponent, own, square);
    if (f2 != 0) {
        return final_score(own & ~f2, opponent | f2 | bit);
    }
    return final_score(own, opponent);
}

} // namespace stello::search
