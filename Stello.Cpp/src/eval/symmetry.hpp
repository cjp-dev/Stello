#pragma once

#include "core/compiler.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace stello::eval::symmetry {

inline constexpr size_t COUNT = 8;

inline uint64_t mirror_columns(uint64_t x) noexcept {
    x = ((x >> 1) & 0x5555'5555'5555'5555ULL) | ((x & 0x5555'5555'5555'5555ULL) << 1);
    x = ((x >> 2) & 0x3333'3333'3333'3333ULL) | ((x & 0x3333'3333'3333'3333ULL) << 2);
    return ((x >> 4) & 0x0F0F'0F0F'0F0F'0F0FULL) | ((x & 0x0F0F'0F0F'0F0F'0F0FULL) << 4);
}

inline uint64_t transpose(uint64_t x) noexcept {
    uint64_t t = 0x0F0F'0F0F'0000'0000ULL & (x ^ (x << 28));
    x ^= t ^ (t >> 28);
    t = 0x3333'0000'3333'0000ULL & (x ^ (x << 14));
    x ^= t ^ (t >> 14);
    t = 0x5500'5500'5500'5500ULL & (x ^ (x << 7));
    return x ^ t ^ (t >> 7);
}

inline uint64_t apply(uint64_t x, size_t sym) noexcept {
    if ((sym & 1U) != 0) {
        x = mirror_columns(x);
    }
    if ((sym & 2U) != 0) {
        x = byteswap64(x);
    }
    if ((sym & 4U) != 0) {
        x = transpose(x);
    }
    return x;
}

inline uint32_t map_square(uint32_t square, size_t sym) noexcept {
    return static_cast<uint32_t>(std::countr_zero(apply(uint64_t{1} << square, sym)));
}

} // namespace stello::eval::symmetry
