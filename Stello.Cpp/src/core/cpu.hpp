#pragma once

#include <cstdint>
#include <optional>

namespace stello::cpu {

enum class CpuTier : uint8_t {
    Portable = 0,
    V3 = 1,
    V4 = 2,
};

/// Whether the evaluation may use BMI2 `pext` (always `false` off x86-64 or in portable mode).
bool fast_pext() noexcept;

/// Whether the search may use SSE4.2 instructions (`crc32`, not portable-only).
bool has_sse42() noexcept;

/// Whether the search may use x86-64-v3 (`AVX2 + BMI1 + BMI2 + POPCNT + LZCNT`, fast `pext`, not portable-only).
bool has_avx2_bmi2() noexcept;

/// Whether the search may use x86-64-v4 256-bit (`AVX-512F/VL/CD/DQ/BW + GFNI` + V3, not portable-only/v3-forced).
bool has_avx512_v4() noexcept;

/// Whether GFNI is available in the active tier.
bool has_gfni() noexcept;

/// Active CPU dispatch tier (auto-detected on first query unless overridden).
CpuTier active_tier() noexcept;

/// Forces a specific tier (or clears override if `std::nullopt`); returns `false` if hardware lacks requested tier.
bool set_forced_tier(std::optional<CpuTier> tier) noexcept;

/// `true` keeps every search on the portable code path; `false` re-enables processor fast paths.
void set_portable_only(bool portable) noexcept;

} // namespace stello::cpu
