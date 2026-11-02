//! What a search is asked to do and what it reports. C#: `Stello.Engine.SearchLimits`, `TimeControlMode`,
//! `ScoreKind`, `SearchInfo` and `SearchResult` (shared by all engines).

use std::fmt;
use std::time::Duration;

use crate::square::{Move, Square};

/// C#: `TimeControlMode`. The discriminants are the C# enum values.
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
#[repr(u8)]
pub enum TimeControlMode {
    /// Search a fixed number of plies (C++: sogedybde).
    FixedDepth = 0,
    /// A fixed thinking time for each move (C++: tid_per_trek).
    TimePerMove = 1,
    /// Share the remaining time for the whole game over the remaining moves (C++: spil_tid).
    TimePerGame = 2,
    /// Solve the position to the end with no time limit.
    Solve = 3,
}

/// C#: `SearchLimits`, without `UseOpeningBook` and `HashBits`, which only `ComputerPlayer` reads.
///
/// The time is in whole milliseconds: C#'s time control converts the `TimeSpan` with `(long)TotalMilliseconds`
/// before it uses it, so a wrapper passes that value.
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct SearchLimits {
    mode: TimeControlMode,
    depth: i32,
    time_ms: i64,
    threads: u32,
}

impl SearchLimits {
    /// The most threads a search may use ([`SearchLimits::with_threads`]). C#: `SearchLimits.MaxThreads`.
    pub const MAX_THREADS: u32 = 256;

    /// C#: `SearchLimits.Solve`.
    pub const SOLVE: SearchLimits = SearchLimits { mode: TimeControlMode::Solve, depth: 0, time_ms: 0, threads: 1 };

    /// C#: `SearchLimits.FixedDepth`. Panics unless `plies` is 1–60 (C# throws `ArgumentOutOfRangeException`).
    pub fn fixed_depth(plies: i32) -> SearchLimits {
        assert!((1..=60).contains(&plies), "a fixed depth is 1-60 plies");
        SearchLimits { mode: TimeControlMode::FixedDepth, depth: plies, time_ms: 0, threads: 1 }
    }

    /// C#: `SearchLimits.TimePerMove`. Panics unless the time is positive (C# throws `ArgumentOutOfRangeException`).
    pub fn time_per_move_ms(time_ms: i64) -> SearchLimits {
        assert!(time_ms > 0, "the time per move must be positive");
        SearchLimits { mode: TimeControlMode::TimePerMove, depth: 0, time_ms, threads: 1 }
    }

    /// C#: `SearchLimits.TimePerGame`: the time left on the computer's clock, which may be negative.
    pub fn time_per_game_ms(remaining_ms: i64) -> SearchLimits {
        SearchLimits { mode: TimeControlMode::TimePerGame, depth: 0, time_ms: remaining_ms, threads: 1 }
    }

    /// The limits from their parts, as a wrapper gets them from C#, or `None` if they are not valid: a fixed depth
    /// needs 1–60 plies, a time per move a time of at least 0 ms (C# requires a positive `TimeSpan`, which
    /// `(long)TotalMilliseconds` may round down to 0). The parts a mode does not use are ignored, as C# keeps them at
    /// zero. The limits use 1 thread.
    pub fn from_parts(mode: TimeControlMode, depth: i32, time_ms: i64) -> Option<SearchLimits> {
        match mode {
            TimeControlMode::FixedDepth if (1..=60).contains(&depth) => {
                Some(SearchLimits { mode, depth, time_ms: 0, threads: 1 })
            }
            TimeControlMode::TimePerMove if time_ms >= 0 => Some(SearchLimits { mode, depth: 0, time_ms, threads: 1 }),
            TimeControlMode::TimePerGame => Some(SearchLimits { mode, depth: 0, time_ms, threads: 1 }),
            TimeControlMode::Solve => Some(SearchLimits::SOLVE),
            _ => None,
        }
    }

    /// The same limits searched with `threads` threads (1 by default), or `None` unless it is 1 to
    /// [`SearchLimits::MAX_THREADS`]. C#: `SearchLimits.Threads`.
    ///
    /// With 1 thread the search runs on the caller's thread alone. More threads share the hash tables; a build
    /// without threads (WebAssembly without atomics) always searches with 1.
    pub fn with_threads(self, threads: u32) -> Option<SearchLimits> {
        (1..=SearchLimits::MAX_THREADS).contains(&threads).then_some(SearchLimits { threads, ..self })
    }

    /// C#: `SearchLimits.Mode`.
    pub fn mode(&self) -> TimeControlMode {
        self.mode
    }

    /// Plies to search in [`TimeControlMode::FixedDepth`]. C#: `SearchLimits.Depth`.
    pub fn depth(&self) -> i32 {
        self.depth
    }

    /// Time for this move, or the time left for the whole game, in milliseconds. C#: `(long)SearchLimits.Time.TotalMilliseconds`.
    pub fn time_ms(&self) -> i64 {
        self.time_ms
    }

    /// The threads to search with, 1 to [`SearchLimits::MAX_THREADS`]. C#: `SearchLimits.Threads`.
    pub fn threads(&self) -> u32 {
        self.threads
    }
}

/// C#: `ScoreKind`. The discriminants are the C# enum values.
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug, Default)]
#[repr(u8)]
pub enum ScoreKind {
    /// No search was needed (pass or only one legal move).
    #[default]
    None = 0,
    /// Evaluation units; beyond ±32600 the game is won or lost.
    Heuristic = 1,
    /// Only the sign is known: win (> 0), draw (0) or loss (< 0).
    WinLossDraw = 2,
    /// Final disc difference with perfect play.
    Exact = 3,
    /// The move comes from the opening book; the score is the book value (not used by the engine).
    Book = 4,
}

impl ScoreKind {
    /// The C# enum name, as the benchmarks' JSON writes it.
    pub const fn name(self) -> &'static str {
        match self {
            ScoreKind::None => "None",
            ScoreKind::Heuristic => "Heuristic",
            ScoreKind::WinLossDraw => "WinLossDraw",
            ScoreKind::Exact => "Exact",
            ScoreKind::Book => "Book",
        }
    }
}

impl fmt::Display for ScoreKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.name())
    }
}

/// Progress from a running search (C++: make_try/make_res). C#: `SearchInfo`.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct SearchInfo {
    /// Plies searched, or empty squares for the endgame search.
    pub depth: i32,
    pub current_move: Square,
    pub best_move: Option<Square>,
    pub score: i32,
    pub kind: ScoreKind,
    pub nodes: u64,
    pub evaluations: u64,
    pub elapsed: Duration,
}

/// C#: `SearchResult`.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct SearchResult {
    /// The move, or a pass when the side to move has none.
    pub mv: Move,
    pub score: i32,
    pub kind: ScoreKind,
    pub depth: i32,
    pub nodes: u64,
    pub evaluations: u64,
    pub elapsed: Duration,
}
