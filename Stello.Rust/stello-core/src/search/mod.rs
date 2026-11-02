//! Stello 2's search: iterative deepening with two hash tables, response killers, Multi-ProbCut and the pattern
//! evaluation, handing over to the endgame solver near the end. C#: `Stello.Engine.V2.SearchEngine` (SearchEngine.cs)
//! and `Search/`; the solver is [`crate::endgame::search`] (C#: Endgame/SearchEngine.Endgame.cs).
//!
//! - [`limits`]: C# `SearchLimits`, `TimeControlMode`, `ScoreKind`, `SearchInfo`, `SearchResult`;
//! - [`options`]: C# `SearchOptions`;
//! - [`table`]: C# `TranspositionTable`, `Bound`;
//! - [`ordering`]: C# `MoveOrdering`;
//! - [`time`]: C# `TimeControl`, `TimeBudget`;
//! - [`engine`]: C# `SearchEngine` (the midgame search and the root of the solver), with the stop signals and the
//!   clock.

pub mod engine;
pub mod limits;
pub mod options;
pub mod ordering;
pub mod table;
pub mod time;

pub use engine::{
    Clock, DEFAULT_PROBCUT_PERCENT, NoStop, SearchEngine, SearchError, SearchSignals, StopSignal, WIN_SCORE,
    system_clock,
};
pub use limits::{ScoreKind, SearchInfo, SearchLimits, SearchResult, TimeControlMode};
pub use options::SearchOptions;
