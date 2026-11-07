//! The last 4 empty squares of the null-window solver (Egaroucid last4_nws ... last1, endsearch_nws_last_generic.hpp,
//! https://github.com/Nyanyan/Egaroucid, © Takuto Yamana, GPL-3.0-or-later). C#: `Last4`, `Last3`, `Last2`, `Last1`
//! and `LastFlipCount` in Endgame/SearchEngine.Endgame.cs.
//!
//! `last4` and `last2` return the score for the side that moved into their position; `last3` and `last1` for the
//! side to move. The pass at each level flips the sign through `pol`. Each node and pass uses the shared abort check.

use super::bits::{FLIP_NEIGHBOURS, flips, is_lone_empty, lone_empties, score_with_empties, score_with_odd_empties};
use super::search::END_INFINITY;
use crate::search::engine::{Abort, Searcher};

impl Searcher<'_> {
    /// C#: `Last4`.
    pub(crate) fn last4(&mut self, own: u64, opponent: u64, mut alpha: i32) -> Result<i32, Abort> {
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
            self.count_node()?;
            if FLIP_NEIGHBOURS[p0 as usize] & o != 0 {
                let f = flips(p, o, p0);
                if f != 0 {
                    v = self.last3(o ^ f, p ^ f ^ (1u64 << p0), alpha, p1, p2, p3, empties)?;
                    if alpha >= v {
                        return Ok(v * pol);
                    }
                }
            }

            if FLIP_NEIGHBOURS[p1 as usize] & o != 0 {
                let f = flips(p, o, p1);
                if f != 0 {
                    let g = self.last3(o ^ f, p ^ f ^ (1u64 << p1), alpha, p0, p2, p3, empties)?;
                    if alpha >= g {
                        return Ok(g * pol);
                    }

                    v = v.min(g);
                }
            }

            if FLIP_NEIGHBOURS[p2 as usize] & o != 0 {
                let f = flips(p, o, p2);
                if f != 0 {
                    let g = self.last3(o ^ f, p ^ f ^ (1u64 << p2), alpha, p0, p1, p3, empties)?;
                    if alpha >= g {
                        return Ok(g * pol);
                    }

                    v = v.min(g);
                }
            }

            if FLIP_NEIGHBOURS[p3 as usize] & o != 0 {
                let f = flips(p, o, p3);
                if f != 0 {
                    let g = self.last3(o ^ f, p ^ f ^ (1u64 << p3), alpha, p0, p1, p2, empties)?;
                    return Ok(v.min(g) * pol);
                }
            }

            if v < END_INFINITY {
                return Ok(v * pol);
            }

            (p, o) = (o, p);
            alpha = -alpha - 1;
            pol = -pol;
            if pol >= 0 {
                break;
            }
        }

        self.evaluations += 1;
        Ok(-score_with_empties(own, 4))
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
    ) -> Result<i32, Abort> {
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
            self.count_node()?;
            if FLIP_NEIGHBOURS[p0 as usize] & o != 0 {
                let f = flips(p, o, p0);
                if f != 0 {
                    v = self.last2(o ^ f, p ^ f ^ (1u64 << p0), alpha, p1, p2)?;
                    if alpha < v {
                        return Ok(v * pol);
                    }
                }
            }

            if FLIP_NEIGHBOURS[p1 as usize] & o != 0 {
                let f = flips(p, o, p1);
                if f != 0 {
                    let g = self.last2(o ^ f, p ^ f ^ (1u64 << p1), alpha, p2, p0)?;
                    if alpha < g {
                        return Ok(g * pol);
                    }

                    v = v.max(g);
                }
            }

            if FLIP_NEIGHBOURS[p2 as usize] & o != 0 {
                let f = flips(p, o, p2);
                if f != 0 {
                    let g = self.last2(o ^ f, p ^ f ^ (1u64 << p2), alpha, p1, p0)?;
                    return Ok(v.max(g) * pol);
                }
            }

            if v > -END_INFINITY {
                return Ok(v * pol);
            }

            (p, o) = (o, p);
            alpha = -alpha - 1;
            pol = -pol;
            if pol >= 0 {
                break;
            }
        }

        self.evaluations += 1;
        Ok(score_with_odd_empties(own, 3))
    }

    /// C#: `Last2`.
    fn last2(&mut self, own: u64, opponent: u64, mut alpha: i32, p0: u32, p1: u32) -> Result<i32, Abort> {
        self.count_node()?;
        let mut v;
        let f0 = if FLIP_NEIGHBOURS[p0 as usize] & opponent != 0 { flips(own, opponent, p0) } else { 0 };
        if f0 != 0 {
            v = self.last1(opponent ^ f0, alpha, p1)?;
            if v > alpha && FLIP_NEIGHBOURS[p1 as usize] & opponent != 0 {
                let f1 = flips(own, opponent, p1);
                if f1 != 0 {
                    v = v.min(self.last1(opponent ^ f1, alpha, p0)?);
                }
            }
        } else {
            let f1 = if FLIP_NEIGHBOURS[p1 as usize] & opponent != 0 { flips(own, opponent, p1) } else { 0 };
            if f1 != 0 {
                v = self.last1(opponent ^ f1, alpha, p0)?;
            } else {
                // The side to move passes; the other side plays.
                self.count_node()?;
                alpha = -alpha - 1;
                let g0 = flips(opponent, own, p0);
                if g0 != 0 {
                    v = self.last1(own ^ g0, alpha, p1)?;
                    if v > alpha {
                        let g1 = flips(opponent, own, p1);
                        if g1 != 0 {
                            v = v.min(self.last1(own ^ g1, alpha, p0)?);
                        }
                    }
                } else {
                    let g1 = flips(opponent, own, p1);
                    v = if g1 != 0 {
                        self.last1(own ^ g1, alpha, p0)?
                    } else {
                        self.evaluations += 1;
                        score_with_empties(own, 2)
                    };
                }

                v = -v;
            }
        }

        Ok(v)
    }

    /// `own` is the side to move; every square but the empty one that is not own's is the opponent's. C#: `Last1`.
    #[inline]
    fn last1(&mut self, own: u64, alpha: i32, square: u32) -> Result<i32, Abort> {
        self.count_node()?;
        let mut flipped = last_flip_count(own, square);
        let mut score = 2 * (own.count_ones() as i32 + flipped + 1) - 64;
        if flipped == 0 {
            // own cannot play there: the opponent may, or the square stays empty.
            self.count_node()?;
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

        self.evaluations += 1;
        Ok(score)
    }
}

