//! C#: `Stello.Engine.V2.SearchEngine` (SearchEngine.cs): the midgame search ported from Minmax.cpp and Kontrol.cpp
//! (iterative deepening with a hash table, response killers and selective search), with the trained pattern
//! evaluation, and the root of the endgame solver.
//!
//! The C# class is split in two: [`SearchEngine`] holds what lives from search to search (the options, the tables,
//! the response table), and [`Searcher`] is one running search (the counters, the clock, the progress callback, the
//! stop signals) with borrows of the engine's tables. The midgame search is an `impl Searcher` block here; the
//! endgame solver (C#'s partial class Endgame/SearchEngine.Endgame.cs) is another one in [`crate::endgame::search`].
//!
//! C# throws to stop a search; here every search function returns `Result<_, Abort>` and `?` unwinds.

use std::sync::atomic::{AtomicBool, AtomicI32, AtomicU64, Ordering};
use std::time::Duration;

use super::limits::{ScoreKind, SearchInfo, SearchLimits, SearchResult, TimeControlMode};
use super::options::SearchOptions;
use super::ordering::{self, MoveOrdering};
use super::pool::{Spawner, ThreadPool, std_spawner};
use super::table::{Bound, TranspositionTable};
use super::time::{self, TimeBudget};
use super::ybwc::{MidgameChild, SplitPoint};
use crate::bits;
use crate::board::{Board, Player};
use crate::endgame;
use crate::endgame::search::{
    EndgameState, ORDER_MOBILITY, ORDER_OFFSET, ORDER_POTENTIAL_MOBILITY, ORDER_VALUE, SOLVE_ORDERING_LOOKS,
};
use crate::eval::{self, EvalWeights, PATTERN_UNITS_PER_DISC};
use crate::square::{Move, Square};

pub(crate) const INFINITY: i32 = 32767;
const ROOT_WINDOW: i32 = 32664;

// C++: the endgame solver takes over when (allway - varlook) <= 7.
const ENDGAME_DISTANCE: i32 = 7;

// No Multi-ProbCut near won or lost scores.
const PROBCUT_SCORE_LIMIT: i32 = 32000;

/// A finished game in the midgame search scores this plus the disc difference (C++: the same).
pub const WIN_SCORE: i32 = 32600;

/// Multi-ProbCut's default t × 100 (Egaroucid's multi_probcut.hpp, with an error model fitted on Stello 2's own
/// searches). C#: `SearchEngine.DefaultProbCutPercent`.
pub const DEFAULT_PROBCUT_PERCENT: i32 = 100;

// From depth 3 (look 2); a cut needs the shallow search to be t standard deviations outside the window.
const PROBCUT_MIN_LOOK: i32 = 2;
const PROBCUT_PRE_CHECK_OFFSET: i32 = 4 * PATTERN_UNITS_PER_DISC;

// From look 4 (5 plies) the moves are ordered by evaluation, and the children's stored bounds are tried first (ETC).
const EVALUATION_ORDER_MIN_LOOK: i32 = 4;
const MIDGAME_ETC_MIN_LOOK: i32 = 4;

/// The most moves a list can hold (a position has at most 33 legal moves; any 64-square board fits).
pub(crate) const MAX_MOVES: usize = 64;

/// t × σ in 1/100 disc by disc count (0–64) and depth (1–60).
type ProbCutErrors = [[i32; 61]; 65];

/// A monotonic clock: the time since any fixed moment. C# uses a `Stopwatch`; WebAssembly has no clock in `std`, so
/// the web build passes one (`performance.now()`) through [`SearchEngine::set_clock`].
pub type Clock = fn() -> Duration;

/// The default [`Clock`]: `std::time::Instant` since the first call. On `wasm32-unknown-unknown`, which has no
/// clock, it is always zero, so time limits never stop a search there until a clock is set.
pub fn system_clock() -> Duration {
    #[cfg(not(all(target_arch = "wasm32", target_os = "unknown")))]
    {
        static ORIGIN: std::sync::OnceLock<std::time::Instant> = std::sync::OnceLock::new();
        ORIGIN.get_or_init(std::time::Instant::now).elapsed()
    }
    #[cfg(all(target_arch = "wasm32", target_os = "unknown"))]
    {
        Duration::ZERO
    }
}

/// What can stop a running search; read at every abort check (every 1024 nodes) and at the start.
/// C#: the `cancellationToken` and `moveNowToken` of `Search`.
pub trait StopSignal {
    /// Stops the search with [`SearchError::Cancelled`].
    fn is_cancelled(&self) -> bool;
    /// Stops the search and returns the best move found so far.
    fn is_move_now(&self) -> bool;
}

/// Stop signals as atomic flags, which another thread (or the FFI wrapper's token callbacks) sets while the search
/// runs.
#[derive(Default, Debug)]
pub struct SearchSignals {
    cancel: AtomicBool,
    move_now: AtomicBool,
}

impl SearchSignals {
    pub fn new() -> SearchSignals {
        SearchSignals::default()
    }

    /// Cancel: the search returns [`SearchError::Cancelled`].
    pub fn cancel(&self) {
        self.cancel.store(true, Ordering::Relaxed);
    }

    /// Move Now: the search returns the best move found so far.
    pub fn move_now(&self) {
        self.move_now.store(true, Ordering::Relaxed);
    }

    /// Clears both flags for the next search.
    pub fn reset(&self) {
        self.cancel.store(false, Ordering::Relaxed);
        self.move_now.store(false, Ordering::Relaxed);
    }
}

impl StopSignal for SearchSignals {
    #[inline]
    fn is_cancelled(&self) -> bool {
        self.cancel.load(Ordering::Relaxed)
    }

    #[inline]
    fn is_move_now(&self) -> bool {
        self.move_now.load(Ordering::Relaxed)
    }
}

/// Never stops: for searches that only the limits end.
#[derive(Clone, Copy, Default, Debug)]
pub struct NoStop;

impl StopSignal for NoStop {
    fn is_cancelled(&self) -> bool {
        false
    }

    fn is_move_now(&self) -> bool {
        false
    }
}

/// Why [`SearchEngine::search`] returned no result.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum SearchError {
    /// The cancel signal was set (C#: `OperationCanceledException`).
    Cancelled,
    /// None of the `only_moves` is legal (C#: `ArgumentException`).
    NoLegalOnlyMove,
}

impl std::fmt::Display for SearchError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            SearchError::Cancelled => "The search was cancelled.",
            SearchError::NoLegalOnlyMove => "None of the moves to search is legal.",
        })
    }
}

impl std::error::Error for SearchError {}

/// Why a running search unwinds. C#: `OperationCanceledException` and `SearchAbortedException`.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub(crate) enum Abort {
    /// Time is up or Move Now: keep the best move found so far.
    Stop,
    /// Cancelled: no result.
    Cancelled,
}

/// C#: `SearchEngine`. An instance runs one search at a time; it keeps its hash tables between searches until
/// [`SearchEngine::clear_hash`].
///
/// A search with [`SearchLimits::threads`] above 1 runs on this thread and on workers of a [`ThreadPool`] that the
/// engine starts at the first such search and keeps for the next ones with as many threads. All threads share the
/// two hash tables; each has its own [`ThreadState`]. With 1 thread no pool is used, and the search is exactly the
/// single-threaded one.
pub struct SearchEngine {
    hash_bits: u32,
    options: SearchOptions,
    probcut_errors: Option<Box<ProbCutErrors>>,
    midgame_table: TranspositionTable,
    endgame_table: TranspositionTable,
    /// The calling thread's state; the pool's workers have their own.
    main: ThreadState,
    /// The workers for searches with more than 1 thread, and the thread count they were started for.
    pool: Option<(u32, ThreadPool<ThreadState>)>,
    spawner: Spawner,
    clock: Clock,
    iteration_done: Option<Box<dyn FnMut(i32, i32) + Send>>,
}

