#include "search/table.hpp"

#include "core/cpu.hpp"

#include <cassert>
#include <cstring>
#include <memory>
#include <new>

namespace stello::search {

namespace {

void zero_slots(void* dst, size_t bytes) noexcept {
#if (defined(__x86_64__) || defined(_M_X64)) && !defined(__EMSCRIPTEN__)
    if (cpu::has_avx2_bmi2()) {
        detail::zero_slots_nt_avx2(dst, bytes);
        return;
    }
#endif
    std::memset(dst, 0, bytes);
}

} // namespace

void TranspositionTable::AlignedDeleter::operator()(Slot* ptr) const noexcept {
    if (ptr != nullptr) {
        ::operator delete[](ptr, std::align_val_t{64});
    }
}

TranspositionTable::TranspositionTable(uint32_t bits)
    : count_(size_t{1} << bits),
      shift_(64U - bits),
      generation_(1) {
    assert(bits >= 1 && bits < 64);
    void* raw = ::operator new[](count_ * sizeof(Slot), std::align_val_t{64});
    zero_slots(raw, count_ * sizeof(Slot));
    slots_.reset(static_cast<Slot*>(raw));
    assert((reinterpret_cast<uintptr_t>(slots_.get()) & 63ULL) == 0ULL);
}

void TranspositionTable::clear() noexcept {
    date_ = 0;
    generation_ = static_cast<uint8_t>((generation_ + 1U) & detail::GENERATION_MASK);
    if (generation_ == 0) {
        zero_slots(slots_.get(), count_ * sizeof(Slot));
        generation_ = 1;
    }
}

void TranspositionTable::store_bounds_at_root_slow(
    uint64_t own,
    uint64_t opponent,
    const StoreParams& p
) const noexcept {
    const detail::HashKey key = detail::compute_hash_key(own, opponent, p.tag, shift_);
    Slot& slot = slots_[key.index];

    const uint8_t d_clamped = static_cast<uint8_t>(std::clamp<int32_t>(p.depth, 0, 63));
    const uint8_t sel = static_cast<uint8_t>(p.selectivity & 0x7U);
    const uint32_t new_level = (static_cast<uint32_t>(d_clamped) << 3) | static_cast<uint32_t>(sel);
    const uint8_t new_cost = static_cast<uint8_t>(p.cost & 0x1FU);

    const int16_t fresh_lo = (p.score > p.alpha_in) ? static_cast<int16_t>(p.score) : TT_SCORE_MIN;
    const int16_t fresh_hi = (p.score < p.beta_in) ? static_cast<int16_t>(p.score) : TT_SCORE_MAX;

    Entry entry;
    entry.verify = key.verify;
    entry.mv = static_cast<int8_t>(p.mv);
    entry.depth = d_clamped;
    entry.selectivity = sel;
    entry.tag = static_cast<uint8_t>(p.tag & 0x3);
    entry.cost = new_cost;
    entry.date = date_;
    entry.generation = generation_;

    size_t victim_idx = 0;
    uint32_t victim_level = (std::numeric_limits<uint32_t>::max)();

    for (size_t i = 0; i < WAYS; ++i) {
        const uint64_t d = std::atomic_ref<uint64_t>(slot.entries[i].data).load(std::memory_order_relaxed);
        const uint64_t k = std::atomic_ref<uint64_t>(slot.entries[i].key_xor).load(std::memory_order_relaxed);
        const uint64_t v = k ^ d;
        const bool live = (static_cast<uint8_t>(d >> detail::GENERATION_SHIFT) == generation_)
                       && (static_cast<uint32_t>(v >> detail::VERIFY_EMPTIES_SHIFT) <= p.root_empties);
        if (!live) {
            if (victim_level != 0U) {
                victim_level = 0U;
                victim_idx = i;
            }
            continue;
        }
        if (v == key.verify && static_cast<int32_t>((d >> detail::TAG_SHIFT) & 0x3ULL) == p.tag) {
            const uint32_t old_level = static_cast<uint32_t>((d >> detail::SEL_SHIFT) & 0x1FFULL);
            if (new_level < old_level) {
                return;
            }
            const int16_t existing_lo = static_cast<int16_t>(static_cast<uint16_t>(d));
            const int16_t existing_hi = static_cast<int16_t>(static_cast<uint16_t>(d >> detail::UPPER_SHIFT));
            const int8_t existing_mv = static_cast<int8_t>(static_cast<int32_t>((d >> detail::MOVE_SHIFT) & 0x7FULL) - 1);
            const uint8_t existing_cost = static_cast<uint8_t>((d >> detail::COST_SHIFT) & 0x1FULL);
            if (new_level == old_level) {
                int16_t lo = existing_lo;
                int16_t hi = existing_hi;
                bool new_sets_lo = false;
                if (p.score < p.beta_in && p.score < hi) {
                    hi = static_cast<int16_t>(p.score);
                    if (p.score > p.alpha_in && p.score < lo) {
                        lo = static_cast<int16_t>(p.score);
                        new_sets_lo = true;
                    }
                }
                if (p.score > p.alpha_in && p.score > lo) {
                    lo = static_cast<int16_t>(p.score);
                    new_sets_lo = true;
                    if (p.score < p.beta_in && p.score > hi) {
                        hi = static_cast<int16_t>(p.score);
                    }
                }
                if (lo > hi) {
                    lo = fresh_lo;
                    hi = fresh_hi;
                    new_sets_lo = (fresh_lo > TT_SCORE_MIN);
                }
                entry.lower = lo;
                entry.upper = hi;
                const bool is_fresh_exact = (p.score > p.alpha_in) && (p.score < p.beta_in);
                const bool adopt_new_mv = (p.mv >= 0) && (
                    new_sets_lo
                    || is_fresh_exact
                    || existing_lo == TT_SCORE_MIN
                    || (existing_mv < 0 && p.score > p.alpha_in && p.score == lo)
                );
                entry.mv = adopt_new_mv ? static_cast<int8_t>(p.mv) : existing_mv;
            } else {
                entry.lower = fresh_lo;
                entry.upper = fresh_hi;
                entry.mv = (p.mv >= 0) ? static_cast<int8_t>(p.mv) : existing_mv;
            }
            entry.cost = (std::max)(new_cost, existing_cost);
            const uint64_t packed = entry.pack();
            std::atomic_ref<uint64_t>(slot.entries[i].key_xor).store(key.verify ^ packed, std::memory_order_relaxed);
            std::atomic_ref<uint64_t>(slot.entries[i].data).store(packed, std::memory_order_relaxed);
            return;
        }
        const uint32_t lvl = detail::writeable_level_packed(d, date_);
        if (lvl < victim_level) {
            victim_level = lvl;
            victim_idx = i;
        }
    }

    entry.lower = fresh_lo;
    entry.upper = fresh_hi;
    const uint64_t packed = entry.pack();
    std::atomic_ref<uint64_t>(slot.entries[victim_idx].key_xor).store(key.verify ^ packed, std::memory_order_relaxed);
    std::atomic_ref<uint64_t>(slot.entries[victim_idx].data).store(packed, std::memory_order_relaxed);
}

} // namespace stello::search
