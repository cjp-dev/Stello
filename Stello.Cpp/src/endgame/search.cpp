#include "search/kernel.hpp"

namespace stello::search {

AbortResult<int32_t> Searcher::solve_child(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t beta,
    int32_t empties
) {
    STELLO_TIER_DISPATCH(solve_child, *this, own, opponent, alpha, beta, empties);
}

AbortResult<int32_t> Searcher::end_null(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t empties,
    uint64_t legal
) {
    STELLO_TIER_DISPATCH(end_null, *this, own, opponent, alpha, empties, legal);
}

AbortResult<int32_t> Searcher::end_window(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t beta,
    int32_t empties,
    bool passed
) {
    STELLO_TIER_DISPATCH(end_window, *this, own, opponent, alpha, beta, empties, passed);
}

AbortResult<int32_t> Searcher::end_deep(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t empties,
    bool passed,
    uint64_t legal
) {
    STELLO_TIER_DISPATCH(end_deep, *this, own, opponent, alpha, empties, passed, legal);
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
    STELLO_TIER_DISPATCH(split_endgame, *this, own, opponent, empties, moves, from, done, alpha, beta, best, best_move);
}

AbortResult<int32_t> Searcher::ordering_value(
    uint64_t own,
    uint64_t opponent,
    int32_t depth,
    int32_t alpha,
    int32_t beta,
    uint64_t known_legal
) {
    STELLO_TIER_DISPATCH(ordering_value, *this, own, opponent, depth, alpha, beta, known_legal);
}

AbortResult<int32_t> Searcher::end_local(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    bool passed,
    uint64_t legal,
    int32_t empties
) {
    STELLO_TIER_DISPATCH(end_local, *this, own, opponent, alpha, passed, legal, empties);
}

AbortResult<int32_t> Searcher::end_simple(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    bool passed,
    uint64_t legal,
    int32_t empties
) {
    STELLO_TIER_DISPATCH(end_simple, *this, own, opponent, alpha, passed, legal, empties);
}

AbortResult<int32_t> Searcher::end_fast(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    bool passed,
    int32_t empties
) {
    STELLO_TIER_DISPATCH(end_fast, *this, own, opponent, alpha, passed, empties);
}

AbortResult<int32_t> Searcher::solve_shallow(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t beta,
    int32_t empties,
    bool passed
) {
    STELLO_TIER_DISPATCH(solve_shallow, *this, own, opponent, alpha, beta, empties, passed);
}

AbortResult<int32_t> Searcher::solve_last2(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    int32_t beta,
    uint64_t empty,
    bool passed
) {
    STELLO_TIER_DISPATCH(solve_last2, *this, own, opponent, alpha, beta, empty, passed);
}

int32_t Searcher::solve_last(uint64_t own, uint64_t opponent, uint64_t empty) noexcept {
    STELLO_TIER_DISPATCH(solve_last, *this, own, opponent, empty);
}

} // namespace stello::search