/// What each search thread keeps from search to search: the solver's local table (C#: `_localTable`) and the
/// response killers (C#: `_ordering`). Only [`SearchEngine::clear_hash`] clears the local table.
#[derive(Default)]
pub struct ThreadState {
    pub(crate) endgame: EndgameState,
    pub(crate) ordering: MoveOrdering,
}

/// What all threads of one search read: the evaluation, Multi-ProbCut errors, shared hash tables, clock and options.
#[derive(Clone, Copy)]
pub(crate) struct SearchContext<'a> {
    pattern: &'a EvalWeights,
    probcut_errors: Option<&'a ProbCutErrors>,
    midgame_table: &'a TranspositionTable,
    endgame_table: &'a TranspositionTable,
    clock: Clock,
    soft_time_percent: Option<i32>,
    ybwc: bool,
}

impl<'a> SearchContext<'a> {
    fn new(
        options: &'a SearchOptions,
        probcut_errors: Option<&'a ProbCutErrors>,
        midgame_table: &'a TranspositionTable,
        endgame_table: &'a TranspositionTable,
        clock: Clock,
    ) -> SearchContext<'a> {
        SearchContext {
            pattern: options.pattern.as_deref().unwrap_or_else(|| EvalWeights::built_in()),
            probcut_errors,
            midgame_table,
            endgame_table,
            clock,
            soft_time_percent: options.soft_time_percent,
            ybwc: options.ybwc,
        }
    }
}

/// The most helpers in a solve without YBWC ([`SearchOptions::ybwc`] off). On FFO #40–49 with 8 threads, 3 helpers
/// made the solve 1.1 times faster and no position slower; 7 made some positions faster but filled the endgame table
/// so that FFO #49 took 15 s instead of 8.8 s. With YBWC there are none: they took the workers YBWC uses better
/// (FFO #40–49 with 8 threads: 11.2 s with them, 8.0 s without).
const SOLVE_HELPERS: usize = 3;

/// The Lazy SMP helpers in the midgame, of `workers` (the threads but the main one): half of the threads, the
/// others being left to YBWC. On mid-bench at depth 16 and 17 this beat both all helpers and none (8 threads:
/// 3.8 s against 4.8 s with Lazy SMP alone and 9.7 s at depth 17 with YBWC alone, 1 thread 8.5 s at depth 16); in the
/// midgame YBWC alone gains little, as its split searches are small.
fn lazy_smp_helpers(workers: usize) -> usize {
    workers.div_ceil(2)
}

/// YBWC splits midgame nodes whose children are searched this many plies deep or more (Egaroucid's
/// `YBWC_MID_SPLIT_MIN_DEPTH`).
const YBWC_MID_MIN_DEPTH: i32 = 6;

/// YBWC splits endgame nodes whose children have this many empty squares or more (Egaroucid's
/// `YBWC_END_SPLIT_MIN_DEPTH`; 15 and 16 were the fastest on FFO #40–49 with 8 and 16 threads).
pub(crate) const YBWC_END_MIN_EMPTIES: i32 = 16;

/// What the main thread needs to start its helpers: the pool, what the helpers read, and where they count.
#[derive(Clone, Copy)]
pub(crate) struct Helpers<'a> {
    pub(crate) pool: &'a ThreadPool<ThreadState>,
    context: SearchContext<'a>,
    /// The helpers to start: the threads but this one.
    count: usize,
    nodes: &'a AtomicU64,
    evaluations: &'a AtomicU64,
}

impl<'a> Helpers<'a> {
    /// A helper's search on a worker: no progress, no time limit, its nodes counted for the main thread.
    pub(crate) fn searcher<'b>(&self, state: &'b mut ThreadState, stop: &'b dyn StopSignal) -> Searcher<'b>
    where
        'a: 'b,
    {
        let mut searcher = Searcher::new(self.context, state, stop);
        searcher.node_sink = Some(self.nodes);
        searcher
    }

    /// Adds a helper's last nodes and its evaluations to the search's counts.
    pub(crate) fn finish(&self, helper: &mut Searcher<'_>) {
        helper.flush_nodes();
        self.evaluations.fetch_add(helper.evaluations, Ordering::Relaxed);
    }
}

/// Stops the helpers when it is dropped: at the end of the main thread's part of a scope, also if it panics (else the
/// scope would wait for the helpers for ever).
struct StopHelpers<'a>(&'a AtomicBool);

impl Drop for StopHelpers<'_> {
    fn drop(&mut self) {
        self.0.store(true, Ordering::Relaxed);
    }
}

/// Whether this build can start threads: not WebAssembly without atomics (the single-threaded web build; phase 8
/// brings threads to the browser).
const THREADS_AVAILABLE: bool = !cfg!(all(target_arch = "wasm32", not(target_feature = "atomics")));

impl SearchEngine {
    /// Each of the two hash tables gets 2^`hash_bits` slots of two entries (C++: HASHSIZE 19, one entry). Panics
    /// unless `hash_bits` is 10–26 (C# throws `ArgumentOutOfRangeException`).
    pub fn new(hash_bits: u32, options: SearchOptions) -> SearchEngine {
        assert!((10..=26).contains(&hash_bits), "hash bits must be 10-26");
        // The solver's stable-disc tables, once per process, outside the first search's time (C#: the static
        // constructor of StableDiscs, which runs in the first search).
        endgame::stable::init();
        let probcut_errors = options
            .selective
            .then(|| probcut_errors(options.probcut_percent.unwrap_or(DEFAULT_PROBCUT_PERCENT) as f64 / 100.0));
        SearchEngine {
            hash_bits,
            options,
            probcut_errors,
            midgame_table: TranspositionTable::new(hash_bits),
            endgame_table: TranspositionTable::new(hash_bits),
            main: ThreadState::default(),
            pool: None,
            spawner: std_spawner,
            clock: system_clock,
            iteration_done: None,
        }
    }

    /// C#: `HashBits`.
    pub fn hash_bits(&self) -> u32 {
        self.hash_bits
    }

    /// C#: `Options`.
    pub fn options(&self) -> &SearchOptions {
        &self.options
    }

    /// C#: `ClearHash`.
    pub fn clear_hash(&mut self) {
        self.midgame_table.clear();
        self.endgame_table.clear();
        self.main.endgame.clear();
        if let Some((_, pool)) = &self.pool {
            pool.for_each_state(|state| state.endgame.clear());
        }
    }

    /// [`SearchOptions::ybwc`], for the next searches.
    pub fn set_ybwc(&mut self, ybwc: bool) {
        self.options.ybwc = ybwc;
    }

    /// The clock that times the search (default [`system_clock`]).
    pub fn set_clock(&mut self, clock: Clock) {
        self.clock = clock;
    }

    /// How the engine starts the threads of searches with more than 1 thread (default [`std_spawner`]); for a build
    /// whose threads are not `std` threads, such as Web Workers. Applies from the next pool the engine starts.
    pub fn set_spawner(&mut self, spawner: Spawner) {
        self.spawner = spawner;
    }

    /// Called after each finished midgame iteration with its depth and score (for the Multi-ProbCut fit).
    /// C#: `IterationDone`.
    pub fn set_iteration_done(&mut self, callback: Option<Box<dyn FnMut(i32, i32) + Send>>) {
        self.iteration_done = callback;
    }

