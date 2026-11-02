//! Stello's Othello engine, ported from Stello 2.0's C# engine (`Stello.Net/Stello.Engine.V2`).
//!
//! Part 1 of the 3.0 plan keeps it node-for-node identical to the C# engine: the same moves, scores and node counts
//! (Plans/making-stello-3-plan.md). The port starts in phase 1.

/// The engine's version, for the wrapper's sanity check.
pub const VERSION: &str = env!("CARGO_PKG_VERSION");
