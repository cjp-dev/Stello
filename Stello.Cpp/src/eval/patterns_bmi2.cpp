#include "eval/patterns.hpp"

#include "core/compiler.hpp"

#if (defined(__x86_64__) || defined(_M_X64)) && !defined(__EMSCRIPTEN__)
#include <immintrin.h>
#define STELLO_HAS_BMI2_INTRIN 1
#else
#define STELLO_HAS_BMI2_INTRIN 0
#endif

namespace stello::eval::detail {

// Isolated in its own translation unit (compiled with -mbmi2 on Clang) so baseline x86-64 builds
// never emit BMI2 instructions outside this dynamically-dispatched function.
STELLO_NOINLINE int32_t Direct::sum_pext(uint64_t own, uint64_t opponent, const int16_t* weights) const noexcept {
#if STELLO_HAS_BMI2_INTRIN
    int32_t sum = 0;
    const uint16_t* pext_tbl = pext_tables_.data();
    for (const auto& feature : pext_) {
        const uint16_t* tbl = pext_tbl + feature.table;
        const uint16_t own_digits = tbl[_pext_u64(own, feature.mask)];
        const uint16_t opp_digits = tbl[_pext_u64(opponent, feature.mask)];
        const uint32_t index = feature.offset + static_cast<uint32_t>(own_digits) + 2U * static_cast<uint32_t>(opp_digits);
        sum += weights[index];
    }
    return sum;
#else
    return sum_portable(own, opponent, {weights, SIZE_MAX});
#endif
}

} // namespace stello::eval::detail
