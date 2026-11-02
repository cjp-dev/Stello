//! Stello's Othello engine, ported from Stello 2.0's C# engine (`Stello.Net/Stello.Engine.V2`) and the shared types
//! of `Stello.Net/Stello.Engine`.
//!
//! Part 1 of the 3.0 plan keeps it node-for-node identical to the C# engine: the same moves, scores and node counts
//! (Plans/making-stello-3-plan.md). Bitboards are `u64` with bit n = square n = row * 8 + column (a1 = 0, h8 = 63).
//!
//! - [`square`], [`board`]: C# `Square`, `Move`, `Player`, `Board`;
//! - [`bits`]: C# `Bitboards` (legal moves, flips, potential mobility);
//! - [`endgame::bits`]: C# `EndgameBits` (the solver's bit helpers, also used by the midgame ordering);
//! - [`eval`]: C# `Evaluation/` (`EvalWeights`, the pattern sets, the symmetries) and `SearchEngine.PatternScore`;
//! - [`search`]: C# `SearchEngine` (the midgame search), `Search/` and the shared `SearchLimits`/`SearchResult` types;
//! - [`endgame`]: C# Endgame/ (`SearchEngine.Endgame.cs`, `LocalTable`, `StableDiscs`), the exact solver.

pub mod bits;
pub mod board;
pub mod cpu;
pub mod endgame;
pub mod eval;
pub mod search;
pub mod square;

pub use board::{Board, Player};
pub use eval::EvalWeights;
pub use search::{
    ScoreKind, SearchEngine, SearchError, SearchInfo, SearchLimits, SearchOptions, SearchResult, SearchSignals,
    StopSignal, TimeControlMode,
};
pub use square::{Move, Square};

/// The engine's version, for the wrapper's sanity check.
pub const VERSION: &str = env!("CARGO_PKG_VERSION");
