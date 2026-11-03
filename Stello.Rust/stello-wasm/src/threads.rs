//! The threads of the threaded build (Plans/making-stello-3-plan.md, phase 8): Web Workers that share the module's
//! memory, started by the engine worker (`engine-worker.js`) before a search that uses them, each running
//! [`pool_worker`]. The engine's [`stello_core::search::ThreadPool`] gets them through [`web_spawner`] (its
//! [`stello_core::search::Spawner`]): a body to run is queued here and taken by a waiting Web Worker. `std::thread`
//! cannot start threads in WebAssembly, which is why the threads come from JavaScript.
//!
//! The Web Workers wait for bodies with `Atomics.wait` (through `std`'s `Condvar`), which a worker may do; the page's
//! main thread never runs this code.

use std::collections::VecDeque;
use std::sync::{Condvar, Mutex};

use wasm_bindgen::prelude::wasm_bindgen;

type Body = Box<dyn FnOnce() + Send>;

/// The bodies to run; the Web Workers that wait for one and were not yet promised one (`idle`); and those that run
/// one (`running`).
struct Queue {
    bodies: VecDeque<Body>,
    idle: usize,
    running: usize,
}

static QUEUE: Mutex<Queue> = Mutex::new(Queue { bodies: VecDeque::new(), idle: 0, running: 0 });
/// A body was queued.
static QUEUED: Condvar = Condvar::new();
/// A Web Worker became idle.
static FREED: Condvar = Condvar::new();
/// How long [`web_spawner`] waits for a Web Worker that is on its way back from the previous pool: it only has to
/// finish the last steps of its body, so this is ample.
const RETURN_WAIT: std::time::Duration = std::time::Duration::from_millis(200);

/// The [`stello_core::search::Spawner`] of the threaded build: runs `body` on a Web Worker that waits in
/// [`pool_worker`], or fails if there is none (the pool then has fewer threads; the engine worker starts enough Web
/// Workers before a search, see `engine-worker.js`).
///
/// The engine starts its pool only after the previous one has ended, but that pool's Web Workers may still be on their
/// way back to [`pool_worker`]'s wait: while some run a body, this waits for one of them to become idle, but only for
/// [`RETURN_WAIT`]. The bodies that run may also be the new pool's own (when fewer Web Workers started than it asks
/// for), and those end only with the pool, so waiting for them without a limit would never end.
pub fn web_spawner(_index: usize, body: Body) -> std::io::Result<()> {
    let mut queue = lock();
    let mut waited = false;
    while queue.idle == 0 {
        if queue.running == 0 || waited {
            return Err(std::io::Error::other("no Web Worker is waiting for a thread"));
        }
        // The futex wait of WebAssembly has a timeout and needs no clock.
        let (next, timeout) = FREED.wait_timeout(queue, RETURN_WAIT).unwrap_or_else(|poison| poison.into_inner());
        queue = next;
        waited = timeout.timed_out();
    }

    queue.idle -= 1;
    queue.running += 1;
    queue.bodies.push_back(body);
    QUEUED.notify_one();
    Ok(())
}

fn lock() -> std::sync::MutexGuard<'static, Queue> {
    QUEUE.lock().unwrap_or_else(|poison| poison.into_inner())
}

/// How many Web Workers wait for a body: the engine worker starts more while there are fewer than a search needs.
#[wasm_bindgen(js_name = idleThreads)]
pub fn idle_threads() -> usize {
    lock().idle
}

/// The body of a pool Web Worker (`engine-pool-worker.js`), after it has instantiated the module on the shared memory:
/// runs the bodies the engine gives it, one after another, and never returns.
#[wasm_bindgen(js_name = poolWorker)]
pub fn pool_worker() {
    let mut queue = lock();
    loop {
        queue.idle += 1;
        FREED.notify_all();
        let body = loop {
            if let Some(body) = queue.bodies.pop_front() {
                break body;
            }
            queue = QUEUED.wait(queue).unwrap_or_else(|poison| poison.into_inner());
        };
        drop(queue);
        body();
        queue = lock();
        queue.running -= 1;
    }
}
