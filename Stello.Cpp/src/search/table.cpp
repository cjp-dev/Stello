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
    : count_(size_t{1} << bits), shift_(64U - bits), generation_(1) {
    assert(bits >= 1 && bits < 64);
    void* raw = ::operator new[](count_ * sizeof(Slot), std::align_val_t{64});
    zero_slots(raw, count_ * sizeof(Slot));
    slots_.reset(static_cast<Slot*>(raw));
    assert((reinterpret_cast<uintptr_t>(slots_.get()) & 63ULL) == 0ULL);
}

void TranspositionTable::clear() noexcept {
    generation_ = static_cast<uint16_t>(generation_ + 1U);
    if (generation_ == 0) {
        zero_slots(slots_.get(), count_ * sizeof(Slot));
        generation_ = 1;
    }
}

} // namespace stello::search
