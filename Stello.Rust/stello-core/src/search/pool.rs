//! A fixed pool of worker threads for the parallel search (Plans/making-stello-3-plan.md, phases 5–8), shaped like
//! Egaroucid's `thread_pool.hpp`: the workers are started once and wait for tasks, and a task is only handed out
//! while a worker is idle, so whoever pushes it never waits for a busy pool (Young Brothers Wait splits only where
//! a thread is free; Lazy SMP helpers take the workers there are).
//!
//! Tasks run inside a [`ThreadPool::scope`], which returns only when every task it started has finished. So a task
//! may borrow what lives outside the scope (the position, the shared hash tables, a split node's window and stop
//! flag), as with `std::thread::scope`. Each worker has a state of its own (`S`: the search's per-thread tables),
//! which lives as long as the pool and which its tasks get as `&mut S`.
//!
//! The pool does not know about searching: stopping tasks early (a helper search when the main iteration ends,
//! siblings after a cutoff) is done by the tasks themselves, through flags they borrow.
//!
//! A pool of 0 workers starts no thread, and [`Scope::try_spawn`] always declines, so nothing extra runs: this is
//! what a search with 1 thread, and every search in WebAssembly without threads, uses.

use std::collections::VecDeque;
use std::panic::{AssertUnwindSafe, catch_unwind, resume_unwind};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::{Arc, Condvar, Mutex, MutexGuard, PoisonError};

/// Starts a thread that runs the body (`index` is the worker's number, from 0, for its name). The default is
/// [`std_spawner`]; a WebAssembly build with threads (phase 8) passes one that starts a Web Worker on the shared
/// memory, since `std::thread::spawn` is not available there.
pub type Spawner = fn(index: usize, body: Box<dyn FnOnce() + Send>) -> std::io::Result<()>;

/// Spawns a `std::thread` named `stello-search-<index>`. The thread is not joined: the pool waits until the body
/// has returned instead (see [`ThreadPool`]'s `Drop`), which also works for threads that cannot be joined.
pub fn std_spawner(index: usize, body: Box<dyn FnOnce() + Send>) -> std::io::Result<()> {
    std::thread::Builder::new().name(format!("stello-search-{index}")).spawn(body).map(drop)
}

/// A task, and the scope to tell when it has finished.
struct Job<S> {
    task: Box<dyn FnOnce(&mut S) + Send>,
    scope: Arc<ScopeTasks>,
}

/// What the workers and the pool share, behind the queue's lock unless it is atomic.
struct Shared<S> {
    queue: Mutex<Queue<S>>,
    /// Workers wait here for a task or for the end.
    work: Condvar,
    /// Signalled when a worker has started or ended: the pool's `new` waits for them to start, its `Drop` for them to
    /// end.
    changed: Condvar,
    /// Workers that wait for a task and were not yet promised one. Changed under the queue's lock; read without it
    /// to decline a task cheaply (C++: `n_idle`).
    idle: AtomicUsize,
    /// Each worker's state; worker i holds lock i while it runs a task, so other threads can reach a state (to clear
    /// it) only between tasks.
    states: Box<[Mutex<S>]>,
}

struct Queue<S> {
    jobs: VecDeque<Job<S>>,
    running: bool,
    /// Workers whose body has not returned yet.
    alive: usize,
}

/// A fixed set of worker threads with a state each. See the module documentation.
pub struct ThreadPool<S: Send + 'static> {
    shared: Arc<Shared<S>>,
}

impl<S: Send + 'static> ThreadPool<S> {
    /// Starts `workers` threads with `spawner`, each with a state from `state`. If a thread cannot be started, the
    /// pool keeps those that did (see [`ThreadPool::workers`]); a search then simply has fewer helpers.
    pub fn new(workers: usize, spawner: Spawner, state: impl Fn() -> S) -> ThreadPool<S> {
        let shared = Arc::new(Shared {
            queue: Mutex::new(Queue { jobs: VecDeque::new(), running: true, alive: 0 }),
            work: Condvar::new(),
            changed: Condvar::new(),
            idle: AtomicUsize::new(0),
            states: (0..workers).map(|_| Mutex::new(state())).collect(),
        });

        for index in 0..workers {
            let worker = Arc::clone(&shared);
            lock(&shared.queue).alive += 1;
            if spawner(index, Box::new(move || worker.run(index))).is_err() {
                lock(&shared.queue).alive -= 1;
                // The states of the workers that did not start are never used.
                break;
            }
        }

        // The first search gets all workers only if they are already waiting for tasks.
        let mut queue = lock(&shared.queue);
        while shared.idle.load(Ordering::Relaxed) < queue.alive {
            queue = shared.changed.wait(queue).unwrap_or_else(PoisonError::into_inner);
        }
        drop(queue);

        ThreadPool { shared }
    }

    /// The workers that were started (not counting the thread that calls [`ThreadPool::scope`]).
    pub fn workers(&self) -> usize {
        lock(&self.shared.queue).alive
    }

    /// The workers that wait for a task right now. Without a lock, so it may be out of date at once; for deciding
    /// cheaply whether to try [`Scope::try_spawn`].
    pub fn idle(&self) -> usize {
        self.shared.idle.load(Ordering::Relaxed)
    }

    /// Calls `f` with each worker's state, waiting for the worker's task if it runs one (e.g. to clear the solver's
    /// local tables when the engine's hash tables are cleared).
    pub fn for_each_state(&self, mut f: impl FnMut(&mut S)) {
        for state in self.shared.states.iter() {
            f(&mut lock(state));
        }
    }

    /// Runs `body`, which may start tasks on idle workers through the [`Scope`] it gets, and returns its result
    /// once every task started in it has finished. A panic in a task is raised again here.
    pub fn scope<'env, R, F>(&'env self, body: F) -> R
    where
        F: for<'scope> FnOnce(&'scope Scope<'scope, 'env, S>) -> R,
    {
        let scope =
            Scope { shared: &self.shared, tasks: Arc::new(ScopeTasks::default()), _borrows: std::marker::PhantomData };

        // The wait is in a guard, so that the tasks are finished before the scope's borrows end even when `body`
        // panics (tests unwind; release builds abort).
        struct WaitOnDrop<'a>(&'a ScopeTasks);
        impl Drop for WaitOnDrop<'_> {
            fn drop(&mut self) {
                self.0.wait();
            }
        }

        let result = {
            let _wait = WaitOnDrop(&scope.tasks);
            body(&scope)
        };

        if let Some(panic) = lock(&scope.tasks.panic).take() {
            resume_unwind(panic);
        }

        result
    }
}

