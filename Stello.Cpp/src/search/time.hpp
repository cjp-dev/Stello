#pragma once

#include "search/limits.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

namespace stello::time {

/// C#: `TimeBudget`. Milliseconds since the search started; `INT64_MAX` is no limit.
struct TimeBudget {
    int64_t soft_ms;
    int64_t hard_ms;

    static constexpr TimeBudget unlimited() noexcept {
        return {
            (std::numeric_limits<int64_t>::max)(),
            (std::numeric_limits<int64_t>::max)(),
        };
    }

    constexpr TimeBudget with_soft_percent(std::optional<int32_t> percent, int64_t offset_ms) const noexcept {
        if (percent.has_value() && hard_ms != (std::numeric_limits<int64_t>::max)()) {
            return {
                offset_ms + (hard_ms - offset_ms) * static_cast<int64_t>(*percent) / 100,
                hard_ms,
            };
        }
        return *this;
    }

    friend constexpr bool operator==(const TimeBudget&, const TimeBudget&) noexcept = default;
};

inline constexpr int64_t FAST_ENDGAME_EMPTIES = 6;

inline TimeBudget for_game(int64_t remaining_ms, int32_t empties) noexcept {
    const int64_t slow_empties = (std::max)(static_cast<int64_t>(empties) - FAST_ENDGAME_EMPTIES, int64_t{0});
    const int64_t moves_left = slow_empties >> 1;
    if (remaining_ms < 0) {
        remaining_ms = 10;
    }

    int64_t move_time = (moves_left > 0) ? (remaining_ms / moves_left) : remaining_ms;
    move_time = (move_time >> 2) + 3 * move_time * (64 - slow_empties) / 256;
    return {move_time * 10 / 15, move_time};
}

inline TimeBudget for_midgame(const SearchLimits& limits, int32_t empties) noexcept {
    switch (limits.mode()) {
        case TimeControlMode::TimePerMove:
            return {limits.time_ms() * 2 / 3, limits.time_ms()};
        case TimeControlMode::TimePerGame:
            return for_game(limits.time_ms(), empties);
        default:
            return TimeBudget::unlimited();
    }
}

inline TimeBudget for_endgame(const SearchLimits& limits, TimeBudget midgame, int64_t elapsed_ms) noexcept {
    if (limits.mode() != TimeControlMode::TimePerGame) {
        return midgame;
    }

    int64_t rest = limits.time_ms() - elapsed_ms;
    if (rest < 0) {
        rest = 10;
    }

    const int64_t move_time = rest / 2;
    return {elapsed_ms + move_time * 10 / 15, elapsed_ms + move_time};
}

inline TimeBudget for_solver(
    const SearchLimits& limits,
    TimeBudget midgame,
    int64_t elapsed_ms,
    std::optional<int32_t> soft_percent
) noexcept {
    const int64_t offset_ms = (limits.mode() == TimeControlMode::TimePerGame) ? elapsed_ms : 0;
    return for_endgame(limits, midgame, elapsed_ms).with_soft_percent(soft_percent, offset_ms);
}

} // namespace stello::time
