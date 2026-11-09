#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <memory>
#include <optional>

namespace stello::search {

/// C#: `Bound`. The discriminants are the C# enum values.
enum class Bound : uint8_t {
    None = 0,
    Exact = 1,
    Lower = 2,
    Upper = 3,
};

struct Entry {
    uint64_t own{0};
    uint64_t opponent{0};
    int16_t value{0};
    int8_t depth{0};
    Bound bound{Bound::None};
    int8_t mv{0};
    uint8_t tag{0};
    uint16_t generation{0};

    uint64_t pack() const noexcept;
    static Entry unpack(uint64_t own, uint64_t opponent, uint64_t data) noexcept;

    constexpr bool is_live(uint16_t current_generation) const noexcept {
        return bound != Bound::None && generation == current_generation;
    }

    constexpr bool matches(uint64_t q_own, uint64_t q_opponent, int32_t q_tag, uint16_t current_generation) const noexcept {
        return is_live(current_generation) && own == q_own && opponent == q_opponent && static_cast<int32_t>(tag) == q_tag;
    }

    constexpr uint32_t empty_count() const noexcept {
        return 64U - static_cast<uint32_t>(std::popcount(own | opponent));
    }

    friend constexpr bool operator==(const Entry&, const Entry&) noexcept = default;
};

struct alignas(64) Slot {
    std::atomic<uint64_t> sequence{0};
    std::array<std::atomic<uint64_t>, 6> words{};
    uint64_t padding_{0};
};

static_assert(sizeof(Slot) == 64);
static_assert(alignof(Slot) == 64);
static_assert(std::atomic<uint64_t>::is_always_lock_free);

/// Cache-line aligned shared hash table with seqlock slots and root-empties aging (Stello 3.1).
class TranspositionTable {
public:
    explicit TranspositionTable(uint32_t bits);

    TranspositionTable(const TranspositionTable&) = delete;
    TranspositionTable& operator=(const TranspositionTable&) = delete;
    TranspositionTable(TranspositionTable&&) noexcept = default;
    TranspositionTable& operator=(TranspositionTable&&) noexcept = default;

    std::optional<Entry> get(uint64_t own, uint64_t opponent, int32_t tag) const noexcept {
        return get_at_root(own, opponent, tag, 64);
    }

    std::optional<Entry> get_at_root(uint64_t own, uint64_t opponent, int32_t tag, uint32_t root_empties) const noexcept;

    void store(
        uint64_t own,
        uint64_t opponent,
        int32_t tag,
        int32_t depth,
        Bound bound,
        int32_t value,
        int32_t mv
    ) const noexcept {
        store_at_root(own, opponent, tag, depth, bound, value, mv, 64);
    }

    void store_at_root(
        uint64_t own,
        uint64_t opponent,
        int32_t tag,
        int32_t depth,
        Bound bound,
        int32_t value,
        int32_t mv,
        uint32_t root_empties
    ) const noexcept;

    void prefetch(uint64_t own, uint64_t opponent, int32_t tag) const noexcept;

    void clear() noexcept;

    size_t slot_index(uint64_t own, uint64_t opponent, int32_t tag) const noexcept;
    const Slot* data() const noexcept { return slots_.get(); }

private:
    struct AlignedDeleter {
        void operator()(Slot* ptr) const noexcept;
    };

    std::unique_ptr<Slot[], AlignedDeleter> slots_;
    size_t count_{0};
    uint32_t shift_{0};
    uint16_t generation_{1};
};

} // namespace stello::search
