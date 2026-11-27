#pragma once

#include "core/bits.hpp"
#include "core/compiler.hpp"
#include "core/cpu.hpp"
#include "endgame/bits.hpp"
#include "eval/eval.hpp"
#include "eval/patterns.hpp"
#include "search/engine.hpp"
#include "search/ybwc.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <span>

#if (defined(__x86_64__) || defined(_M_X64)) && !defined(__EMSCRIPTEN__)
#define STELLO_HAS_X86_64_V3 1
#define STELLO_HAS_X86_64_V4 1
#else
#define STELLO_HAS_X86_64_V3 0
#define STELLO_HAS_X86_64_V4 0
#endif

namespace stello::search {

inline cpu::CpuTier dispatch_tier() noexcept {
#if STELLO_HAS_X86_64_V3
    return cpu::active_tier();
#else
    return cpu::CpuTier::Portable;
#endif
}

template <cpu::CpuTier Tier>
struct Kernel {
    static STELLO_FORCEINLINE uint64_t flips(uint64_t own, uint64_t opponent, uint32_t square) noexcept;
    static STELLO_FORCEINLINE uint64_t legal_moves(uint64_t own, uint64_t opponent) noexcept;
    static STELLO_FORCEINLINE void dual_legal_moves(
        uint64_t own,
        uint64_t opponent,
        uint64_t& own_out,
        uint64_t& opp_out
    ) noexcept;
    static STELLO_FORCEINLINE void legal_and_potential(
        uint64_t own,
        uint64_t opponent,
        uint64_t& legal_out,
        int32_t& pot_out
    ) noexcept;
    static STELLO_FORCEINLINE uint64_t stable_of(uint64_t own, uint64_t opponent) noexcept;
    static STELLO_FORCEINLINE uint64_t transpose(uint64_t x) noexcept;
    static STELLO_FORCEINLINE uint16_t simul_last_flip(uint64_t own, uint32_t square) noexcept;
    static STELLO_FORCEINLINE Board play_unchecked(const Board& board, Player player, uint32_t square) noexcept;

