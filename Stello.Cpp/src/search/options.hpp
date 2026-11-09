#pragma once

#include "eval/weights.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace stello {

/// C#: `SearchOptions`.
struct SearchOptions {
    /// Another pattern evaluation, or `nullptr` for the built-in `EvalWeights::built_in()`.
    std::shared_ptr<const eval::EvalWeights> pattern;

    /// No new iteration after this share (1-100) of the hard time limit (`std::nullopt`: 2/3).
    std::optional<int32_t> soft_time_percent;

    /// Multi-ProbCut; off only to measure plain searches.
    bool selective{true};

    /// Multi-ProbCut's t * 100 (`std::nullopt`: DEFAULT_PROBCUT_PERCENT).
    std::optional<int32_t> probcut_percent;

    /// With more than 1 thread: YBWC + Lazy SMP midgame helpers; false for Lazy SMP alone.
    bool ybwc{true};
};

} // namespace stello
