#pragma once

namespace stello::cpu {

/// Whether the evaluation may use BMI2 `pext` (always `false` off x86-64 or in portable mode).
bool fast_pext() noexcept;

/// `true` keeps every search on the portable code path; `false` re-enables processor fast paths.
void set_portable_only(bool portable) noexcept;

} // namespace stello::cpu
