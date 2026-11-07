//! Stello 2's exact endgame solver. C#: `Stello.Engine.V2.SearchEngine`'s partial class
//! Endgame/SearchEngine.Endgame.cs, and the shallow solver at the end of SearchEngine.cs (`SolveShallow`,
//! `SolveLast2`, `SolveLast`). The last four empty squares are in [`super::last`], the local table in
//! [`super::local`] and the stable discs in [`super::stable`].
//!
//! The search of the last 13 empty squares is adapted from Egaroucid src/engine/endsearch_nws.hpp,
//! endsearch_nws_last_generic.hpp and stability_cutoff.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana,
//! GPL-3.0-or-later, through the C# port in Stello.Net/Egaroucid.Engine/EndSearch.cs. Above 13 empty squares the
//! null-window and principal-variation search follows the shape of Egaroucid's midsearch_nws.hpp and midsearch.hpp
//! (transposition cutoffs, enhanced transposition cutoff, move ordering by mobility and a shallow search), written on
//! Stello's transposition table and ordered by Stello's pattern evaluation.
//!
//! The solver is an `impl Searcher` block, as the C# partial class: it shares the node and evaluation counters, the
//! abort checks, the reports and both hash tables with the midgame. The midgame calls it through
//! [`Searcher::solve_child`] (from `solve_root`). State that lives from search to search (C#: `_localTable`) is in
//! [`EndgameState`], which `SearchEngine::clear_hash` clears.
//!
//! With more than 1 thread, `end_window` and `end_deep` split their younger brothers over idle threads (YBWC,
//! [`crate::search::ybwc`]) when the children have [`YBWC_END_MIN_EMPTIES`] empty squares or more.
//!
//! Scores are final disc differences for the side to move, the empty squares going to the winner. Null-window
//! searches return fail-soft bounds: a score above alpha is a lower bound, at or below alpha an upper bound.

use super::bits::{
    LEGAL_UNDEFINED, QUADRANT_BIT, STABILITY_THRESHOLD, STABILITY_THRESHOLD_NWS, flips, legal_moves, lone_empties,
    moves_corners_twice, parity, potential_mobility, weighted_moves,
};
use super::local::LocalTable;
use super::stable;
use crate::bits;
use crate::eval;
use crate::search::engine::{Abort, INFINITY, Searcher, YBWC_END_MIN_EMPTIES, final_score};
use crate::search::table::Bound;
use crate::search::ybwc::SplitChild;

pub(crate) const END_INFINITY: i32 = 127;
pub(crate) const END_FAST_EMPTIES: i32 = 6;
pub(crate) const END_SIMPLE_EMPTIES: i32 = 10;
pub(crate) const END_LOCAL_EMPTIES: i32 = 13;
pub(crate) const END_ETC_MIN_EMPTIES: i32 = 12;

/// In solve mode, the midgame search to this depth first orders the moves at the root.
pub(crate) const SOLVE_ORDERING_LOOKS: i32 = 4;

// Egaroucid's move-ordering weights (move_ordering.hpp); values are compared against an offset of 38. The first four
// are also the midgame's evaluation ordering (SearchEngine.OrderByEvaluation).
pub(crate) const ORDER_OFFSET: i32 = 38;
pub(crate) const ORDER_MOBILITY: i32 = 35;
pub(crate) const ORDER_POTENTIAL_MOBILITY: i32 = 17;
pub(crate) const ORDER_VALUE: i32 = 269;
pub(crate) const ORDER_VALUE_DEEP: i32 = 94;
pub(crate) const ORDER_NULL_MOBILITY: i32 = 17;
pub(crate) const ORDER_NULL_VALUE: i32 = 7;
pub(crate) const ORDER_NULL_VALUE_DEEP: i32 = 25;
pub(crate) const ORDER_LOCAL_MOBILITY: i32 = 40;
pub(crate) const ORDER_SIMPLE_MOBILITY: i32 = 18;
pub(crate) const ORDER_SIMPLE_PARITY: i32 = 17;
pub(crate) const ORDER_HASH_MOVE: i32 = 10_000_000;

// Quadrants a1-d4, e1-h4, a5-d8, e5-h8; squares in regions with an odd number of empties are tried first.
// C#: SearchEngine.Quadrants (SearchEngine.cs).
const QUADRANTS: [u64; 4] =
    [0x0000_0000_0F0F_0F0F, 0x0000_0000_F0F0_F0F0, 0x0F0F_0F0F_0000_0000, 0xF0F0_F0F0_0000_0000];

/// The solver's state that lives from search to search: C#'s `_localTable` (Endgame/LocalTable.cs).
#[derive(Default)]
pub struct EndgameState {
    pub(crate) local: LocalTable,
}

impl EndgameState {
    /// Part of `SearchEngine::clear_hash` (C#: `_localTable.Clear()`).
    pub fn clear(&mut self) {
        self.local.clear();
    }
}

/// A move in the solver's move lists. C#: `EndMove`.
#[derive(Clone, Copy, Default)]
pub(crate) struct EndMove {
    pub square: u32,
    pub flips: u64,
    pub value: i32,
    /// The opponent's moves after this move, or [`LEGAL_UNDEFINED`].
    pub replies: u64,
}