    static STELLO_FORCEINLINE eval::PatternState eval_state_of(
        const eval::PatternSet& set,
        uint64_t digit1,
        uint64_t digit2
    ) noexcept;
    static STELLO_FORCEINLINE eval::PatternState eval_state_of_node(
        const eval::PatternSet& set,
        uint64_t own,
        uint64_t opponent,
        uint32_t discs
    ) noexcept {
        return (discs & 1U) == 0U ? eval_state_of(set, own, opponent) : eval_state_of(set, opponent, own);
    }
    static STELLO_FORCEINLINE void advance_eval_state(
        const eval::PatternSet& set,
        eval::PatternState& next,
        const eval::PatternState& cur,
        bool mover_is_even_disc,
        uint32_t square,
        uint64_t flips
    ) noexcept;
    static STELLO_FORCEINLINE int32_t sum_eval_state(
        const eval::PatternSet& set,
        const eval::PatternState& cur,
        const int32_t* packed_pair_weights,
        bool odd_slot,
        bool vpaddw_safe
    ) noexcept;
    static STELLO_FORCEINLINE int32_t sum_eval_after_move(
        const eval::PatternSet& set,
        const eval::PatternState& cur,
        bool mover_is_even_disc,
        uint32_t square,
        uint64_t flips,
        const int32_t* packed_child_pair_weights,
        bool child_odd_slot,
        bool vpaddw_safe
    ) noexcept;
    static STELLO_FORCEINLINE int32_t disc_score(
        const eval::EvalWeights& weights,
        uint64_t own,
        uint64_t opponent,
        uint64_t known_own_legal = endgame::bits::LEGAL_UNDEFINED
    ) noexcept {
        const uint32_t discs = static_cast<uint32_t>(std::popcount(own | opponent));
        const int32_t* const pw = weights.packed_weights_for_discs(discs);
        const bool is_odd = (discs & 1U) != 0U;
        int32_t sum = !is_odd
            ? weights.set().sum_weights_packed(own, opponent, pw, false)
            : weights.set().sum_weights_packed(opponent, own, pw, true);
        if (weights.has_mobility()) {
            uint32_t m_own = 0;
            uint32_t m_opp = 0;
            if (known_own_legal != endgame::bits::LEGAL_UNDEFINED) {
                m_own = static_cast<uint32_t>(std::popcount(known_own_legal));
                m_opp = static_cast<uint32_t>(std::popcount(legal_moves(opponent, own)));
            } else {
                m_own = static_cast<uint32_t>(std::popcount(legal_moves(own, opponent)));
                m_opp = static_cast<uint32_t>(std::popcount(legal_moves(opponent, own)));
            }
            sum += weights.mobility_bonus_for_discs(discs, m_own, m_opp);
        }
        return std::clamp(sum / weights.units_per_disc(), -64, 64);
    }
    static STELLO_FORCEINLINE int32_t eval_after_move_score_bounded(
        const eval::EvalWeights& weights,
        const eval::PatternState& cur,
        bool mover_is_even_disc,
        uint32_t child_discs,
        const int32_t* child_w,
        int32_t skip_thresh,
        int32_t current_best,
        uint64_t own,
        uint64_t other,
        uint32_t square,
        uint64_t move_flips
    ) noexcept {
        if (move_flips == other) [[unlikely]] {
            return WIN_SCORE + 64;
        }
        int32_t raw = sum_eval_after_move(
            weights.set(), cur, mover_is_even_disc, square, move_flips, child_w, mover_is_even_disc, weights.packed_vpaddw_safe());
        if (weights.has_mobility()) {
            const uint64_t child_own = other ^ move_flips;
            const uint64_t child_other = own | move_flips | (uint64_t{1} << square);
            const uint32_t m_own = static_cast<uint32_t>(std::popcount(legal_moves(child_own, child_other)));
            const uint32_t m_opp = static_cast<uint32_t>(std::popcount(legal_moves(child_other, child_own)));
            raw += weights.mobility_bonus_for_discs(child_discs, m_own, m_opp);
        }
        if (raw >= skip_thresh) {
            return current_best;
        }
        return -eval::clamp_pattern_score(weights, raw);
    }
    static STELLO_FORCEINLINE int32_t eval_state_score(
        const eval::EvalWeights& weights,
        const eval::PatternState& cur,
        uint64_t own,
        uint64_t opponent,
        uint32_t discs,
        const int32_t* stage_w
    ) noexcept {
        int32_t raw = sum_eval_state(weights.set(), cur, stage_w, (discs & 1U) != 0U, weights.packed_vpaddw_safe());
        if (weights.has_mobility()) {
            const uint32_t m_own = static_cast<uint32_t>(std::popcount(legal_moves(own, opponent)));
            const uint32_t m_opp = static_cast<uint32_t>(std::popcount(legal_moves(opponent, own)));
            raw += weights.mobility_bonus_for_discs(discs, m_own, m_opp);
        }
        return eval::clamp_pattern_score(weights, raw);
    }

    // Midgame & top-level driver (search/engine_impl.hpp)
    static SearchOutcome run(
        Searcher& s,
        const Board& board,
        Player player,
        const SearchLimits& limits,
        std::optional<uint64_t> only_moves
    );
    static AbortVoid iterate(
        Searcher& s,
        const Board& board,
        Player player,
        const SearchLimits& limits,
        std::span<int32_t> root_moves,
        Best& best
    );
    static AbortResult<bool> midgame_iterations(
        Searcher& s,
        const Board& board,
        Player player,
        const SearchLimits& limits,
        time::TimeBudget budget,
        std::span<int32_t> root_moves,
        Best& best,
        std::atomic<int32_t>* main_look
    );
    static AbortVoid search_root(
        Searcher& s,
        const Board& board,
        Player player,
        int32_t look,
        std::span<int32_t> root_moves,
        Best& best
    );
    static AbortResult<ScoreMove> search(
        Searcher& s,
        const Board& board,
        Player player,
        int32_t look,
        int32_t ply,
        int32_t previous_move,
        int32_t alpha,
        int32_t beta
    );
    static AbortResult<ScoreMove> search_inc(
        Searcher& s,
        const Board& board,
        Player player,
        int32_t look,
        int32_t ply,
        int32_t previous_move,
        int32_t alpha,
        int32_t beta,
        const eval::PatternState& eval_state,
        uint64_t known_legal = endgame::bits::LEGAL_UNDEFINED
    );
    static STELLO_NOINLINE AbortResult<ScoreMove> search_inc_deep(
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
    );
    static AbortResult<ScoreMove> solve_root(
        Searcher& s,
        const Board& board,
        Player player,
        int32_t alpha,
        int32_t beta,
        ScoreKind kind,
        std::span<int32_t> root_moves,
        Best& best
    );
    static AbortVoid order_by_evaluation(
        Searcher& s,
        std::span<int32_t> list,
        int32_t hash_move,
        const Board& board,
        Player player
    );
    static AbortVoid order_by_evaluation(
        Searcher& s,
        std::span<int32_t> list,
        int32_t hash_move,
        const Board& board,
        Player player,
        const eval::PatternState& eval_state
    );
    static AbortVoid split_midgame(
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
    );
    static AbortResult<std::optional<int32_t>> try_probcut(
        Searcher& s,
        const Board& board,
        Player player,
        int32_t look,
        int32_t ply,
        int32_t previous_move,
        int32_t alpha,
        int32_t beta
    );
    static AbortResult<std::optional<int32_t>> try_probcut_inc(
        Searcher& s,
        const Board& board,
        Player player,
        int32_t look,
        int32_t ply,
        int32_t previous_move,
        int32_t alpha,
        int32_t beta,
        const eval::PatternState& eval_state
    );
    static AbortResult<std::optional<int32_t>> probcut_searches(
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
    );
    static AbortResult<std::optional<int32_t>> probcut_searches_inc(
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
    );
    static AbortResult<int32_t> evaluate_leaf(Searcher& s, const Board& board, Player player);
    static AbortResult<bool> lazy_smp_midgame(
        Searcher& s,
        const Helpers& smp_helpers,
        const Board& board,
        Player player,
        const SearchLimits& limits,
        time::TimeBudget budget,
        std::span<int32_t> root_moves,
        Best& best
    );
    static AbortResult<ScoreMove> solve_iteration(
        Searcher& s,
        const Board& board,
        Player player,
        int32_t alpha,
        int32_t beta,
        ScoreKind kind,
        std::span<int32_t> root_moves,
        Best& best
    );

