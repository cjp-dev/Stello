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
constexpr uint8_t kPortable = 1;
constexpr uint8_t kFastPext = 2;

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
    if (static_cast<uint32_t>(leaf0[0]) < 7U) {
        return kPortable;
    }
    int leaf7[4]{};
    query_cpuid(7, 0, leaf7);
    const bool has_bmi2 = (static_cast<uint32_t>(leaf7[1]) & (1U << 8)) != 0;
    return (has_bmi2 && !slow_pext()) ? kFastPext : kPortable;
}

#else

uint8_t detect_features() noexcept {
    return kPortable;
}

#endif

STELLO_NOINLINE uint8_t detect() noexcept {
    const uint8_t state = detect_features();
    g_state.store(state, std::memory_order_relaxed);
    return state;
}

} // namespace

bool fast_pext() noexcept {
    const uint8_t state = g_state.load(std::memory_order_relaxed);
    if (state == kUnknown) {
        return detect() == kFastPext;
    }
    return state == kFastPext;
}

void set_portable_only(bool portable) noexcept {
    g_state.store(portable ? kPortable : detect_features(), std::memory_order_relaxed);
}

} // namespace stello::cpu