// The longest move lists: a position has at most as many moves as empty squares (C#: stackalloc of the count).
const DEEP_MOVES: usize = crate::search::engine::MAX_MOVES;
const LOCAL_MOVES: usize = END_LOCAL_EMPTIES as usize;

impl Searcher<'_> {
    /// The score of a position reached by a root move, within (alpha, beta). C#: `SolveChild`.
    ///
    /// Called by `solve_root` (C#: `SolveRoot`, in SearchEngine.cs) for the win/loss/draw search with the window
    /// (-1, 1) and then the exact one.
    pub(crate) fn solve_child(
        &mut self,
        own: u64,
        opponent: u64,
        alpha: i32,
        beta: i32,
        empties: i32,
    ) -> Result<i32, Abort> {
        if beta - alpha == 1 {
            self.end_null(own, opponent, alpha, empties, LEGAL_UNDEFINED)
        } else {
            self.end_window(own, opponent, alpha, beta, empties, false)
        }
    }

    /// A null-window search of a position reached by a move. C#: `EndNull`.
    fn end_null(&mut self, own: u64, opponent: u64, alpha: i32, empties: i32, legal: u64) -> Result<i32, Abort> {
        if empties <= 4 {
            return self.solve_shallow(own, opponent, alpha, alpha + 1, empties, false);
        }

        if empties <= END_LOCAL_EMPTIES {
            self.end_local(own, opponent, alpha, false, legal, empties)
        } else {
            self.end_deep(own, opponent, alpha, empties, false, legal)
        }
    }

    // ---- With a window (principal variation nodes) ----

    /// C#: `EndWindow`.
    fn end_window(
        &mut self,
        own: u64,
        opponent: u64,
        mut alpha: i32,
        mut beta: i32,
        empties: i32,
        passed: bool,
    ) -> Result<i32, Abort> {
        if empties <= END_FAST_EMPTIES {
            return self.solve_shallow(own, opponent, alpha, beta, empties, passed);
        }

        self.count_node()?;
        let legal = legal_moves(own, opponent);
        if legal == 0 {
            if passed {
                self.evaluations += 1;
                return Ok(final_score(own, opponent));
            }

            return Ok(-self.end_window(opponent, own, -beta, -alpha, empties, true)?);
        }

        if !passed && beta >= STABILITY_THRESHOLD[(64 - empties) as usize] {
            // The opponent's stable discs cap the score.
            let limit = 64 - 2 * stable::of(opponent, own).count_ones() as i32;
            if limit <= alpha {
                return Ok(limit);
            }

            beta = beta.min(limit);
        }

        let mut hash_move = -1;
        if let Some(entry) = self.endgame_table.get_at_root(own, opponent, 0, self.root_empties) {
            let value = entry.value as i32;
            match entry.bound {
                Bound::Exact => return Ok(value),
                Bound::Lower => alpha = alpha.max(value),
                _ => beta = beta.min(value),
            }

            if alpha >= beta {
                return Ok(value);
            }

            hash_move = entry.mv as i32;
        }

        let count = legal.count_ones() as usize;
        let mut list = [EndMove::default(); DEEP_MOVES];
        let moves = &mut list[..count];
        if fill_end_moves(own, opponent, legal, moves) {
            return Ok(64);
        }

        if empties >= END_ETC_MIN_EMPTIES {
            // A child whose stored upper bound already gives a cutoff makes the search unnecessary.
            for mv in moves.iter() {
                let (child_own, child_opponent) = child(own, opponent, mv);
                if let Some(entry) = self.endgame_table.get_at_root(child_own, child_opponent, 0, self.root_empties)
                    && entry.bound != Bound::Lower
                    && -(entry.value as i32) >= beta
                {
                    return Ok(-(entry.value as i32));
                }
            }
        }

        if count > 1 {
            let depth = (empties >> 3).min(2);
            let order_alpha = -(beta + 8).min(64);
            let order_beta = -(alpha - 12).max(-64);
            let empty = !(own | opponent);
            for mv in moves.iter_mut() {
                if mv.square as i32 == hash_move {
                    mv.value = ORDER_HASH_MOVE;
                    continue;
                }

                let (child_own, child_opponent) = child(own, opponent, mv);
                mv.replies = legal_moves(child_own, child_opponent);
                let child_empty = empty & !(1u64 << mv.square);
                mv.value = (ORDER_OFFSET - weighted_moves(mv.replies)) * ORDER_MOBILITY
                    + (ORDER_OFFSET - potential_mobility(child_opponent, child_empty)) * ORDER_POTENTIAL_MOBILITY
                    + (64 - self.ordering_value(child_own, child_opponent, depth, order_alpha, order_beta)?)
                        * (ORDER_VALUE + depth * ORDER_VALUE_DEEP);
            }
        }

        let alpha_start = alpha;
        let mut best = -END_INFINITY;
        let mut best_move = -1;
        for i in 0..count {
            if i == 1 && empties > YBWC_END_MIN_EMPTIES && self.may_split() {
                // YBWC: the younger brothers in parallel, in their order.
                let children = end_children(own, opponent, empties, moves, i, 0);
                self.split(children.as_slice(), alpha, beta, &mut best, &mut best_move)?;
                break;
            }

            swap_next_best(moves, i);
            let mv = moves[i];
            let (child_own, child_opponent) = child(own, opponent, &mv);
            let score = if i == 0 {
                -self.end_window(child_own, child_opponent, -beta, -alpha, empties - 1, false)?
            } else {
                let score = -self.end_null(child_own, child_opponent, -alpha - 1, empties - 1, mv.replies)?;
                if score > alpha && score < beta {
                    -self.end_window(child_own, child_opponent, -beta, -score, empties - 1, false)?
                } else {
                    score
                }
            };

            if score > best {
                best = score;
                best_move = mv.square as i32;
                if best > alpha {
                    alpha = best;
                    if alpha >= beta {
                        break;
                    }
                }
            }
        }

        let bound = if best >= beta {
            Bound::Lower
        } else if best > alpha_start {
            Bound::Exact
        } else {
            Bound::Upper
        };
        self.endgame_table.store_at_root(own, opponent, 0, empties, bound, best, best_move, self.root_empties);
        Ok(best)
    }

    // ---- Null window above 13 empty squares ----

    /// C#: `EndDeep`.
    fn end_deep(
        &mut self,
        own: u64,
        opponent: u64,
        alpha: i32,
        empties: i32,
        passed: bool,
        mut legal: u64,
    ) -> Result<i32, Abort> {
        if empties <= END_LOCAL_EMPTIES {
            return self.end_local(own, opponent, alpha, passed, legal, empties);
        }

        self.count_node()?;
        if legal == LEGAL_UNDEFINED {
            legal = legal_moves(own, opponent);
        }

        if legal == 0 {
            if passed {
                self.evaluations += 1;
                return Ok(final_score(own, opponent));
            }

            return Ok(-self.end_deep(opponent, own, -alpha - 1, empties, true, LEGAL_UNDEFINED)?);
        }

        if !passed && let Some(cut) = stability_cut(own, opponent, alpha, empties) {
            return Ok(cut);
        }

        let mut hash_move = -1;
        if let Some(entry) = self.endgame_table.get_at_root(own, opponent, 0, self.root_empties) {
            let value = entry.value as i32;
            if entry.bound != Bound::Upper && value > alpha {
                return Ok(value);
            }

            if entry.bound != Bound::Lower && value <= alpha {
                return Ok(value);
            }

            hash_move = entry.mv as i32;
        }

        let count = legal.count_ones() as usize;
        let mut list = [EndMove::default(); DEEP_MOVES];
        let moves = &mut list[..count];
        if fill_end_moves(own, opponent, legal, moves) {
            return Ok(64);
        }

        let mut best = -END_INFINITY;
        let mut best_move = -1;
        let mut done = 0u64;
        let mut hash_move_searched = false;
        if empties >= END_ETC_MIN_EMPTIES {
            // Enhanced transposition cutoff: the children's stored bounds may already decide this node.
            for mv in moves.iter() {
                let (child_own, child_opponent) = child(own, opponent, mv);
                let Some(entry) = self.endgame_table.get_at_root(child_own, child_opponent, 0, self.root_empties)
                else {
                    continue;
                };
                let value = -(entry.value as i32);
                if entry.bound != Bound::Lower && value > alpha {
                    return Ok(value);
                }

                if entry.bound != Bound::Upper && value <= alpha {
                    if value > best {
                        best = value;
                        best_move = mv.square as i32;
                    }
                    done |= 1u64 << mv.square;
                }
            }
        }

        if hash_move >= 0 && (legal & !done) & (1u64 << hash_move) != 0 {
            // The stored best move first, before spending time on ordering the others.
            if let Some(mv) = moves.iter().find(|m| m.square as i32 == hash_move) {
                let (child_own, child_opponent) = child(own, opponent, mv);
                let score =
                    -self.end_deep(child_own, child_opponent, -alpha - 1, empties - 1, false, LEGAL_UNDEFINED)?;
                hash_move_searched = true;
                if score > best {
                    best = score;
                    best_move = hash_move;
                }

                done |= 1u64 << hash_move;
            }
        }

        if best <= alpha {
            if count > 1 {
                let depth = empties >> 4;
                let order_alpha = -(alpha + 6).min(64);
                let order_beta = -(alpha - 16).max(-64);
                for mv in moves.iter_mut() {
                    if done & (1u64 << mv.square) != 0 {
                        mv.value = i32::MIN;
                        continue;
                    }

                    let (child_own, child_opponent) = child(own, opponent, mv);
                    mv.replies = legal_moves(child_own, child_opponent);
                    mv.value = (ORDER_OFFSET - weighted_moves(mv.replies)) * ORDER_NULL_MOBILITY
                        + (64 - self.ordering_value(child_own, child_opponent, depth, order_alpha, order_beta)?)
                            * (ORDER_NULL_VALUE + depth * ORDER_NULL_VALUE_DEEP);
                }
            }

            // Whether a child was searched here (the stored best move), so that the others may be split off. Children
            // decided by the ETC were not searched, so `best_move` cannot tell.
            let mut searched = hash_move_searched;
            for i in 0..count {
                if searched && empties > YBWC_END_MIN_EMPTIES && self.may_split() {
                    // YBWC: the younger brothers in parallel, in their order.
                    let children = end_children(own, opponent, empties, moves, i, done);
                    if !children.is_empty() {
                        self.split(children.as_slice(), alpha, alpha + 1, &mut best, &mut best_move)?;
                    }
                    break;
                }

                swap_next_best(moves, i);
                let mv = moves[i];
                if done & (1u64 << mv.square) != 0 {
                    continue;
                }

                searched = true;
                let (child_own, child_opponent) = child(own, opponent, &mv);
                let score = -self.end_deep(child_own, child_opponent, -alpha - 1, empties - 1, false, mv.replies)?;
                if score > best {
                    best = score;
                    best_move = mv.square as i32;
                    if best > alpha {
                        break;
                    }
                }
            }
        }

        let bound = if best > alpha { Bound::Lower } else { Bound::Upper };
        self.endgame_table.store_at_root(own, opponent, 0, empties, bound, best, best_move, self.root_empties);
        Ok(best)
    }

    /// The value of a position for ordering, in discs: the pattern evaluation after a small alpha-beta search.
    /// C#: `OrderingValue`.
    fn ordering_value(&mut self, own: u64, opponent: u64, depth: i32, mut alpha: i32, beta: i32) -> Result<i32, Abort> {
        self.count_node()?;
        if depth == 0 {
            self.evaluations += 1;
            return Ok(eval::disc_score(self.pattern, own, opponent));
        }

        let mut legal = legal_moves(own, opponent);
        if legal == 0 {
            return if legal_moves(opponent, own) == 0 {
                self.evaluations += 1;
                Ok(final_score(own, opponent))
            } else {
                Ok(-self.ordering_value(opponent, own, depth, -beta, -alpha)?)
            };
        }

        let mut best = -END_INFINITY;
        while legal != 0 {
            let square = legal.trailing_zeros();
            legal &= legal - 1;
            let flipped = flips(own, opponent, square);
            let score =
                -self.ordering_value(opponent ^ flipped, own ^ flipped ^ (1u64 << square), depth - 1, -beta, -alpha)?;
            if score > best {
                best = score;
                if best > alpha {
                    alpha = best;
                    if alpha >= beta {
                        break;
                    }
                }
            }
        }

        Ok(best)
    }

    // ---- Null window at 11 to 13 empty squares, with the local table ----

    /// C#: `EndLocal`.
    fn end_local(
        &mut self,
        own: u64,
        opponent: u64,
        alpha: i32,
        passed: bool,
        mut legal: u64,
        empties: i32,
    ) -> Result<i32, Abort> {
        if empties <= END_SIMPLE_EMPTIES {
            return self.end_simple(own, opponent, alpha, passed, legal, empties);
        }

        self.count_node()?;
        if !passed && let Some(cut) = stability_cut(own, opponent, alpha, empties) {
            return Ok(cut);
        }

        if legal == LEGAL_UNDEFINED {
            legal = legal_moves(own, opponent);
        }

        if legal == 0 {
            return if passed {
                self.evaluations += 1;
                Ok(final_score(own, opponent))
            } else {
                Ok(-self.end_local(opponent, own, -alpha - 1, true, LEGAL_UNDEFINED, empties)?)
            };
        }

        let count = legal.count_ones() as usize;
        let mut list = [EndMove::default(); LOCAL_MOVES];
        let moves = &mut list[..count];
        if fill_end_moves(own, opponent, legal, moves) {
            return Ok(64);
        }

        let mut best = -END_INFINITY;
        let mut done = 0u64;
        if count > 1 {
            for mv in moves.iter_mut() {
                let (child_own, child_opponent) = child(own, opponent, mv);
                let slot = self.endgame.local.slot(child_own, child_opponent, empties - 1);
                let stored = self.endgame.local.get(slot);
                if stored.matches(child_own, child_opponent) {
                    if alpha < stored.lower {
                        return Ok(stored.lower);
                    }

                    if stored.upper <= alpha {
                        best = best.max(stored.upper);
                        done |= 1u64 << mv.square;
                        continue;
                    }
                }

                mv.replies = legal_moves(child_own, child_opponent);
                let replies = moves_corners_twice(mv.replies);
                if replies <= 1 {
                    // At most one reply: search it now, it is cheap and often decides the node.
                    let score =
                        -self.end_local(child_own, child_opponent, -alpha - 1, false, mv.replies, empties - 1)?;
                    if score > best {
                        best = score;
                        if alpha < best {
                            self.endgame.local.set(slot, child_own, child_opponent, best, 64);
                            return Ok(best);
                        }
                    }

                    self.endgame.local.set(slot, child_own, child_opponent, -64, score);
                    done |= 1u64 << mv.square;
                    continue;
                }

                // Egaroucid adds a small move-ordering evaluation here. The pattern evaluation saves about 20 % of
                // the nodes but costs more time than that, so the opponent's mobility orders alone.
                mv.value = (ORDER_OFFSET - replies) * ORDER_LOCAL_MOBILITY;
            }
        }

        let undone = undone_first(moves, done);
        let moves = &mut moves[..undone];
        for i in 0..moves.len() {
            swap_next_best(moves, i);
            let mv = moves[i];
            let (child_own, child_opponent) = child(own, opponent, &mv);
            let slot = self.endgame.local.slot(child_own, child_opponent, empties - 1);
            let score = -self.end_local(child_own, child_opponent, -alpha - 1, false, mv.replies, empties - 1)?;
            if score > best {
                best = score;
                if alpha < best {
                    self.endgame.local.set(slot, child_own, child_opponent, best, 64);
                    break;
                }
            }

            self.endgame.local.set(slot, child_own, child_opponent, -64, score);
        }

        Ok(best)
    }

    // ---- Null window at 7 to 10 empty squares: parity and mobility ordering ----

    /// C#: `EndSimple`.
    fn end_simple(
        &mut self,
        own: u64,
        opponent: u64,
        alpha: i32,
        passed: bool,
        mut legal: u64,
        empties: i32,
    ) -> Result<i32, Abort> {
        if empties <= END_FAST_EMPTIES {
            return self.end_fast(own, opponent, alpha, passed, empties);
        }

        self.count_node()?;
        if !passed && let Some(cut) = stability_cut(own, opponent, alpha, empties) {
            return Ok(cut);
        }

        if legal == LEGAL_UNDEFINED {
            legal = legal_moves(own, opponent);
        }

        if legal == 0 {
            return if passed {
                self.evaluations += 1;
                Ok(final_score(own, opponent))
            } else {
                Ok(-self.end_simple(opponent, own, -alpha - 1, true, LEGAL_UNDEFINED, empties)?)
            };
        }

        let count = legal.count_ones() as usize;
        let mut list = [EndMove::default(); LOCAL_MOVES];
        let moves = &mut list[..count];
        if fill_end_moves(own, opponent, legal, moves) {
            return Ok(64);
        }

        let mut best = -END_INFINITY;
        let mut done = 0u64;
        if count > 1 {
            let parity = parity(!(own | opponent));
            for mv in moves.iter_mut() {
                mv.value = if parity & QUADRANT_BIT[mv.square as usize] != 0 { ORDER_SIMPLE_PARITY } else { 0 };
                let (child_own, child_opponent) = child(own, opponent, mv);
                let slot = self.endgame.local.slot(child_own, child_opponent, empties - 1);
                let stored = self.endgame.local.get(slot);
                if stored.matches(child_own, child_opponent) {
                    if alpha < stored.lower {
                        return Ok(stored.lower);
                    }

                    if stored.upper <= alpha {
                        best = best.max(stored.upper);
                        done |= 1u64 << mv.square;
                        continue;
                    }
                }

                mv.replies = legal_moves(child_own, child_opponent);
                let replies = moves_corners_twice(mv.replies);
                if replies <= 1 {
                    let score =
                        -self.end_simple(child_own, child_opponent, -alpha - 1, false, mv.replies, empties - 1)?;
                    if score > best {
                        best = score;
                        if alpha < best {
                            self.endgame.local.set(slot, child_own, child_opponent, best, 64);
                            return Ok(best);
                        }
                    }

                    self.endgame.local.set(slot, child_own, child_opponent, -64, score);
                    done |= 1u64 << mv.square;
                    continue;
                }

                mv.value += (ORDER_OFFSET - replies) * ORDER_SIMPLE_MOBILITY;
            }
        }

        let undone = undone_first(moves, done);
        let moves = &mut moves[..undone];
        for i in 0..moves.len() {
            swap_next_best(moves, i);
            let mv = moves[i];
            let (child_own, child_opponent) = child(own, opponent, &mv);
            let slot = self.endgame.local.slot(child_own, child_opponent, empties - 1);
            let score = -self.end_simple(child_own, child_opponent, -alpha - 1, false, mv.replies, empties - 1)?;
            if score > best {
                best = score;
                if alpha < best {
                    self.endgame.local.set(slot, child_own, child_opponent, best, 64);
                    break;
                }
            }

            self.endgame.local.set(slot, child_own, child_opponent, -64, score);
        }

        Ok(best)
    }

    // ---- Null window at 5 and 6 empty squares: lone empty squares first ----

    /// C#: `EndFast`.
    fn end_fast(&mut self, own: u64, opponent: u64, alpha: i32, passed: bool, empties: i32) -> Result<i32, Abort> {
        self.count_node()?;
        if !passed && let Some(cut) = stability_cut(own, opponent, alpha, empties) {
            return Ok(cut);
        }

        let mut legal = legal_moves(own, opponent);
        if legal == 0 {
            return if passed {
                self.evaluations += 1;
                Ok(final_score(own, opponent))
            } else {
                Ok(-self.end_fast(opponent, own, -alpha - 1, true, empties)?)
            };
        }

        let mut best = -END_INFINITY;
        let mut first = legal & lone_empties(own, opponent);
        if first == 0 {
            first = legal;
        }

        let mut batch = first;
        while batch != 0 {
            legal ^= batch;
            while batch != 0 {
                let square = batch.trailing_zeros();
                batch &= batch - 1;
                let flipped = flips(own, opponent, square);
                let child_own = opponent ^ flipped;
                let child_opponent = own ^ flipped ^ (1u64 << square);
                let score = if empties == 5 {
                    self.last4(child_own, child_opponent, alpha)?
                } else {
                    -self.end_fast(child_own, child_opponent, -alpha - 1, false, empties - 1)?
                };
                if score > alpha {
                    return Ok(score);
                }

                best = best.max(score);
            }

            batch = legal;
        }

        Ok(best)
    }

    /// Near the end: no move list, no hash table; empty squares in odd regions first (parity). C#: `SolveShallow`
    /// (SearchEngine.cs), which `EndNull` calls at 4 empty squares or fewer and `EndWindow` at 6 or fewer.
    pub(crate) fn solve_shallow(
        &mut self,
        own: u64,
        opponent: u64,
        mut alpha: i32,
        beta: i32,
        empties: i32,
        passed: bool,
    ) -> Result<i32, Abort> {
        self.count_node()?;

        let empty = !(own | opponent);
        if empties == 1 {
            return Ok(self.solve_last(own, opponent, empty));
        }

        if empties == 2 {
            return self.solve_last2(own, opponent, alpha, beta, empty, passed);
        }

        let mut odd = 0;
        for quadrant in QUADRANTS {
            if (empty & quadrant).count_ones() & 1 != 0 {
                odd |= quadrant;
            }
        }

        let mut best = -INFINITY;
        for pass in 0..2 {
            let mut squares = empty & if pass == 0 { odd } else { !odd };
            while squares != 0 {
                let square = squares.trailing_zeros();
                squares &= squares - 1;
                let flips = bits::flips(own, opponent, square);
                if flips == 0 {
                    continue;
                }

                let score = -self.solve_shallow(
                    opponent & !flips,
                    own | flips | (1u64 << square),
                    -beta,
                    -alpha,
                    empties - 1,
                    false,
                )?;
                if score > best {
                    best = score;
                    if best > alpha {
                        alpha = best;
                        if alpha >= beta {
                            return Ok(best);
                        }
                    }
                }
            }
        }

        if best > -INFINITY {
            return Ok(best);
        }

        if passed {
            self.evaluations += 1;
            return Ok(final_score(own, opponent));
        }

        Ok(-self.solve_shallow(opponent, own, -beta, -alpha, empties, true)?)
    }

    // Two empty squares, without the parity loop; the node itself is counted by the caller. C#: `SolveLast2`.
    fn solve_last2(
        &mut self,
        own: u64,
        opponent: u64,
        alpha: i32,
        beta: i32,
        empty: u64,
        passed: bool,
    ) -> Result<i32, Abort> {
        let first = empty.trailing_zeros();
        let second = (empty & (empty - 1)).trailing_zeros();
        let mut best = -INFINITY;

        let flips = bits::flips(own, opponent, first);
        if flips != 0 {
            self.count_node()?;
            best = -self.solve_last(opponent & !flips, own | flips | (1u64 << first), 1u64 << second);
            if best >= beta {
                return Ok(best);
            }
        }

        let flips = bits::flips(own, opponent, second);
        if flips != 0 {
            self.count_node()?;
            best = best.max(-self.solve_last(opponent & !flips, own | flips | (1u64 << second), 1u64 << first));
        }

        if best > -INFINITY {
            return Ok(best);
        }

        if passed {
            self.evaluations += 1;
            return Ok(final_score(own, opponent));
        }

        self.count_node()?;

        Ok(-self.solve_last2(opponent, own, -beta, -alpha, empty, true)?)
    }

    // C#: `SolveLast`.
    fn solve_last(&mut self, own: u64, opponent: u64, empty: u64) -> i32 {
        self.evaluations += 1;
        let square = empty.trailing_zeros();
        let bit = 1u64 << square;

        let flips = bits::flips(own, opponent, square);
        if flips != 0 {
            return final_score(own | flips | bit, opponent & !flips);
        }

        let flips = bits::flips(opponent, own, square);
        if flips != 0 { final_score(own & !flips, opponent | flips | bit) } else { final_score(own, opponent) }
    }
}

