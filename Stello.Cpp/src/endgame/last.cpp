#include "search/kernel.hpp"

namespace stello::search {

AbortResult<int32_t> Searcher::last4(uint64_t own, uint64_t opponent, int32_t alpha) {
    STELLO_TIER_DISPATCH(last4, *this, own, opponent, alpha);
}

AbortResult<int32_t> Searcher::last3(
    uint64_t own,
    uint64_t opponent,
    int32_t alpha,
    uint32_t p0,
    uint32_t p1,
    uint32_t p2,
    uint64_t empties4
) {
    STELLO_TIER_DISPATCH(last3, *this, own, opponent, alpha, p0, p1, p2, empties4);
}

AbortResult<int32_t> Searcher::last2(uint64_t own, uint64_t opponent, int32_t alpha, uint32_t p0, uint32_t p1) {
    STELLO_TIER_DISPATCH(last2, *this, own, opponent, alpha, p0, p1);
}

AbortResult<int32_t> Searcher::last1(uint64_t own, int32_t alpha, uint32_t square) {
    STELLO_TIER_DISPATCH(last1, *this, own, alpha, square);
}

} // namespace stello::search
