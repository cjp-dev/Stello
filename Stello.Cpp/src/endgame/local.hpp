#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace stello::endgame {

struct LocalEntry {
    uint64_t own{0};
    uint64_t opponent{0};
    int32_t lower{0};
    int32_t upper{0};

    constexpr bool matches(uint64_t q_own, uint64_t q_opponent) const noexcept {
        return own == q_own && opponent == q_opponent;
    }

    friend constexpr bool operator==(const LocalEntry&, const LocalEntry&) noexcept = default;
};

class LocalTable {
public:
    static constexpr uint32_t SIZE_BITS = 10;
    static constexpr size_t SIZE = size_t{1} << SIZE_BITS;
    static constexpr int32_t FIRST_EMPTIES = 6;
    static constexpr size_t ROWS = static_cast<size_t>(13 - FIRST_EMPTIES);

    LocalTable() : entries_(ROWS * SIZE) {}

    void clear() noexcept {
        std::fill(entries_.begin(), entries_.end(), LocalEntry{});
    }

    size_t slot(uint64_t own, uint64_t opponent, int32_t empties) const noexcept {
        const uint64_t hash =
            ((own * 0x9dda'1c54'cfe6'b6e9ULL) ^ (opponent * 0xa2e6'c030'0831'e05aULL)) >> (64U - SIZE_BITS);
        return static_cast<size_t>(empties - FIRST_EMPTIES) * SIZE + static_cast<size_t>(hash);
    }

    const LocalEntry& get(size_t s) const noexcept {
        return entries_[s];
    }

    void set(size_t s, uint64_t own, uint64_t opponent, int32_t lower, int32_t upper) noexcept {
        entries_[s] = LocalEntry{own, opponent, lower, upper};
    }

private:
    std::vector<LocalEntry> entries_;
};

} // namespace stello::endgame
