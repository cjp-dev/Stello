//! Young Brothers Wait Concept (Plans/making-stello-3-plan.md, phase 7), after Egaroucid's ybwc.hpp: once the first
//! child of a node has been searched, its younger brothers are searched in parallel, each on a worker that is idle
//! at that moment, while this thread searches the brothers that found no worker. All of them are first searched
//! with a null window at the node's alpha; a brother that fails high either cuts the node off (at null-window nodes
//! and when it reaches beta) or is searched again with the full window afterwards. A cutoff stops the brothers that
//! still run through their split point's flag.
//!
//! Split searches may split again, so the split points form a chain: a search stops when its split point or any
//! above it is stopped. The thread that splits waits for its brothers in [`ThreadPool::scope_polling`] and checks
//! its own stop signals and time while it waits, so a stop reaches the brothers even then.
//!
//! [`ThreadPool::scope_polling`]: super::pool::ThreadPool::scope_polling

use std::sync::atomic::{AtomicBool, AtomicI32, Ordering};

use super::engine::{Abort, Helpers, MAX_MOVES, Searcher, StopSignal, ThreadState};
use crate::board::{Board, Player};

/// A node whose brothers are searched in parallel: stopped after a cutoff or when the search above it stops.
pub(crate) struct SplitPoint<'a> {
    stop: AtomicBool,
    /// A brother failed high (the reason for the stop, when it is set).
    cut: AtomicBool,
    parent: Option<&'a SplitPoint<'a>>,
}

impl SplitPoint<'_> {
    /// Whether this split point or one above it is stopped.
    fn stopped(&self) -> bool {
        let mut point = Some(self);
        while let Some(current) = point {
            if current.stop.load(Ordering::Relaxed) {
                return true;
            }
            point = current.parent;
        }
        false
    }
}

/// The stop signal of a search split off at a split point: Move Now once the split point is stopped. Time limits are
/// checked by the search itself (it has the main search's clock and limit).
struct SplitStop<'a>(&'a SplitPoint<'a>);

impl StopSignal for SplitStop<'_> {
    fn is_cancelled(&self) -> bool {
        false
    }

    fn is_move_now(&self) -> bool {
        self.0.stopped()
    }
}

/// Stops a split point's brothers when dropped before [`StopUnlessFinished::finish`]: when this thread's part of the
/// split returns early (a stop) or panics, so that the scope does not wait for brothers that are no longer needed.
struct StopUnlessFinished<'a>(&'a AtomicBool, bool);

impl StopUnlessFinished<'_> {
    fn finish(mut self) {
        self.1 = true;
    }
}

impl Drop for StopUnlessFinished<'_> {
    fn drop(&mut self) {
        if !self.1 {
            self.0.store(true, Ordering::Relaxed);
        }
    }
}

