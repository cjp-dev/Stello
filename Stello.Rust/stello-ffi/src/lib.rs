//! The C ABI that .NET calls: `Stello.Net/Stello.Engine/Native/StelloNative.cs` (`[LibraryImport("stello_engine")]`),
//! used by the `SearchEngine` wrapper (Plans/making-stello-3-plan.md, phase 2).
//!
//! # The objects
//! - An **engine** ([`StelloEngine`], an opaque handle) is a [`stello_core::SearchEngine`]: its hash tables, options and
//!   evaluation. [`stello_engine_create`] makes one and [`stello_engine_destroy`] frees it. It runs one search at a
//!   time; engines on different threads are independent (they share only tables that are built once per process and
//!   then only read).
//! - **Signals** ([`SearchSignals`], an opaque handle) are two atomic flags, cancel and Move Now, that a running search
//!   reads every 1024 nodes. Any thread may set them while a search runs ([`stello_signals_cancel`],
//!   [`stello_signals_move_now`]); [`stello_signals_reset`] clears them for the next search.
//!
//! # Rules for the caller
//! - Every function checks its arguments and returns a status code ([`STELLO_OK`], ...) instead of failing; no panic
//!   crosses the boundary (the release profile aborts on a panic, and every entry point also catches one in builds
//!   that unwind).
//! - A handle must come from the matching create function and must not be used after it is destroyed. An engine must
//!   not be used by two threads at the same time; signals may.
//! - Callbacks run on the thread that called [`stello_engine_search`], during the call. They must not unwind (a .NET
//!   `[UnmanagedCallersOnly]` method catches its exceptions) and must not call into the same engine.
//!
//! # Layout
//! The `#[repr(C)]` structs have their fields in a fixed order with explicit sizes; their total sizes are checked at
//! compile time and by the .NET tests. Squares are 0–63 (a1 = 0, h8 = 63, bit n of a bitboard); -1 is none.
//! [`stello_abi_version`] changes whenever any of this changes.

use std::ffi::c_void;
use std::panic::{AssertUnwindSafe, catch_unwind};
use std::sync::Arc;

use stello_core::search::NoStop;
use stello_core::{
    Board, EvalWeights, Player, ScoreKind, SearchEngine, SearchError, SearchInfo, SearchLimits, SearchOptions,
    SearchResult, SearchSignals, StopSignal, TimeControlMode,
};

/// The call succeeded.
pub const STELLO_OK: i32 = 0;
/// The search was cancelled (the cancel signal was set). C#: `OperationCanceledException`.
pub const STELLO_CANCELLED: i32 = 1;
/// None of the moves in `only_moves` is legal. C#: `ArgumentException`.
pub const STELLO_NO_LEGAL_ONLY_MOVE: i32 = 2;
/// An argument is not valid: a null handle or pointer, hash bits outside 10–26, a square with both colours, a player
/// other than 0 or 1, or limits that C#'s `SearchLimits` would not allow.
pub const STELLO_INVALID_ARGUMENT: i32 = 3;
/// The evaluation file could not be read (not a Stello evaluation file, version 2).
pub const STELLO_INVALID_EVALUATION: i32 = 4;
/// The engine failed unexpectedly (a panic, caught in builds that unwind; release builds abort instead).
pub const STELLO_INTERNAL_ERROR: i32 = 5;

/// The version of this ABI (functions, structs, codes). .NET checks it before it uses the library.
pub const ABI_VERSION: u32 = 1;

/// An engine: C# `SearchEngine`'s state (hash tables, options, evaluation). Opaque to the caller.
pub struct StelloEngine {
    engine: SearchEngine,
}

/// Progress of a running search (C#: `SearchInfo`), passed to the progress callback. 48 bytes.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct StelloSearchInfo {
    /// Nodes searched so far.
    pub nodes: u64,
    /// Evaluations so far.
    pub evaluations: u64,
    /// Time since the search started, in nanoseconds.
    pub elapsed_ns: u64,
    /// Plies searched, or empty squares for the endgame search.
    pub depth: i32,
    /// The root move being searched (0–63).
    pub current_move: i32,
    /// The best move so far (0–63), or -1.
    pub best_move: i32,
    /// The best score so far.
    pub score: i32,
    /// C# `ScoreKind`: 0 None, 1 Heuristic, 2 WinLossDraw, 3 Exact, 4 Book.
    pub kind: i32,
    /// Always 0 (padding to a multiple of 8 bytes).
    pub reserved: i32,
}

