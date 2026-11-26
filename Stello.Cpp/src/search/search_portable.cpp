#include "search/kernel.hpp"
#include "search/kernel_impl.hpp"
#include "search/microbench_impl.hpp"

#if STELLO_HAS_X86_64_V3
namespace stello::search::detail::microbench {
extern template std::vector<MicrobenchStats> run_kernel<cpu::CpuTier::V3>(const MicrobenchOptions& opts);
} // namespace stello::search::detail::microbench
#endif
#if STELLO_HAS_X86_64_V4
namespace stello::search::detail::microbench {
extern template std::vector<MicrobenchStats> run_kernel<cpu::CpuTier::V4>(const MicrobenchOptions& opts);
} // namespace stello::search::detail::microbench
#endif

#include "endgame/last_impl.hpp"
#include "endgame/search_impl.hpp"
#include "search/engine_impl.hpp"
#include "search/ybwc_impl.hpp"

namespace stello::search {

namespace detail {

bool fill_end_moves_for_test(uint64_t own, uint64_t opponent, uint64_t legal, std::span<EndMove> moves) noexcept {
    return endgame_search::fill_end_moves<cpu::CpuTier::Portable>(own, opponent, legal, moves);
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

template struct Kernel<cpu::CpuTier::Portable>;

template AbortVoid Kernel<cpu::CpuTier::Portable>::split<MidgameChild>(
    Searcher&,
    std::span<const MidgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&,
    bool
);

template AbortVoid Kernel<cpu::CpuTier::Portable>::split<EndgameChild>(
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
        "flips_smartclr",
        "flips_gfni",
        "last_flip_count",
        "solve_1",
        "legal_moves",
        "potential_mobility",
        "legal_and_potential",
        "dual_legal_moves",
        "stable_of",
        "transpose",
        "horizontal_mirror",
        "hash_board",
        "eval_state_of",
        "eval_sum",
        "eval_after_move",
        "eval_advance",
    };
}

std::vector<MicrobenchStats> run_microbench(const MicrobenchOptions& opts, cpu::CpuTier tier) {
#if STELLO_HAS_X86_64_V4
    if (tier == cpu::CpuTier::V4) {
        return detail::microbench::run_kernel<cpu::CpuTier::V4>(opts);
    }
#endif
#if STELLO_HAS_X86_64_V3
    if (tier == cpu::CpuTier::V3) {
        return detail::microbench::run_kernel<cpu::CpuTier::V3>(opts);
    }
#endif
    (void)tier;
    return detail::microbench::run_kernel<cpu::CpuTier::Portable>(opts);
}

} // namespace stello::search
