#pragma once

#include "core/bits.hpp"
#include "core/square.hpp"

#include <bit>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace stello {

enum class Player : uint8_t {
    Black = 0,
    White = 1,
};

inline constexpr Player opponent(Player p) noexcept {
    return p == Player::Black ? Player::White : Player::Black;
}

inline constexpr size_t player_index(Player p) noexcept {
    return static_cast<size_t>(p);
}

enum class BoardParseErrorKind : uint8_t {
    None = 0,
    WrongLength,
    InvalidCharacter,
};

struct BoardParseError {
    BoardParseErrorKind kind{BoardParseErrorKind::None};
    char invalid_char{0};

    constexpr bool ok() const noexcept { return kind == BoardParseErrorKind::None; }
    std::string message() const;
    friend constexpr bool operator==(const BoardParseError&, const BoardParseError&) noexcept = default;
};

class Board {
public:
    constexpr Board() noexcept = default;

    constexpr Board(uint64_t black, uint64_t white) noexcept : black_(black), white_(white) {
        assert((black & white) == 0 && "A square cannot hold both a black and a white disc.");
    }

    static constexpr Board initial() noexcept {
        return Board((uint64_t{1} << 28) | (uint64_t{1} << 35),
                     (uint64_t{1} << 27) | (uint64_t{1} << 36));
    }

    static constexpr Board from_own(Player player, uint64_t own, uint64_t opp) noexcept {
        return player == Player::Black ? Board(own, opp) : Board(opp, own);
    }

    constexpr uint64_t black() const noexcept { return black_; }
    constexpr uint64_t white() const noexcept { return white_; }
    constexpr uint64_t empty() const noexcept { return ~(black_ | white_); }
    constexpr uint32_t empty_count() const noexcept { return static_cast<uint32_t>(std::popcount(empty())); }

    constexpr std::optional<Player> get(Square square) const noexcept {
        if ((black_ & square.bit()) != 0) {
            return Player::Black;
        }
        if ((white_ & square.bit()) != 0) {
            return Player::White;
        }
        return std::nullopt;
    }

    constexpr uint64_t discs(Player player) const noexcept {
        return player == Player::Black ? black_ : white_;
    }

    constexpr uint32_t count(Player player) const noexcept {
        return static_cast<uint32_t>(std::popcount(discs(player)));
    }

    inline uint64_t legal_moves(Player player) const noexcept {
        return bits::legal_moves(discs(player), discs(opponent(player)));
    }

    inline bool has_legal_move(Player player) const noexcept {
        return legal_moves(player) != 0;
    }

    inline bool is_game_over() const noexcept {
        return !has_legal_move(Player::Black) && !has_legal_move(Player::White);
    }

    inline bool is_legal(Player player, Square square) const noexcept {
        return (legal_moves(player) & square.bit()) != 0;
    }

    inline uint64_t flips(Player player, Square square) const noexcept {
        if ((empty() & square.bit()) == 0) {
            return 0;
        }
        return bits::flips(discs(player), discs(opponent(player)), square.index());
    }

    inline std::optional<Board> play(Player player, Square square) const noexcept {
        const uint64_t f = flips(player, square);
        if (f == 0) {
            return std::nullopt;
        }
        return play_flips(player, square.index(), f);
    }

    constexpr Board play_flips(Player player, uint32_t square, uint64_t f) const noexcept {
        const uint64_t placed = f | (uint64_t{1} << square);
        return player == Player::Black
                 ? Board(black_ | placed, white_ & ~f)
                 : Board(black_ & ~f, white_ | placed);
    }

    inline Board play_unchecked(Player player, uint32_t square) const noexcept {
        const uint64_t f = bits::flips(discs(player), discs(opponent(player)), square);
        return play_flips(player, square, f);
    }

    static std::optional<Board> parse(std::string_view text, BoardParseError* error = nullptr) noexcept;
    std::string to_string() const;

    friend constexpr bool operator==(const Board&, const Board&) noexcept = default;

private:
    uint64_t black_{0};
    uint64_t white_{0};
};

uint64_t perft(const Board& board, Player player, uint32_t depth) noexcept;

} // namespace stello