/// The result of a search (C#: `SearchResult`). 40 bytes.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct StelloSearchResult {
    /// Nodes searched.
    pub nodes: u64,
    /// Evaluations.
    pub evaluations: u64,
    /// The time the search took, in nanoseconds.
    pub elapsed_ns: u64,
    /// The move (0–63), or -1 for a pass (the side to move has no legal move).
    pub mv: i32,
    /// The score, as C#'s `SearchResult.Score`.
    pub score: i32,
    /// C# `ScoreKind`: 0 None, 1 Heuristic, 2 WinLossDraw, 3 Exact, 4 Book.
    pub kind: i32,
    /// Plies searched, or empty squares when solved; 0 when no search was needed.
    pub depth: i32,
}

const _: () = assert!(size_of::<StelloSearchInfo>() == 48);
const _: () = assert!(size_of::<StelloSearchResult>() == 40);

// An engine may move between threads between searches (C# runs searches on any thread).
const _: () = {
    const fn is_send<T: Send>() {}
    is_send::<StelloEngine>();
    is_send::<SearchSignals>();
};

/// Called with the progress of a running search (`info` is valid only during the call) and the caller's `user`
/// pointer: after every root move, and as a heartbeat (the last report again) when 100 ms passed without one.
pub type StelloProgressCallback = Option<unsafe extern "C" fn(info: *const StelloSearchInfo, user: *mut c_void)>;

/// Called after each finished midgame iteration with its depth (plies) and score, and the caller's `user` pointer.
/// C#: `SearchEngine.IterationDone` (the trainer's Multi-ProbCut fit).
pub type StelloIterationCallback = Option<unsafe extern "C" fn(depth: i32, score: i32, user: *mut c_void)>;

/// The engine version as major * 10000 + minor * 100 + patch, so .NET can check it loaded the right library.
#[unsafe(no_mangle)]
pub extern "C" fn stello_version() -> u32 {
    let mut parts = stello_core::VERSION.split('.').map(|part| part.parse::<u32>().unwrap_or(0));
    let (major, minor, patch) = (parts.next().unwrap_or(0), parts.next().unwrap_or(0), parts.next().unwrap_or(0));
    major * 10_000 + minor * 100 + patch
}

/// [`ABI_VERSION`]: the version of the functions, structs and codes of this library.
#[unsafe(no_mangle)]
pub extern "C" fn stello_abi_version() -> u32 {
    ABI_VERSION
}

/// Runs `body`, turning a panic (in builds that unwind) into [`STELLO_INTERNAL_ERROR`].
fn guarded(body: impl FnOnce() -> i32) -> i32 {
    catch_unwind(AssertUnwindSafe(body)).unwrap_or(STELLO_INTERNAL_ERROR)
}

/// Writes `value` to `status` if it is not null.
///
/// # Safety
/// `status` is null or valid for a write.
unsafe fn set_status(status: *mut i32, value: i32) {
    if !status.is_null() {
        // SAFETY: the caller passes null or a valid pointer.
        unsafe { status.write(value) };
    }
}

