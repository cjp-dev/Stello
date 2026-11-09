#include "core/board.hpp"

#include <bit>
#include <cctype>

namespace stello {

std::string BoardParseError::message() const {
    switch (kind) {
        case BoardParseErrorKind::None:
            return {};
        case BoardParseErrorKind::WrongLength:
            return "A board must have exactly 64 squares.";
        case BoardParseErrorKind::InvalidCharacter: {
            std::string msg = "Invalid square character '";
            msg.push_back(invalid_char);
            msg += "'.";
            return msg;
        }
    }
    return {};
}

std::optional<Board> Board::parse(std::string_view text, BoardParseError* error) noexcept {
    if (error) {
        *error = BoardParseError{};
    }
    uint64_t black = 0;
    uint64_t white = 0;
    uint32_t index = 0;
    for (unsigned char ch : text) {
        if (std::isspace(ch)) {
            continue;
        }
        if (index == 64) {
            if (error) {
                error->kind = BoardParseErrorKind::WrongLength;
            }
            return std::nullopt;
        }
        switch (ch) {
            case 'X':
            case 'x':
                black |= uint64_t{1} << index;
                break;
            case 'O':
            case 'o':
                white |= uint64_t{1} << index;
                break;
            case '-':
            case '.':
                break;
            default:
                if (error) {
                    error->kind = BoardParseErrorKind::InvalidCharacter;
                    error->invalid_char = static_cast<char>(ch);
                }
                return std::nullopt;
        }
        ++index;
    }

    if (index == 64) {
        return Board(black, white);
    }
    if (error) {
        error->kind = BoardParseErrorKind::WrongLength;
    }
    return std::nullopt;
}

std::string Board::to_string() const {
    std::string out;
    out.reserve(71);
    for (uint8_t index = 0; index < 64; ++index) {
        const auto cell = get(Square(index));
        char c = '-';
        if (cell == Player::Black) {
            c = 'X';
        } else if (cell == Player::White) {
            c = 'O';
        }
        out.push_back(c);
        if ((index & 7) == 7 && index != 63) {
            out.push_back('\n');
        }
    }
    return out;
}

uint64_t perft(const Board& board, Player player, uint32_t depth) noexcept {
    if (depth == 0) {
        return 1;
    }

    const uint64_t moves = board.legal_moves(player);
    if (moves == 0) {
        return board.has_legal_move(opponent(player))
                 ? perft(board, opponent(player), depth - 1)
                 : 1;
    }

    const uint64_t own = board.discs(player);
    const uint64_t opp = board.discs(opponent(player));
    uint64_t nodes = 0;
    uint64_t rest = moves;
    while (rest != 0) {
        const uint32_t square = static_cast<uint32_t>(std::countr_zero(rest));
        rest &= (rest - 1);
        const uint64_t flipped = bits::flips(own, opp, square);
        nodes += perft(board.play_flips(player, square, flipped), opponent(player), depth - 1);
    }
    return nodes;
}

} // namespace stello
