#pragma once

#include "eval/weights.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace stello::eval {

inline constexpr int32_t PATTERN_UNITS_PER_DISC = 100;

inline int32_t clamp_pattern_score(const EvalWeights& weights, int32_t raw_sum) noexcept {
    const int32_t raw = raw_sum * PATTERN_UNITS_PER_DISC / weights.units_per_disc();
    return std::clamp(raw, -64 * PATTERN_UNITS_PER_DISC, 64 * PATTERN_UNITS_PER_DISC);
}

/// Minimum `raw_sum` for which `-clamp_pattern_score(weights, raw_sum) <= best` holds.
/// Returns `INT32_MAX / 2` when no `raw_sum` can satisfy the inequality (`best < -64 * 100`).
inline int32_t raw_skip_threshold(int32_t best, int32_t units_per_disc) noexcept {
    if (best < -64 * PATTERN_UNITS_PER_DISC) {
        return (std::numeric_limits<int32_t>::max)() / 2;
    }
    if (best >= 64 * PATTERN_UNITS_PER_DISC) {
        return (std::numeric_limits<int32_t>::min)() / 2;
    }
    const int32_t target = -best;
    if (target > 0) {
        return (target * units_per_disc + PATTERN_UNITS_PER_DISC - 1) / PATTERN_UNITS_PER_DISC;
    }
    return -(((-target + 1) * units_per_disc - 1) / PATTERN_UNITS_PER_DISC);
}

inline int32_t pattern_score(const EvalWeights& weights, uint64_t own, uint64_t opponent) noexcept {
    return clamp_pattern_score(weights, weights.evaluate(own, opponent));
}

inline int32_t disc_score(const EvalWeights& weights, uint64_t own, uint64_t opponent) noexcept {
    const int32_t raw = weights.evaluate(own, opponent) / weights.units_per_disc();
    return std::clamp(raw, -64, 64);
}

} // namespace stello::eval