/// The number of discs `own` flips by playing `square` when every other square is the opponent's.
/// C#: `LastFlipCount`, which computes the flips themselves; this counts them per line (Edax's count_last_flip): the
/// row, the column and the two diagonals through the square, each read into one byte with the square's place in
/// it, and [`LAST_FLIP_COUNTS`] counts the opponent's discs between it and the nearest own disc on each side.
#[inline]
fn last_flip_count(own: u64, square: u32) -> i32 {
    let (column, row) = (square & 7, square >> 3);
    let own = own & !(1u64 << square);
    let line = |mask: u64, multiplier: u64| ((own & mask).wrapping_mul(multiplier) >> 56) as usize;
    let lines = &LAST_LINES[square as usize];
    // A row: its byte as it is. A column: bit r of the byte is row r. A diagonal: bit c is column c, and the bits
    // beyond its ends are 0, which counts as the opponent's but can never be outflanked.
    let counts = &LAST_FLIP_COUNTS;
    (counts[column as usize][((own >> (8 * row)) & 0xFF) as usize]
        + counts[row as usize][line(lines.column, lines.column_multiplier)]
        + counts[column as usize][line(lines.diagonal, 0x0101_0101_0101_0101)]
        + counts[column as usize][line(lines.anti_diagonal, 0x0101_0101_0101_0101)]) as i32
}

/// The masks of one square's column and diagonals, and the multiplier that gathers its column into the top byte.
struct LastLines {
    column: u64,
    column_multiplier: u64,
    diagonal: u64,
    anti_diagonal: u64,
}