/// A child of a split node: how to search it with a null window and with a full one, from the parent's side.
pub(crate) trait SplitChild: Copy + Send + Sync {
    /// The child's score from the parent's side with the null window (alpha, alpha + 1).
    fn null_window(&self, searcher: &mut Searcher<'_>, alpha: i32) -> Result<i32, Abort>;

    /// The child's score with the window (alpha, beta), after its null-window search gave `null_score` (> alpha).
    fn window(&self, searcher: &mut Searcher<'_>, alpha: i32, beta: i32, null_score: i32) -> Result<i32, Abort>;

    /// The move's square.
    fn square(&self) -> i32;
}

/// A midgame child: the position after `mv` (`player` to move), searched at `look` plies from `ply`.
#[derive(Clone, Copy)]
pub(crate) struct MidgameChild {
    pub board: Board,
    pub player: Player,
    pub look: i32,
    pub ply: i32,
    pub mv: i32,
}

impl SplitChild for MidgameChild {
    fn null_window(&self, searcher: &mut Searcher<'_>, alpha: i32) -> Result<i32, Abort> {
        let (value, reply) =
            searcher.search(&self.board, self.player, self.look, self.ply, self.mv, -alpha - 1, -alpha)?;
        searcher.ordering.record_response(self.player, self.mv, reply, -value <= alpha);
        Ok(-value)
    }

    fn window(&self, searcher: &mut Searcher<'_>, alpha: i32, beta: i32, _null_score: i32) -> Result<i32, Abort> {
        // As the single-threaded principal variation search: the window from alpha.
        let (value, reply) = searcher.search(&self.board, self.player, self.look, self.ply, self.mv, -beta, -alpha)?;
        searcher.ordering.record_response(self.player, self.mv, reply, -value <= alpha);
        Ok(-value)
    }

    fn square(&self) -> i32 {
        self.mv
    }
}

/// What a split-off search copies from the search that split: the stage of the iteration and the time limit.
#[derive(Clone, Copy)]
struct TaskSetup {
    hash_get_height: i32,
    hash_put_height: i32,
    in_probcut: bool,
    start: std::time::Duration,
    hard_limit_ms: i64,
}

/// No result yet in a brother's slot.
const NO_RESULT: i32 = i32::MIN;

impl<'a> Searcher<'a> {
    /// Whether this search may split its nodes: the main search and split-off searches of a search with more than 1
    /// thread (not the Lazy SMP helpers).
    #[inline]
    pub(crate) fn may_split(&self) -> bool {
        self.helpers.is_some() && self.ybwc
    }

    /// Searches the younger brothers `children` of a node, in parallel where workers are idle, after its first child
    /// (YBWC). `alpha` and `beta` are the node's window, `best` and `best_move` its best score and move so far (from
    /// the first child), which are updated. Returns when the node's score is known: `best` is then at least `beta`
    /// (a cutoff), or every child has been searched.
    pub(crate) fn split<C: SplitChild>(
        &mut self,
        children: &[C],
        mut alpha: i32,
        beta: i32,
        best: &mut i32,
        best_move: &mut i32,
    ) -> Result<(), Abort> {
        let helpers = self.helpers.expect("only a search with helpers splits");
        debug_assert!(children.len() <= MAX_MOVES);
        let mut pending: u64 = if children.len() == 64 { u64::MAX } else { (1u64 << children.len()) - 1 };
        let mut scores = [0i32; MAX_MOVES];

        while pending != 0 {
            alpha = alpha.max(*best);
            let window_alpha = alpha;
            let point = SplitPoint { stop: AtomicBool::new(false), cut: AtomicBool::new(false), parent: self.split };
            let results = [const { AtomicI32::new(NO_RESULT) }; MAX_MOVES];
            let setup = TaskSetup {
                hash_get_height: self.hash_get_height,
                hash_put_height: self.hash_put_height,
                in_probcut: self.in_probcut,
                start: self.start,
                hard_limit_ms: self.hard_limit_ms,
            };
            let (stop, clock) = (self.stop, self.clock);
            let mut spawned = 0u64;
            let mut fail_highs = 0u64;

            let local = helpers.pool.scope_polling(
                |scope| {
                    let guard = StopUnlessFinished(&point.stop, false);
                    let mut rest = pending;
                    while rest != 0 {
                        if point.stop.load(Ordering::Relaxed) {
                            break;
                        }

                        let i = rest.trailing_zeros() as usize;
                        let bit = 1u64 << i;
                        rest &= !bit;
                        let child = children[i];
                        // The last brother is searched here: this thread would only wait otherwise.
                        if rest != 0 && helpers.pool.idle() > 0 {
                            let (point, results) = (&point, &results);
                            let started = scope.try_spawn(move |state: &mut ThreadState| {
                                let flag = SplitStop(point);
                                let mut task = helpers.searcher(state, &flag);
                                task.split_task(helpers, point, setup);
                                if let Ok(score) = child.null_window(&mut task, window_alpha) {
                                    results[i].store(score, Ordering::Relaxed);
                                    if score > window_alpha {
                                        point.cut.store(true, Ordering::Relaxed);
                                        point.stop.store(true, Ordering::Relaxed);
                                    }
                                }
                                helpers.finish(&mut task);
                            });
                            if started {
                                spawned |= bit;
                                continue;
                            }
                        }

                        let score = child.null_window(self, window_alpha)?;
                        pending &= !bit;
                        if score > window_alpha {
                            fail_highs |= bit;
                            scores[i] = score;
                            point.cut.store(true, Ordering::Relaxed);
                            point.stop.store(true, Ordering::Relaxed);
                            break;
                        }

                        if score > *best {
                            *best = score;
                            *best_move = child.square();
                        }
                    }

                    guard.finish();
                    Ok(())
                },
                || {
                    // While this thread waits: its own stop signals and time, as at its abort checks.
                    if !point.stop.load(Ordering::Relaxed)
                        && (stop.is_cancelled()
                            || stop.is_move_now()
                            || clock().saturating_sub(setup.start).as_millis() as i64 >= setup.hard_limit_ms)
                    {
                        point.stop.store(true, Ordering::Relaxed);
                    }
                },
            );
            local?;

            let mut missing = false;
            let mut tasks = spawned;
            while tasks != 0 {
                let i = tasks.trailing_zeros() as usize;
                tasks &= tasks - 1;
                let score = results[i].load(Ordering::Relaxed);
                if score == NO_RESULT {
                    missing = true;
                    continue;
                }

                pending &= !(1u64 << i);
                if score > window_alpha {
                    fail_highs |= 1u64 << i;
                    scores[i] = score;
                } else if score > *best {
                    *best = score;
                    *best_move = children[i].square();
                }
            }

            // Brothers stopped without a cutoff here: the search above stopped (or time is up).
            if missing && !point.cut.load(Ordering::Relaxed) {
                return Err(self.abort_reason());
            }

            // A brother at beta cuts the node off (at a null-window node, every brother that fails high does).
            let mut highs = fail_highs;
            while highs != 0 {
                let i = highs.trailing_zeros() as usize;
                highs &= highs - 1;
                if scores[i] >= beta {
                    if scores[i] > *best {
                        *best = scores[i];
                        *best_move = children[i].square();
                    }
                    return Ok(());
                }
            }

            // The others are searched again with the full window, here, in their order.
            let mut highs = fail_highs;
            while highs != 0 {
                let i = highs.trailing_zeros() as usize;
                highs &= highs - 1;
                let score = children[i].window(self, alpha.max(*best), beta, scores[i])?;
                if score > *best {
                    *best = score;
                    *best_move = children[i].square();
                    if *best >= beta {
                        return Ok(());
                    }
                }
            }

            // What is left was stopped by a brother's fail high: another round with the higher alpha.
        }

        Ok(())
    }

    /// Makes this search (on a worker) one split off at `point`.
    fn split_task(&mut self, helpers: Helpers<'a>, point: &'a SplitPoint<'a>, setup: TaskSetup) {
        self.helpers = Some(helpers);
        self.split = Some(point);
        self.hash_get_height = setup.hash_get_height;
        self.hash_put_height = setup.hash_put_height;
        self.in_probcut = setup.in_probcut;
        self.start = setup.start;
        self.hard_limit_ms = setup.hard_limit_ms;
    }

    /// Why a search stops when the brothers it waited for were stopped from above.
    fn abort_reason(&self) -> Abort {
        if self.stop.is_cancelled() { Abort::Cancelled } else { Abort::Stop }
    }
}
