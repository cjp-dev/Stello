//! The C ABI that .NET calls (`[LibraryImport("stello_engine")]`). The engine API comes in phase 2.

/// The engine version as major * 10000 + minor * 100 + patch, so .NET can check it loaded the right library.
#[unsafe(no_mangle)]
pub extern "C" fn stello_version() -> u32 {
    let mut parts = stello_core::VERSION.split('.').map(|part| part.parse::<u32>().unwrap_or(0));
    let (major, minor, patch) = (parts.next().unwrap_or(0), parts.next().unwrap_or(0), parts.next().unwrap_or(0));
    major * 10_000 + minor * 100 + patch
}
