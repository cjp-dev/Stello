#include "search/table.hpp"

#include <cassert>
#include <cstring>
#include <memory>
#include <new>

namespace stello::search {

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

void TranspositionTable::clear() noexcept {
    generation_ = static_cast<uint16_t>(generation_ + 1U);
    if (generation_ == 0) {
        std::memset(slots_.get(), 0, count_ * sizeof(Slot));
        generation_ = 1;
    }
}

} // namespace stello::search
