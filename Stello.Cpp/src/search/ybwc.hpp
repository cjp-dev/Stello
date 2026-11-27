#pragma once

#include "core/board.hpp"
#include "endgame/bits.hpp"
#include "eval/patterns.hpp"
#include "search/abort.hpp"
#include "search/engine.hpp"

#include <atomic>
#include <cstdint>

namespace stello::search {

/// A node whose younger brothers are searched in parallel; stopped on a cutoff or parent stop.
struct SplitPoint {
    static constexpr uint8_t STOP_BIT = 1;
    static constexpr uint8_t CUT_BIT = 2;

    std::atomic<uint8_t> state{0};
    const SplitPoint* parent{nullptr};

    void request_stop() noexcept { state.fetch_or(STOP_BIT, std::memory_order_relaxed); }

    void request_cut() noexcept { state.fetch_or(STOP_BIT | CUT_BIT, std::memory_order_relaxed); }

    bool is_stopped() const noexcept { return (state.load(std::memory_order_relaxed) & STOP_BIT) != 0; }

    bool is_cut() const noexcept { return (state.load(std::memory_order_relaxed) & CUT_BIT) != 0; }

    bool stopped() const noexcept {
        const SplitPoint* point = this;
        while (point != nullptr) {
            if (point->is_stopped()) {
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
    const eval::PatternState* parent_eval{nullptr};
    uint64_t move_flips{0};
    uint64_t known_legal{endgame::bits::LEGAL_UNDEFINED};
    bool parent_is_even{false};

    AbortResult<int32_t> null_window(Searcher& searcher, int32_t alpha) const;
    AbortResult<int32_t> window(Searcher& searcher, int32_t alpha, int32_t beta, int32_t null_score) const;
    int32_t square() const noexcept { return mv; }
    int32_t priority() const noexcept { return look; }
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
    int32_t priority() const noexcept { return empties; }
};

/// State copied by a worker when branching at a split point.
struct TaskSetup {
    int32_t hash_get_height{0};
    int32_t hash_put_height{0};
    bool in_probcut{false};
    int32_t end_mpc_stage{0};
    Duration start{Duration::zero()};
    int64_t hard_limit_ms{0};
};

/// Packs `(move, score)` into one 64-bit atomic word so heartbeats never read a torn pair.
inline uint64_t pack_live_best(int32_t mv, int32_t score) noexcept {
    return (static_cast<uint64_t>(static_cast<uint32_t>(score)) << 32)
        | static_cast<uint64_t>(static_cast<uint32_t>(mv));
}

inline void unpack_live_best(uint64_t packed, int32_t& mv, int32_t& score) noexcept {
    mv = static_cast<int32_t>(static_cast<uint32_t>(packed & 0xFFFF'FFFFULL));
    score = static_cast<int32_t>(static_cast<uint32_t>(packed >> 32));
}

} // namespace stello::search
