#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <memory>
#include <optional>

#if !defined(__EMSCRIPTEN__) && (defined(__x86_64__) || defined(_M_X64))
#include <xmmintrin.h>
#endif

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#endif

namespace stello::search {

/// C#: `Bound`. The discriminants are the C# enum values.
enum class Bound : uint8_t {
    None = 0,
    Exact = 1,
    Lower = 2,
    Upper = 3,
};

namespace detail {

inline constexpr uint32_t DEPTH_SHIFT = 16;
inline constexpr uint32_t BOUND_SHIFT = 24;
inline constexpr uint32_t MOVE_SHIFT = 32;
inline constexpr uint32_t TAG_SHIFT = 40;
inline constexpr uint32_t GENERATION_SHIFT = 48;
inline constexpr uint64_t LOCKED = 1ULL;

inline constexpr Bound bound_from_bits(uint64_t bits) noexcept {
    switch (bits & 0xFFULL) {
        case 1: return Bound::Exact;
        case 2: return Bound::Lower;
        case 3: return Bound::Upper;
        default: return Bound::None;
    }
}

inline void spin_loop_hint() noexcept {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    _mm_pause();
#elif defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#endif
}

} // namespace detail

struct Entry {
    uint64_t own{0};
    uint64_t opponent{0};
    int16_t value{0};
    int8_t depth{0};
    Bound bound{Bound::None};
    int8_t mv{0};
    uint8_t tag{0};
    uint16_t generation{0};

    uint64_t pack() const noexcept {
        return static_cast<uint64_t>(static_cast<uint16_t>(value))
             | (static_cast<uint64_t>(static_cast<uint8_t>(depth)) << detail::DEPTH_SHIFT)
             | (static_cast<uint64_t>(static_cast<uint8_t>(bound)) << detail::BOUND_SHIFT)
             | (static_cast<uint64_t>(static_cast<uint8_t>(mv)) << detail::MOVE_SHIFT)
             | (static_cast<uint64_t>(tag) << detail::TAG_SHIFT)
             | (static_cast<uint64_t>(generation) << detail::GENERATION_SHIFT);
    }

    static Entry unpack(uint64_t own, uint64_t opponent, uint64_t data) noexcept {
        Entry e;
        e.own = own;
        e.opponent = opponent;
        e.value = static_cast<int16_t>(static_cast<uint16_t>(data));
        e.depth = static_cast<int8_t>(static_cast<uint8_t>(data >> detail::DEPTH_SHIFT));
        e.bound = detail::bound_from_bits(data >> detail::BOUND_SHIFT);
        e.mv = static_cast<int8_t>(static_cast<uint8_t>(data >> detail::MOVE_SHIFT));
        e.tag = static_cast<uint8_t>(data >> detail::TAG_SHIFT);
        e.generation = static_cast<uint16_t>(data >> detail::GENERATION_SHIFT);
        return e;
    }

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

namespace detail {

inline Entry load_entry(const Slot& slot, size_t i) noexcept {
    const size_t base = 3 * i;
    return Entry::unpack(
        slot.words[base].load(std::memory_order_relaxed),
        slot.words[base + 1].load(std::memory_order_relaxed),
        slot.words[base + 2].load(std::memory_order_relaxed));
}

inline void put_entry(Slot& slot, size_t i, const Entry& entry) noexcept {
    const size_t base = 3 * i;
    slot.words[base].store(entry.own, std::memory_order_relaxed);
    slot.words[base + 1].store(entry.opponent, std::memory_order_relaxed);
    slot.words[base + 2].store(entry.pack(), std::memory_order_relaxed);
}

inline uint64_t lock_slot(Slot& slot) noexcept {
    for (;;) {
        const uint64_t seq = slot.sequence.load(std::memory_order_relaxed);
        uint64_t expected = seq;
        if ((seq & LOCKED) == 0
            && slot.sequence.compare_exchange_weak(
                expected, seq | LOCKED, std::memory_order_acquire, std::memory_order_relaxed))
        {
            std::atomic_thread_fence(std::memory_order_release);
            return seq;
        }
        spin_loop_hint();
    }
}

inline void unlock_slot(Slot& slot, uint64_t seq) noexcept {
    slot.sequence.store(seq + 2ULL, std::memory_order_release);
}

} // namespace detail

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

    std::optional<Entry> get_at_root(
        uint64_t own,
        uint64_t opponent,
        int32_t tag,
        uint32_t root_empties
    ) const noexcept {
        const Slot& slot = slots_[slot_index(own, opponent, tag)];
        const uint64_t seq = slot.sequence.load(std::memory_order_acquire);
        if ((seq & detail::LOCKED) != 0) {
            return std::nullopt;
        }

        auto hit = [&](const Entry& e) noexcept -> std::optional<Entry> {
            if (e.matches(own, opponent, tag, generation_) && e.empty_count() <= root_empties) {
                return e;
            }
            return std::nullopt;
        };

        std::optional<Entry> found = hit(detail::load_entry(slot, 0));
        if (!found.has_value()) {
            found = hit(detail::load_entry(slot, 1));
        }

        std::atomic_thread_fence(std::memory_order_acquire);
        if (slot.sequence.load(std::memory_order_relaxed) == seq) {
            return found;
        }
        return std::nullopt;
    }

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
    ) const noexcept {
        Slot& slot = slots_[slot_index(own, opponent, tag)];
        const uint16_t gen = generation_;
        Entry entry;
        entry.own = own;
        entry.opponent = opponent;
        entry.value = static_cast<int16_t>(value);
        entry.depth = static_cast<int8_t>(depth);
        entry.bound = bound;
        entry.mv = static_cast<int8_t>(mv);
        entry.tag = static_cast<uint8_t>(tag);
        entry.generation = gen;

        const uint64_t seq = detail::lock_slot(slot);
        const Entry deep = detail::load_entry(slot, 0);
        const bool deep_live = deep.is_live(gen) && deep.empty_count() <= root_empties;

        if (deep_live && deep.matches(own, opponent, tag, gen)) {
            if (depth >= static_cast<int32_t>(deep.depth)) {
                detail::put_entry(slot, 0, entry);
            }
        } else if (!deep_live && deep.is_live(gen)) {
            detail::put_entry(slot, 0, entry);
        } else if (!deep_live || depth >= static_cast<int32_t>(deep.depth)) {
            detail::put_entry(slot, 1, deep);
            detail::put_entry(slot, 0, entry);
        } else {
            detail::put_entry(slot, 1, entry);
        }

        detail::unlock_slot(slot, seq);
    }

    void prefetch(uint64_t own, uint64_t opponent, int32_t tag) const noexcept {
#if !defined(__EMSCRIPTEN__) && (defined(__x86_64__) || defined(_M_X64))
        const Slot* slot = &slots_[slot_index(own, opponent, tag)];
        _mm_prefetch(reinterpret_cast<const char*>(slot), _MM_HINT_T0);
#else
        (void)own;
        (void)opponent;
        (void)tag;
#endif
    }

    void clear() noexcept;

    size_t slot_index(uint64_t own, uint64_t opponent, int32_t tag) const noexcept {
        uint64_t hash = own * 0x9E37'79B9'7F4A'7C15ULL;
        hash ^= std::rotl(opponent * 0xC2B2'AE3D'27D4'EB4FULL, 31) + static_cast<uint64_t>(static_cast<int64_t>(tag));
        hash *= 0xD6E8'FEB8'6659'FD93ULL;
        return static_cast<size_t>(hash >> shift_);
    }

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