// ---- Helpers ----

/// The stable-disc cutoff of a null-window search, if it applies. C#: `StabilityCut` (`int.MinValue` for none).
#[inline]
fn stability_cut(own: u64, opponent: u64, alpha: i32, empties: i32) -> Option<i32> {
    if alpha >= STABILITY_THRESHOLD_NWS[(64 - empties) as usize] {
        let limit = 64 - 2 * stable::of(opponent, own).count_ones() as i32;
        if limit <= alpha {
            return Some(limit);
        }
    }

    None
}

/// Fills the moves with their flips; true if one of them takes every opponent disc (the game is won 64-0).
/// C#: `FillEndMoves`.
#[inline]
fn fill_end_moves(own: u64, opponent: u64, mut legal: u64, moves: &mut [EndMove]) -> bool {
    for mv in moves.iter_mut() {
        let square = legal.trailing_zeros();
        legal &= legal - 1;
        let flipped = flips(own, opponent, square);
        if flipped == opponent {
            return true;
        }

        *mv = EndMove { square, flips: flipped, value: 0, replies: LEGAL_UNDEFINED };
    }

    false
}

/// A child of a split endgame node (YBWC): the position after `square`, with `empties` empty squares.
#[derive(Clone, Copy, Default)]
pub(crate) struct EndgameChild {
    own: u64,
    opponent: u64,
    empties: i32,
    replies: u64,
    square: i32,
}

