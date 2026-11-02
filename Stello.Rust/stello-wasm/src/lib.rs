//! The engine for the web app's Web Worker (`Stello.Net/Stello.Web/wwwroot/js/engine-worker.js`), built with
//! `wasm-bindgen --target web` (Plans/making-stello-3-plan.md, phase 3).
//!
//! The worker calls [`init`] once and then [`search`] for every computer move; the page does the book lookup and the
//! book learning itself. A search runs to the end inside the call, so the worker cannot receive messages while it
//! runs: the page stops it through a `SharedArrayBuffer` instead, which the [`Host`]'s `signal()` reads with
//! `Atomics.load` at every abort check of the engine (every 1024 nodes).
//!
//! WebAssembly has no clock in `std`, so the engine is timed with `performance.now()`.

mod protocol;

use std::cell::RefCell;
use std::sync::Arc;
use std::time::Duration;

use stello_core::{EvalWeights, SearchEngine, SearchError, SearchInfo, SearchOptions, Square, StopSignal};
use wasm_bindgen::prelude::{JsError, wasm_bindgen};

/// How often progress is posted when neither the depth nor the best move changed: posting every report (each root
/// move, and the engine's 100 ms heartbeats) would flood the page. The .NET worker of Stello 2.0 did the same.
const PROGRESS_INTERVAL_MS: f64 = 100.0;

/// The values of the stop signal in the `SharedArrayBuffer` (`engine-client.js`); 0 means run.
const SIGNAL_MOVE_NOW: i32 = 1;
const SIGNAL_CANCEL: i32 = 2;

#[wasm_bindgen]
extern "C" {
    /// The worker's side of a search: `{ progress(json), signal() }`.
    pub type Host;

    /// Posts a progress report (the JSON of `EngineProtocol`'s `ProgressReport`) to the page.
    #[wasm_bindgen(method)]
    fn progress(this: &Host, json: &str);

    /// The stop signal: 0 run, 1 Move Now, 2 cancel.
    #[wasm_bindgen(method)]
    fn signal(this: &Host) -> i32;

    /// `performance.now()`: milliseconds since the worker started, with fractions.
    #[wasm_bindgen(js_namespace = performance, js_name = now)]
    fn performance_now() -> f64;
}

/// The engine and the evaluation it was made with (to make it again for other hash bits).
struct State {
    engine: SearchEngine,
    pattern: Option<Arc<EvalWeights>>,
}

thread_local! {
    // A worker runs one engine; wasm32-unknown-unknown has one thread.
    static STATE: RefCell<Option<State>> = const { RefCell::new(None) };
}

/// The engine's version, for the worker's sanity check.
#[wasm_bindgen]
pub fn version() -> String {
    stello_core::VERSION.to_string()
}

/// Makes the engine (C#: `new SearchEngine(hashBits)`): `evaluation` is the bytes of an evaluation file
/// (`stello-eval.bin`), or `undefined` for the built-in one; `hash_bits` is 10–26. The evaluation is decoded here,
/// so the first search does not wait for it.
#[wasm_bindgen]
pub fn init(evaluation: Option<Vec<u8>>, hash_bits: u32) -> Result<(), JsError> {
    if !(10..=26).contains(&hash_bits) {
        return Err(JsError::new(&format!("Hash bits must be 10-26, not {hash_bits}.")));
    }

    let pattern = match evaluation {
        Some(bytes) => Some(Arc::new(EvalWeights::read(&bytes).map_err(|error| JsError::new(&error.to_string()))?)),
        None => {
            EvalWeights::built_in();
            None
        }
    };
    let engine = new_engine(hash_bits, pattern.clone());
    STATE.with(|state| *state.borrow_mut() = Some(State { engine, pattern }));
    Ok(())
}

/// Finds the best move (C#: `SearchEngine.Search`) for `request`, the JSON of `EngineProtocol`'s `MoveRequest`,
/// and returns the JSON of a `MoveResponse`. Progress goes to `host.progress()`: new depths and new best moves at
/// once, other reports at most every 100 ms. A request with other hash bits than the engine's makes a new engine
/// (empty hash tables), as `ComputerPlayer` did in the worker of Stello 2.0.
///
/// Throws if the engine is not initialised, the request is not valid, or the search was cancelled ("cancelled").
#[wasm_bindgen]
pub fn search(request: &str, host: &Host) -> Result<String, JsError> {
    let request = protocol::read_request(request).map_err(|message| JsError::new(&message))?;
    STATE.with(|state| {
        let mut state = state.borrow_mut();
        let state = state.as_mut().ok_or_else(|| JsError::new("The engine has not been initialised."))?;
        if state.engine.hash_bits() != request.hash_bits {
            state.engine = new_engine(request.hash_bits, state.pattern.clone());
        }

        let mut poster = ProgressPoster::default();
        let mut report = |info: &SearchInfo| poster.report(host, info);
        let result = state.engine.search(
            &request.board,
            request.player,
            &request.limits,
            Some(&mut report),
            &HostSignal(host),
            None,
        );
        match result {
            Ok(found) => Ok(protocol::write_result(&found)),
            Err(SearchError::Cancelled) => Err(JsError::new("cancelled")),
            Err(error) => Err(JsError::new(&error.to_string())),
        }
    })
}

fn new_engine(hash_bits: u32, pattern: Option<Arc<EvalWeights>>) -> SearchEngine {
    let mut engine = SearchEngine::new(hash_bits, SearchOptions { pattern, ..SearchOptions::default() });
    engine.set_clock(browser_clock);
    engine
}

fn browser_clock() -> Duration {
    Duration::from_secs_f64(performance_now().max(0.0) / 1000.0)
}

/// The page's stop signal, read at the engine's abort checks.
struct HostSignal<'a>(&'a Host);

impl StopSignal for HostSignal<'_> {
    fn is_cancelled(&self) -> bool {
        self.0.signal() == SIGNAL_CANCEL
    }

    fn is_move_now(&self) -> bool {
        self.0.signal() == SIGNAL_MOVE_NOW
    }
}

/// Posts a report if the depth or the best move changed, or [`PROGRESS_INTERVAL_MS`] passed since the last post
/// (C#: the .NET worker's `ProgressPoster`).
#[derive(Default)]
struct ProgressPoster {
    last: Option<Posted>,
}

struct Posted {
    at_ms: f64,
    depth: i32,
    best_move: Option<Square>,
}

impl ProgressPoster {
    fn report(&mut self, host: &Host, info: &SearchInfo) {
        let now = performance_now();
        if let Some(last) = &self.last
            && last.depth == info.depth
            && last.best_move == info.best_move
            && now - last.at_ms < PROGRESS_INTERVAL_MS
        {
            return;
        }

        self.last = Some(Posted { at_ms: now, depth: info.depth, best_move: info.best_move });
        host.progress(&protocol::write_progress(info));
    }
}
