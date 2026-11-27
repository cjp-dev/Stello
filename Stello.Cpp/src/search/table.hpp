#pragma once

#include "core/compiler.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>

#if !defined(__EMSCRIPTEN__) && (defined(__x86_64__) || defined(_M_X64))
#include <xmmintrin.h>
#endif

namespace stello::search {

/// C#: `Bound`. Kept for unit-test compatibility overloads.
enum class Bound : uint8_t {
    None = 0,
    Exact = 1,
    Lower = 2,
    Upper = 3,
};

inline constexpr int32_t ENDGAME_TT_TAG = 2;
inline constexpr int16_t TT_SCORE_MIN = -32768; // sentinel: no lower bound
inline constexpr int16_t TT_SCORE_MAX = 32767;  // sentinel: no upper bound
inline constexpr uint8_t SEL_MIDGAME_MPC = 1;
inline constexpr uint8_t SEL_EXACT = 7;

inline uint8_t search_cost(uint64_t delta_nodes) noexcept {
    const int w = std::bit_width(delta_nodes);
    return static_cast<uint8_t>(w > 31 ? 31 : w);
}

namespace detail {

// bits  0..15: int16_t lower (16 bits)
// bits 16..31: int16_t upper (16 bits)
// bits 32..38: mv + 1        (7 bits: 0=-1, 1..64=sq 0..63)
// bits 39..40: tag           (2 bits: 0=Black mid, 1=White mid, 2=Endgame)
// bits 41..45: cost          (5 bits: 0..31)
// bits 46..48: selectivity   (3 bits: 0..7; 1..6=MPC stages, 7=exact/non-selective)
// bits 49..54: depth         (6 bits: 0..63)
// bits 55..56: date          (2 bits: 0..3)
// bits 57..63: generation    (7 bits: 1..127; 0=empty/cleared)
inline constexpr uint32_t UPPER_SHIFT = 16;
inline constexpr uint32_t MOVE_SHIFT = 32;
inline constexpr uint32_t TAG_SHIFT = 39;
inline constexpr uint32_t COST_SHIFT = 41;
inline constexpr uint32_t SEL_SHIFT = 46;
inline constexpr uint32_t DEPTH_SHIFT = 49;
inline constexpr uint32_t DATE_SHIFT = 55;
inline constexpr uint32_t GENERATION_SHIFT = 57;
inline constexpr uint8_t GENERATION_MASK = 0x7FU;

inline constexpr uint32_t VERIFY_EMPTIES_SHIFT = 57;
inline constexpr uint64_t VERIFY_HASH_MASK = (uint64_t{1} << VERIFY_EMPTIES_SHIFT) - 1ULL;

#if (defined(__x86_64__) || defined(_M_X64)) && !defined(__EMSCRIPTEN__)
void zero_slots_nt_avx2(void* dst, size_t bytes) noexcept;
#endif

struct HashKey {
    size_t index;
    uint64_t verify; // top 7 bits = empties (0..64), low 57 bits = independent verification hash
};

inline size_t compute_slot_index(uint64_t own, uint64_t opponent, int32_t tag, uint32_t shift) noexcept {
    const uint64_t a = own * 0x9E37'79B9'7F4A'7C15ULL;
    const uint64_t b = opponent * 0xC2B2'AE3D'27D4'EB4FULL;
    const uint64_t t = static_cast<uint64_t>(static_cast<uint32_t>(tag));
    const uint64_t h1 = (a ^ std::rotl(b, 31) ^ (t * 0x94D0'49BB'1331'11EBULL)) * 0xD6E8'FEB8'6659'FD93ULL;
    return static_cast<size_t>(h1 >> shift);
}

inline HashKey compute_hash_key(uint64_t own, uint64_t opponent, int32_t tag, uint32_t shift) noexcept {
    const uint64_t a = own * 0x9E37'79B9'7F4A'7C15ULL;
    const uint64_t b = opponent * 0xC2B2'AE3D'27D4'EB4FULL;
    const uint64_t t = static_cast<uint64_t>(static_cast<uint32_t>(tag));
    const uint64_t h1 = (a ^ std::rotl(b, 31) ^ (t * 0x94D0'49BB'1331'11EBULL)) * 0xD6E8'FEB8'6659'FD93ULL;
    uint64_t h2 = (std::rotl(a, 27) ^ b ^ (t * 0xBF58'476D'1CE4'E5B9ULL)) * 0x94D0'49BB'1331'11EBULL;
    h2 ^= h2 >> 30;
    const uint64_t empties = 64ULL - static_cast<uint64_t>(std::popcount(own | opponent));
    return {
        static_cast<size_t>(h1 >> shift),
        (empties << VERIFY_EMPTIES_SHIFT) | (h2 & VERIFY_HASH_MASK),
    };
}

} // namespace detail

struct Entry {
    uint64_t verify{0};
    int16_t lower{0};
    int16_t upper{0};
    int8_t mv{-1};
    uint8_t depth{0};
    uint8_t selectivity{0};
    uint8_t tag{0};
    uint8_t cost{0};
    uint8_t date{0};
    uint8_t generation{0};

    uint64_t pack() const noexcept {
        return static_cast<uint64_t>(static_cast<uint16_t>(lower))
             | (static_cast<uint64_t>(static_cast<uint16_t>(upper)) << detail::UPPER_SHIFT)
             | ((static_cast<uint64_t>(static_cast<uint8_t>(static_cast<int32_t>(mv) + 1)) & 0x7FULL) << detail::MOVE_SHIFT)
             | ((static_cast<uint64_t>(depth) & 0x3FULL) << detail::DEPTH_SHIFT)
             | ((static_cast<uint64_t>(selectivity) & 0x7ULL) << detail::SEL_SHIFT)
             | ((static_cast<uint64_t>(tag) & 0x3ULL) << detail::TAG_SHIFT)
             | ((static_cast<uint64_t>(cost) & 0x1FULL) << detail::COST_SHIFT)
             | ((static_cast<uint64_t>(date) & 0x3ULL) << detail::DATE_SHIFT)
             | ((static_cast<uint64_t>(generation) & detail::GENERATION_MASK) << detail::GENERATION_SHIFT);
    }

    static Entry unpack(uint64_t verify, uint64_t data) noexcept {
        Entry e;
        e.verify = verify;
        e.lower = static_cast<int16_t>(static_cast<uint16_t>(data));
        e.upper = static_cast<int16_t>(static_cast<uint16_t>(data >> detail::UPPER_SHIFT));
        e.mv = static_cast<int8_t>(static_cast<int32_t>((data >> detail::MOVE_SHIFT) & 0x7FULL) - 1);
        e.depth = static_cast<uint8_t>((data >> detail::DEPTH_SHIFT) & 0x3FULL);
        e.selectivity = static_cast<uint8_t>((data >> detail::SEL_SHIFT) & 0x7ULL);
        e.tag = static_cast<uint8_t>((data >> detail::TAG_SHIFT) & 0x3ULL);
        e.cost = static_cast<uint8_t>((data >> detail::COST_SHIFT) & 0x1FULL);
        e.date = static_cast<uint8_t>((data >> detail::DATE_SHIFT) & 0x3ULL);
        e.generation = static_cast<uint8_t>((data >> detail::GENERATION_SHIFT) & detail::GENERATION_MASK);
        return e;
    }

    constexpr uint32_t level() const noexcept {
        return (static_cast<uint32_t>(depth) << 3) | static_cast<uint32_t>(selectivity & 0x7U);
    }

    constexpr bool is_live(uint8_t current_generation) const noexcept {
        return generation != 0 && generation == current_generation;
    }

    constexpr bool matches(uint64_t q_verify, int32_t q_tag, uint8_t current_generation) const noexcept {
        return is_live(current_generation) && verify == q_verify && static_cast<int32_t>(tag) == q_tag;
    }

    constexpr uint32_t empty_count() const noexcept {
        return static_cast<uint32_t>(verify >> detail::VERIFY_EMPTIES_SHIFT);
    }

    friend constexpr bool operator==(const Entry&, const Entry&) noexcept = default;
};

struct PackedEntry {
    uint64_t key_xor;
    uint64_t data;
};

struct alignas(64) Slot {
    std::array<PackedEntry, 4> entries;
};

static_assert(sizeof(PackedEntry) == 16);
static_assert(sizeof(Slot) == 64);
static_assert(alignof(Slot) == 64);
static_assert(std::is_trivially_copyable_v<Slot>);
static_assert(std::atomic_ref<uint64_t>::is_always_lock_free);

namespace detail {

inline uint32_t writeable_level(const Entry& e, uint8_t current_date) noexcept {
    const uint32_t is_current = (e.date == current_date) ? 1U : 0U;
    const uint32_t lvl = e.level(); // 9 bits: (depth << 3) | selectivity (0..511)
    const uint32_t c = static_cast<uint32_t>(e.cost & 0x1FU);
    return 1U + (is_current << 14) + (lvl << 5) + c;
}

inline constexpr uint32_t writeable_level_packed(uint64_t d, uint8_t current_date) noexcept {
    const uint32_t is_current = (static_cast<uint8_t>((d >> DATE_SHIFT) & 0x3ULL) == current_date) ? 0x4000U : 0U;
    const uint32_t lvl_cost = static_cast<uint32_t>((d >> COST_SHIFT) & 0x3FFFULL);
    return 1U + is_current + lvl_cost;
}

} // namespace detail

/// Cache-line aligned lockless shared unified hash table: 4-way 64 B Hyatt-XOR buckets with dual bounds.
class TranspositionTable {
public:
    static constexpr size_t WAYS = 4;

    struct StoreParams {
        int32_t alpha_in;
        int32_t beta_in;
        int32_t score;
        int32_t tag;
        int32_t depth;
        int32_t mv;
        uint32_t root_empties;
        uint8_t selectivity;
        uint8_t cost;
    };

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
        const detail::HashKey key = detail::compute_hash_key(own, opponent, tag, shift_);
        if (static_cast<uint32_t>(key.verify >> detail::VERIFY_EMPTIES_SHIFT) > root_empties) {
            return std::nullopt;
        }

        Slot& slot = slots_[key.index];
        for (size_t i = 0; i < WAYS; ++i) {
            const uint64_t d = std::atomic_ref<uint64_t>(slot.entries[i].data).load(std::memory_order_relaxed);
            const uint64_t k = std::atomic_ref<uint64_t>(slot.entries[i].key_xor).load(std::memory_order_relaxed);
            if ((k ^ d) == key.verify
                && static_cast<uint8_t>(d >> detail::GENERATION_SHIFT) == generation_
                && static_cast<int32_t>((d >> detail::TAG_SHIFT) & 0x3ULL) == tag)
            {
                return Entry::unpack(key.verify, d);
            }
        }
        return std::nullopt;
    }

    STELLO_NOINLINE void store_bounds_at_root_slow(
        uint64_t own,
        uint64_t opponent,
        const StoreParams& p
    ) const noexcept;

    void store_bounds_at_root(
        uint64_t own,
        uint64_t opponent,
        int32_t tag,
        int32_t depth,
        uint8_t selectivity,
        int32_t alpha_in,
        int32_t beta_in,
        int32_t score,
        int32_t mv,
        uint32_t root_empties,
        uint8_t cost = 0
    ) const noexcept {
        const StoreParams params{
            alpha_in,
            beta_in,
            score,
            tag,
            depth,
            mv,
            root_empties,
            selectivity,
            cost,
        };
        store_bounds_at_root_slow(own, opponent, params);
    }

    void store(
        uint64_t own,
        uint64_t opponent,
        int32_t tag,
        int32_t depth,
        Bound bound,
        int32_t value,
        int32_t mv,
        uint8_t cost = 0,
        uint8_t selectivity = SEL_EXACT
    ) const noexcept {
        store_at_root(own, opponent, tag, depth, bound, value, mv, 64, cost, selectivity);
    }

    void store_at_root(
        uint64_t own,
        uint64_t opponent,
        int32_t tag,
        int32_t depth,
        Bound bound,
        int32_t value,
        int32_t mv,
        uint32_t root_empties,
        uint8_t cost = 0,
        uint8_t selectivity = SEL_EXACT
    ) const noexcept {
        int32_t alpha_in = value;
        int32_t beta_in = value;
        switch (bound) {
            case Bound::Exact:
                alpha_in = value - 1;
                beta_in = value + 1;
                break;
            case Bound::Lower:
                alpha_in = value - 1;
                beta_in = value;
                break;
            case Bound::Upper:
                alpha_in = value;
                beta_in = value + 1;
                break;
            case Bound::None:
                return;
        }
        store_bounds_at_root(own, opponent, tag, depth, selectivity, alpha_in, beta_in, value, mv, root_empties, cost);
    }

    void prefetch(uint64_t own, uint64_t opponent, int32_t tag) const noexcept {
#if !defined(__EMSCRIPTEN__) && (defined(__x86_64__) || defined(_M_X64))
        const char* ptr = reinterpret_cast<const char*>(&slots_[slot_index(own, opponent, tag)]);
        _mm_prefetch(ptr, _MM_HINT_T0);
#else
        (void)own;
        (void)opponent;
        (void)tag;
#endif
    }

    void advance_date() noexcept { date_ = static_cast<uint8_t>((date_ + 1U) & 0x3U); }
    uint8_t current_date() const noexcept { return date_; }
    void clear() noexcept;

    size_t slot_index(uint64_t own, uint64_t opponent, int32_t tag) const noexcept {
        return detail::compute_slot_index(own, opponent, tag, shift_);
    }

    uint64_t verify_key(uint64_t own, uint64_t opponent, int32_t tag) const noexcept {
        return detail::compute_hash_key(own, opponent, tag, shift_).verify;
    }

    const Slot* data() const noexcept { return slots_.get(); }

private:
    struct AlignedDeleter {
        void operator()(Slot* ptr) const noexcept;
    };

    std::unique_ptr<Slot[], AlignedDeleter> slots_;
    size_t count_{0};
    uint32_t shift_{0};
    uint8_t generation_{1};
    uint8_t date_{0};
};

} // namespace stello::search