impl SplitChild for EndgameChild {
    fn null_window(&self, searcher: &mut Searcher<'_>, alpha: i32) -> Result<i32, Abort> {
        Ok(-searcher.end_null(self.own, self.opponent, -alpha - 1, self.empties, self.replies)?)
    }

    fn window(&self, searcher: &mut Searcher<'_>, alpha: i32, beta: i32, null_score: i32) -> Result<i32, Abort> {
        // As the single-threaded search: from the null window's lower bound.
        Ok(-searcher.end_window(self.own, self.opponent, -beta, -alpha.max(null_score), self.empties, false)?)
    }

    fn square(&self) -> i32 {
        self.square
    }
}

/// The children of `moves[from..]` that are not in `done`, best first (the order of [`swap_next_best`]).
fn end_children(
    own: u64,
    opponent: u64,
    empties: i32,
    moves: &mut [EndMove],
    from: usize,
    done: u64,
) -> ArrayVec<EndgameChild> {
    let mut children = ArrayVec::default();
    for i in from..moves.len() {
        swap_next_best(moves, i);
        let mv = moves[i];
        if done & (1u64 << mv.square) == 0 {
            let (child_own, child_opponent) = child(own, opponent, &mv);
            children.push(EndgameChild {
                own: child_own,
                opponent: child_opponent,
                empties: empties - 1,
                replies: mv.replies,
                square: mv.square as i32,
            });
        }
    }
    children
}