/// Makes an engine (C#: `new SearchEngine(hashBits, options)`). Returns the engine, or null with the reason in
/// `status` ([`STELLO_INVALID_ARGUMENT`], [`STELLO_INVALID_EVALUATION`]); `status` gets [`STELLO_OK`] on success and
/// may be null.
///
/// - `hash_bits`: 10–26; each of the two hash tables gets 2^`hash_bits` slots of two 24-byte entries;
/// - `selective`: 0 turns Multi-ProbCut off (C#: `SearchOptions.Selective`), anything else keeps it on;
/// - `probcut_percent`: Multi-ProbCut's t × 100, or -1 for the default (C#: `SearchOptions.ProbCutPercent`), 0–100000;
/// - `soft_time_percent`: no new iteration after this share (0–100) of the hard time limit, or -1 for 2/3
///   (C#: `SearchOptions.SoftTimePercent`);
/// - `eval`, `eval_length`: the bytes of an evaluation file (`stello-eval.bin`, gzip, version 2), read like C#'s
///   `EvalWeights.Read`; null (or a length of 0) for the built-in evaluation. The bytes are only read during the call.
///
/// # Safety
/// `eval` is null or points to `eval_length` readable bytes; `status` is null or valid for a write.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn stello_engine_create(
    hash_bits: i32,
    selective: i32,
    probcut_percent: i32,
    soft_time_percent: i32,
    eval: *const u8,
    eval_length: usize,
    status: *mut i32,
) -> *mut StelloEngine {
    let mut engine: *mut StelloEngine = std::ptr::null_mut();
    let code = guarded(|| {
        if !(10..=26).contains(&hash_bits)
            || !(-1..=100_000).contains(&probcut_percent)
            || !(-1..=100).contains(&soft_time_percent)
        {
            return STELLO_INVALID_ARGUMENT;
        }

        let pattern = if eval.is_null() || eval_length == 0 {
            None
        } else {
            // SAFETY: the caller passes `eval_length` readable bytes at `eval`.
            let bytes = unsafe { std::slice::from_raw_parts(eval, eval_length) };
            match EvalWeights::read(bytes) {
                Ok(weights) => Some(Arc::new(weights)),
                Err(_) => return STELLO_INVALID_EVALUATION,
            }
        };

        let options = SearchOptions {
            pattern,
            soft_time_percent: (soft_time_percent >= 0).then_some(soft_time_percent),
            selective: selective != 0,
            probcut_percent: (probcut_percent >= 0).then_some(probcut_percent),
        };
        engine = Box::into_raw(Box::new(StelloEngine { engine: SearchEngine::new(hash_bits as u32, options) }));
        STELLO_OK
    });

    // SAFETY: the caller passes null or a valid pointer.
    unsafe { set_status(status, code) };
    engine
}

/// Frees an engine made by [`stello_engine_create`]. Null is ignored.
///
/// # Safety
/// `engine` is null or an engine that is not in use and is not used again.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn stello_engine_destroy(engine: *mut StelloEngine) {
    if !engine.is_null() {
        // SAFETY: the engine came from Box::into_raw in stello_engine_create and is freed once.
        drop(unsafe { Box::from_raw(engine) });
    }
}

/// Clears the hash tables and the solver's local table (C#: `SearchEngine.ClearHash`).
/// Returns [`STELLO_OK`], or [`STELLO_INVALID_ARGUMENT`] for a null engine.
///
/// # Safety
/// `engine` is null or a valid engine that no other thread uses during the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn stello_engine_clear_hash(engine: *mut StelloEngine) -> i32 {
    // SAFETY: the caller passes null or a valid engine used by this thread only.
    let Some(engine) = (unsafe { engine.as_mut() }) else { return STELLO_INVALID_ARGUMENT };
    guarded(|| {
        engine.engine.clear_hash();
        STELLO_OK
    })
}

/// A pointer that the iteration callback carries; the caller is responsible for it (see
/// [`stello_engine_set_iteration_done`]).
struct UserPointer(*mut c_void);

// SAFETY: the pointer is only passed back to the caller's callback, on the thread that runs the search.
unsafe impl Send for UserPointer {}

/// Sets the callback called after each finished midgame iteration (C#: `SearchEngine.IterationDone`), with `user`
/// passed back to it; a null `callback` removes it. It stays set for later searches until it is changed, so the
/// caller must keep `user` valid until then. Returns [`STELLO_OK`], or [`STELLO_INVALID_ARGUMENT`] for a null engine.
///
/// # Safety
/// `engine` is null or a valid engine that no other thread uses during the call; `callback` is null or a function
/// that does not unwind, and `user` is what it expects during every search until the callback is changed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn stello_engine_set_iteration_done(
    engine: *mut StelloEngine,
    callback: StelloIterationCallback,
    user: *mut c_void,
) -> i32 {
    // SAFETY: the caller passes null or a valid engine used by this thread only.
    let Some(engine) = (unsafe { engine.as_mut() }) else { return STELLO_INVALID_ARGUMENT };
    guarded(|| {
        let callback = callback.map(|callback| {
            let user = UserPointer(user);
            Box::new(move |depth: i32, score: i32| {
                let user = &user;
                // SAFETY: the caller keeps `user` valid while the callback is set.
                unsafe { callback(depth, score, user.0) }
            }) as Box<dyn FnMut(i32, i32) + Send>
        });
        engine.engine.set_iteration_done(callback);
        STELLO_OK
    })
}

