//! Stello 2's exact endgame solver. C#: `Stello.Engine.V2.SearchEngine`'s partial class
//! Endgame/SearchEngine.Endgame.cs (with `Endgame/LocalTable.cs` and `Endgame/StableDiscs.cs`), and the shallow
//! solver at the end of SearchEngine.cs (`SolveShallow`, `SolveLast2`, `SolveLast`).
//!
//! **Phase 1 step 4 fills this in.** So far it has the constants the midgame uses, the node counter of the solver,
//! the shallow solver from SearchEngine.cs, and [`Searcher::solve_child`], the one entry point the midgame calls
//! (from `solve_root`), as a `todo!()`. The solver is an `impl Searcher` block, as the C# partial class: it shares
//! the node and evaluation counters, the abort checks, the reports and both hash tables
//! (`self.endgame_table`, `self.midgame_table`) with the midgame. State that lives from search to search (C#:
//! `_localTable`) goes into [`EndgameState`], which `SearchEngine::clear_hash` clears.
//!
//! Scores are final disc differences for the side to move, the empty squares going to the winner. Null-window
//! searches return fail-soft bounds: a score above alpha is a lower bound, at or below alpha an upper bound.

// The constants and the shallow solver are for the solver of phase 1 step 4; remove this when it calls them.
#![allow(dead_code)]

use crate::bits;
use crate::search::engine::{Abort, INFINITY, Searcher, final_score};

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

/// The solver's state that lives from search to search: C#'s `_localTable` (Endgame/LocalTable.cs) goes here.
#[derive(Default)]
pub struct EndgameState {}

impl EndgameState {
    /// Part of `SearchEngine::clear_hash` (C#: `_localTable.Clear()`).
    pub fn clear(&mut self) {}
}

impl Searcher<'_> {
    /// The score of a position reached by a root move, within (alpha, beta). C#: `SolveChild` (null window:
    /// `EndNull(own, opponent, alpha, empties, LegalUndefined)`, else `EndWindow(..., passed: false)`).
    ///
    /// Called by `solve_root` (C#: `SolveRoot`, in SearchEngine.cs and already ported) for the win/loss/draw search
    /// with the window (-1, 1) and then the exact one.
    pub(crate) fn solve_child(
        &mut self,
        _own: u64,
        _opponent: u64,
        _alpha: i32,
        _beta: i32,
        _empties: i32,
    ) -> Result<i32, Abort> {
        todo!("the endgame solver is phase 1 step 4 (Plans/making-stello-3-plan.md)")
    }

    /// Counts a node of the solver and checks for a stop every 1024 nodes from the last check (C#: `CountEndNode`;
    /// the midgame and `solve_shallow` check when `nodes % 1024 == 0` instead).
    #[inline]
    pub(crate) fn count_end_node(&mut self) -> Result<(), Abort> {
        self.nodes += 1;
        if self.nodes >= self.next_abort_check {
            self.next_abort_check = self.nodes + 1024;
            self.check_abort()?;
        }

        Ok(())
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
        self.nodes += 1;
        if self.nodes & 1023 == 0 {
            self.check_abort()?;
        }

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
            self.nodes += 1;
            best = -self.solve_last(opponent & !flips, own | flips | (1u64 << first), 1u64 << second);
            if best >= beta {
                return Ok(best);
            }
        }

        let flips = bits::flips(own, opponent, second);
        if flips != 0 {
            self.nodes += 1;
            best = best.max(-self.solve_last(opponent & !flips, own | flips | (1u64 << second), 1u64 << first));
        }

        if best > -INFINITY {
            return Ok(best);
        }

        if passed {
            self.evaluations += 1;
            return Ok(final_score(own, opponent));
        }

        self.nodes += 1;
        if self.nodes & 1023 == 0 {
            self.check_abort()?;
        }

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

#[cfg(test)]
mod tests {
    use crate::bits;
    use crate::board::{Board, Player};
    use crate::search::engine::final_score;
    use crate::search::{NoStop, SearchEngine, SearchOptions};

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
        let mut searcher = engine.searcher(None, &NoStop);
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
}
