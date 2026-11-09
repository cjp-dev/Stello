#include "search/ordering.hpp"

#include "core/compiler.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <utility>

namespace stello::search {

namespace {

constexpr std::array<int8_t, 64> BASE_SQUARE_SCORES = {
    127, -32, 22, 11, 11, 22, -32, 127,
    -32, -64, -8, -5, -5, -8, -64, -32,
     22,  -8, 12,  1,  1, 12,  -8,  22,
     11,  -5,  1,  0,  0,  1,  -5,  11,
     11,  -5,  1,  0,  0,  1,  -5,  11,
     22,  -8, 12,  1,  1, 12,  -8,  22,
    -32, -64, -8, -5, -5, -8, -64, -32,
    127, -32, 22, 11, 11, 22, -32, 127,
};

struct CornerArea {
    size_t corner;
    size_t c1;
    size_t beyond1;
    size_t c2;
    size_t beyond2;
    size_t x;
};

constexpr std::array<CornerArea, 4> CORNER_AREAS = {{
    {0, 1, 2, 8, 16, 9},
    {7, 6, 5, 15, 23, 14},
    {56, 48, 40, 57, 58, 49},
    {63, 62, 61, 55, 47, 54},
}};

inline size_t response_row(Player replier, int32_t previous_move) noexcept {
    assert(previous_move >= 0 && previous_move < 64);
    return (player_index(replier) * 64 + static_cast<size_t>(previous_move)) * 64;
}

} // namespace

MoveOrdering::MoveOrdering() : responses_(std::make_unique<std::array<int32_t, 2 * 64 * 64>>()) {
    clear_responses();
}

void MoveOrdering::clear_responses() noexcept {
    responses_->fill(0);
}

void MoveOrdering::record_response(Player replier, int32_t mv, int32_t reply, bool refuted) noexcept {
    if (reply >= 0) {
        assert(mv >= 0 && mv < 64 && reply < 64);
        int32_t& cell = (*responses_)[response_row(replier, mv) + static_cast<size_t>(reply)];
        cell = stello::wrapping_add(cell, refuted ? 4 : 1);
    }
}

void MoveOrdering::order(
    std::span<int32_t> moves,
    int32_t hash_move,
    int32_t previous_move,
    Player player,
    const Board& board
) const noexcept {
    size_t start = 0;
    if (hash_move >= 0) {
        for (size_t idx = 0; idx < moves.size(); ++idx) {
            if (moves[idx] == hash_move) {
                std::swap(moves[0], moves[idx]);
                start = 1;
                break;
            }
        }
    }

    std::array<int32_t, 64> keys{};
    if (previous_move >= 0) {
        const size_t row = response_row(player, previous_move);
        auto rest = moves.subspan(start);
        for (size_t idx = 0; idx < rest.size(); ++idx) {
            keys[idx] = (*responses_)[row + static_cast<size_t>(rest[idx])];
        }

        const size_t count = rest.size();
        sort_descending(rest, std::span<int32_t>(keys.data(), count));

        size_t i = 0;
        while (moves.size() - start >= 2 && keys[i] != 0) {
            ++start;
            ++i;
        }
    }

    if (moves.size() - start <= 1) {
        return;
    }

    const auto scores = square_scores(board, player);
    auto remaining = moves.subspan(start);
    for (size_t idx = 0; idx < remaining.size(); ++idx) {
        keys[idx] = static_cast<int32_t>(scores[static_cast<size_t>(remaining[idx])]);
    }

    const size_t count = remaining.size();
    sort_descending(remaining, std::span<int32_t>(keys.data(), count));
}

std::array<int8_t, 64> square_scores(const Board& board, Player player) noexcept {
    auto scores = BASE_SQUARE_SCORES;
    const uint64_t own = board.discs(player);
    const uint64_t opp = board.discs(opponent(player));

    for (const auto& area : CORNER_AREAS) {
        if ((own & (uint64_t{1} << area.corner)) != 0) {
            scores[area.c1] = 34;
            scores[area.c2] = 34;
            scores[area.x] = 24;
        } else if ((opp & (uint64_t{1} << area.corner)) != 0) {
            scores[area.c1] = ((opp & (uint64_t{1} << area.beyond1)) != 0) ? 32 : -32;
            scores[area.c2] = ((opp & (uint64_t{1} << area.beyond2)) != 0) ? 32 : -32;
            scores[area.x] = -16;
        } else {
            scores[area.c1] = -16;
            scores[area.c2] = -16;
            scores[area.x] = -64;
        }
    }

    return scores;
}

void sort_descending(std::span<int32_t> moves, std::span<int32_t> keys) noexcept {
    const size_t count = moves.size();
    assert(count <= 64 && keys.size() == count);
    std::array<int32_t, 64> sorted_moves{};
    std::array<int32_t, 64> sorted_keys{};

    for (size_t i = 0; i < count; ++i) {
        const int32_t key = keys[i];
        size_t before = 0;
        for (size_t j = 0; j < i; ++j) {
            before += static_cast<size_t>(keys[j] >= key);
        }
        size_t after = 0;
        for (size_t j = i + 1; j < count; ++j) {
            after += static_cast<size_t>(keys[j] > key);
        }
        const size_t pos = before + after;
        sorted_moves[pos] = moves[i];
        sorted_keys[pos] = key;
    }

    for (size_t i = 0; i < count; ++i) {
        moves[i] = sorted_moves[i];
        keys[i] = sorted_keys[i];
    }
}

} // namespace stello::search
