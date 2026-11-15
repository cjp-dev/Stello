#include "search/kernel.hpp"

namespace stello::search {

AbortResult<int32_t> MidgameChild::null_window(Searcher& searcher, int32_t alpha) const {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::child_null_window(searcher, *this, alpha);
    }
#endif
    return Kernel<false>::child_null_window(searcher, *this, alpha);
}

AbortResult<int32_t> MidgameChild::window(
    Searcher& searcher,
    int32_t alpha,
    int32_t beta,
    int32_t null_score
) const {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::child_window(searcher, *this, alpha, beta, null_score);
    }
#endif
    return Kernel<false>::child_window(searcher, *this, alpha, beta, null_score);
}

AbortResult<int32_t> EndgameChild::null_window(Searcher& searcher, int32_t alpha) const {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::child_null_window(searcher, *this, alpha);
    }
#endif
    return Kernel<false>::child_null_window(searcher, *this, alpha);
}

AbortResult<int32_t> EndgameChild::window(
    Searcher& searcher,
    int32_t alpha,
    int32_t beta,
    int32_t null_score
) const {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::child_window(searcher, *this, alpha, beta, null_score);
    }
#endif
    return Kernel<false>::child_window(searcher, *this, alpha, beta, null_score);
}

void Searcher::split_task(const Helpers& h, const SplitPoint* point, const TaskSetup& setup) noexcept {
    helpers = h;
    split_point = point;
    hash_get_height = setup.hash_get_height;
    hash_put_height = setup.hash_put_height;
    in_probcut = setup.in_probcut;
    start = setup.start;
    hard_limit_ms = setup.hard_limit_ms;
}

template <typename ChildT>
AbortVoid Searcher::split(
    std::span<const ChildT> children,
    int32_t alpha,
    int32_t beta,
    int32_t& best,
    int32_t& best_move,
    bool is_root
) {
#if STELLO_HAS_X86_64_V3
    if (dispatch_v3()) {
        return Kernel<true>::split(*this, children, alpha, beta, best, best_move, is_root);
    }
#endif
    return Kernel<false>::split(*this, children, alpha, beta, best, best_move, is_root);
}

template AbortVoid Searcher::split<MidgameChild>(
    std::span<const MidgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);

template AbortVoid Searcher::split<EndgameChild>(
    std::span<const EndgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);

} // namespace stello::search
