#include "search/table.hpp"

#include <bit>
#include <cassert>
#include <cstring>
#include <memory>
#include <new>

#if !defined(__EMSCRIPTEN__) && (defined(__x86_64__) || defined(_M_X64))
#include <xmmintrin.h>
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace stello::search {

namespace {

constexpr uint32_t DEPTH_SHIFT = 16;
constexpr uint32_t BOUND_SHIFT = 24;
constexpr uint32_t MOVE_SHIFT = 32;
constexpr uint32_t TAG_SHIFT = 40;
constexpr uint32_t GENERATION_SHIFT = 48;
constexpr uint64_t LOCKED = 1ULL;

inline Bound bound_from_bits(uint64_t bits) noexcept {
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

} // namespace

uint64_t Entry::pack() const noexcept {
    return static_cast<uint64_t>(static_cast<uint16_t>(value))
         | (static_cast<uint64_t>(static_cast<uint8_t>(depth)) << DEPTH_SHIFT)
         | (static_cast<uint64_t>(static_cast<uint8_t>(bound)) << BOUND_SHIFT)
         | (static_cast<uint64_t>(static_cast<uint8_t>(mv)) << MOVE_SHIFT)
         | (static_cast<uint64_t>(tag) << TAG_SHIFT)
         | (static_cast<uint64_t>(generation) << GENERATION_SHIFT);
}

Entry Entry::unpack(uint64_t own, uint64_t opponent, uint64_t data) noexcept {
    Entry e;
    e.own = own;
    e.opponent = opponent;
    e.value = static_cast<int16_t>(static_cast<uint16_t>(data));
    e.depth = static_cast<int8_t>(static_cast<uint8_t>(data >> DEPTH_SHIFT));
    e.bound = bound_from_bits(data >> BOUND_SHIFT);
    e.mv = static_cast<int8_t>(static_cast<uint8_t>(data >> MOVE_SHIFT));
    e.tag = static_cast<uint8_t>(data >> TAG_SHIFT);
    e.generation = static_cast<uint16_t>(data >> GENERATION_SHIFT);
    return e;
}

void TranspositionTable::AlignedDeleter::operator()(Slot* ptr) const noexcept {
    if (ptr != nullptr) {
        ::operator delete[](ptr, std::align_val_t{64});
    }
}

TranspositionTable::TranspositionTable(uint32_t bits)
    : count_(size_t{1} << bits), shift_(64U - bits), generation_(1) {
    assert(bits >= 1 && bits < 64);
    void* raw = ::operator new[](count_ * sizeof(Slot), std::align_val_t{64});
    std::memset(raw, 0, count_ * sizeof(Slot));
    Slot* typed = static_cast<Slot*>(raw);
    for (size_t i = 0; i < count_; ++i) {
        std::construct_at(&typed[i]);
    }
    slots_.reset(typed);
    assert((reinterpret_cast<uintptr_t>(slots_.get()) & 63ULL) == 0ULL);
}

size_t TranspositionTable::slot_index(uint64_t own, uint64_t opponent, int32_t tag) const noexcept {
    uint64_t hash = own * 0x9E37'79B9'7F4A'7C15ULL;
    hash ^= std::rotl(opponent * 0xC2B2'AE3D'27D4'EB4FULL, 31) + static_cast<uint64_t>(static_cast<int64_t>(tag));
    hash *= 0xD6E8'FEB8'6659'FD93ULL;
    return static_cast<size_t>(hash >> shift_);
}

std::optional<Entry> TranspositionTable::get_at_root(
    uint64_t own,
    uint64_t opponent,
    int32_t tag,
    uint32_t root_empties
) const noexcept {
    const Slot& slot = slots_[slot_index(own, opponent, tag)];
    const uint64_t seq = slot.sequence.load(std::memory_order_acquire);
    if ((seq & LOCKED) != 0) {
        return std::nullopt;
    }

    auto hit = [&](const Entry& e) noexcept -> std::optional<Entry> {
        if (e.matches(own, opponent, tag, generation_) && e.empty_count() <= root_empties) {
            return e;
        }
        return std::nullopt;
    };

    std::optional<Entry> found = hit(load_entry(slot, 0));
    if (!found.has_value()) {
        found = hit(load_entry(slot, 1));
    }

    std::atomic_thread_fence(std::memory_order_acquire);
    if (slot.sequence.load(std::memory_order_relaxed) == seq) {
        return found;
    }
    return std::nullopt;
}

void TranspositionTable::store_at_root(
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

    const uint64_t seq = lock_slot(slot);
    const Entry deep = load_entry(slot, 0);
    const bool deep_live = deep.is_live(gen) && deep.empty_count() <= root_empties;

    if (deep_live && deep.matches(own, opponent, tag, gen)) {
        if (depth >= static_cast<int32_t>(deep.depth)) {
            put_entry(slot, 0, entry);
        }
    } else if (!deep_live && deep.is_live(gen)) {
        put_entry(slot, 0, entry);
    } else if (!deep_live || depth >= static_cast<int32_t>(deep.depth)) {
        put_entry(slot, 1, deep);
        put_entry(slot, 0, entry);
    } else {
        put_entry(slot, 1, entry);
    }

    unlock_slot(slot, seq);
}

void TranspositionTable::prefetch(uint64_t own, uint64_t opponent, int32_t tag) const noexcept {
#if !defined(__EMSCRIPTEN__) && (defined(__x86_64__) || defined(_M_X64))
    const Slot* slot = &slots_[slot_index(own, opponent, tag)];
    _mm_prefetch(reinterpret_cast<const char*>(slot), _MM_HINT_T0);
#else
    (void)own;
    (void)opponent;
    (void)tag;
#endif
}

void TranspositionTable::clear() noexcept {
    generation_ = static_cast<uint16_t>(generation_ + 1U);
    if (generation_ == 0) {
        std::memset(slots_.get(), 0, count_ * sizeof(Slot));
        generation_ = 1;
    }
}

} // namespace stello::search
