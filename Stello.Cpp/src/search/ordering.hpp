#pragma once

#include "core/board.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <span>

namespace stello::search {

/// C#: `MoveOrdering`.
class MoveOrdering {
public:
    MoveOrdering();

    void clear_responses() noexcept;

    void record_response(Player replier, int32_t mv, int32_t reply, bool refuted) noexcept;

    void order(
        std::span<int32_t> moves,
        int32_t hash_move,
        int32_t previous_move,
        Player player,
        const Board& board
    ) const noexcept;

private:
    std::unique_ptr<std::array<int32_t, 2 * 64 * 64>> responses_;
};

std::array<int8_t, 64> square_scores(const Board& board, Player player) noexcept;

/// Stable sort, highest key first (branchless rank sort matching Rust `sort_descending`).
void sort_descending(std::span<int32_t> moves, std::span<int32_t> keys) noexcept;

} // namespace stello::search
