#include "core/cpu.hpp"

#include "core/compiler.hpp"

#include <atomic>
#include <cstdint>

#if (defined(__x86_64__) || defined(_M_X64)) && !defined(__EMSCRIPTEN__)
#define STELLO_X86_64_CPUID 1
#ifdef _WIN32
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#else
#define STELLO_X86_64_CPUID 0
#endif

namespace stello::cpu {
namespace {

constexpr uint8_t kUnknown = 0;
constexpr uint8_t kInitializedFlag = 1 << 0;
constexpr uint8_t kFastPextFlag = 1 << 1;
constexpr uint8_t kAvx2Bmi2Flag = 1 << 2;
constexpr uint8_t kSse42Flag = 1 << 3;
constexpr uint8_t kGfniFlag = 1 << 4;
constexpr uint8_t kAvx512V4Flag = 1 << 5;

std::atomic<uint8_t> g_hw_state{kUnknown};
std::atomic<uint8_t> g_state{kUnknown};

#if STELLO_X86_64_CPUID

void query_cpuid(int leaf, int subleaf, int regs[4]) noexcept {
#ifdef _WIN32
    __cpuidex(regs, leaf, subleaf);
#else
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    __cpuid_count(static_cast<unsigned int>(leaf), static_cast<unsigned int>(subleaf), eax, ebx, ecx, edx);
    regs[0] = static_cast<int>(eax);
    regs[1] = static_cast<int>(ebx);
    regs[2] = static_cast<int>(ecx);
    regs[3] = static_cast<int>(edx);
#endif
}

uint64_t read_xcr0() noexcept {
#if defined(_MSC_VER) && !defined(__clang__)
    return _xgetbv(0);
#else
    uint32_t eax = 0;
    uint32_t edx = 0;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
    return (static_cast<uint64_t>(edx) << 32) | static_cast<uint64_t>(eax);
#endif
}

bool slow_pext() noexcept {
    int vendor[4]{};
    query_cpuid(0, 0, vendor);
    const uint32_t ebx = static_cast<uint32_t>(vendor[1]);
    const uint32_t ecx = static_cast<uint32_t>(vendor[2]);
    const uint32_t edx = static_cast<uint32_t>(vendor[3]);

    const bool amd = (ebx == 0x6874'7541U && edx == 0x6974'6E65U && ecx == 0x444D'4163U);   // "AuthenticAMD"
    const bool hygon = (ebx == 0x6F67'7948U && edx == 0x6E65'476EU && ecx == 0x656E'6975U); // "HygonGenuine"

    int leaf1[4]{};
    query_cpuid(1, 0, leaf1);
    const uint32_t signature = static_cast<uint32_t>(leaf1[0]);
    const uint32_t base_family = (signature >> 8) & 0xFU;
    const uint32_t family = (base_family == 0xFU) ? base_family + ((signature >> 20) & 0xFFU) : base_family;
    return (amd || hygon) && family < 0x19U;
}

uint8_t detect_features() noexcept {
    int leaf0[4]{};
    query_cpuid(0, 0, leaf0);
    const uint32_t max_leaf = static_cast<uint32_t>(leaf0[0]);

    int leaf1[4]{};
    query_cpuid(1, 0, leaf1);
    const uint32_t leaf1_ecx = static_cast<uint32_t>(leaf1[2]);
    const bool has_sse42_hw = (leaf1_ecx & (1U << 20)) != 0;
    const bool has_popcnt = (leaf1_ecx & (1U << 23)) != 0;
    const bool has_osxsave = (leaf1_ecx & (1U << 27)) != 0;
    const bool has_avx = (leaf1_ecx & (1U << 28)) != 0;

    uint8_t flags = kInitializedFlag;
    if (has_sse42_hw) {
        flags = static_cast<uint8_t>(flags | kSse42Flag);
    }
    if (max_leaf < 7U) {
        return flags;
    }

    int leaf7[4]{};
    query_cpuid(7, 0, leaf7);
    const uint32_t leaf7_ebx = static_cast<uint32_t>(leaf7[1]);
    const uint32_t leaf7_ecx = static_cast<uint32_t>(leaf7[2]);
    const bool has_bmi1 = (leaf7_ebx & (1U << 3)) != 0;
    const bool has_avx2 = (leaf7_ebx & (1U << 5)) != 0;
    const bool has_bmi2 = (leaf7_ebx & (1U << 8)) != 0;
    const bool has_gfni_hw = (leaf7_ecx & (1U << 8)) != 0;
    const bool fast_pext_hw = has_bmi2 && !slow_pext();

    if (fast_pext_hw) {
        flags = static_cast<uint8_t>(flags | kFastPextFlag);
    }
    if (!fast_pext_hw || !has_bmi1 || !has_avx2 || !has_popcnt || !has_osxsave || !has_avx) {
        return flags;
    }

    const uint64_t xcr0 = read_xcr0();
    if ((xcr0 & 0x6ULL) != 0x6ULL) {
        return flags;
    }

    int ext0[4]{};
    query_cpuid(static_cast<int>(0x8000'0000U), 0, ext0);
    if (static_cast<uint32_t>(ext0[0]) < 0x8000'0001U) {
        return flags;
    }

    int ext1[4]{};
    query_cpuid(static_cast<int>(0x8000'0001U), 0, ext1);
    const bool has_lzcnt = (static_cast<uint32_t>(ext1[2]) & (1U << 5)) != 0;
    if (!has_lzcnt) {
        return flags;
    }
    flags = static_cast<uint8_t>(flags | kAvx2Bmi2Flag);

    if (has_gfni_hw) {
        flags = static_cast<uint8_t>(flags | kGfniFlag);
    }

    const bool has_avx512f = (leaf7_ebx & (1U << 16)) != 0;
    const bool has_avx512dq = (leaf7_ebx & (1U << 17)) != 0;
    const bool has_avx512cd = (leaf7_ebx & (1U << 28)) != 0;
    const bool has_avx512bw = (leaf7_ebx & (1U << 30)) != 0;
    const bool has_avx512vl = (leaf7_ebx & (1U << 31)) != 0;
    const bool os_avx512 = (xcr0 & 0xE6ULL) == 0xE6ULL;
    if (has_sse42_hw && has_gfni_hw && has_avx512f && has_avx512dq && has_avx512cd && has_avx512bw
        && has_avx512vl && os_avx512)
    {
        flags = static_cast<uint8_t>(flags | kAvx512V4Flag);
    }
    return flags;
}

#else

uint8_t detect_features() noexcept {
    return kInitializedFlag;
}

#endif

uint8_t hw_flags() noexcept {
    const uint8_t cached = g_hw_state.load(std::memory_order_relaxed);
    if (cached != kUnknown) {
        return cached;
    }
    const uint8_t detected = detect_features();
    uint8_t expected = kUnknown;
    if (g_hw_state.compare_exchange_strong(expected, detected, std::memory_order_relaxed)) {
        return detected;
    }
    return expected;
}

STELLO_NOINLINE uint8_t detect() noexcept {
    const uint8_t detected = hw_flags();
    uint8_t expected = kUnknown;
    if (g_state.compare_exchange_strong(expected, detected, std::memory_order_relaxed)) {
        return detected;
    }
    return expected;
}

uint8_t feature_flags() noexcept {
    const uint8_t state = g_state.load(std::memory_order_relaxed);
    if (state == kUnknown) {
        return detect();
    }
    return state;
}

} // namespace

bool fast_pext() noexcept {
    return (feature_flags() & kFastPextFlag) != 0;
}

bool has_sse42() noexcept {
    return (feature_flags() & kSse42Flag) != 0;
}

bool has_avx2_bmi2() noexcept {
    return (feature_flags() & kAvx2Bmi2Flag) != 0;
}

bool has_avx512_v4() noexcept {
    return (feature_flags() & kAvx512V4Flag) != 0;
}

bool has_gfni() noexcept {
    return (feature_flags() & kGfniFlag) != 0;
}

CpuTier active_tier() noexcept {
    const uint8_t flags = feature_flags();
    if ((flags & kAvx512V4Flag) != 0) {
        return CpuTier::V4;
    }
    if ((flags & kAvx2Bmi2Flag) != 0) {
        return CpuTier::V3;
    }
    return CpuTier::Portable;
}

bool set_forced_tier(std::optional<CpuTier> tier) noexcept {
    const uint8_t hw = hw_flags();
    if (!tier.has_value()) {
        g_state.store(hw, std::memory_order_relaxed);
        return true;
    }
    switch (*tier) {
        case CpuTier::Portable:
            g_state.store(kInitializedFlag, std::memory_order_relaxed);
            return true;
        case CpuTier::V3:
            if ((hw & kAvx2Bmi2Flag) == 0) {
                return false;
            }
            g_state.store(static_cast<uint8_t>(hw & ~kAvx512V4Flag), std::memory_order_relaxed);
            return true;
        case CpuTier::V4:
            if ((hw & kAvx512V4Flag) == 0) {
                return false;
            }
            g_state.store(hw, std::memory_order_relaxed);
            return true;
    }
    return false;
}

void set_portable_only(bool portable) noexcept {
    (void)set_forced_tier(portable ? std::optional<CpuTier>(CpuTier::Portable) : std::nullopt);
}

} // namespace stello::cpu
