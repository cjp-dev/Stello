//! Stable discs, which no later move can flip: on the edges from a table of all 8-square lines, inside from full
//! lines and by growing from discs already stable.
//!
//! C#: `Stello.Engine.V2.Endgame.StableDiscs`. Adapted from Egaroucid src/engine/stability.hpp
//! (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana, GPL-3.0-or-later, through the C# port in
//! Stello.Net/Egaroucid.Engine. The calculation is the same under a half turn of the board, so it works on Stello's
//! square numbering unchanged.

use std::sync::OnceLock;

/// `[own * 256 + opponent]`: the stable squares of an edge line, laid out as a row (bits 0..7) and as a column
/// (bits 0, 8, .., 56). C#: `EdgeRow`, `EdgeColumn`. Built on first use (1 MB; C#: the static constructor).
struct EdgeTables {
    row: Vec<u64>,
    column: Vec<u64>,
}

static EDGES: OnceLock<EdgeTables> = OnceLock::new();

fn edges() -> &'static EdgeTables {
    EDGES.get_or_init(build_edges)
}

/// Builds the edge tables now rather than in the first search (`SearchEngine::new` calls it).
pub fn init() {
    edges();
}

fn build_edges() -> EdgeTables {
    let mut row = vec![0u64; 256 * 256];
    let mut column = vec![0u64; 256 * 256];
    let mut memo = vec![-1i16; 256 * 256];
    for b in 0..256usize {
        for w in b..256usize {
            if b & w != 0 {
                continue;
            }

            let stable = line_stability(b as i32, w as i32, &mut memo);
            let (mut r, mut c) = (0u64, 0u64);
            for place in 0..8 {
                if (stable >> place) & 1 != 0 {
                    r |= 1u64 << place;
                    c |= 1u64 << (place * 8);
                }
            }

            row[b * 256 + w] = r;
            row[w * 256 + b] = r;
            column[b * 256 + w] = c;
            column[w * 256 + b] = c;
        }
    }

    EdgeTables { row, column }
}

/// The stable discs of `own` (Egaroucid `calc_stability`). C#: `StableDiscs.Of`.
#[inline]
pub fn of(own: u64, opponent: u64) -> u64 {
    let tables = edges();
    let mut stable = 0u64;
    let inner = own & 0x007E_7E7E_7E7E_7E00;
    let mut found = tables.row[(own & 0xFF) as usize * 256 + (opponent & 0xFF) as usize];
    found |= tables.row[(own >> 56) as usize * 256 + (opponent >> 56) as usize] << 56;
    found |= tables.column[column(own, 0) * 256 + column(opponent, 0)];
    found |= tables.column[column(own, 7) * 256 + column(opponent, 7)] << 7;

    let discs = own | opponent;
    let full_h = full_rows(discs);
    let full_v = full_columns(discs);
    let (full_d7, full_d9) = full_diagonals(discs);
    found |= full_h & full_v & full_d7 & full_d9;
    found &= own;
    while found & !stable != 0 {
        stable |= found;
        let h = (stable >> 1) | (stable << 1) | full_h;
        let v = (stable >> 8) | (stable << 8) | full_v;
        let d7 = (stable >> 7) | (stable << 7) | full_d7;
        let d9 = (stable >> 9) | (stable << 9) | full_d9;
        found = h & v & d7 & d9 & inner;
    }

    stable
}

/// Column `c` (0 = the column of bit 0) as 8 bits, bit k from row k. C#: `Column`.
#[inline]
fn column(x: u64, c: u32) -> usize {
    (((x >> c) & 0x0101_0101_0101_0101).wrapping_mul(0x0102_0408_1020_4080) >> 56) as usize
}