impl<S: Send + 'static> Drop for ThreadPool<S> {
    /// Ends the workers and waits until they have. No task runs any more: every scope has ended.
    fn drop(&mut self) {
        let mut queue = lock(&self.shared.queue);
        queue.running = false;
        self.shared.work.notify_all();
        while queue.alive > 0 {
            queue = self.shared.changed.wait(queue).unwrap_or_else(PoisonError::into_inner);
        }
    }
}

impl<S: Send + 'static> Shared<S> {
    /// A worker's body: takes tasks until the pool ends.
    fn run(&self, index: usize) {
        let mut queue = lock(&self.queue);
        // Idle, and promised to whoever pushes the next task.
        self.idle.fetch_add(1, Ordering::Relaxed);
        self.changed.notify_all();
        loop {
            let job = loop {
                if let Some(job) = queue.jobs.pop_front() {
                    break job;
                }

                if !queue.running {
                    self.idle.fetch_sub(1, Ordering::Relaxed);
                    queue.alive -= 1;
                    self.changed.notify_all();
                    return;
                }

                queue = self.work.wait(queue).unwrap_or_else(PoisonError::into_inner);
            };

            drop(queue);
            // The task catches its own panic, so the worker lives on (and the state's lock is not poisoned).
            (job.task)(&mut lock(&self.states[index]));
            queue = lock(&self.queue);
            // Idle again before the scope learns that the task finished, so that a task the scope's thread pushes
            // next (the next search's helpers) finds this worker.
            self.idle.fetch_add(1, Ordering::Relaxed);
            job.scope.finish();
        }
    }
}

/// The tasks of one [`ThreadPool::scope`]: how many still run, and the first panic.
#[derive(Default)]
struct ScopeTasks {
    running: Mutex<usize>,
    finished: Condvar,
    panic: Mutex<Option<Box<dyn std::any::Any + Send>>>,
}

impl ScopeTasks {
    fn finish(&self) {
        let mut running = lock(&self.running);
        *running -= 1;
        if *running == 0 {
            self.finished.notify_all();
        }
    }

    fn wait(&self) {
        let mut running = lock(&self.running);
        while *running > 0 {
            running = self.finished.wait(running).unwrap_or_else(PoisonError::into_inner);
        }
    }
}

/// Starts tasks inside a [`ThreadPool::scope`]. `'env` is what tasks may borrow from outside the scope; `'scope` is
/// the scope itself.
pub struct Scope<'scope, 'env: 'scope, S: Send + 'static> {
    shared: &'env Arc<Shared<S>>,
    tasks: Arc<ScopeTasks>,
    // Invariant in both lifetimes, as std::thread::Scope, so that a task cannot be given a shorter borrow.
    _borrows: std::marker::PhantomData<(&'scope mut &'scope (), &'env mut &'env ())>,
}

