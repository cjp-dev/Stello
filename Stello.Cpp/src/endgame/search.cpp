#include "search/kernel.hpp"

namespace stello::search {

AbortResult<int32_t> Searcher::solve_child(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t beta,
    int32_t empties
) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::solve_child(*this, own, opponent, alpha, beta, empties);
    }
#endif
    return Kernel<false>::solve_child(*this, own, opponent, alpha, beta, empties);
}

AbortResult<int32_t> Searcher::end_null(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t empties,
    uint64_t legal
) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::end_null(*this, own, opponent, alpha, empties, legal);
    }
#endif
    return Kernel<false>::end_null(*this, own, opponent, alpha, empties, legal);
}

AbortResult<int32_t> Searcher::end_window(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t beta,
    int32_t empties,
    bool passed
) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::end_window(*this, own, opponent, alpha, beta, empties, passed);
    }
#endif
    return Kernel<false>::end_window(*this, own, opponent, alpha, beta, empties, passed);
}

AbortResult<int32_t> Searcher::end_deep(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t empties,
    bool passed,
    uint64_t legal
) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::end_deep(*this, own, opponent, alpha, empties, passed, legal);
    }
#endif
    return Kernel<false>::end_deep(*this, own, opponent, alpha, empties, passed, legal);
}

AbortVoid Searcher::split_endgame(
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
) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::split_endgame(*this, own, opponent, empties, moves, from, done, alpha, beta, best, best_move);
    }
#endif
    return Kernel<false>::split_endgame(*this, own, opponent, empties, moves, from, done, alpha, beta, best, best_move);
}

AbortResult<int32_t> Searcher::ordering_value(
    uint64_t own,
    uint64_t opponent,
    int32_t depth,
    int32_t alpha,
    int32_t beta
) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::ordering_value(*this, own, opponent, depth, alpha, beta);
    }
#endif
    return Kernel<false>::ordering_value(*this, own, opponent, depth, alpha, beta);
}

AbortResult<int32_t> Searcher::end_local(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    bool passed,
    uint64_t legal,
    int32_t empties
) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::end_local(*this, own, opponent, alpha, passed, legal, empties);
    }
#endif
    return Kernel<false>::end_local(*this, own, opponent, alpha, passed, legal, empties);
}

AbortResult<int32_t> Searcher::end_simple(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    bool passed,
    uint64_t legal,
    int32_t empties
) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::end_simple(*this, own, opponent, alpha, passed, legal, empties);
    }
#endif
    return Kernel<false>::end_simple(*this, own, opponent, alpha, passed, legal, empties);
}

AbortResult<int32_t> Searcher::end_fast(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    bool passed,
    int32_t empties
) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::end_fast(*this, own, opponent, alpha, passed, empties);
    }
#endif
    return Kernel<false>::end_fast(*this, own, opponent, alpha, passed, empties);
}

AbortResult<int32_t> Searcher::solve_shallow(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t beta,
    int32_t empties,
    bool passed
) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::solve_shallow(*this, own, opponent, alpha, beta, empties, passed);
    }
#endif
    return Kernel<false>::solve_shallow(*this, own, opponent, alpha, beta, empties, passed);
}

AbortResult<int32_t> Searcher::solve_last2(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t beta,
    uint64_t empty,
    bool passed
) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::solve_last2(*this, own, opponent, alpha, beta, empty, passed);
    }
#endif
    return Kernel<false>::solve_last2(*this, own, opponent, alpha, beta, empty, passed);
}

int32_t Searcher::solve_last(uint64_t own, uint64_t opponent, uint64_t empty) noexcept {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::solve_last(*this, own, opponent, empty);
    }
#endif
    return Kernel<false>::solve_last(*this, own, opponent, empty);
}

} // namespace stello::search