/// A list of at most [`DEEP_MOVES`] items on the stack.
pub(crate) struct ArrayVec<T: Copy + Default> {
    items: [T; DEEP_MOVES],
    len: usize,
}

impl<T: Copy + Default> Default for ArrayVec<T> {
    fn default() -> Self {
        ArrayVec { items: [T::default(); DEEP_MOVES], len: 0 }
    }
}

impl<T: Copy + Default> ArrayVec<T> {
    fn push(&mut self, item: T) {
        self.items[self.len] = item;
        self.len += 1;
    }

    fn is_empty(&self) -> bool {
        self.len == 0
    }

    fn as_slice(&self) -> &[T] {
        &self.items[..self.len]
    }
}

/// The position after a move, for the opponent. C#: `Child`.
#[inline]
fn child(own: u64, opponent: u64, mv: &EndMove) -> (u64, u64) {
    (opponent ^ mv.flips, own ^ mv.flips ^ (1u64 << mv.square))
}

/// Moves the best of `moves[start..]` to `start`; the first of equal values wins (Egaroucid swap_next_best_move).
/// C#: `SwapNextBest`.
#[inline]
fn swap_next_best(moves: &mut [EndMove], start: usize) {
    let mut top = start;
    for i in start + 1..moves.len() {
        if moves[i].value > moves[top].value {
            top = i;
        }
    }

    if top != start {
        moves.swap(start, top);
    }
}

