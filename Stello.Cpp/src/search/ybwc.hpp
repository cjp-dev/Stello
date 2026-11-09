#pragma once

#include "core/board.hpp"
#include "search/abort.hpp"
#include "search/engine.hpp"

#include <atomic>
#include <cstdint>

namespace stello::search {

/// A node whose younger brothers are searched in parallel; stopped on a cutoff or parent stop.
struct SplitPoint {
    std::atomic<bool> stop{false};
    std::atomic<bool> cut{false};
    const SplitPoint* parent{nullptr};

    bool stopped() const noexcept {
        const SplitPoint* point = this;
        while (point != nullptr) {
            if (point->stop.load(std::memory_order_relaxed)) {
                return true;
            }
            point = point->parent;
        }
        return false;
    }
};

/// A midgame child node queued for YBWC split evaluation.
struct MidgameChild {
    Board board{};
    Player player{Player::Black};
    int32_t look{0};
    int32_t ply{0};
    int32_t mv{0};

    AbortResult<int32_t> null_window(Searcher& searcher, int32_t alpha) const;
    AbortResult<int32_t> window(Searcher& searcher, int32_t alpha, int32_t beta, int32_t null_score) const;
    int32_t square() const noexcept { return mv; }
};

/// An endgame child node queued for YBWC split evaluation.
struct EndgameChild {
    uint64_t own{0};
    uint64_t opponent{0};
    int32_t empties{0};
    uint64_t replies{0};
    int32_t mv{0};

    AbortResult<int32_t> null_window(Searcher& searcher, int32_t alpha) const;
    AbortResult<int32_t> window(Searcher& searcher, int32_t alpha, int32_t beta, int32_t null_score) const;
    int32_t square() const noexcept { return mv; }
};

/// State copied by a worker when branching at a split point.
struct TaskSetup {
    int32_t hash_get_height{0};
    int32_t hash_put_height{0};
    bool in_probcut{false};
    Duration start{Duration::zero()};
    int64_t hard_limit_ms{0};
};

} // namespace stello::search