    /// Finds the best move for `player` (C++: getcomputer). C#: `Search`.
    ///
    /// - `progress` gets a report after every root move and a heartbeat (the last report again) when 100 ms passed
    ///   without one;
    /// - `stop`: cancel returns [`SearchError::Cancelled`]; Move Now returns the best move found so far;
    /// - `only_moves`: search only these moves, even if just one is left (C++ calclib: the best move not yet in the
    ///   book).
    ///
    /// With more than 1 thread ([`SearchLimits::threads`]) this thread searches as with 1, and the others help it
    /// (Plans/making-stello-3-plan.md, phases 6 and 7): in the midgame half of them run Lazy SMP helpers that search
    /// the root a few plies deeper into the shared hash tables ([`Searcher::lazy_smp_midgame`]), and the others take
    /// the younger brothers of the nodes this thread and they search (YBWC, [`Searcher::split`]); in the solve all of
    /// them take part in YBWC. The result is this thread's, with the nodes and evaluations of all threads.
    pub fn search(
        &mut self,
        board: &Board,
        player: Player,
        limits: &SearchLimits,
        progress: Option<&mut dyn FnMut(&SearchInfo)>,
        stop: &dyn StopSignal,
        only_moves: Option<u64>,
    ) -> Result<SearchResult, SearchError> {
        if stop.is_cancelled() {
            return Err(SearchError::Cancelled);
        }

        let progress = progress.map(|p| p as &mut dyn FnMut(&SearchInfo));
        let threads = if THREADS_AVAILABLE { limits.threads() } else { 1 };
        if threads == 1 {
            return self.searcher(progress, stop).run(board, player, limits, only_moves);
        }

        if self.pool.as_ref().is_none_or(|(size, _)| *size != threads) {
            // The old workers end first, so there are never more threads than asked for.
            self.pool = None;
            self.pool = Some((threads, ThreadPool::new(threads as usize - 1, self.spawner, ThreadState::default)));
        }

        let SearchEngine {
            options,
            probcut_errors,
            midgame_table,
            endgame_table,
            main,
            pool,
            clock,
            iteration_done,
            ..
        } = self;
        let context = SearchContext::new(options, probcut_errors.as_deref(), midgame_table, endgame_table, *clock);
        let pool = &pool.as_ref().expect("the pool was started above").1;
        // The response killers of the helpers come from other positions; each search starts them afresh, as the
        // main thread's.
        pool.for_each_state(|state| state.ordering.clear_responses());
        let (nodes, evaluations) = (AtomicU64::new(0), AtomicU64::new(0));
        let helpers = Helpers { pool, context, count: threads as usize - 1, nodes: &nodes, evaluations: &evaluations };

        let mut searcher = Searcher::new(context, main, stop);
        searcher.progress = progress;
        searcher.iteration_done = iteration_done.as_deref_mut();
        searcher.helpers = Some(helpers);
        searcher.node_source = Some(&nodes);
        let result = searcher.run(board, player, limits, only_moves);
        result.map(|mut result| {
            result.nodes += nodes.load(Ordering::Relaxed);
            result.evaluations += evaluations.load(Ordering::Relaxed);
            result
        })
    }

    /// A new search on this thread with this engine's tables, with the counters at zero and the clock started (C#:
    /// the start of `Search`). The endgame's tests call the solver through it.
    pub(crate) fn searcher<'a>(
        &'a mut self,
        progress: Option<&'a mut dyn FnMut(&SearchInfo)>,
        stop: &'a dyn StopSignal,
    ) -> Searcher<'a> {
        let SearchEngine { options, probcut_errors, midgame_table, endgame_table, main, clock, iteration_done, .. } =
            self;
        let context = SearchContext::new(options, probcut_errors.as_deref(), midgame_table, endgame_table, *clock);
        let mut searcher = Searcher::new(context, main, stop);
        searcher.progress = progress;
        searcher.iteration_done = iteration_done.as_deref_mut();
        searcher
    }

    /// [`SearchEngine::search`] without progress reports or stop signals.
    pub fn search_simple(&mut self, board: &Board, player: Player, limits: &SearchLimits) -> SearchResult {
        self.search(board, player, limits, None, &NoStop, None).expect("a search without stop signals finishes")
    }
}

/// The stop signal of a helper thread: Move Now once the main search has ended.
struct FlagStop<'a>(&'a AtomicBool);

impl StopSignal for FlagStop<'_> {
    fn is_cancelled(&self) -> bool {
        false
    }

    #[inline]
    fn is_move_now(&self) -> bool {
        self.0.load(Ordering::Relaxed)
    }
}

/// C#: `SearchEngine.Best`.
#[derive(Clone, Copy, Debug)]
pub(crate) struct Best {
    pub mv: i32,
    pub score: i32,
    pub kind: ScoreKind,
    pub depth: i32,
}

/// One running search on one thread: C#'s per-search fields, with borrows of the engine's shared tables and of the
/// thread's own [`ThreadState`].
pub(crate) struct Searcher<'a> {
    pub(crate) pattern: &'a EvalWeights,
    probcut_errors: Option<&'a ProbCutErrors>,
    pub(crate) midgame_table: &'a TranspositionTable,
    pub(crate) endgame_table: &'a TranspositionTable,
    pub(crate) endgame: &'a mut EndgameState,
    pub(crate) ordering: &'a mut MoveOrdering,
    progress: Option<&'a mut dyn FnMut(&SearchInfo)>,
    iteration_done: Option<&'a mut (dyn FnMut(i32, i32) + Send + 'static)>,
    pub(crate) stop: &'a dyn StopSignal,
    pub(crate) clock: Clock,
    pub(crate) start: Duration,
    soft_time_percent: Option<i32>,
    /// [`SearchOptions::ybwc`]: whether this search splits its nodes when it has helpers.
    pub(crate) ybwc: bool,
    /// The split point this search was split off at (YBWC), if it was.
    pub(crate) split: Option<&'a SplitPoint<'a>>,
    /// The main thread's helpers in a search with more than 1 thread (Lazy SMP); `None` with 1 thread and in the
    /// helpers themselves.
    pub(crate) helpers: Option<Helpers<'a>>,
    /// A helper adds its nodes here at each abort check ([`Searcher::flush_nodes`]); the main thread reports its own
    /// nodes plus these.
    node_sink: Option<&'a AtomicU64>,
    node_source: Option<&'a AtomicU64>,
    flushed_nodes: u64,

    pub(crate) nodes: u64,
    pub(crate) evaluations: u64,
    /// The endgame's abort check (C#: `_nextAbortCheck`); the midgame checks when `nodes % 1024 == 0`.
    pub(crate) next_abort_check: u64,
    pub(crate) hard_limit_ms: i64,
    pub(crate) hash_get_height: i32,
    pub(crate) hash_put_height: i32,
    pub(crate) in_probcut: bool,
    last_heartbeat_ms: i64,
    pub(crate) report_depth: i32,
    pub(crate) report_move: i32,
    pub(crate) report_best_move: i32,
    pub(crate) report_score: i32,
    pub(crate) report_kind: ScoreKind,
}