/// Makes a pair of stop signals (cancel and Move Now), both clear. Never null.
#[unsafe(no_mangle)]
pub extern "C" fn stello_signals_create() -> *mut SearchSignals {
    Box::into_raw(Box::new(SearchSignals::new()))
}

/// Frees signals made by [`stello_signals_create`]. Null is ignored.
///
/// # Safety
/// `signals` is null or signals that no search and no other thread uses any more.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn stello_signals_destroy(signals: *mut SearchSignals) {
    if !signals.is_null() {
        // SAFETY: the signals came from Box::into_raw in stello_signals_create and are freed once.
        drop(unsafe { Box::from_raw(signals) });
    }
}

/// Sets the cancel signal: a search that reads it returns [`STELLO_CANCELLED`]. Any thread may call it. Null is
/// ignored.
///
/// # Safety
/// `signals` is null or valid signals.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn stello_signals_cancel(signals: *const SearchSignals) {
    // SAFETY: the caller passes null or valid signals; setting them is atomic.
    if let Some(signals) = unsafe { signals.as_ref() } {
        signals.cancel();
    }
}

/// Sets the Move Now signal: a search that reads it returns the best move found so far. Any thread may call it.
/// Null is ignored.
///
/// # Safety
/// `signals` is null or valid signals.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn stello_signals_move_now(signals: *const SearchSignals) {
    // SAFETY: the caller passes null or valid signals; setting them is atomic.
    if let Some(signals) = unsafe { signals.as_ref() } {
        signals.move_now();
    }
}

/// Clears both signals for the next search. Null is ignored.
///
/// # Safety
/// `signals` is null or valid signals.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn stello_signals_reset(signals: *const SearchSignals) {
    // SAFETY: the caller passes null or valid signals; clearing them is atomic.
    if let Some(signals) = unsafe { signals.as_ref() } {
        signals.reset();
    }
}

/// Finds the best move (C#: `SearchEngine.Search`) and writes it to `result`.
///
/// - `black`, `white`: the discs (bit n = square n); no square may have both;
/// - `player`: the side to move, 0 Black or 1 White;
/// - `mode`, `depth`, `time_ms`: C# `SearchLimits`: `mode` is `TimeControlMode` (0 FixedDepth with `depth` 1–60,
///   1 TimePerMove with `time_ms` ≥ 0, 2 TimePerGame with the time left in `time_ms`, which may be negative, 3 Solve);
///   the time is `(long)SearchLimits.Time.TotalMilliseconds`, as C#'s time control uses it;
/// - `only_moves`, `has_only_moves`: if `has_only_moves` is not 0, search only these moves, even if just one is left
///   (C#: `onlyMoves`);
/// - `signals`: the stop signals read during the search, or null for none;
/// - `progress`, `progress_user`: the progress callback (see [`StelloProgressCallback`]) and its user pointer, or a
///   null callback for no reports (and no heartbeats);
/// - `result`: written on [`STELLO_OK`] only.
///
/// Returns [`STELLO_OK`], [`STELLO_CANCELLED`], [`STELLO_NO_LEGAL_ONLY_MOVE`] or [`STELLO_INVALID_ARGUMENT`].
///
/// # Safety
/// `engine` is null or a valid engine that no other thread uses during the call; `signals` is null or valid signals
/// for the whole call; `progress` is null or a function that does not unwind and gets `progress_user`; `result` is
/// null or valid for a write.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn stello_engine_search(
    engine: *mut StelloEngine,
    black: u64,
    white: u64,
    player: i32,
    mode: i32,
    depth: i32,
    time_ms: i64,
    only_moves: u64,
    has_only_moves: i32,
    signals: *const SearchSignals,
    progress: StelloProgressCallback,
    progress_user: *mut c_void,
    result: *mut StelloSearchResult,
) -> i32 {
    // SAFETY: the caller passes null or a valid engine used by this thread only.
    let Some(engine) = (unsafe { engine.as_mut() }) else { return STELLO_INVALID_ARGUMENT };
    if result.is_null() || black & white != 0 {
        return STELLO_INVALID_ARGUMENT;
    }

    let player = match player {
        0 => Player::Black,
        1 => Player::White,
        _ => return STELLO_INVALID_ARGUMENT,
    };
    let mode = match mode {
        0 => TimeControlMode::FixedDepth,
        1 => TimeControlMode::TimePerMove,
        2 => TimeControlMode::TimePerGame,
        3 => TimeControlMode::Solve,
        _ => return STELLO_INVALID_ARGUMENT,
    };
    let Some(limits) = SearchLimits::from_parts(mode, depth, time_ms) else { return STELLO_INVALID_ARGUMENT };

    guarded(|| {
        // SAFETY: the caller passes null or signals valid for the whole call.
        let stop: &dyn StopSignal = match unsafe { signals.as_ref() } {
            Some(signals) => signals,
            None => &NoStop,
        };
        let mut report = |info: &SearchInfo| {
            let native = StelloSearchInfo {
                nodes: info.nodes,
                evaluations: info.evaluations,
                elapsed_ns: nanoseconds(info.elapsed),
                depth: info.depth,
                current_move: info.current_move.index() as i32,
                best_move: info.best_move.map_or(-1, |square| square.index() as i32),
                score: info.score,
                kind: kind_code(info.kind),
                reserved: 0,
            };
            if let Some(progress) = progress {
                // SAFETY: the caller's callback takes this pointer and its user pointer, and does not unwind.
                unsafe { progress(&native, progress_user) };
            }
        };
        let reporter: Option<&mut dyn FnMut(&SearchInfo)> = if progress.is_some() { Some(&mut report) } else { None };
        let board = Board::new(black, white);
        let only_moves = (has_only_moves != 0).then_some(only_moves);

        match engine.engine.search(&board, player, &limits, reporter, stop, only_moves) {
            Ok(found) => {
                // SAFETY: checked not null above; the caller passes a pointer valid for a write.
                unsafe { result.write(native_result(&found)) };
                STELLO_OK
            }
            Err(SearchError::Cancelled) => STELLO_CANCELLED,
            Err(SearchError::NoLegalOnlyMove) => STELLO_NO_LEGAL_ONLY_MOVE,
        }
    })
}