/// Moves the moves not in `done` (those the local table did not decide) to the front, in their order, and returns
/// how many there are, so that decided moves cannot change the order of the rest.
fn undone_first(moves: &mut [EndMove], done: u64) -> usize {
    let mut remaining = 0;
    for i in 0..moves.len() {
        if done & (1u64 << moves[i].square) == 0 {
            moves.swap(remaining, i);
            remaining += 1;
        }
    }

    remaining
}

#[cfg(test)]
mod tests {
    use super::{DEEP_MOVES, END_INFINITY, EndMove, fill_end_moves, swap_next_best, undone_first};
    use crate::bits;
    use crate::board::{Board, Player};
    use crate::search::engine::{Abort, final_score};
    use crate::search::table::Bound;
    use crate::search::{NoStop, SearchEngine, SearchOptions, SearchSignals};

    // Plain minimax to the end, the empty squares going to the winner.
    fn exact(own: u64, opponent: u64, passed: bool) -> i32 {
        let mut moves = bits::legal_moves(own, opponent);
        if moves == 0 {
            return if passed { final_score(own, opponent) } else { -exact(opponent, own, true) };
        }

        let mut best = -i32::MAX;
        while moves != 0 {
            let square = moves.trailing_zeros();
            moves &= moves - 1;
            let flips = bits::flips(own, opponent, square);
            best = best.max(-exact(opponent & !flips, own | flips | (1 << square), false));
        }

        best
    }

