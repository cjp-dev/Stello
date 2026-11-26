#include "endgame/stable.hpp"

#include "core/compiler.hpp"

#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <thread>
#include <utility>
#include <vector>

namespace stello::endgame::stable {

namespace {

constexpr uint8_t STATE_UNINIT = 0;
constexpr uint8_t STATE_BUILDING = 1;
constexpr uint8_t STATE_READY = 2;

alignas(64) std::array<uint8_t, 256U * 256U> g_edge_table{};
std::atomic<uint8_t> g_edges_state{STATE_UNINIT};

std::pair<int32_t, int32_t> play_on_line(int32_t p, int32_t o, int32_t place) noexcept {
    const uint32_t pu = static_cast<uint32_t>(p);
    const uint32_t ou = static_cast<uint32_t>(o);
    uint32_t np = pu | (uint32_t{1} << place);

    int32_t i = place - 1;
    while (i > 0 && ((ou >> i) & 1U) != 0) {
        --i;
    }

    // Mask shift count by 31 when i reaches -1 (matching Rust/C# shift semantics without UB).
    if (((pu >> (i & 31)) & 1U) != 0) {
        for (int32_t j = place - 1; j >= i + 1; --j) {
            np ^= (uint32_t{1} << j);
        }
    }

    i = place + 1;
    while (i < 7 && ((ou >> i) & 1U) != 0) {
        ++i;
    }

    if (((pu >> (i & 31)) & 1U) != 0) {
        for (int32_t j = place + 1; j < i; ++j) {
            np ^= (uint32_t{1} << j);
        }
    }

    return {static_cast<int32_t>(np), static_cast<int32_t>(ou & ~np)};
}

int32_t line_stability(int32_t b, int32_t w, std::vector<int16_t>& memo) noexcept {
    const size_t idx = static_cast<size_t>(b) * 256U + static_cast<size_t>(w);
    const int16_t known = memo[idx];
    if (known >= 0) {
        return static_cast<int32_t>(known);
    }

    int32_t result = b | w;
    const int32_t empties = (~result) & 0xFF;
    for (int32_t i = 0; i < 8; ++i) {
        if (((empties >> i) & 1) != 0) {
            const auto [nb1, nw1] = play_on_line(b, w, i);
            result &= b | nw1;
            result &= line_stability(nb1, nw1, memo);

            const auto [nw2, nb2] = play_on_line(w, b, i);
            result &= w | nb2;
            result &= line_stability(nb2, nw2, memo);
        }
    }

    memo[idx] = static_cast<int16_t>(result);
    return result;
}

STELLO_NOINLINE void build_edges_slow() noexcept {
    uint8_t expected = STATE_UNINIT;
    if (g_edges_state.compare_exchange_strong(
            expected, STATE_BUILDING, std::memory_order_acq_rel, std::memory_order_acquire))
    {
        std::vector<int16_t> memo(256U * 256U, -1);

        for (size_t b = 0; b < 256U; ++b) {
            for (size_t w = b; w < 256U; ++w) {
                if ((b & w) != 0) {
                    continue;
                }

                const int32_t st = line_stability(static_cast<int32_t>(b), static_cast<int32_t>(w), memo);
                g_edge_table[b * 256U + w] = static_cast<uint8_t>(st & static_cast<int32_t>(b));
                g_edge_table[w * 256U + b] = static_cast<uint8_t>(st & static_cast<int32_t>(w));
            }
        }

        g_edges_state.store(STATE_READY, std::memory_order_release);
        return;
    }

    while (g_edges_state.load(std::memory_order_acquire) != STATE_READY) {
        std::this_thread::yield();
    }
}

inline void ensure_edges() noexcept {
    if (g_edges_state.load(std::memory_order_acquire) != STATE_READY) [[unlikely]] {
        build_edges_slow();
    }
}

inline uint64_t full_rows(uint64_t full) noexcept {
    full &= full >> 1;
    full &= full >> 2;
    full &= full >> 4;
    return (full & 0x0101'0101'0101'0101ULL) * 0xFFULL;
}

inline uint64_t full_columns(uint64_t full) noexcept {
    full &= std::rotr(full, 8);
    full &= std::rotr(full, 16);
    full &= std::rotr(full, 32);
    return full;
}

inline std::pair<uint64_t, uint64_t> full_diagonals(uint64_t full) noexcept {
    constexpr uint64_t EDGE = 0xFF81'8181'8181'81FFULL;
    uint64_t l7 = full;
    uint64_t r7 = full;
    l7 &= EDGE | (l7 >> 7);
    r7 &= EDGE | (r7 << 7);
    l7 &= 0xFFFF'0303'0303'0303ULL | (l7 >> 14);
    r7 &= 0xC0C0'C0C0'C0C0'FFFFULL | (r7 << 14);
    l7 &= 0xFFFF'FFFF'0F0F'0F0FULL | (l7 >> 28);
    r7 &= 0xF0F0'F0F0'FFFF'FFFFULL | (r7 << 28);
    const uint64_t full_d7 = l7 & r7;

    uint64_t l9 = full;
    uint64_t r9 = full;
    l9 &= EDGE | (l9 >> 9);
    r9 &= EDGE | (r9 << 9);
    l9 &= 0xFFFF'C0C0'C0C0'C0C0ULL | (l9 >> 18);
    r9 &= 0x0303'0303'0303'FFFFULL | (r9 << 18);
    const uint64_t full_d9 = l9 & r9 & (0x0F0F'0F0F'F0F0'F0F0ULL | (l9 >> 36) | (r9 << 36));
    return {full_d7, full_d9};
}

} // namespace

void init() noexcept {
    ensure_edges();
}

const uint8_t* edge_table() noexcept {
    ensure_edges();
    return g_edge_table.data();
}

int32_t edge_stability_count(uint64_t own, uint64_t opponent) noexcept {
    ensure_edges();
    const uint32_t packed =
        static_cast<uint32_t>(g_edge_table[((own & 0xFFU) << 8) | (opponent & 0xFFU)])
        | (static_cast<uint32_t>(g_edge_table[((own >> 56) << 8) | (opponent >> 56)]) << 8)
        | (static_cast<uint32_t>(g_edge_table[(pack_a1a8(own) << 8) | pack_a1a8(opponent)]) << 16)
        | (static_cast<uint32_t>(g_edge_table[(pack_h1h8(own) << 8) | pack_h1h8(opponent)]) << 24);
    return static_cast<int32_t>(std::popcount(packed & 0xFFFF'7E7EU));
}

int32_t corner_stability(uint64_t discs) noexcept {
    const uint32_t p2187 = static_cast<uint32_t>(std::rotr(discs, 48));
    uint32_t stable = 0x0081'8100U & p2187;
    stable |= ((((stable * 5U) >> 1) & 0x0042'4200U) | (stable << 8) | (stable >> 8)) & p2187;
    return static_cast<int32_t>(std::popcount(stable));
}

int32_t bit_weighted_count(uint64_t x) noexcept {
    const uint32_t ah18 = static_cast<uint32_t>(std::rotr(x, 56)) & 0x8181U;
    return static_cast<int32_t>(std::popcount(x)) + static_cast<int32_t>(std::popcount(ah18));
}

uint64_t transpose_portable(uint64_t x) noexcept {
    uint64_t t = (x ^ (x >> 7)) & 0x00AA'00AA'00AA'00AAULL;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & 0x0000'CCCC'0000'CCCCULL;
    x = x ^ t ^ (t << 14);
    t = (x ^ (x >> 28)) & 0x0000'0000'F0F0'F0F0ULL;
    x = x ^ t ^ (t << 28);
    return x;
}

uint64_t of(uint64_t own, uint64_t opponent) noexcept {
    ensure_edges();
    uint64_t stable_mask = 0;
    const uint64_t inner = own & 0x007E'7E7E'7E7E'7E00ULL;

    uint64_t found = static_cast<uint64_t>(g_edge_table[((own & 0xFFULL) << 8) | (opponent & 0xFFULL)])
                   | (static_cast<uint64_t>(g_edge_table[((own >> 56) << 8) | (opponent >> 56)]) << 56)
                   | unpack_a2a7(g_edge_table[(pack_a1a8(own) << 8) | pack_a1a8(opponent)])
                   | unpack_h2h7(g_edge_table[(pack_h1h8(own) << 8) | pack_h1h8(opponent)]);

    const uint64_t discs = own | opponent;
    const uint64_t full_h = full_rows(discs);
    const uint64_t full_v = full_columns(discs);
    const auto [full_d7, full_d9] = full_diagonals(discs);
    found |= full_h & full_v & full_d7 & full_d9 & inner;

    while ((found & ~stable_mask) != 0) {
        stable_mask |= found;
        const uint64_t h = (stable_mask >> 1) | (stable_mask << 1) | full_h;
        const uint64_t v = (stable_mask >> 8) | (stable_mask << 8) | full_v;
        const uint64_t d7 = (stable_mask >> 7) | (stable_mask << 7) | full_d7;
        const uint64_t d9 = (stable_mask >> 9) | (stable_mask << 9) | full_d9;
        found = h & v & d7 & d9 & inner;
    }

    return stable_mask;
}

} // namespace stello::endgame::stable
