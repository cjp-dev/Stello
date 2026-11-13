#include "search/kernel.hpp"
#include "search/kernel_impl.hpp"

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
    int32_t&
);

template AbortVoid Kernel<false>::split<EndgameChild>(
    Searcher&,
    std::span<const EndgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&
);

} // namespace stello::search