    #[test]
    fn shallow_solver_finds_the_exact_score_and_correct_bounds() {
        let mut engine = SearchEngine::new(10, SearchOptions::default());
        let mut searcher = engine.searcher(None, &NoStop, 64);
        let mut seed = 0x2545_F491_4F6C_DD1Du64;
        let mut checked = 0;
        for _ in 0..300 {
            // A random game until 6 or fewer squares are empty.
            let (mut board, mut player) = (Board::INITIAL, Player::Black);
            while board.empty_count() > 6 && !board.is_game_over() {
                let moves = board.legal_moves(player);
                if moves != 0 {
                    seed ^= seed << 13;
                    seed ^= seed >> 7;
                    seed ^= seed << 17;
                    let mut pick = (seed % moves.count_ones() as u64) as u32;
                    let mut rest = moves;
                    while pick > 0 {
                        rest &= rest - 1;
                        pick -= 1;
                    }
                    board = board.play_unchecked(player, rest.trailing_zeros());
                }
                player = player.opponent();
            }

            let (own, opponent) = (board.discs(player), board.discs(player.opponent()));
            let empties = board.empty_count() as i32;
            if empties == 0 {
                continue;
            }

            let score = exact(own, opponent, false);
            assert_eq!(searcher.solve_shallow(own, opponent, -64, 64, empties, false), Ok(score), "{board}");
            for alpha in [-10, -1, 0, 4] {
                let bound = searcher.solve_shallow(own, opponent, alpha, alpha + 1, empties, false).unwrap();
                assert_eq!(bound > alpha, score > alpha, "{board} alpha {alpha}");
            }
            checked += 1;
        }

        assert!(checked > 200);
        assert!(searcher.nodes > 0 && searcher.evaluations > 0);
    }

