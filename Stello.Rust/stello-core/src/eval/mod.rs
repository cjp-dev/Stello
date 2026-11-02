//! The pattern evaluation. C#: `Stello.Engine.V2.Evaluation` (`EvalWeights`, `PatternShape`, `PatternSet`,
//! `Symmetry`), and the search's scaling of it (`SearchEngine.PatternScore`).

pub mod patterns;
pub mod symmetry;
pub mod weights;

pub use patterns::{PatternSet, PatternShape};
pub use weights::{EvalError, EvalWeights};

/// The midgame search's unit: scores are in 1/100 discs. C#: `SearchEngine.PatternUnitsPerDisc`.
pub const PATTERN_UNITS_PER_DISC: i32 = 100;

/// The evaluation in the search's units (1/100 discs), capped at ±64 discs, the largest possible result.
/// C#: `SearchEngine.PatternScore` (C# integer division, which truncates towards zero as Rust's does).
#[inline]
pub fn pattern_score(weights: &EvalWeights, own: u64, opponent: u64) -> i32 {
    (weights.evaluate(own, opponent) * PATTERN_UNITS_PER_DISC / weights.units_per_disc())
        .clamp(-64 * PATTERN_UNITS_PER_DISC, 64 * PATTERN_UNITS_PER_DISC)
}

/// The evaluation in whole discs, capped at ±64: the endgame's ordering value at depth 0.
/// C#: `SearchEngine.OrderingValue` (Endgame/SearchEngine.Endgame.cs) when `depth == 0`.
#[inline]
pub fn disc_score(weights: &EvalWeights, own: u64, opponent: u64) -> i32 {
    (weights.evaluate(own, opponent) / weights.units_per_disc()).clamp(-64, 64)
}
