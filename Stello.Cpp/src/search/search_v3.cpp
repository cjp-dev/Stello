#include "search/kernel.hpp"

#if STELLO_HAS_X86_64_V3

#include "endgame/bits_avx2.hpp"

namespace stello::search {

template <>
STELLO_FORCEINLINE uint64_t Kernel<true>::flips(
    uint64_t own,
    uint64_t opponent,
    uint32_t square
) noexcept {
    return endgame::bits::flips_avx2(own, opponent, square);
}

template <>
STELLO_FORCEINLINE uint64_t Kernel<true>::legal_moves(uint64_t own, uint64_t opponent) noexcept {
    return endgame::bits::legal_moves_avx2(own, opponent);
}

} // namespace stello::search

#include "search/kernel_impl.hpp"

#include "endgame/last_impl.hpp"
#include "endgame/search_impl.hpp"
#include "search/engine_impl.hpp"
#include "search/ybwc_impl.hpp"

namespace stello::search {

// Isolated in its own x86-64-v3 translation unit so baseline x86-64 builds never execute
// AVX2/BMI1/BMI2/POPCNT/LZCNT instructions outside this CPUID-dispatched specialization.
template struct Kernel<true>;

template AbortVoid Kernel<true>::split<MidgameChild>(
    Searcher&,
    std::span<const MidgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&
);

template AbortVoid Kernel<true>::split<EndgameChild>(
    Searcher&,
    std::span<const EndgameChild>,
    int32_t,
    int32_t,
    int32_t&,
    int32_t&
);

} // namespace stello::search

#endif
