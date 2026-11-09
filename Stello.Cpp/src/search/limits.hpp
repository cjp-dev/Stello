#pragma once

#include "core/square.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>

namespace stello {

using Duration = std::chrono::nanoseconds;

/// C#: `TimeControlMode`. The discriminants are the C# enum values.
enum class TimeControlMode : uint8_t {
    FixedDepth = 0,
    TimePerMove = 1,
    TimePerGame = 2,
    Solve = 3,
};

/// C#: `SearchLimits`, without `UseOpeningBook` and `HashBits`.
class SearchLimits {
public:
    static constexpr uint32_t MAX_THREADS = 256;

    constexpr TimeControlMode mode() const noexcept { return mode_; }
    constexpr int32_t depth() const noexcept { return depth_; }
    constexpr int64_t time_ms() const noexcept { return time_ms_; }
    constexpr uint32_t threads() const noexcept { return threads_; }

    static constexpr SearchLimits solve() noexcept {
        return SearchLimits(TimeControlMode::Solve, 0, 0, 1);
    }

    static inline SearchLimits fixed_depth(int32_t plies) noexcept {
        assert(plies >= 1 && plies <= 60 && "a fixed depth is 1-60 plies");
        return SearchLimits(TimeControlMode::FixedDepth, plies, 0, 1);
    }

    static inline SearchLimits time_per_move_ms(int64_t time_ms) noexcept {
        assert(time_ms > 0 && "the time per move must be positive");
        return SearchLimits(TimeControlMode::TimePerMove, 0, time_ms, 1);
    }

    static constexpr SearchLimits time_per_game_ms(int64_t remaining_ms) noexcept {
        return SearchLimits(TimeControlMode::TimePerGame, 0, remaining_ms, 1);
    }

    static inline std::optional<SearchLimits> from_parts(TimeControlMode mode, int32_t depth, int64_t time_ms) noexcept {
        switch (mode) {
            case TimeControlMode::FixedDepth:
                if (depth >= 1 && depth <= 60) {
                    return SearchLimits(mode, depth, 0, 1);
                }
                return std::nullopt;
            case TimeControlMode::TimePerMove:
                if (time_ms >= 0) {
                    return SearchLimits(mode, 0, time_ms, 1);
                }
                return std::nullopt;
            case TimeControlMode::TimePerGame:
                return SearchLimits(mode, 0, time_ms, 1);
            case TimeControlMode::Solve:
                return SearchLimits::solve();
        }
        return std::nullopt;
    }

    inline std::optional<SearchLimits> with_threads(uint32_t threads) const noexcept {
        if (threads >= 1 && threads <= MAX_THREADS) {
            return SearchLimits(mode_, depth_, time_ms_, threads);
        }
        return std::nullopt;
    }

    friend constexpr bool operator==(const SearchLimits&, const SearchLimits&) noexcept = default;

private:
    constexpr SearchLimits(TimeControlMode mode, int32_t depth, int64_t time_ms, uint32_t threads) noexcept
        : mode_(mode), depth_(depth), time_ms_(time_ms), threads_(threads) {}

    TimeControlMode mode_{TimeControlMode::Solve};
    int32_t depth_{0};
    int64_t time_ms_{0};
    uint32_t threads_{1};
};

/// C#: `ScoreKind`. The discriminants are the C# enum values.
enum class ScoreKind : uint8_t {
    None = 0,
    Heuristic = 1,
    WinLossDraw = 2,
    Exact = 3,
    Book = 4,
};

inline constexpr std::string_view score_kind_name(ScoreKind kind) noexcept {
    switch (kind) {
        case ScoreKind::None: return "None";
        case ScoreKind::Heuristic: return "Heuristic";
        case ScoreKind::WinLossDraw: return "WinLossDraw";
        case ScoreKind::Exact: return "Exact";
        case ScoreKind::Book: return "Book";
    }
    return "None";
}

/// Progress from a running search. C#: `SearchInfo`.
struct SearchInfo {
    int32_t depth{0};
    Square current_move{};
    std::optional<Square> best_move;
    int32_t score{0};
    ScoreKind kind{ScoreKind::None};
    uint64_t nodes{0};
    uint64_t evaluations{0};
    Duration elapsed{Duration::zero()};
};

/// Final result of a search. C#: `SearchResult`.
struct SearchResult {
    Move mv{};
    int32_t score{0};
    ScoreKind kind{ScoreKind::None};
    int32_t depth{0};
    uint64_t nodes{0};
    uint64_t evaluations{0};
    Duration elapsed{Duration::zero()};
};

} // namespace stello