    // Endgame search routines (endgame/search_impl.hpp)
    static AbortResult<int32_t> solve_child(
        Searcher& s,
        uint64_t own,
        uint64_t opponent,
        int32_t alpha,
        int32_t beta,
        int32_t empties
    );
    static AbortResult<int32_t> end_null(
        Searcher& s,
        uint64_t own,
        uint64_t opponent,
        int32_t alpha,
        int32_t empties,
        uint64_t legal
    );
    static AbortResult<int32_t> end_window(
        Searcher& s,
        uint64_t own,
        uint64_t opponent,
        int32_t alpha,
        int32_t beta,
        int32_t empties,
        bool passed
    );
    static AbortResult<int32_t> end_deep(
        Searcher& s,
        uint64_t own,
        uint64_t opponent,
        int32_t alpha,
        int32_t empties,
        bool passed,
        uint64_t legal
    );
    static AbortVoid split_endgame(
        Searcher& s,
        uint64_t own,
        uint64_t opponent,
        int32_t empties,
        std::span<EndMove> moves,
        size_t from,
        uint64_t done,
        int32_t alpha,
        int32_t beta,
        int32_t& best,
        int32_t& best_move
    );
    static AbortResult<int32_t> ordering_value(
        Searcher& s,
        uint64_t own,
        uint64_t opponent,
        int32_t depth,
        int32_t alpha,
        int32_t beta,
        uint64_t known_legal = endgame::bits::LEGAL_UNDEFINED
    );
    static AbortResult<std::optional<int32_t>> try_end_probcut(
        Searcher& s,
        uint64_t own,
        uint64_t opponent,
        int32_t alpha,
        int32_t beta,
        int32_t empties
    );
    static AbortResult<int32_t> end_local(
        Searcher& s,
        uint64_t own,
        uint64_t opponent,
        int32_t alpha,
        bool passed,
        uint64_t legal,
        int32_t empties
    );
    static AbortResult<int32_t> end_simple(
        Searcher& s,
        uint64_t own,
        uint64_t opponent,
        int32_t alpha,
        bool passed,
        uint64_t legal,
        int32_t empties
    );
    static AbortResult<int32_t> end_fast(
        Searcher& s,
        uint64_t own,
        uint64_t opponent,
        int32_t alpha,
        bool passed,
        int32_t empties,
        uint64_t known_legal = endgame::bits::LEGAL_UNDEFINED
    );
    static AbortResult<int32_t> solve_shallow(
        Searcher& s,
        uint64_t own,
        uint64_t opponent,
        int32_t alpha,
        int32_t beta,
        int32_t empties,
        bool passed
    );
    static AbortResult<int32_t> solve_last2(
        Searcher& s,
        uint64_t own,
        uint64_t opponent,
        int32_t alpha,
        int32_t beta,
        uint64_t empty,
        bool passed
    );
    static int32_t solve_last(Searcher& s, uint64_t own, uint64_t opponent, uint64_t empty) noexcept;

