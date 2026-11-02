//! The last 4 empty squares of the null-window solver (Egaroucid last4_nws ... last1, endsearch_nws_last_generic.hpp,
//! https://github.com/Nyanyan/Egaroucid, © Takuto Yamana, GPL-3.0-or-later). C#: `Last4`, `Last3`, `Last2`, `Last1`
//! and `LastFlipCount` in Endgame/SearchEngine.Endgame.cs.
//!
//! `last4` and `last2` return the score for the side that moved into their position; `last3` and `last1` for the
//! side to move. The pass at each level flips the sign through `pol`. They count nodes but do not check for a stop
//! (C# neither); `end_fast` above them does.

use super::bits::{FLIP_NEIGHBOURS, flips, is_lone_empty, lone_empties, score_with_empties, score_with_odd_empties};
use super::search::END_INFINITY;
use crate::search::engine::Searcher;

impl Searcher<'_> {
    /// C#: `Last4`.
    pub(crate) fn last4(&mut self, own: u64, opponent: u64, mut alpha: i32) -> i32 {
        let empties = !(own | opponent);
        let mut lone = lone_empties(own, opponent);
        if lone == 0 {
            lone = empties;
        }

        let rest = empties & !lone;
        let p0 = lone.trailing_zeros();
        lone &= lone.wrapping_sub(1);
        if lone == 0 {
            lone = rest;
        }

        let p1 = lone.trailing_zeros();
        lone &= lone.wrapping_sub(1);
        if lone == 0 {
            lone = rest;
        }

        let p2 = lone.trailing_zeros();
        lone &= lone.wrapping_sub(1);
        let p3 = if lone != 0 { lone.trailing_zeros() } else { rest.trailing_zeros() };

        let (mut p, mut o) = (own, opponent);
        let mut v = END_INFINITY;
        let mut pol = 1;
        loop {
            self.nodes += 1;
            if FLIP_NEIGHBOURS[p0 as usize] & o != 0 {
                let f = flips(p, o, p0);
                if f != 0 {
                    v = self.last3(o ^ f, p ^ f ^ (1u64 << p0), alpha, p1, p2, p3, empties);
                    if alpha >= v {
                        return v * pol;
                    }
                }
            }

            if FLIP_NEIGHBOURS[p1 as usize] & o != 0 {
                let f = flips(p, o, p1);
                if f != 0 {
                    let g = self.last3(o ^ f, p ^ f ^ (1u64 << p1), alpha, p0, p2, p3, empties);
                    if alpha >= g {
                        return g * pol;
                    }

                    v = v.min(g);
                }
            }

            if FLIP_NEIGHBOURS[p2 as usize] & o != 0 {
                let f = flips(p, o, p2);
                if f != 0 {
                    let g = self.last3(o ^ f, p ^ f ^ (1u64 << p2), alpha, p0, p1, p3, empties);
                    if alpha >= g {
                        return g * pol;
                    }

                    v = v.min(g);
                }
            }

            if FLIP_NEIGHBOURS[p3 as usize] & o != 0 {
                let f = flips(p, o, p3);
                if f != 0 {
                    let g = self.last3(o ^ f, p ^ f ^ (1u64 << p3), alpha, p0, p1, p2, empties);
                    return v.min(g) * pol;
                }
            }

            if v < END_INFINITY {
                return v * pol;
            }

            (p, o) = (o, p);
            alpha = -alpha - 1;
            pol = -pol;
            if pol >= 0 {
                break;
            }
        }

        -score_with_empties(own, 4)
    }

    /// C#: `Last3`.
    #[allow(clippy::too_many_arguments)]
    fn last3(
        &mut self,
        own: u64,
        opponent: u64,
        mut alpha: i32,
        mut p0: u32,
        mut p1: u32,
        mut p2: u32,
        empties4: u64,
    ) -> i32 {
        // A square alone in its region first (the parity is read on the position with 4 empty squares, as in
        // Egaroucid).
        if is_lone_empty(p2, empties4) {
            std::mem::swap(&mut p2, &mut p0);
        } else if is_lone_empty(p1, empties4) {
            std::mem::swap(&mut p1, &mut p0);
        }

        let (mut p, mut o) = (own, opponent);
        let mut v = -END_INFINITY;
        let mut pol = 1;
        loop {
            self.nodes += 1;
            if FLIP_NEIGHBOURS[p0 as usize] & o != 0 {
                let f = flips(p, o, p0);
                if f != 0 {
                    v = self.last2(o ^ f, p ^ f ^ (1u64 << p0), alpha, p1, p2);
                    if alpha < v {
                        return v * pol;
                    }
                }
            }

            if FLIP_NEIGHBOURS[p1 as usize] & o != 0 {
                let f = flips(p, o, p1);
                if f != 0 {
                    let g = self.last2(o ^ f, p ^ f ^ (1u64 << p1), alpha, p2, p0);
                    if alpha < g {
                        return g * pol;
                    }

                    v = v.max(g);
                }
            }

            if FLIP_NEIGHBOURS[p2 as usize] & o != 0 {
                let f = flips(p, o, p2);
                if f != 0 {
                    let g = self.last2(o ^ f, p ^ f ^ (1u64 << p2), alpha, p1, p0);
                    return v.max(g) * pol;
                }
            }

            if v > -END_INFINITY {
                return v * pol;
            }

            (p, o) = (o, p);
            alpha = -alpha - 1;
            pol = -pol;
            if pol >= 0 {
                break;
            }
        }

        score_with_odd_empties(own, 3)
    }

    /// C#: `Last2`.
    fn last2(&mut self, own: u64, opponent: u64, mut alpha: i32, p0: u32, p1: u32) -> i32 {
        self.nodes += 1;
        let mut v;
        let f0 = if FLIP_NEIGHBOURS[p0 as usize] & opponent != 0 { flips(own, opponent, p0) } else { 0 };
        if f0 != 0 {
            v = self.last1(opponent ^ f0, alpha, p1);
            if v > alpha && FLIP_NEIGHBOURS[p1 as usize] & opponent != 0 {
                let f1 = flips(own, opponent, p1);
                if f1 != 0 {
                    v = v.min(self.last1(opponent ^ f1, alpha, p0));
                }
            }
        } else {
            let f1 = if FLIP_NEIGHBOURS[p1 as usize] & opponent != 0 { flips(own, opponent, p1) } else { 0 };
            if f1 != 0 {
                v = self.last1(opponent ^ f1, alpha, p0);
            } else {
                // The side to move passes; the other side plays.
                self.nodes += 1;
                alpha = -alpha - 1;
                let g0 = flips(opponent, own, p0);
                if g0 != 0 {
                    v = self.last1(own ^ g0, alpha, p1);
                    if v > alpha {
                        let g1 = flips(opponent, own, p1);
                        if g1 != 0 {
                            v = v.min(self.last1(own ^ g1, alpha, p0));
                        }
                    }
                } else {
                    let g1 = flips(opponent, own, p1);
                    v = if g1 != 0 { self.last1(own ^ g1, alpha, p0) } else { score_with_empties(own, 2) };
                }

                v = -v;
            }
        }

        v
    }

    /// `own` is the side to move; every square but the empty one that is not own's is the opponent's. C#: `Last1`.
    #[inline]
    fn last1(&mut self, own: u64, alpha: i32, square: u32) -> i32 {
        self.nodes += 1;
        let mut flipped = last_flip_count(own, square);
        let mut score = 2 * (own.count_ones() as i32 + flipped + 1) - 64;
        if flipped == 0 {
            // own cannot play there: the opponent may, or the square stays empty.
            self.nodes += 1;
            let pass_score = score - 2;
            if score <= 0 {
                score = pass_score;
            }

            if score > alpha {
                flipped = last_flip_count(!own, square);
                if flipped != 0 {
                    score = pass_score - 2 * flipped;
                }
            }
        }

        score
    }
}

/// C#: `LastFlipCount`.
#[inline]
fn last_flip_count(own: u64, square: u32) -> i32 {
    let bit = 1u64 << square;
    flips(own & !bit, !own & !bit, square).count_ones() as i32
}
