#pragma once

#include "eval/weights.hpp"

#include <algorithm>
#include <cstdint>

namespace stello::eval {

inline constexpr int32_t PATTERN_UNITS_PER_DISC = 100;

inline int32_t clamp_pattern_score(const EvalWeights& weights, int32_t raw_sum) noexcept {
    const int32_t raw = raw_sum * PATTERN_UNITS_PER_DISC / weights.units_per_disc();
    return std::clamp(raw, -64 * PATTERN_UNITS_PER_DISC, 64 * PATTERN_UNITS_PER_DISC);
}

inline int32_t pattern_score(const EvalWeights& weights, uint64_t own, uint64_t opponent) noexcept {
    return clamp_pattern_score(weights, weights.evaluate(own, opponent));
}

inline int32_t disc_score(const EvalWeights& weights, uint64_t own, uint64_t opponent) noexcept {
    const int32_t raw = weights.evaluate(own, opponent) / weights.units_per_disc();
    return std::clamp(raw, -64, 64);
}

} // namespace stello::eval
