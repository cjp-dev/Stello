#pragma once

#include <cstdint>

#if defined(_MSC_VER)
#include <cstdlib>
#define STELLO_FORCEINLINE __forceinline
#define STELLO_NOINLINE __declspec(noinline)
#else
#define STELLO_FORCEINLINE inline __attribute__((always_inline))
#define STELLO_NOINLINE __attribute__((noinline))
#endif

namespace stello {

inline constexpr int32_t wrapping_add(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

inline constexpr int16_t wrapping_add_i16(int16_t a, int16_t b) noexcept {
    return static_cast<int16_t>(static_cast<uint16_t>(a) + static_cast<uint16_t>(b));
}

inline uint64_t byteswap64(uint64_t x) noexcept {
#if defined(_MSC_VER) && !defined(__clang__)
    return _byteswap_uint64(x);
#elif defined(__clang__) || defined(__GNUC__)
    return __builtin_bswap64(x);
#else
    return ((x & 0x0000'0000'0000'00FFULL) << 56)
         | ((x & 0x0000'0000'0000'FF00ULL) << 40)
         | ((x & 0x0000'0000'00FF'0000ULL) << 24)
         | ((x & 0x0000'0000'FF00'0000ULL) << 8)
         | ((x & 0x0000'00FF'0000'0000ULL) >> 8)
         | ((x & 0x0000'FF00'0000'0000ULL) >> 24)
         | ((x & 0x00FF'0000'0000'0000ULL) >> 40)
         | ((x & 0xFF00'0000'0000'0000ULL) >> 56);
#endif
}

} // namespace stello