    // Last-4 routines (endgame/last_impl.hpp)
    static AbortResult<int32_t> last4(Searcher& s, uint64_t own, uint64_t opponent, int32_t alpha);
    static AbortResult<int32_t> last3(
        Searcher& s,
        uint64_t own,
        uint64_t opponent,
        int32_t alpha,
        uint32_t p0,
        uint32_t p1,
        uint32_t p2,
        uint64_t empties4
    );
    static AbortResult<int32_t> last2(
        Searcher& s,
        uint64_t own,
        uint64_t opponent,
        int32_t alpha,
        uint32_t p0,
        uint32_t p1
    );
    static AbortResult<int32_t> last1(Searcher& s, uint64_t own, int32_t alpha, uint32_t square);

    // YBWC split routines (search/ybwc_impl.hpp)
    template <typename ChildT>
    static AbortVoid split(
        Searcher& s,
        std::span<const ChildT> children,
        int32_t alpha,
        int32_t beta,
        int32_t& best,
        int32_t& best_move,
        bool is_root = false
    );
    static AbortResult<int32_t> child_null_window(Searcher& searcher, const MidgameChild& child, int32_t alpha);
    static AbortResult<int32_t> child_window(
        Searcher& searcher,
        const MidgameChild& child,
        int32_t alpha,
        int32_t beta,
        int32_t null_score
    );
    static AbortResult<int32_t> child_null_window(Searcher& searcher, const EndgameChild& child, int32_t alpha);
    static AbortResult<int32_t> child_window(
        Searcher& searcher,
        const EndgameChild& child,
        int32_t alpha,
        int32_t beta,
        int32_t null_score
    );
};

extern template struct Kernel<cpu::CpuTier::Portable>;
extern template AbortVoid Kernel<cpu::CpuTier::Portable>::split<MidgameChild>(
    Searcher&,
    std::span<const MidgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);
extern template AbortVoid Kernel<cpu::CpuTier::Portable>::split<EndgameChild>(
    Searcher&,
    std::span<const EndgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);

#if STELLO_HAS_X86_64_V3
template <>
uint64_t Kernel<cpu::CpuTier::V3>::flips(uint64_t own, uint64_t opponent, uint32_t square) noexcept;
template <>
uint64_t Kernel<cpu::CpuTier::V3>::legal_moves(uint64_t own, uint64_t opponent) noexcept;
template <>
void Kernel<cpu::CpuTier::V3>::dual_legal_moves(
    uint64_t own,
    uint64_t opponent,
    uint64_t& own_out,
    uint64_t& opp_out
) noexcept;
template <>
void Kernel<cpu::CpuTier::V3>::legal_and_potential(
    uint64_t own,
    uint64_t opponent,
    uint64_t& legal_out,
    int32_t& pot_out
) noexcept;
template <>
uint64_t Kernel<cpu::CpuTier::V3>::stable_of(uint64_t own, uint64_t opponent) noexcept;
template <>
uint64_t Kernel<cpu::CpuTier::V3>::transpose(uint64_t x) noexcept;
template <>
uint16_t Kernel<cpu::CpuTier::V3>::simul_last_flip(uint64_t own, uint32_t square) noexcept;
template <>
eval::PatternState Kernel<cpu::CpuTier::V3>::eval_state_of(
    const eval::PatternSet& set,
    uint64_t digit1,
    uint64_t digit2
) noexcept;
template <>
void Kernel<cpu::CpuTier::V3>::advance_eval_state(
    const eval::PatternSet& set,
    eval::PatternState& next,
    const eval::PatternState& cur,
    bool mover_is_even_disc,
    uint32_t square,
    uint64_t flips
) noexcept;
template <>
int32_t Kernel<cpu::CpuTier::V3>::sum_eval_state(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    const int32_t* packed_pair_weights,
    bool odd_slot,
    bool vpaddw_safe
) noexcept;
template <>
int32_t Kernel<cpu::CpuTier::V3>::sum_eval_after_move(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    bool mover_is_even_disc,
    uint32_t square,
    uint64_t flips,
    const int32_t* packed_child_pair_weights,
    bool child_odd_slot,
    bool vpaddw_safe
) noexcept;

extern template struct Kernel<cpu::CpuTier::V3>;
extern template AbortVoid Kernel<cpu::CpuTier::V3>::split<MidgameChild>(
    Searcher&,
    std::span<const MidgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);
extern template AbortVoid Kernel<cpu::CpuTier::V3>::split<EndgameChild>(
    Searcher&,
    std::span<const EndgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);
#endif

#if STELLO_HAS_X86_64_V4
template <>
uint64_t Kernel<cpu::CpuTier::V4>::flips(uint64_t own, uint64_t opponent, uint32_t square) noexcept;
template <>
uint64_t Kernel<cpu::CpuTier::V4>::legal_moves(uint64_t own, uint64_t opponent) noexcept;
template <>
void Kernel<cpu::CpuTier::V4>::dual_legal_moves(
    uint64_t own,
    uint64_t opponent,
    uint64_t& own_out,
    uint64_t& opp_out
) noexcept;
template <>
void Kernel<cpu::CpuTier::V4>::legal_and_potential(
    uint64_t own,
    uint64_t opponent,
    uint64_t& legal_out,
    int32_t& pot_out
) noexcept;
template <>
uint64_t Kernel<cpu::CpuTier::V4>::stable_of(uint64_t own, uint64_t opponent) noexcept;
template <>
uint64_t Kernel<cpu::CpuTier::V4>::transpose(uint64_t x) noexcept;
template <>
uint16_t Kernel<cpu::CpuTier::V4>::simul_last_flip(uint64_t own, uint32_t square) noexcept;
template <>
eval::PatternState Kernel<cpu::CpuTier::V4>::eval_state_of(
    const eval::PatternSet& set,
    uint64_t digit1,
    uint64_t digit2
) noexcept;
template <>
void Kernel<cpu::CpuTier::V4>::advance_eval_state(
    const eval::PatternSet& set,
    eval::PatternState& next,
    const eval::PatternState& cur,
    bool mover_is_even_disc,
    uint32_t square,
    uint64_t flips
) noexcept;
template <>
int32_t Kernel<cpu::CpuTier::V4>::sum_eval_state(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    const int32_t* packed_pair_weights,
    bool odd_slot,
    bool vpaddw_safe
) noexcept;
template <>
int32_t Kernel<cpu::CpuTier::V4>::sum_eval_after_move(
    const eval::PatternSet& set,
    const eval::PatternState& cur,
    bool mover_is_even_disc,
    uint32_t square,
    uint64_t flips,
    const int32_t* packed_child_pair_weights,
    bool child_odd_slot,
    bool vpaddw_safe
) noexcept;

extern template struct Kernel<cpu::CpuTier::V4>;
extern template AbortVoid Kernel<cpu::CpuTier::V4>::split<MidgameChild>(
    Searcher&,
    std::span<const MidgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);
extern template AbortVoid Kernel<cpu::CpuTier::V4>::split<EndgameChild>(
    Searcher&,
    std::span<const EndgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);
#endif

} // namespace stello::search

