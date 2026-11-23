#include "search/kernel.hpp"
#include "search/kernel_impl.hpp"
#include "search/microbench_impl.hpp"

#if STELLO_HAS_X86_64_V3
namespace stello::search::detail::microbench {
extern template std::vector<MicrobenchStats> run_kernel<true>(const MicrobenchOptions& opts);
} // namespace stello::search::detail::microbench
#endif

#include "endgame/last_impl.hpp"
#include "endgame/search_impl.hpp"
#include "search/engine_impl.hpp"
#include "search/ybwc_impl.hpp"

namespace stello::search {

namespace detail {

bool fill_end_moves_for_test(uint64_t own, uint64_t opponent, uint64_t legal, std::span<EndMove> moves) noexcept {
    return endgame_search::fill_end_moves<false>(own, opponent, legal, moves);
}

size_t undone_first_for_test(std::span<EndMove> moves, uint64_t done) noexcept {
    return endgame_search::undone_first(moves, done);
}

void swap_next_best_for_test(std::span<EndMove> moves, size_t start) noexcept {
    endgame_search::swap_next_best(moves, start);
}

int32_t last_flip_count_for_test(uint64_t own, uint32_t square) noexcept {
    return last::last_flip_count(own, square);
}

} // namespace detail

template struct Kernel<false>;

template AbortVoid Kernel<false>::split<MidgameChild>(
    Searcher&,
    std::span<const MidgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);

template AbortVoid Kernel<false>::split<EndgameChild>(
    Searcher&,
    std::span<const EndgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);

std::vector<std::string_view> microbench_names() {
    return {
        "flips",
        "last_flip_count",
        "solve_1",
        "legal_moves",
        "potential_mobility",
        "legal_and_potential",
        "stable_of",
        "eval_sum",
        "eval_after_move",
        "eval_advance",
    };
}

std::vector<MicrobenchStats> run_microbench(const MicrobenchOptions& opts, bool use_avx2) {
#if STELLO_HAS_X86_64_V3
    if (use_avx2) {
        return detail::microbench::run_kernel<true>(opts);
    }
#else
    (void)use_avx2;
#endif
    return detail::microbench::run_kernel<false>(opts);
}

} // namespace stello::search
