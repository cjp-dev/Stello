#include "search/kernel.hpp"

namespace stello::search {

AbortResult<int32_t> MidgameChild::null_window(Searcher& searcher, int32_t alpha) const {
    STELLO_TIER_DISPATCH(child_null_window, searcher, *this, alpha);
}

AbortResult<int32_t> MidgameChild::window(
    Searcher& searcher,
    int32_t alpha,
    int32_t beta,
    int32_t null_score
) const {
    STELLO_TIER_DISPATCH(child_window, searcher, *this, alpha, beta, null_score);
}

AbortResult<int32_t> EndgameChild::null_window(Searcher& searcher, int32_t alpha) const {
    STELLO_TIER_DISPATCH(child_null_window, searcher, *this, alpha);
}

AbortResult<int32_t> EndgameChild::window(
    Searcher& searcher,
    int32_t alpha,
    int32_t beta,
    int32_t null_score
) const {
    STELLO_TIER_DISPATCH(child_window, searcher, *this, alpha, beta, null_score);
}

void Searcher::split_task(const Helpers& h, const SplitPoint* point, const TaskSetup& setup) noexcept {
    helpers = h;
    split_point = point;
    hash_get_height = setup.hash_get_height;
    hash_put_height = setup.hash_put_height;
    in_probcut = setup.in_probcut;
    end_mpc_stage = setup.end_mpc_stage;
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
    STELLO_TIER_DISPATCH(split, *this, children, alpha, beta, best, best_move, is_root);
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