static LAST_LINES: [LastLines; 64] = last_lines();

/// `[place][own discs on the line]`: the opponent's discs flipped by a move at `place` of an 8-square line whose
/// other squares are all the opponent's but for the own discs.
static LAST_FLIP_COUNTS: [[u8; 256]; 8] = last_flip_counts();

const fn last_lines() -> [LastLines; 64] {
    let mut lines = [const { LastLines { column: 0, column_multiplier: 0, diagonal: 0, anti_diagonal: 0 } }; 64];
    let mut square = 0;
    while square < 64 {
        let (column, row) = ((square & 7) as i32, (square >> 3) as i32);
        let mut line = LastLines { column: 0, column_multiplier: 0, diagonal: 0, anti_diagonal: 0 };
        let mut r = 0;
        while r < 8 {
            line.column |= 1u64 << (r * 8 + column);
            // Square (column c, row r) times 2^(56 - c - 7r) lands on bit 56 + r without carries.
            line.column_multiplier |= 1u64 << (56 - column - 7 * r);
            let c = column + (r - row);
            if c >= 0 && c < 8 {
                line.diagonal |= 1u64 << (r * 8 + c);
            }

            let c = column - (r - row);
            if c >= 0 && c < 8 {
                line.anti_diagonal |= 1u64 << (r * 8 + c);
            }

            r += 1;
        }

        lines[square] = line;
        square += 1;
    }

    lines
}

const fn last_flip_counts() -> [[u8; 256]; 8] {
    let mut counts = [[0u8; 256]; 8];
    let mut place = 0;
    while place < 8 {
        let mut own = 0;
        while own < 256 {
            let mut count = 0;
            // Towards higher bits, then towards lower ones: the run of opponent's discs up to an own disc.
            let mut run = 0;
            let mut i = place + 1;
            while i < 8 && own & (1 << i) == 0 {
                run += 1;
                i += 1;
            }
            if i < 8 {
                count += run;
            }

            run = 0;
            let mut i = place as i32 - 1;
            while i >= 0 && own & (1 << i) == 0 {
                run += 1;
                i -= 1;
            }
            if i >= 0 {
                count += run;
            }

            counts[place][own] = count;
            own += 1;
        }
        place += 1;
    }

    counts
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::search::{SearchEngine, SearchOptions, SearchSignals};

    #[test]
    fn last_flip_counts_equal_the_flips() {
        let mut state = 0x9E37_79B9_7F4A_7C15u64;
        let mut next = || {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            state
        };
        for round in 0..20_000 {
            let own = match round % 4 {
                0 => next(),
                1 => next() & next(),
                2 => next() | next(),
                _ => next() & next() & next(),
            };
            for square in 0..64 {
                let bit = 1u64 << square;
                let expected = flips(own & !bit, !own & !bit, square).count_ones() as i32;
                assert_eq!(last_flip_count(own, square), expected, "{own:#x} {square}");
            }
        }
    }

    #[test]
    fn last_square_pass_paths_reach_the_shared_abort_check() {
        let mut engine = SearchEngine::new(10, SearchOptions::default());
        let stop = SearchSignals::new();
        stop.cancel();
        let mut searcher = engine.searcher(None, &stop, 4);

        searcher.nodes = 1022;
        searcher.next_abort_check = 1024;
        assert!(matches!(searcher.last4(0, !0b1111u64, 0), Err(Abort::Cancelled)));
        searcher.nodes = 1022;
        searcher.next_abort_check = 1024;
        assert!(matches!(searcher.last3(0, !0b111u64, 0, 0, 1, 2, 0b111), Err(Abort::Cancelled)));
        searcher.nodes = 1022;
        searcher.next_abort_check = 1024;
        assert!(matches!(searcher.last2(0, !0b11u64, 0, 0, 1), Err(Abort::Cancelled)));
        searcher.nodes = 1022;
        searcher.next_abort_check = 1024;
        assert!(matches!(searcher.last1(0, 0, 0), Err(Abort::Cancelled)));
    }
}
