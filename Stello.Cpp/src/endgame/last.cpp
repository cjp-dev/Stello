#include "search/kernel.hpp"

namespace stello::search {

AbortResult<int32_t> Searcher::last4(uint64_t own, uint64_t opponent, int32_t alpha) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::last4(*this, own, opponent, alpha);
    }
#endif
    return Kernel<false>::last4(*this, own, opponent, alpha);
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
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::last3(*this, own, opponent, alpha, p0, p1, p2, empties4);
    }
#endif
    return Kernel<false>::last3(*this, own, opponent, alpha, p0, p1, p2, empties4);
}

AbortResult<int32_t> Searcher::last2(uint64_t own, uint64_t opponent, int32_t alpha, uint32_t p0, uint32_t p1) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::last2(*this, own, opponent, alpha, p0, p1);
    }
#endif
    return Kernel<false>::last2(*this, own, opponent, alpha, p0, p1);
}

AbortResult<int32_t> Searcher::last1(uint64_t own, int32_t alpha, uint32_t square) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::last1(*this, own, alpha, square);
    }
#endif
    return Kernel<false>::last1(*this, own, alpha, square);
}

} // namespace stello::search
