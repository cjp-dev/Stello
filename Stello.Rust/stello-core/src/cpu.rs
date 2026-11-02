//! Which processor-specific fast paths the engine may use, decided once at run time. The shipped libraries are built
//! for plain x86-64 (and WebAssembly), so the fast paths are `#[target_feature]` functions chosen here:
//!
//! - BMI2 `pext` for the evaluation's pattern indices ([`crate::eval::PatternSet`]), where it is fast: Intel since
//!   Haswell and AMD since Zen 3. AMD's Zen 1 and 2 (and Hygon's Zen 1 copies) run `pext` in microcode, taking dozens
//!   of cycles, so they keep the portable path.
//!
//! Every path gives exactly the same results; only the speed differs. [`set_portable_only`] turns the fast paths off,
//! so that tests and the command-line tool can check the portable code on a desktop processor too.

use std::sync::atomic::{AtomicU8, Ordering};

const UNKNOWN: u8 = 0;
const PORTABLE: u8 = 1;
const FAST_PEXT: u8 = 2;

static STATE: AtomicU8 = AtomicU8::new(UNKNOWN);

/// Whether the evaluation may use BMI2 `pext` (always `false` off x86-64).
#[inline]
pub fn fast_pext() -> bool {
    match STATE.load(Ordering::Relaxed) {
        UNKNOWN => detect() == FAST_PEXT,
        state => state == FAST_PEXT,
    }
}

/// `true` keeps every search on the portable code (the code WebAssembly runs); `false` lets the processor's fast
/// paths be used again. For checks: the results are the same either way.
pub fn set_portable_only(portable: bool) {
    STATE.store(if portable { PORTABLE } else { detect_features() }, Ordering::Relaxed);
}

#[cold]
fn detect() -> u8 {
    let state = detect_features();
    STATE.store(state, Ordering::Relaxed);
    state
}

#[cfg(target_arch = "x86_64")]
fn detect_features() -> u8 {
    if std::arch::is_x86_feature_detected!("bmi2") && !slow_pext() { FAST_PEXT } else { PORTABLE }
}

#[cfg(not(target_arch = "x86_64"))]
fn detect_features() -> u8 {
    PORTABLE
}

/// AMD and Hygon processors before family 19h (Zen 3) run `pext` and `pdep` in microcode.
#[cfg(target_arch = "x86_64")]
fn slow_pext() -> bool {
    use std::arch::x86_64::__cpuid;

    // Leaves 0 and 1 exist on every x86-64 processor.
    let (vendor, signature) = (__cpuid(0), __cpuid(1).eax);
    let amd = (vendor.ebx, vendor.edx, vendor.ecx) == (0x6874_7541, 0x6974_6E65, 0x444D_4163); // "AuthenticAMD"
    let hygon = (vendor.ebx, vendor.edx, vendor.ecx) == (0x6F67_7948, 0x6E65_476E, 0x656E_6975); // "HygonGenuine"
    let base_family = (signature >> 8) & 0xF;
    let family = if base_family == 0xF { base_family + ((signature >> 20) & 0xFF) } else { base_family };
    (amd || hygon) && family < 0x19
}

#[cfg(test)]
mod tests {
    #[test]
    #[cfg(target_arch = "x86_64")]
    fn vendor_strings_are_read_as_cpuid_returns_them() {
        // "AuthenticAMD" is returned in ebx, edx, ecx as little-endian ASCII.
        assert_eq!(u32::from_le_bytes(*b"Auth"), 0x6874_7541);
        assert_eq!(u32::from_le_bytes(*b"enti"), 0x6974_6E65);
        assert_eq!(u32::from_le_bytes(*b"cAMD"), 0x444D_4163);
        assert_eq!(u32::from_le_bytes(*b"Hygo"), 0x6F67_7948);
        assert_eq!(u32::from_le_bytes(*b"nGen"), 0x6E65_476E);
        assert_eq!(u32::from_le_bytes(*b"uine"), 0x656E_6975);
    }
}
