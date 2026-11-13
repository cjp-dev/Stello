#pragma once

#include "core/bits.hpp"
#include "core/compiler.hpp"
#include "core/cpu.hpp"
#include "endgame/bits.hpp"
#include "search/engine.hpp"
#include "search/ybwc.hpp"

#include <atomic>
#include <cstdint>
#include <optional>
#include <span>

#if (defined(__x86_64__) || defined(_M_X64)) && !defined(__EMSCRIPTEN__)
#define STELLO_HAS_X86_64_V3 1
#else
#define STELLO_HAS_X86_64_V3 0
#endif

namespace stello::search {

inline bool dispatch_v3() noexcept {
#if STELLO_HAS_X86_64_V3
    return cpu::has_avx2_bmi2();
#else
    return false;
#endif
}

template <bool UseAvx2>
struct Kernel {
    static STELLO_FORCEINLINE uint64_t flips(uint64_t own, uint64_t opponent, uint32_t square) noexcept;
    static STELLO_FORCEINLINE uint64_t legal_moves(uint64_t own, uint64_t opponent) noexcept;
    static STELLO_FORCEINLINE Board play_unchecked(const Board& board, Player player, uint32_t square) noexcept;

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
        int32_t& best_move
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
        int32_t beta
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
        int32_t empties
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
        int32_t& best_move
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

extern template struct Kernel<false>;
extern template AbortVoid Kernel<false>::split<MidgameChild>(
    Searcher&,
    std::span<const MidgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&
);
extern template AbortVoid Kernel<false>::split<EndgameChild>(
    Searcher&,
    std::span<const EndgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&
);

#if STELLO_HAS_X86_64_V3
template <>
uint64_t Kernel<true>::flips(uint64_t own, uint64_t opponent, uint32_t square) noexcept;
template <>
uint64_t Kernel<true>::legal_moves(uint64_t own, uint64_t opponent) noexcept;

extern template struct Kernel<true>;
extern template AbortVoid Kernel<true>::split<MidgameChild>(
    Searcher&,
    std::span<const MidgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&
);
extern template AbortVoid Kernel<true>::split<EndgameChild>(
    Searcher&,
    std::span<const EndgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&
);
#endif

} // namespace stello::search