/// The line after the side owning `p` plays `place`, flipping `o`'s discs towards both ends of the line: (p, o).
/// C#: `PlayOnLine`.
fn play_on_line(p: i32, o: i32, place: i32) -> (i32, i32) {
    let mut np = p | (1 << place);
    let mut i = place - 1;
    while i > 0 && (o >> i) & 1 != 0 {
        i -= 1;
    }

    // C# shifts by i = -1 when place is 0 (the loop does not run); C# masks the count to 31, and the bit tested is
    // then bit 31 of p, which is 0.
    if (p >> (i & 31)) & 1 != 0 {
        for j in (i + 1..place).rev() {
            np ^= 1 << j;
        }
    }

    i = place + 1;
    while i < 7 && (o >> i) & 1 != 0 {
        i += 1;
    }

    if (p >> (i & 31)) & 1 != 0 {
        for j in place + 1..i {
            np ^= 1 << j;
        }
    }

    (np, o & !np)
}

/// C#: `LineStability`.
fn line_stability(b: i32, w: i32, memo: &mut [i16]) -> i32 {
    let known = memo[(b * 256 + w) as usize];
    if known >= 0 {
        return known as i32;
    }

    let mut result = b | w;
    let empties = !(b | w);
    for i in 0..8 {
        if (empties >> i) & 1 != 0 {
            let (nb, nw) = play_on_line(b, w, i);
            result &= b | nw;
            result &= line_stability(nb, nw, memo);
            let (nw, nb) = play_on_line(w, b, i);
            result &= w | nb;
            result &= line_stability(nb, nw, memo);
        }
    }

    memo[(b * 256 + w) as usize] = result as i16;
    result
}

#[inline]
fn full_rows(mut full: u64) -> u64 {
    full &= full >> 1;
    full &= full >> 2;
    full &= full >> 4;
    (full & 0x0101_0101_0101_0101).wrapping_mul(0xFF)
}

#[inline]
fn full_columns(mut full: u64) -> u64 {
    // C#: (full >> 8) | (full << 56) and so on.
    full &= full.rotate_right(8);
    full &= full.rotate_right(16);
    full &= full.rotate_right(32);
    full
}

#[inline]
fn full_diagonals(full: u64) -> (u64, u64) {
    const EDGE: u64 = 0xFF81_8181_8181_81FF;
    let (mut l7, mut r7) = (full, full);
    l7 &= EDGE | (l7 >> 7);
    r7 &= EDGE | (r7 << 7);
    l7 &= 0xFFFF_0303_0303_0303 | (l7 >> 14);
    r7 &= 0xC0C0_C0C0_C0C0_FFFF | (r7 << 14);
    l7 &= 0xFFFF_FFFF_0F0F_0F0F | (l7 >> 28);
    r7 &= 0xF0F0_F0F0_FFFF_FFFF | (r7 << 28);
    let full_d7 = l7 & r7;

    let (mut l9, mut r9) = (full, full);
    l9 &= EDGE | (l9 >> 9);
    r9 &= EDGE | (r9 << 9);
    l9 &= 0xFFFF_C0C0_C0C0_C0C0 | (l9 >> 18);
    r9 &= 0x0303_0303_0303_FFFF | (r9 << 18);
    let full_d9 = l9 & r9 & (0x0F0F_0F0F_F0F0_F0F0 | (l9 >> 36) | (r9 << 36));
    (full_d7, full_d9)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn corners_and_full_edges_are_stable() {
        // A corner disc is stable; a disc next to it on an otherwise empty edge is not.
        assert_eq!(of(1, 0), 1);
        assert_eq!(of(2, 0), 0);
        // A full first row of own discs is stable.
        assert_eq!(of(0xFF, 0), 0xFF);
        // The start position has no stable discs.
        assert_eq!(of(0x0000_0008_1000_0000, 0x0000_0010_0800_0000), 0);
        // A full board: every disc is stable.
        assert_eq!(of(0x5555_5555_5555_5555, 0xAAAA_AAAA_AAAA_AAAA), 0x5555_5555_5555_5555);
    }
}