#if STELLO_HAS_X86_64_V4
#define STELLO_TIER_DISPATCH(method, ...)                                                    \
    switch (::stello::search::dispatch_tier()) {                                             \
        case ::stello::cpu::CpuTier::V4:                                                     \
            return ::stello::search::Kernel<::stello::cpu::CpuTier::V4>::method(__VA_ARGS__); \
        case ::stello::cpu::CpuTier::V3:                                                     \
            return ::stello::search::Kernel<::stello::cpu::CpuTier::V3>::method(__VA_ARGS__); \
        default:                                                                             \
            return ::stello::search::Kernel<::stello::cpu::CpuTier::Portable>::method(__VA_ARGS__); \
    }
#elif STELLO_HAS_X86_64_V3
#define STELLO_TIER_DISPATCH(method, ...)                                                    \
    if (::stello::search::dispatch_tier() == ::stello::cpu::CpuTier::V3) {                   \
        return ::stello::search::Kernel<::stello::cpu::CpuTier::V3>::method(__VA_ARGS__);     \
    }                                                                                        \
    return ::stello::search::Kernel<::stello::cpu::CpuTier::Portable>::method(__VA_ARGS__)
#else
#define STELLO_TIER_DISPATCH(method, ...) \
    return ::stello::search::Kernel<::stello::cpu::CpuTier::Portable>::method(__VA_ARGS__)
#endif