impl<'scope, 'env, S: Send + 'static> Scope<'scope, 'env, S> {
    /// Runs `task` on a worker that is idle right now and returns true, or returns false at once without running it
    /// when every worker is busy (or there are none). The task gets the worker's state.
    pub fn try_spawn<F>(&self, task: F) -> bool
    where
        F: FnOnce(&mut S) + Send + 'scope,
    {
        // Cheap refusal first: YBWC asks at every split, mostly while all threads are busy (C++: the same check
        // before the task object is made).
        if self.shared.idle.load(Ordering::Relaxed) == 0 {
            return false;
        }

        let mut queue = lock(&self.shared.queue);
        if self.shared.idle.load(Ordering::Relaxed) == 0 || !queue.running {
            return false;
        }

        *lock(&self.tasks.running) += 1;
        let tasks = Arc::clone(&self.tasks);
        let task: Box<dyn FnOnce(&mut S) + Send + 'scope> = Box::new(move |state: &mut S| {
            if let Err(panic) = catch_unwind(AssertUnwindSafe(|| task(state))) {
                lock(&tasks.panic).get_or_insert(panic);
            }
        });
        // SAFETY: only the lifetime changes. The worker calls the task, which consumes it and so drops everything it
        // borrows, before it calls `ScopeTasks::finish`; `ThreadPool::scope` does not return (not even when its body
        // panics) until every task of the scope has finished. So no borrow is used after 'scope.
        let task = unsafe {
            std::mem::transmute::<Box<dyn FnOnce(&mut S) + Send + 'scope>, Box<dyn FnOnce(&mut S) + Send>>(task)
        };
        self.shared.idle.fetch_sub(1, Ordering::Relaxed);
        queue.jobs.push_back(Job { task, scope: Arc::clone(&self.tasks) });
        self.shared.work.notify_one();
        true
    }
}

/// Locks a mutex, ignoring poison: a task's panic is caught and raised again by its scope, and the pool's own
/// critical sections do not panic.
fn lock<T>(mutex: &Mutex<T>) -> MutexGuard<'_, T> {
    mutex.lock().unwrap_or_else(PoisonError::into_inner)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicBool, AtomicU64};

    #[test]
    fn a_pool_without_workers_runs_nothing_else() {
        let pool = ThreadPool::new(0, std_spawner, || 0u32);
        assert_eq!(pool.workers(), 0);
        let ran = pool.scope(|scope| scope.try_spawn(|_| unreachable!("no worker")));
        assert!(!ran);
    }

    #[test]
    fn tasks_borrow_from_outside_and_finish_before_the_scope_returns() {
        let pool = ThreadPool::new(3, std_spawner, || 0u64);
        // Workers become idle shortly after they start.
        while pool.idle() < 3 {
            std::thread::yield_now();
        }

        let total = AtomicU64::new(0);
        let release = AtomicBool::new(false);
        let started = pool.scope(|scope| {
            let mut started = 0;
            for i in 1..=3u64 {
                let (total, release) = (&total, &release);
                if scope.try_spawn(move |count: &mut u64| {
                    while !release.load(Ordering::Acquire) {
                        std::hint::spin_loop();
                    }
                    *count += 1;
                    total.fetch_add(i, Ordering::Relaxed);
                }) {
                    started += 1;
                }
            }
            // All three workers are busy now.
            assert!(!scope.try_spawn(|_| unreachable!("no idle worker")));
            release.store(true, Ordering::Release);
            started
        });
        assert_eq!(started, 3);
        assert_eq!(total.load(Ordering::Relaxed), 6);
        // A worker is idle again once its task has finished.
        assert_eq!(pool.idle(), 3);

        let mut tasks = 0;
        pool.for_each_state(|count| tasks += *count);
        assert_eq!(tasks, 3);
    }

    #[test]
    fn long_tasks_stop_through_a_flag_they_borrow() {
        let pool = ThreadPool::new(2, std_spawner, || ());
        while pool.idle() < 2 {
            std::thread::yield_now();
        }

        let stop = AtomicBool::new(false);
        let spins = AtomicU64::new(0);
        pool.scope(|scope| {
            for _ in 0..2 {
                assert!(scope.try_spawn(|_| {
                    while !stop.load(Ordering::Relaxed) {
                        spins.fetch_add(1, Ordering::Relaxed);
                        std::hint::spin_loop();
                    }
                }));
            }
            while spins.load(Ordering::Relaxed) < 1000 {
                std::hint::spin_loop();
            }
            stop.store(true, Ordering::Relaxed);
        });
        assert!(spins.load(Ordering::Relaxed) >= 1000);
    }

    #[test]
    fn a_panic_in_a_task_reaches_the_scope_and_the_pool_lives_on() {
        let pool = ThreadPool::new(1, std_spawner, || ());
        while pool.idle() < 1 {
            std::thread::yield_now();
        }

        let caught = catch_unwind(AssertUnwindSafe(|| {
            pool.scope(|scope| assert!(scope.try_spawn(|_| panic!("task failed"))));
        }));
        assert!(caught.is_err());

        while pool.idle() < 1 {
            std::thread::yield_now();
        }
        let ran = AtomicBool::new(false);
        pool.scope(|scope| assert!(scope.try_spawn(|_| ran.store(true, Ordering::Relaxed))));
        assert!(ran.load(Ordering::Relaxed));
    }

    #[test]
    fn a_spawner_that_fails_leaves_the_workers_that_started() {
        fn fails_after_one(index: usize, body: Box<dyn FnOnce() + Send>) -> std::io::Result<()> {
            if index == 0 { std_spawner(index, body) } else { Err(std::io::Error::other("no more threads")) }
        }

        let pool = ThreadPool::new(4, fails_after_one, || ());
        assert_eq!(pool.workers(), 1);
    }
}