impl<'a> Searcher<'a> {
    fn new(context: SearchContext<'a>, thread: &'a mut ThreadState, stop: &'a dyn StopSignal) -> Searcher<'a> {
        Searcher {
            pattern: context.pattern,
            probcut_errors: context.probcut_errors,
            midgame_table: context.midgame_table,
            endgame_table: context.endgame_table,
            endgame: &mut thread.endgame,
            ordering: &mut thread.ordering,
            progress: None,
            iteration_done: None,
            stop,
            clock: context.clock,
            start: (context.clock)(),
            soft_time_percent: context.soft_time_percent,
            ybwc: context.ybwc,
            split: None,
            helpers: None,
            node_sink: None,
            node_source: None,
            flushed_nodes: 0,
            nodes: 0,
            evaluations: 0,
            next_abort_check: 1024,
            hard_limit_ms: i64::MAX,
            hash_get_height: 0,
            hash_put_height: 0,
            in_probcut: false,
            last_heartbeat_ms: 0,
            report_depth: 0,
            report_move: -1,
            report_best_move: -1,
            report_score: 0,
            report_kind: ScoreKind::None,
        }
    }

    fn run(
        &mut self,
        board: &Board,
        player: Player,
        limits: &SearchLimits,
        only_moves: Option<u64>,
    ) -> Result<SearchResult, SearchError> {
        let mut moves = board.legal_moves(player);
        if let Some(allowed) = only_moves {
            moves &= allowed;
            if moves == 0 {
                return Err(SearchError::NoLegalOnlyMove);
            }
        }

        if moves == 0 {
            return Ok(self.result(Best { mv: -1, score: 0, kind: ScoreKind::None, depth: 0 }));
        }

        if moves.count_ones() == 1 && limits.mode() != TimeControlMode::Solve && only_moves.is_none() {
            return Ok(self.result(Best {
                mv: moves.trailing_zeros() as i32,
                score: 0,
                kind: ScoreKind::None,
                depth: 0,
            }));
        }

        self.ordering.clear_responses();
        let mut root = [0i32; MAX_MOVES];
        let count = fill(&mut root, moves);
        let root_moves = &mut root[..count];
        self.ordering.order(root_moves, -1, -1, player, board);

        let mut best = Best { mv: root_moves[0], score: 0, kind: ScoreKind::None, depth: 0 };
        match self.iterate(board, player, limits, root_moves, &mut best) {
            // Time is up or "move now": keep the best move found so far.
            Ok(()) | Err(Abort::Stop) => Ok(self.result(best)),
            Err(Abort::Cancelled) => Err(SearchError::Cancelled),
        }
    }

    // The iterations, then the solver; returns when `best` is the result.
    fn iterate(
        &mut self,
        board: &Board,
        player: Player,
        limits: &SearchLimits,
        root_moves: &mut [i32],
        best: &mut Best,
    ) -> Result<(), Abort> {
        let empties = board.empty_count() as i32;
        let mut budget = time::for_midgame(limits, empties).with_soft_percent(self.soft_time_percent, 0);
        self.hard_limit_ms = budget.hard_ms;

        if limits.mode() != TimeControlMode::Solve {
            let done = match self.helpers {
                Some(helpers) => self.lazy_smp_midgame(helpers, board, player, limits, budget, root_moves, best)?,
                None => self.midgame_iterations(board, player, limits, budget, root_moves, best, None)?,
            };
            if done {
                return Ok(());
            }
        } else {
            let mut look = 0;
            while look < SOLVE_ORDERING_LOOKS && empties - 1 - look > ENDGAME_DISTANCE {
                self.hash_get_height = look - 1;
                self.hash_put_height = look;
                self.search_root(board, player, look, root_moves, best)?;
                look += 1;
            }
        }

        budget = time::for_solver(limits, budget, self.elapsed_ms(), self.soft_time_percent);
        self.hard_limit_ms = budget.hard_ms;

        // Win/loss/draw first with a null window around 0; it is much faster than the exact score.
        let (wld, wld_move) = self.solve_iteration(board, player, -1, 1, ScoreKind::WinLossDraw, root_moves, best)?;
        let keep_midgame_move = wld < 0 && best.kind == ScoreKind::Heuristic;
        *best = Best {
            mv: if keep_midgame_move { best.mv } else { wld_move },
            score: wld,
            kind: if wld == 0 { ScoreKind::Exact } else { ScoreKind::WinLossDraw },
            depth: empties,
        };

        if wld == 0 || self.stop_iterating(limits, budget, 0) {
            return Ok(());
        }

        let (alpha, beta) = if wld > 0 { (wld - 1, 65) } else { (-65, wld + 1) };
        let (exact, exact_move) =
            self.solve_iteration(board, player, alpha, beta, ScoreKind::Exact, root_moves, best)?;
        *best = Best { mv: exact_move, score: exact, kind: ScoreKind::Exact, depth: empties };
        Ok(())
    }

    /// [`Searcher::solve_root`], with up to [`SOLVE_HELPERS`] helpers on the other threads: each solves the same
    /// window, its root moves rotated by its number plus one, so that the threads begin with different moves. They
    /// fill the shared endgame table and stop when this thread's solve ends; their results are not used. Egaroucid
    /// has no helpers in its last iteration, and splits the endgame with YBWC instead (phase 7), as this search does
    /// unless [`SearchOptions::ybwc`] is off.
    #[allow(clippy::too_many_arguments)]
    fn solve_iteration(
        &mut self,
        board: &Board,
        player: Player,
        alpha: i32,
        beta: i32,
        kind: ScoreKind,
        root_moves: &mut [i32],
        best: &mut Best,
    ) -> Result<(i32, i32), Abort> {
        let Some(helpers) = self.helpers.filter(|_| !self.ybwc) else {
            return self.solve_root(board, player, alpha, beta, kind, root_moves, best);
        };

        let stop = AtomicBool::new(false);
        let (board, count, start) = (*board, root_moves.len(), *best);
        let mut order = [0i32; MAX_MOVES];
        order[..count].copy_from_slice(root_moves);
        helpers.pool.scope(|scope| {
            for i in 0..helpers.count.min(SOLVE_HELPERS) {
                let stop = &stop;
                let started = scope.try_spawn(move |state: &mut ThreadState| {
                    let flag = FlagStop(stop);
                    let mut helper = helpers.searcher(state, &flag);
                    let (mut moves, mut best) = (order, start);
                    moves[..count].rotate_left((i + 1) % count);
                    // Stopped or not, what it solved is in the table; the result is not used.
                    let _ = helper.solve_root(&board, player, alpha, beta, kind, &mut moves[..count], &mut best);
                    helpers.finish(&mut helper);
                });
                if !started {
                    break;
                }
            }

            let _stop_helpers = StopHelpers(&stop);
            self.solve_root(&board, player, alpha, beta, kind, root_moves, best)
        })
    }

    /// The midgame iterations with Lazy SMP helpers on the other threads (Egaroucid's ai.hpp, with the helpers'
    /// depths from there): each helper runs an iterative deepening of its own on the root, 1 + ctz(i + 1) plies ahead
    /// of this thread (helper i from 0: +1, +2, +1, +3, +1, +2, +1, ...), and jumps ahead again when this thread
    /// catches up. They fill the shared hash table, which this thread reads, and stop when its iterations end; their
    /// results are not used. Returns what [`Searcher::midgame_iterations`] returns.
    ///
    /// Egaroucid restarts its helpers at each iteration of the main search and only up to depth 10 (deeper, its main
    /// search splits with YBWC), the helpers at the main depth searching with less Multi-ProbCut. Here that did not
    /// reach depth 14 any faster; helpers that keep searching across the main iterations did (1.5 times with 4 and 8
    /// threads, Plans/making-stello-3-findings.md).
    ///
    /// Only [`lazy_smp_helpers`] of the workers are helpers; the others stay idle for YBWC splits.
    #[allow(clippy::too_many_arguments)]
    fn lazy_smp_midgame(
        &mut self,
        helpers: Helpers<'a>,
        board: &Board,
        player: Player,
        limits: &SearchLimits,
        budget: TimeBudget,
        root_moves: &mut [i32],
        best: &mut Best,
    ) -> Result<bool, Abort> {
        let stop = AtomicBool::new(false);
        let main_look = AtomicI32::new(0);
        // The last midgame iteration: below it the solver takes over.
        let last_look = board.empty_count() as i32 - 2 - ENDGAME_DISTANCE;
        if last_look < 0 {
            // No midgame iteration: the search goes straight to the solver, so no helper is started here.
            return self.midgame_iterations(board, player, limits, budget, root_moves, best, None);
        }
        let (board, count, start) = (*board, root_moves.len(), *best);
        let mut order = [0i32; MAX_MOVES];
        order[..count].copy_from_slice(root_moves);
        helpers.pool.scope(|scope| {
            let lazy = if self.ybwc { lazy_smp_helpers(helpers.count) } else { helpers.count };
            for i in 0..lazy {
                let ahead = 1 + (i as u32 + 1).trailing_zeros() as i32;
                let (stop, main_look) = (&stop, &main_look);
                let started = scope.try_spawn(move |state: &mut ThreadState| {
                    let flag = FlagStop(stop);
                    let mut helper = helpers.searcher(state, &flag);
                    let (mut moves, mut best) = (order, start);
                    let mut look = 0;
                    loop {
                        look = look.max(main_look.load(Ordering::Relaxed) + ahead).min(last_look);
                        helper.hash_get_height = look - 1;
                        helper.hash_put_height = look;
                        // Stopped or not, what it found is in the table.
                        if helper.search_root(&board, player, look, &mut moves[..count], &mut best).is_err()
                            || look == last_look
                        {
                            break;
                        }
                        look += 1;
                    }
                    helpers.finish(&mut helper);
                });
                if !started {
                    break;
                }
            }

            let _stop_helpers = StopHelpers(&stop);
            self.midgame_iterations(&board, player, limits, budget, root_moves, best, Some(&main_look))
        })
    }

    /// The midgame iterations; true if the search is done (no solve after them). `main_look` gets the look of each
    /// iteration, for the helpers.
    #[allow(clippy::too_many_arguments)]
    fn midgame_iterations(
        &mut self,
        board: &Board,
        player: Player,
        limits: &SearchLimits,
        budget: TimeBudget,
        root_moves: &mut [i32],
        best: &mut Best,
        main_look: Option<&AtomicI32>,
    ) -> Result<bool, Abort> {
        let empties = board.empty_count() as i32;
        let mut look = 0;
        while empties - 1 - look > ENDGAME_DISTANCE {
            if let Some(main_look) = main_look {
                main_look.store(look, Ordering::Relaxed);
            }
            self.hash_get_height = look - 1;
            self.hash_put_height = look;
            self.search_root(board, player, look, root_moves, best)?;
            if let Some(callback) = self.iteration_done.as_mut() {
                callback(look + 1, best.score);
            }

            if self.stop_iterating(limits, budget, look + 1) {
                return Ok(true);
            }

            look += 1;
        }
        Ok(false)
    }

    fn search_root(
        &mut self,
        board: &Board,
        player: Player,
        look: i32,
        root_moves: &mut [i32],
        best: &mut Best,
    ) -> Result<(), Abort> {
        let opponent = player.opponent();
        let mut alpha = -ROOT_WINDOW;
        let beta = ROOT_WINDOW;
        let mut best_score = -INFINITY;
        let mut best_move = -1;
        let mut improvements = [0i32; MAX_MOVES];
        let mut improved = 0;

        for (i, &mv) in root_moves.iter().enumerate() {
            self.report_depth = look + 1;
            self.report_move = mv;
            self.report_best_move = if best_move >= 0 { best_move } else { best.mv };
            self.report_score = if best_move >= 0 { best_score } else { best.score };
            self.report_kind = ScoreKind::Heuristic;
            let child = board.play_unchecked(player, mv as u32);
            let score;

            if look == 0 {
                score = -self.evaluate_leaf(&child, opponent);
            } else {
                let child_look = look - 1;
                let reply;

                // C++ zero_findmax: null window after the first move once the search is deeper than 3 plies.
                if i == 0 || look <= 2 {
                    let (value, r) = self.search(&child, opponent, child_look, 1, mv, -beta, -alpha)?;
                    (score, reply) = (-value, r);
                } else {
                    let (value, r) = self.search(&child, opponent, child_look, 1, mv, -alpha - 1, -alpha)?;
                    if -value > alpha && -value < beta {
                        let (value, r) = self.search(&child, opponent, child_look, 1, mv, -beta, -alpha)?;
                        (score, reply) = (-value, r);
                    } else {
                        (score, reply) = (-value, r);
                    }
                }

                self.ordering.record_response(opponent, mv, reply, score <= alpha);
            }

            if score > best_score {
                best_score = score;
                best_move = mv;
                alpha = alpha.max(score);
                improvements[improved] = mv;
                improved += 1;
                *best = Best { mv, score, kind: ScoreKind::Heuristic, depth: look + 1 };
            }

            self.report(look + 1, mv, best_move, best_score, ScoreKind::Heuristic);
        }

        move_to_front(root_moves, &improvements[..improved]);
        Ok(())
    }

    /// The node search: the score and the best move (-1 if none). C++: findmax2 (findmax/findmax1 kept a move tree
    /// for ordering; the hash table does that here). C#: `Search`.
    #[allow(clippy::too_many_arguments)]
    pub(crate) fn search(
        &mut self,
        board: &Board,
        player: Player,
        mut look: i32,
        ply: i32,
        previous_move: i32,
        mut alpha: i32,
        beta: i32,
    ) -> Result<(i32, i32), Abort> {
        self.nodes += 1;
        if self.nodes & 1023 == 0 {
            self.check_abort()?;
        }

        let opponent = player.opponent();
        let own = board.discs(player);
        let other = board.discs(opponent);
        let moves = bits::legal_moves(own, other);

        if moves == 0 {
            if bits::legal_moves(other, own) == 0 {
                self.evaluations += 1;
                return Ok((game_over_score(own, other), -1));
            }

            let (value, _) = self.search(board, opponent, look, ply, -1, -beta, -alpha)?;
            return Ok((-value, -1));
        }

        let mut hash_move = -1;
        let hash_get = ply <= self.hash_get_height;
        let hash_put = ply <= self.hash_put_height;
        let tag = player.index() as i32;
        if hash_put && !hash_get {
            self.midgame_table.prefetch(own, other, tag);
        }

        if hash_get
            && let Some(entry) = self.midgame_table.get(own, other, tag)
            && entry.mv >= 0
            && moves & (1u64 << entry.mv) != 0
        {
            let value = entry.value as i32;
            if entry.depth as i32 >= look
                && match entry.bound {
                    Bound::Exact => true,
                    Bound::Upper => value <= alpha,
                    Bound::Lower => value >= beta,
                    Bound::None => false,
                }
            {
                return Ok((value, entry.mv as i32));
            }

            hash_move = entry.mv as i32;
        }

        // Multi-ProbCut: a shallow null-window search predicts that the full search falls outside the window.
        if self.probcut_errors.is_some()
            && look >= PROBCUT_MIN_LOOK
            && !self.in_probcut
            && let Some(cut) = self.try_probcut(board, player, look, ply, previous_move, alpha, beta)?
        {
            return Ok((cut, -1));
        }

        let stored_look = look;
        let move_count = moves.count_ones() as usize;

        // A single reply at the horizon is searched one ply deeper.
        if move_count == 1 && look == 0 {
            look = 1;
        }

        let mut buffer = [0i32; MAX_MOVES];
        fill(&mut buffer, moves);
        let list = &mut buffer[..move_count];

        // Enhanced transposition cutoff: a child whose stored upper bound already refutes it gives a cutoff here.
        // C#: ply + 1 <= _hashGetHeight.
        if look >= MIDGAME_ETC_MIN_LOOK && ply < self.hash_get_height {
            for &mv in list.iter() {
                let child = board.play_unchecked(player, mv as u32);
                if let Some(entry) =
                    self.midgame_table.get(child.discs(opponent), child.discs(player), opponent.index() as i32)
                    && entry.depth as i32 >= look - 1
                    && entry.bound != Bound::Lower
                    && -(entry.value as i32) >= beta
                {
                    return Ok((-(entry.value as i32), mv));
                }
            }
        }

        if look >= EVALUATION_ORDER_MIN_LOOK {
            self.order_by_evaluation(list, hash_move, board, player);
        } else {
            self.ordering.order(list, hash_move, previous_move, player, board);
        }

        let mut best = -INFINITY;
        let mut best_move = -1;
        let mut bound = Bound::Lower;
        let alpha_start = alpha;
        for (index, &mv) in list.iter().enumerate() {
            // YBWC: after the first child, the younger brothers in parallel.
            if index == 1 && look >= YBWC_MID_MIN_DEPTH && self.may_split() {
                let mut children =
                    [MidgameChild { board: *board, player: opponent, look: 0, ply: 0, mv: 0 }; MAX_MOVES];
                for (child, &mv) in children.iter_mut().zip(&list[1..]) {
                    *child = MidgameChild {
                        board: board.play_unchecked(player, mv as u32),
                        player: opponent,
                        look: look - 1,
                        ply: ply + 1,
                        mv,
                    };
                }
                self.split(&children[..list.len() - 1], alpha, beta, &mut best, &mut best_move)?;
                bound = if best >= beta {
                    Bound::Lower
                } else if best > alpha_start {
                    Bound::Exact
                } else {
                    Bound::Upper
                };
                break;
            }

            let child = board.play_unchecked(player, mv as u32);
            let score;

            if look == 0 {
                score = -self.evaluate_leaf(&child, opponent);
            } else if best == -INFINITY || beta - alpha == 1 {
                let (value, reply) = self.search(&child, opponent, look - 1, ply + 1, mv, -beta, -alpha)?;
                score = -value;
                self.ordering.record_response(opponent, mv, reply, score <= alpha);
            } else {
                // Principal variation search: a later move first only has to show it is not better.
                let (mut value, mut reply) =
                    self.search(&child, opponent, look - 1, ply + 1, mv, -alpha - 1, -alpha)?;
                if -value > alpha && -value < beta {
                    (value, reply) = self.search(&child, opponent, look - 1, ply + 1, mv, -beta, -alpha)?;
                }

                score = -value;
                self.ordering.record_response(opponent, mv, reply, score <= alpha);
            }

            if score > best {
                bound = if score >= beta {
                    Bound::Lower
                } else if score > alpha {
                    Bound::Exact
                } else {
                    Bound::Upper
                };
                alpha = alpha.max(score);
                best = score;
                best_move = mv;
                if best >= beta {
                    break;
                }
            }
        }

        if hash_put {
            self.midgame_table.store(own, other, tag, stored_look, bound, best, best_move);
        }

        Ok((best, best_move))
    }

    // Egaroucid's midgame ordering (move_ordering.hpp, depth 0): the hash move, then fewer replies, less potential
    // mobility for the opponent and a better evaluation first. The response killers are not used here: with them
    // first the search was weaker.
    fn order_by_evaluation(&mut self, list: &mut [i32], hash_move: i32, board: &Board, player: Player) {
        let opponent = player.opponent();
        let empty = !(board.discs(player) | board.discs(opponent));
        let mut keys = [0i32; MAX_MOVES];
        for (key, &mv) in keys.iter_mut().zip(list.iter()) {
            if mv == hash_move {
                *key = i32::MAX;
                continue;
            }

            let child = board.play_unchecked(player, mv as u32);
            let child_own = child.discs(opponent);
            let child_other = child.discs(player);
            let replies = endgame::bits::weighted_moves(bits::legal_moves(child_own, child_other));
            let potential = endgame::bits::potential_mobility(child_other, empty & !(1u64 << mv));
            let value = self.evaluate_leaf(&child, opponent);
            *key = (ORDER_OFFSET - replies) * ORDER_MOBILITY + (ORDER_OFFSET - potential) * ORDER_POTENTIAL_MOBILITY
                - value * ORDER_VALUE / PATTERN_UNITS_PER_DISC;
        }

        let count = list.len();
        ordering::sort_descending(list, &mut keys[..count]);
    }

    /// The cut score if Multi-ProbCut cuts this node. C#: `TryProbCut`.
    #[allow(clippy::too_many_arguments)]
    fn try_probcut(
        &mut self,
        board: &Board,
        player: Player,
        look: i32,
        ply: i32,
        previous_move: i32,
        alpha: i32,
        beta: i32,
    ) -> Result<Option<i32>, Abort> {
        let depth = look + 1;
        let shallow_look = probcut_shallow_depth(depth) - 1;
        let errors = self.probcut_errors.expect("Multi-ProbCut is on");
        let error = errors[64 - board.empty_count() as usize][depth as usize];
        let error0 = PATTERN_UNITS_PER_DISC.max(error - PROBCUT_PRE_CHECK_OFFSET);
        let static0 = self.evaluate_leaf(board, player);

        // C#'s finally: the flag is cleared even when the search stops.
        self.in_probcut = true;
        let cut =
            self.probcut_searches(board, player, shallow_look, ply, previous_move, alpha, beta, error, error0, static0);
        self.in_probcut = false;
        cut
    }

    #[allow(clippy::too_many_arguments)]
    fn probcut_searches(
        &mut self,
        board: &Board,
        player: Player,
        shallow_look: i32,
        ply: i32,
        previous_move: i32,
        alpha: i32,
        beta: i32,
        error: i32,
        error0: i32,
        static0: i32,
    ) -> Result<Option<i32>, Abort> {
        if static0 >= beta + error0 && beta + error < PROBCUT_SCORE_LIMIT {
            let pc_beta = beta + error;
            if self.search(board, player, shallow_look, ply, previous_move, pc_beta - 1, pc_beta)?.0 >= pc_beta {
                return Ok(Some(beta));
            }
        }

        if static0 <= alpha - error0 && alpha - error > -PROBCUT_SCORE_LIMIT {
            let pc_alpha = alpha - error;
            if self.search(board, player, shallow_look, ply, previous_move, pc_alpha, pc_alpha + 1)?.0 <= pc_alpha {
                return Ok(Some(alpha));
            }
        }

        Ok(None)
    }

    /// The root of the solver: the score and move for `kind` (win/loss/draw or exact) within (alpha, beta).
    /// C#: `SolveRoot`.
    #[allow(clippy::too_many_arguments)]
    fn solve_root(
        &mut self,
        board: &Board,
        player: Player,
        mut alpha: i32,
        beta: i32,
        kind: ScoreKind,
        root_moves: &mut [i32],
        best: &mut Best,
    ) -> Result<(i32, i32), Abort> {
        let own = board.discs(player);
        let opponent = board.discs(player.opponent());
        let empties = board.empty_count() as i32;
        let mut best_score = -INFINITY;
        let mut best_move = -1;
        let mut improvements = [0i32; MAX_MOVES];
        let mut improved = 0;

        for (i, &mv) in root_moves.iter().enumerate() {
            let has_solved_move = best_move >= 0 || matches!(best.kind, ScoreKind::WinLossDraw | ScoreKind::Exact);
            self.report_depth = if has_solved_move { empties } else { best.depth };
            self.report_move = mv;
            self.report_best_move = if best_move >= 0 { best_move } else { best.mv };
            self.report_score = if best_move >= 0 { best_score } else { best.score };
            self.report_kind = if best_move >= 0 { kind } else { best.kind };
            let flips = bits::flips(own, opponent, mv as u32);
            let child_own = own | flips | (1u64 << mv);
            let child_opponent = opponent & !flips;
            let score = if i == 0 {
                -self.solve_child(child_opponent, child_own, -beta, -alpha, empties - 1)?
            } else {
                let value = -self.solve_child(child_opponent, child_own, -alpha - 1, -alpha, empties - 1)?;
                if value > alpha && value < beta {
                    -self.solve_child(child_opponent, child_own, -beta, -alpha, empties - 1)?
                } else {
                    value
                }
            };

            if score > best_score {
                best_score = score;
                best_move = mv;
                alpha = alpha.max(score);
                improvements[improved] = mv;
                improved += 1;
                // What a stop in the middle of the pass returns. A win is proven by one move; a draw or a score is
                // only a bound until every move is searched, so the exact pass keeps the win/loss/draw result, and a
                // draw here keeps its move without claiming a solved score.
                if kind == ScoreKind::WinLossDraw && score > 0 {
                    *best = Best { mv, score, kind, depth: empties };
                } else if kind == ScoreKind::WinLossDraw && score == 0 {
                    *best = Best { mv, score, kind: ScoreKind::Heuristic, depth: best.depth };
                }
            }

            self.report(empties, mv, best_move, best_score, kind);
            if best_score >= beta {
                break;
            }
        }

        move_to_front(root_moves, &improvements[..improved]);
        Ok((best_score, best_move))
    }

    /// The evaluation of a leaf, counted as a node and an evaluation. C#: `EvaluateLeaf`.
    #[inline]
    pub(crate) fn evaluate_leaf(&mut self, board: &Board, player: Player) -> i32 {
        self.nodes += 1;
        self.evaluations += 1;
        eval::pattern_score(self.pattern, board.discs(player), board.discs(player.opponent()))
    }

    fn stop_iterating(&self, limits: &SearchLimits, budget: TimeBudget, next_look: i32) -> bool {
        match limits.mode() {
            TimeControlMode::FixedDepth => next_look >= limits.depth(),
            TimeControlMode::Solve => false,
            _ => self.elapsed_ms() > budget.soft_ms,
        }
    }

    /// The heartbeat, then the stop signals and the hard time limit. C#: `CheckAbort`.
    #[cold]
    pub(crate) fn check_abort(&mut self) -> Result<(), Abort> {
        self.flush_nodes();
        if self.progress.is_some() && self.report_move >= 0 && self.elapsed_ms() - self.last_heartbeat_ms >= 100 {
            self.report(
                self.report_depth,
                self.report_move,
                self.report_best_move,
                self.report_score,
                self.report_kind,
            );
        }

        if self.stop.is_cancelled() {
            return Err(Abort::Cancelled);
        }

        if self.stop.is_move_now() || self.elapsed_ms() >= self.hard_limit_ms {
            return Err(Abort::Stop);
        }

        Ok(())
    }

    /// Adds a helper's nodes since the last flush to the main thread's count.
    fn flush_nodes(&mut self) {
        if let Some(sink) = self.node_sink {
            sink.fetch_add(self.nodes - self.flushed_nodes, Ordering::Relaxed);
            self.flushed_nodes = self.nodes;
        }
    }

    /// C#: `Report`.
    pub(crate) fn report(&mut self, depth: i32, mv: i32, best_move: i32, score: i32, kind: ScoreKind) {
        let Some(progress) = self.progress.as_mut() else { return };
        let elapsed = (self.clock)().saturating_sub(self.start);
        self.last_heartbeat_ms = elapsed.as_millis() as i64;
        progress(&SearchInfo {
            depth,
            current_move: Square::new(mv as u8),
            best_move: (best_move >= 0).then(|| Square::new(best_move as u8)),
            score,
            kind,
            nodes: self.nodes + self.node_source.map_or(0, |helpers| helpers.load(Ordering::Relaxed)),
            evaluations: self.evaluations,
            elapsed,
        });
    }

    fn elapsed(&self) -> Duration {
        (self.clock)().saturating_sub(self.start)
    }

    /// C#: `_clock.ElapsedMilliseconds` (whole milliseconds).
    fn elapsed_ms(&self) -> i64 {
        self.elapsed().as_millis() as i64
    }

    fn result(&self, best: Best) -> SearchResult {
        SearchResult {
            mv: if best.mv >= 0 { Move(Some(Square::new(best.mv as u8))) } else { Move::PASS },
            score: best.score,
            kind: best.kind,
            depth: best.depth,
            nodes: self.nodes,
            evaluations: self.evaluations,
            elapsed: self.elapsed(),
        }
    }
}

// Egaroucid's shallow depth: about half the depth, with the same parity.
fn probcut_shallow_depth(depth: i32) -> i32 {
    ((depth / 2) & !1) + (depth & 1)
}

/// t × σ in 1/100 disc by disc count and depth. σ = deep − shallow search, fitted by the trainer's mpc command on
/// 11,700 test positions searched to depth 13 without selectivity (Plans/improve-stello-strength-findings.md).
/// C#: `ProbCutErrors` (the same double arithmetic, in the same order).
fn probcut_errors(t: f64) -> Box<ProbCutErrors> {
    let mut errors = Box::new([[0i32; 61]; 65]);
    for (discs, row) in errors.iter_mut().enumerate() {
        let x = discs as f64 / 64.0;
        for depth in 1..=60 {
            let shallow = probcut_shallow_depth(depth);
            let sigma = 1.8871 + 2.9543 * x + 0.0660 * x * x - 0.6749 * shallow as f64 + 0.2778 * depth as f64;
            row[depth as usize] = (t * sigma.max(0.5) * PATTERN_UNITS_PER_DISC as f64).ceil() as i32;
        }
    }

    errors
}

/// The final disc difference for the side owning `own`, the empty squares going to the winner. C#: `FinalScore`.
#[inline]
pub(crate) fn final_score(own: u64, opponent: u64) -> i32 {
    let own_count = own.count_ones() as i32;
    let opponent_count = opponent.count_ones() as i32;
    let empties = 64 - own_count - opponent_count;
    let difference = own_count - opponent_count;
    if difference > 0 {
        difference + empties
    } else if difference < 0 {
        difference - empties
    } else {
        0
    }
}

/// C#: `GameOverScore`: ±([`WIN_SCORE`] + the final disc difference).
#[inline]
fn game_over_score(own: u64, opponent: u64) -> i32 {
    let difference = final_score(own, opponent);
    if difference > 0 {
        difference + WIN_SCORE
    } else if difference < 0 {
        difference - WIN_SCORE
    } else {
        0
    }
}

/// The squares of `moves`, lowest first; returns the count. C#: `Fill`.
#[inline]
pub(crate) fn fill(list: &mut [i32], mut moves: u64) -> usize {
    let mut count = 0;
    while moves != 0 {
        list[count] = moves.trailing_zeros() as i32;
        count += 1;
        moves &= moves - 1;
    }

    count
}

// Each new best move goes to the front, so the last best move is searched first next time (C++: put_in_front).
fn move_to_front(moves: &mut [i32], improvements: &[i32]) {
    for &mv in improvements {
        let index = moves.iter().position(|&m| m == mv).expect("an improvement is a root move");
        moves[..=index].rotate_right(1);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn search_with_cached_bound(
        bound: Bound,
        value: i32,
        depth: i32,
        mv: i32,
        options: SearchOptions,
    ) -> ((i32, i32), u64) {
        let mut engine = SearchEngine::new(10, options);
        let board = Board::INITIAL;
        let player = Player::Black;
        let SearchEngine { options, probcut_errors, midgame_table, endgame_table, main, clock, .. } = &mut engine;
        midgame_table.store(
            board.discs(player),
            board.discs(player.opponent()),
            player.index() as i32,
            depth,
            bound,
            value,
            mv,
        );
        let context = SearchContext::new(options, probcut_errors.as_deref(), midgame_table, endgame_table, *clock);
        let mut searcher = Searcher::new(context, main, &NoStop);
        searcher.hash_get_height = 0;
        searcher.hash_put_height = 0;
        let result = searcher.search(&board, player, 2, 0, -1, 0, 1).unwrap();
        (result, searcher.nodes)
    }

    #[test]
    fn midgame_upper_bound_at_alpha_cuts_off() {
        let mv = Board::INITIAL.legal_moves(Player::Black).trailing_zeros() as i32;
        let options = SearchOptions { selective: false, ..SearchOptions::default() };
        let (result, nodes) = search_with_cached_bound(Bound::Upper, 0, 2, mv, options);
        assert_eq!(nodes, 1);
        assert_eq!(result, (0, mv));
    }

    #[test]
    fn midgame_hash_cutoffs_respect_bounds_depth_and_move() {
        let mv = Board::INITIAL.legal_moves(Player::Black).trailing_zeros() as i32;
        let options = SearchOptions { selective: false, ..SearchOptions::default() };
        for (bound, value, depth, stored_move, cuts) in [
            (Bound::Upper, -1, 2, mv, true),
            (Bound::Upper, 1, 2, mv, false),
            (Bound::Upper, 0, 1, mv, false),
            (Bound::Upper, 0, 2, -1, false),
            (Bound::Upper, 0, 2, 0, false),
            (Bound::Lower, 1, 2, mv, true),
            (Bound::Lower, 0, 2, mv, false),
            (Bound::Exact, 0, 2, mv, true),
        ] {
            let (_, nodes) = search_with_cached_bound(bound, value, depth, stored_move, options.clone());
            assert_eq!(nodes == 1, cuts, "{bound:?}, value {value}, depth {depth}, move {stored_move}");
        }
    }

    #[test]
    fn engines_and_signals_can_move_to_other_threads() {
        fn send<T: Send>() {}
        fn sync<T: Sync>() {}
        send::<SearchEngine>();
        send::<SearchSignals>();
        sync::<SearchSignals>();
    }

    /// Random endings with 12–18 empty squares and a legal move for the side to move.
    fn random_endings(count: usize) -> Vec<(Board, Player)> {
        let mut seed = 0x9E37_79B9_7F4A_7C15u64;
        let mut endings = Vec::new();
        while endings.len() < count {
            let empties = 12 + endings.len() as u32 % 7;
            let (mut board, mut player) = (Board::INITIAL, Player::Black);
            while board.empty_count() > empties && !board.is_game_over() {
                let mut moves = board.legal_moves(player);
                if moves != 0 {
                    seed ^= seed << 13;
                    seed ^= seed >> 7;
                    seed ^= seed << 17;
                    for _ in 0..seed % moves.count_ones() as u64 {
                        moves &= moves - 1;
                    }
                    board = board.play_unchecked(player, moves.trailing_zeros());
                }
                player = player.opponent();
            }
            if board.empty_count() == empties && board.legal_moves(player) != 0 {
                endings.push((board, player));
            }
        }
        endings
    }

    /// Eight threads solve different positions at the same time on one engine's small tables, twice each without
    /// clearing them, so entries of all of them replace each other in the same slots all the time: every score is
    /// the single-threaded solver's.
    #[test]
    fn threads_solving_different_positions_on_one_table_get_exact_scores() {
        let endings = random_endings(48);
        let expected: Vec<i32> = endings
            .iter()
            .map(|(board, player)| {
                SearchEngine::new(16, SearchOptions::default())
                    .search_simple(board, *player, &SearchLimits::SOLVE)
                    .score
            })
            .collect();

        const THREADS: usize = 8;
        let mut engine = SearchEngine::new(10, SearchOptions::default());
        let pool = ThreadPool::new(THREADS - 1, std_spawner, ThreadState::default);
        let SearchEngine { options, probcut_errors, midgame_table, endgame_table, main, clock, .. } = &mut engine;
        let context = SearchContext::new(options, probcut_errors.as_deref(), midgame_table, endgame_table, *clock);
        let solve = |thread: usize, state: &mut ThreadState| -> Vec<(usize, i32)> {
            let mut scores = Vec::new();
            for _ in 0..2 {
                for (i, (board, player)) in endings.iter().enumerate().filter(|(i, _)| i % THREADS == thread) {
                    let mut searcher = Searcher::new(context, state, &NoStop);
                    scores.push((i, searcher.run(board, *player, &SearchLimits::SOLVE, None).unwrap().score));
                }
            }
            scores
        };

        let results = std::sync::Mutex::new(Vec::new());
        pool.scope(|scope| {
            for thread in 1..THREADS {
                let (solve, results) = (&solve, &results);
                assert!(scope.try_spawn(move |state: &mut ThreadState| {
                    let scores = solve(thread, state);
                    results.lock().unwrap().extend(scores);
                }));
            }
            let scores = solve(0, main);
            results.lock().unwrap().extend(scores);
        });

        let results = results.into_inner().unwrap();
        assert_eq!(results.len(), 2 * endings.len());
        for (i, score) in results {
            assert_eq!(score, expected[i], "{}", endings[i].0);
        }
    }

    #[test]
    fn shallow_depth_keeps_the_parity() {
        let shallow: Vec<i32> = (1..=10).map(probcut_shallow_depth).collect();
        assert_eq!(shallow, [1, 0, 1, 2, 3, 2, 3, 4, 5, 4]);
    }

    #[test]
    fn errors_follow_the_fitted_model() {
        let errors = probcut_errors(1.0);
        // 30 discs, depth 6 (shallow 2): σ = 1.8871 + 2.9543 * 0.46875 + 0.0660 * 0.46875² - 1.3498 + 1.6668.
        assert_eq!(errors[30][6], 361);
        assert!(errors.iter().all(|row| row[1..].iter().all(|&e| e >= 50)));
    }

    #[test]
    fn move_to_front_puts_the_last_improvement_first() {
        let mut moves = [1, 2, 3, 4];
        move_to_front(&mut moves, &[3, 4]);
        assert_eq!(moves, [4, 3, 1, 2]);
    }

    #[test]
    fn final_scores_give_the_empties_to_the_winner() {
        assert_eq!(final_score(0b111, 0b1), 2 + 60);
        assert_eq!(final_score(0b1, 0b111), -2 - 60);
        assert_eq!(game_over_score(0b11, 0b1100), 0);
        assert_eq!(game_over_score(u64::MAX, 0), 64 + WIN_SCORE);
    }
}
