//! Move generation on raw bitboards (bit n = square index n, a1 = 0, h8 = 63) for the search.
//!
//! C#: `Stello.Engine.Bitboards` (shared by Stello 2.0 and the engine being built).

const NOT_COLUMN_A: u64 = 0xFEFE_FEFE_FEFE_FEFE;
const NOT_COLUMN_H: u64 = 0x7F7F_7F7F_7F7F_7F7F;
const INNER_COLUMNS: u64 = 0x7E7E_7E7E_7E7E_7E7E;
const INNER_ROWS: u64 = 0x00FF_FFFF_FFFF_FF00;
const INNER: u64 = INNER_COLUMNS & INNER_ROWS;

/// The squares where `own` can play against `opponent`. C#: `Bitboards.LegalMoves`.
#[inline]
pub fn legal_moves(own: u64, opponent: u64) -> u64 {
    let empty = !(own | opponent);
    let horizontal = opponent & INNER_COLUMNS;
    let vertical = opponent & INNER_ROWS;
    let diagonal = opponent & INNER;

    let moves = scan_left(own, horizontal, 1)
        | scan_right(own, horizontal, 1)
        | scan_left(own, vertical, 8)
        | scan_right(own, vertical, 8)
        | scan_left(own, diagonal, 7)
        | scan_right(own, diagonal, 7)
        | scan_left(own, diagonal, 9)
        | scan_right(own, diagonal, 9);

    moves & empty
}

/// The discs `own` flips by playing `square` (0 if none). The square is not checked for being empty.
/// C#: `Bitboards.Flips`.
#[inline]
pub fn flips(own: u64, opponent: u64, square: u32) -> u64 {
    let bit = 1u64 << square;
    ray_left(own, opponent, bit, 1, NOT_COLUMN_A)
        | ray_left(own, opponent, bit, 7, NOT_COLUMN_H)
        | ray_left(own, opponent, bit, 8, u64::MAX)
        | ray_left(own, opponent, bit, 9, NOT_COLUMN_A)
        | ray_right(own, opponent, bit, 1, NOT_COLUMN_H)
        | ray_right(own, opponent, bit, 7, NOT_COLUMN_A)
        | ray_right(own, opponent, bit, 8, u64::MAX)
        | ray_right(own, opponent, bit, 9, NOT_COLUMN_H)
}

/// The number of (empty square, direction) pairs where the neighbour is an opponent disc (C++: countmov potential).
/// C#: `Bitboards.PotentialMobility`. Stello 2.0's search uses [`crate::endgame::bits::potential_mobility`] instead.
#[inline]
pub fn potential_mobility(own: u64, opponent: u64) -> i32 {
    let empty = !(own | opponent);
    (((opponent << 1) & NOT_COLUMN_A & empty).count_ones()
        + ((opponent >> 1) & NOT_COLUMN_H & empty).count_ones()
        + ((opponent << 8) & empty).count_ones()
        + ((opponent >> 8) & empty).count_ones()
        + ((opponent << 7) & NOT_COLUMN_H & empty).count_ones()
        + ((opponent << 9) & NOT_COLUMN_A & empty).count_ones()
        + ((opponent >> 7) & NOT_COLUMN_A & empty).count_ones()
        + ((opponent >> 9) & NOT_COLUMN_H & empty).count_ones()) as i32
}

#[inline(always)]
fn scan_left(own: u64, mask: u64, shift: u32) -> u64 {
    let mut run = mask & (own << shift);
    run |= mask & (run << shift);
    run |= mask & (run << shift);
    run |= mask & (run << shift);
    run |= mask & (run << shift);
    run |= mask & (run << shift);
    run << shift
}

#[inline(always)]
fn scan_right(own: u64, mask: u64, shift: u32) -> u64 {
    let mut run = mask & (own >> shift);
    run |= mask & (run >> shift);
    run |= mask & (run >> shift);
    run |= mask & (run >> shift);
    run |= mask & (run >> shift);
    run |= mask & (run >> shift);
    run >> shift
}

#[inline(always)]
fn ray_left(own: u64, opponent: u64, bit: u64, shift: u32, mask: u64) -> u64 {
    let mut line = 0;
    let mut next = (bit << shift) & mask;
    while next & opponent != 0 {
        line |= next;
        next = (next << shift) & mask;
    }

    if next & own != 0 { line } else { 0 }
}

#[inline(always)]
fn ray_right(own: u64, opponent: u64, bit: u64, shift: u32, mask: u64) -> u64 {
    let mut line = 0;
    let mut next = (bit >> shift) & mask;
    while next & opponent != 0 {
        line |= next;
        next = (next >> shift) & mask;
    }

    if next & own != 0 { line } else { 0 }
}
