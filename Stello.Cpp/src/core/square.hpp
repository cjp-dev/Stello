#pragma once

#include <bit>
#include <cassert>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace stello {

/// A board square: `index = row * 8 + column`, so a1 = 0 and h8 = 63 (bit `n` of a bitboard).
class Square {
public:
    constexpr Square() noexcept = default;

    explicit constexpr Square(uint8_t index) noexcept : index_(index) {
        assert(index < 64 && "a square index is 0-63");
    }

    static constexpr Square at(uint8_t column, uint8_t row) noexcept {
        assert(column < 8 && row < 8 && "column and row are 0-7");
        return Square(static_cast<uint8_t>(row * 8 + column));
    }

    constexpr uint8_t index() const noexcept { return index_; }
    constexpr uint8_t column() const noexcept { return index_ & 7; }
    constexpr uint8_t row() const noexcept { return index_ >> 3; }
    constexpr uint64_t bit() const noexcept { return uint64_t{1} << index_; }

    constexpr int32_t to_legacy() const noexcept {
        return (static_cast<int32_t>(row()) + 1) * 10 + static_cast<int32_t>(column()) + 1;
    }

    static inline std::optional<Square> from_legacy(int32_t legacy) noexcept {
        const int32_t row = legacy / 10 - 1;
        const int32_t column = legacy % 10 - 1;
        if (legacy >= 0 && row >= 0 && row < 8 && column >= 0 && column < 8) {
            return Square::at(static_cast<uint8_t>(column), static_cast<uint8_t>(row));
        }
        return std::nullopt;
    }

    static inline std::optional<Square> parse(std::string_view text) noexcept {
        if (text.size() != 2) {
            return std::nullopt;
        }
        const uint8_t ch0 = static_cast<uint8_t>(text[0]);
        const uint8_t lower = (ch0 >= 'A' && ch0 <= 'Z') ? static_cast<uint8_t>(ch0 + ('a' - 'A')) : ch0;
        const uint8_t column = static_cast<uint8_t>(lower - static_cast<uint8_t>('a'));
        const uint8_t row = static_cast<uint8_t>(static_cast<uint8_t>(text[1]) - static_cast<uint8_t>('1'));
        if (column < 8 && row < 8) {
            return Square::at(column, row);
        }
        return std::nullopt;
    }

    inline std::string to_string() const {
        std::string s(2, ' ');
        s[0] = static_cast<char>('a' + column());
        s[1] = static_cast<char>('1' + row());
        return s;
    }

    friend constexpr bool operator==(Square a, Square b) noexcept = default;
    friend constexpr auto operator<=>(Square a, Square b) noexcept = default;

    class MaskRange {
    public:
        class Iterator {
        public:
            explicit constexpr Iterator(uint64_t mask) noexcept : rest_(mask) {}

            constexpr Square operator*() const noexcept {
                return Square(static_cast<uint8_t>(std::countr_zero(rest_)));
            }

            constexpr Iterator& operator++() noexcept {
                rest_ &= (rest_ - 1);
                return *this;
            }

            constexpr bool operator!=(const Iterator& other) const noexcept {
                return rest_ != other.rest_;
            }

        private:
            uint64_t rest_;
        };

        explicit constexpr MaskRange(uint64_t mask) noexcept : mask_(mask) {}
        constexpr Iterator begin() const noexcept { return Iterator(mask_); }
        constexpr Iterator end() const noexcept { return Iterator(0); }

    private:
        uint64_t mask_;
    };

    static constexpr MaskRange in_mask(uint64_t mask) noexcept { return MaskRange(mask); }

private:
    uint8_t index_{0};
};

/// A move in a game: a disc placed on a square, or a pass.
struct Move {
    std::optional<Square> square;

    constexpr Move() noexcept = default;
    explicit constexpr Move(std::optional<Square> sq) noexcept : square(sq) {}
    constexpr Move(Square sq) noexcept : square(sq) {} // NOLINT(google-explicit-constructor)

    static constexpr Move pass() noexcept { return Move(std::nullopt); }
    constexpr bool is_pass() const noexcept { return !square.has_value(); }

    static inline std::optional<Move> parse(std::string_view text) noexcept {
        if (text.size() == 4) {
            const char c0 = text[0];
            const char c1 = text[1];
            const char c2 = text[2];
            const char c3 = text[3];
            const auto eq = [](char a, char expected) noexcept {
                return a == expected || a == static_cast<char>(expected - 'a' + 'A');
            };
            if (eq(c0, 'p') && eq(c1, 'a') && eq(c2, 's') && eq(c3, 's')) {
                return Move::pass();
            }
        }
        if (const auto sq = Square::parse(text)) {
            return Move(*sq);
        }
        return std::nullopt;
    }

    inline std::string to_string() const {
        return square ? square->to_string() : std::string("pass");
    }

    friend constexpr bool operator==(Move a, Move b) noexcept = default;
};

} // namespace stello