    #[test]
    fn ordering_value_counts_terminal_evaluations_and_shallow_paths_check_abort() {
        let mut engine = SearchEngine::new(10, SearchOptions::default());
        {
            let mut searcher = engine.searcher(None, &NoStop, 64);
            searcher.ordering_value(0, u64::MAX, 0, -64, 64).unwrap();
            assert_eq!((searcher.nodes, searcher.evaluations), (1, 1));
            assert_eq!(searcher.ordering_value(0, u64::MAX, 1, -64, 64), Ok(final_score(0, u64::MAX)));
            assert_eq!((searcher.nodes, searcher.evaluations), (2, 2));
        }

        let stop = SearchSignals::new();
        stop.cancel();
        let mut searcher = engine.searcher(None, &stop, 4);
        searcher.nodes = 1023;
        searcher.next_abort_check = 1024;
        assert!(matches!(searcher.ordering_value(0, 0, 0, -64, 64), Err(Abort::Cancelled)));

        searcher.nodes = 1023;
        searcher.next_abort_check = 1024;
        assert!(matches!(searcher.solve_shallow(0, !0b1111u64, -64, 64, 4, false), Err(Abort::Cancelled)));

        searcher.nodes = 1023;
        searcher.next_abort_check = 1024;
        assert!(matches!(searcher.solve_last2(0, !0b11u64, -64, 64, 0b11, false), Err(Abort::Cancelled)));
    }

    #[test]
    fn undone_first_keeps_decided_moves_out_of_the_sort() {
        let mut moves = [
            EndMove { square: 1, value: -5, ..EndMove::default() },
            EndMove { square: 2, value: 100, ..EndMove::default() },
            EndMove { square: 3, value: 5, ..EndMove::default() },
            EndMove { square: 4, value: 200, ..EndMove::default() },
        ];
        let order_count = undone_first(&mut moves, (1 << 2) | (1 << 4));

        assert_eq!(order_count, 2);
        assert_eq!([moves[0].square, moves[1].square], [1, 3]);
        for i in 0..order_count {
            swap_next_best(&mut moves[..order_count], i);
        }
        assert_eq!([moves[0].square, moves[1].square], [3, 1]);
    }

    #[test]
    fn end_deep_stores_the_move_with_the_best_etc_fail_low_value() {
        let mut seed = 0x2545_F491_4F6C_DD1Du64;
        let (board, player, legal) = loop {
            let (mut board, mut player) = (Board::INITIAL, Player::Black);
            while board.empty_count() > 14 {
                let mut moves = board.legal_moves(player);
                if moves == 0 {
                    player = player.opponent();
                    moves = board.legal_moves(player);
                }
                if moves == 0 {
                    break;
                }
                seed ^= seed << 13;
                seed ^= seed >> 7;
                seed ^= seed << 17;
                for _ in 0..seed % moves.count_ones() as u64 {
                    moves &= moves - 1;
                }
                board = board.play_unchecked(player, moves.trailing_zeros());
                player = player.opponent();
            }

            if board.empty_count() != 14 {
                continue;
            }
            let mut legal = board.legal_moves(player);
            if legal == 0 {
                player = player.opponent();
                legal = board.legal_moves(player);
            }
            if legal == 0 {
                continue;
            }
            let count = legal.count_ones() as usize;
            let mut moves = [EndMove::default(); DEEP_MOVES];
            if !fill_end_moves(board.discs(player), board.discs(player.opponent()), legal, &mut moves[..count]) {
                break (board, player, legal);
            }
        };

        let mut engine = SearchEngine::new(12, SearchOptions::default());
        let root_empties = board.empty_count();
        let own = board.discs(player);
        let opponent = board.discs(player.opponent());
        let mut searcher = engine.searcher(None, &NoStop, root_empties);
        let mut best_move = -1;
        let mut best_value = -END_INFINITY;
        let mut index = 0;
        let mut remaining = legal;
        while remaining != 0 {
            let square = remaining.trailing_zeros();
            remaining &= remaining - 1;
            let flips = bits::flips(own, opponent, square);
            let child_own = opponent ^ flips;
            let child_opponent = own ^ flips ^ (1u64 << square);
            let value = 10 + index;
            if value > best_value {
                best_value = value;
                best_move = square as i32;
            }
            searcher.endgame_table.store_at_root(
                child_own,
                child_opponent,
                0,
                root_empties as i32 - 1,
                Bound::Exact,
                -value,
                -1,
                root_empties,
            );
            index += 1;
        }

        assert_eq!(searcher.end_deep(own, opponent, 63, 14, true, legal), Ok(best_value));
        assert_eq!(
            searcher.endgame_table.get_at_root(own, opponent, 0, root_empties).map(|entry| entry.mv as i32),
            Some(best_move)
        );
    }
}