fn native_result(result: &SearchResult) -> StelloSearchResult {
    StelloSearchResult {
        nodes: result.nodes,
        evaluations: result.evaluations,
        elapsed_ns: nanoseconds(result.elapsed),
        mv: result.mv.0.map_or(-1, |square| square.index() as i32),
        score: result.score,
        kind: kind_code(result.kind),
        depth: result.depth,
    }
}

fn kind_code(kind: ScoreKind) -> i32 {
    kind as u8 as i32
}

fn nanoseconds(duration: std::time::Duration) -> u64 {
    u64::try_from(duration.as_nanos()).unwrap_or(u64::MAX)
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn count_reports(_info: *const StelloSearchInfo, user: *mut c_void) {
        // SAFETY: the tests pass a pointer to a u32.
        unsafe { *(user as *mut u32) += 1 };
    }

    unsafe extern "C" fn record_iteration(depth: i32, _score: i32, user: *mut c_void) {
        // SAFETY: the tests pass a pointer to a Vec<i32>.
        unsafe { (*(user as *mut Vec<i32>)).push(depth) };
    }

    fn create(hash_bits: i32) -> (*mut StelloEngine, i32) {
        let mut status = -1;
        // SAFETY: no evaluation bytes; status is a local.
        let engine = unsafe { stello_engine_create(hash_bits, 1, -1, -1, std::ptr::null(), 0, &mut status) };
        (engine, status)
    }

    #[test]
    fn version_and_layout() {
        assert_eq!(stello_version(), 30_000);
        assert_eq!(stello_abi_version(), ABI_VERSION);
        assert_eq!(std::mem::offset_of!(StelloSearchInfo, depth), 24);
        assert_eq!(std::mem::offset_of!(StelloSearchResult, mv), 24);
    }

    #[test]
    fn create_checks_its_arguments() {
        let (engine, status) = create(9);
        assert!(engine.is_null());
        assert_eq!(status, STELLO_INVALID_ARGUMENT);

        let bytes = [1u8, 2, 3];
        let mut status = -1;
        // SAFETY: three readable bytes; status is a local.
        let engine = unsafe { stello_engine_create(12, 1, -1, -1, bytes.as_ptr(), bytes.len(), &mut status) };
        assert!(engine.is_null());
        assert_eq!(status, STELLO_INVALID_EVALUATION);

        let file = EvalWeights::built_in_file();
        // SAFETY: the built-in file's bytes; status is a local.
        let engine = unsafe { stello_engine_create(12, 0, 150, 50, file.as_ptr(), file.len(), &mut status) };
        assert!(!engine.is_null());
        assert_eq!(status, STELLO_OK);
        // SAFETY: made above, not used again.
        unsafe { stello_engine_destroy(engine) };
    }

    #[test]
    fn search_checks_its_arguments_and_finds_a_move() {
        let (engine, status) = create(12);
        assert_eq!(status, STELLO_OK);
        let initial = Board::INITIAL;
        let mut result = StelloSearchResult::default();
        let search = |player: i32, mode: i32, depth: i32, only: Option<u64>, result: &mut StelloSearchResult| {
            // SAFETY: the engine is valid; no signals or callback; result is a local.
            unsafe {
                stello_engine_search(
                    engine,
                    initial.black(),
                    initial.white(),
                    player,
                    mode,
                    depth,
                    0,
                    only.unwrap_or(0),
                    only.is_some() as i32,
                    std::ptr::null(),
                    None,
                    std::ptr::null_mut(),
                    result,
                )
            }
        };

        assert_eq!(search(2, 0, 4, None, &mut result), STELLO_INVALID_ARGUMENT);
        assert_eq!(search(0, 4, 4, None, &mut result), STELLO_INVALID_ARGUMENT);
        assert_eq!(search(0, 0, 61, None, &mut result), STELLO_INVALID_ARGUMENT);
        assert_eq!(search(0, 0, 4, Some(1), &mut result), STELLO_NO_LEGAL_ONLY_MOVE);
        assert_eq!(search(0, 0, 4, None, &mut result), STELLO_OK);
        assert_eq!(result.depth, 4);
        assert_eq!(result.kind, ScoreKind::Heuristic as i32);
        assert!(initial.legal_moves(Player::Black) & (1 << result.mv) != 0);
        // SAFETY: the engine is valid.
        assert_eq!(unsafe { stello_engine_clear_hash(engine) }, STELLO_OK);
        // SAFETY: null engine and result.
        assert_eq!(
            unsafe {
                stello_engine_search(
                    std::ptr::null_mut(),
                    0,
                    0,
                    0,
                    0,
                    1,
                    0,
                    0,
                    0,
                    std::ptr::null(),
                    None,
                    std::ptr::null_mut(),
                    std::ptr::null_mut(),
                )
            },
            STELLO_INVALID_ARGUMENT
        );
        // SAFETY: made above, not used again.
        unsafe { stello_engine_destroy(engine) };
    }

    #[test]
    fn callbacks_and_signals() {
        let (engine, _) = create(12);
        let mut iterations: Vec<i32> = Vec::new();
        let mut reports = 0u32;
        let signals = stello_signals_create();
        let initial = Board::INITIAL;
        let mut result = StelloSearchResult::default();
        // SAFETY: the engine and signals are valid; the user pointers point to locals that outlive the searches.
        unsafe {
            stello_engine_set_iteration_done(engine, Some(record_iteration), &mut iterations as *mut Vec<i32> as _);
            let code = stello_engine_search(
                engine,
                initial.black(),
                initial.white(),
                0,
                0,
                5,
                0,
                0,
                0,
                signals,
                Some(count_reports),
                &mut reports as *mut u32 as _,
                &mut result,
            );
            assert_eq!(code, STELLO_OK);
            stello_engine_set_iteration_done(engine, None, std::ptr::null_mut());

            stello_signals_cancel(signals);
            let code = stello_engine_search(
                engine,
                initial.black(),
                initial.white(),
                0,
                0,
                5,
                0,
                0,
                0,
                signals,
                None,
                std::ptr::null_mut(),
                &mut result,
            );
            assert_eq!(code, STELLO_CANCELLED);
            stello_signals_reset(signals);
            stello_signals_move_now(signals);
            let code = stello_engine_search(
                engine,
                initial.black(),
                initial.white(),
                0,
                3,
                0,
                0,
                0,
                0,
                signals,
                None,
                std::ptr::null_mut(),
                &mut result,
            );
            assert_eq!(code, STELLO_OK);
            stello_signals_destroy(signals);
            stello_engine_destroy(engine);
        }

        assert_eq!(iterations, [1, 2, 3, 4, 5]);
        // One report per root move (4 at the start) and depth.
        assert!(reports >= 20, "{reports} reports");
    }
}
