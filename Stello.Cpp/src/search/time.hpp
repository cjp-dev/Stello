#pragma once

#include "search/limits.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

namespace stello::time {

inline constexpr int64_t FAST_ENDGAME_EMPTIES = 6;
inline constexpr int64_t MAX_TIME_MS = (std::numeric_limits<int64_t>::max)() / 1024;

inline constexpr int64_t clamp_time_ms(int64_t ms) noexcept {
    return (std::clamp)(ms, int64_t{0}, MAX_TIME_MS);
}

inline constexpr int64_t clamp_add_ms(int64_t a, int64_t b) noexcept {
    const int64_t max_val = (std::numeric_limits<int64_t>::max)();
    if (a >= max_val - b) {
        return MAX_TIME_MS;
    }
    return clamp_time_ms(a + b);
}

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
            const int64_t pct = (std::clamp)(static_cast<int64_t>(*percent), int64_t{0}, int64_t{100});
            const int64_t clamped_offset = (std::clamp)(offset_ms, int64_t{0}, hard_ms);
            const int64_t span = (std::min)(hard_ms - clamped_offset, MAX_TIME_MS);
            return {
                clamp_add_ms(clamped_offset, span * pct / 100),
                hard_ms,
            };
        }
        return *this;
    }

    friend constexpr bool operator==(const TimeBudget&, const TimeBudget&) noexcept = default;
};

inline TimeBudget for_game(int64_t remaining_ms, int32_t empties) noexcept {
    const int64_t slow_empties = (std::max)(static_cast<int64_t>(empties) - FAST_ENDGAME_EMPTIES, int64_t{0});
    const int64_t moves_left = slow_empties >> 1;
    if (remaining_ms < 0) {
        remaining_ms = 10;
    } else {
        remaining_ms = clamp_time_ms(remaining_ms);
    }

    int64_t move_time = (moves_left > 0) ? (remaining_ms / moves_left) : remaining_ms;
    move_time = (move_time >> 2) + 3 * (move_time / 256) * (64 - slow_empties)
        + 3 * (move_time % 256) * (64 - slow_empties) / 256;
    return {move_time / 15 * 10 + (move_time % 15) * 10 / 15, move_time};
}

inline TimeBudget for_midgame(const SearchLimits& limits, int32_t empties) noexcept {
    switch (limits.mode()) {
        case TimeControlMode::TimePerMove: {
            const int64_t ms = clamp_time_ms(limits.time_ms());
            return {ms / 3 * 2 + (ms % 3) * 2 / 3, ms};
        }
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

    const int64_t clamped_elapsed = clamp_time_ms(elapsed_ms);
    int64_t rest = 10;
    if (limits.time_ms() >= 0) {
        const int64_t clamped_limit = clamp_time_ms(limits.time_ms());
        rest = (clamped_limit < clamped_elapsed) ? 10 : (clamped_limit - clamped_elapsed);
    }

    const int64_t move_time = rest / 2;
    return {
        clamp_add_ms(clamped_elapsed, move_time / 15 * 10 + (move_time % 15) * 10 / 15),
        clamp_add_ms(clamped_elapsed, move_time),
    };
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
