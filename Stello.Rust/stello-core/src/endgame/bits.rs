//! Bit tricks and small tables for the endgame solver; the midgame search uses some of them too (weighted mobility
//! and potential mobility in its move ordering).
//!
//! C#: `Stello.Engine.V2.Endgame.EndgameBits`. Adapted from Egaroucid src/engine/search.hpp, endsearch_common.hpp,
//! move_ordering.hpp, flip_generic.hpp and mobility_generic.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto
//! Yamana, GPL-3.0-or-later, through the C# port in Stello.Net/Egaroucid.Engine. Egaroucid numbers the squares the
//! other way round (bit 63 is a1); reversing the bits turns the board half a turn, and every table here is the same
//! after that turn, so they are used as they are.

/// A value no legal-move set can have (the four centre squares), for "not computed yet". C#: `LegalUndefined`.
pub const LEGAL_UNDEFINED: u64 = 0x0000_0018_1800_0000;

/// The neighbours of each square through which a disc can be flipped: the 3 × 3 neighbourhood without the neighbours
/// in directions too short to flip anything (Egaroucid `bit_around`). C#: `FlipNeighbours`.
#[rustfmt::skip]
pub static FLIP_NEIGHBOURS: [u64; 64] = [
    0x0000000000000302, 0x0000000000000604, 0x0000000000000e0a, 0x0000000000001c14,
    0x0000000000003828, 0x0000000000007050, 0x0000000000006020, 0x000000000000c040,
    0x0000000000030200, 0x0000000000060400, 0x00000000000e0a00, 0x00000000001c1400,
    0x0000000000382800, 0x0000000000705000, 0x0000000000602000, 0x0000000000c04000,
    0x0000000003020300, 0x0000000006040600, 0x000000000e0a0e00, 0x000000001c141c00,
    0x0000000038283800, 0x0000000070507000, 0x0000000060206000, 0x00000000c040c000,
    0x0000000302030000, 0x0000000604060000, 0x0000000e0a0e0000, 0x0000001c141c0000,
    0x0000003828380000, 0x0000007050700000, 0x0000006020600000, 0x000000c040c00000,
    0x0000030203000000, 0x0000060406000000, 0x00000e0a0e000000, 0x00001c141c000000,
    0x0000382838000000, 0x0000705070000000, 0x0000602060000000, 0x0000c040c0000000,
    0x0003020300000000, 0x0006040600000000, 0x000e0a0e00000000, 0x001c141c00000000,
    0x0038283800000000, 0x0070507000000000, 0x0060206000000000, 0x00c040c000000000,
    0x0002030000000000, 0x0004060000000000, 0x000a0e0000000000, 0x00141c0000000000,
    0x0028380000000000, 0x0050700000000000, 0x0020600000000000, 0x0040c00000000000,
    0x0203000000000000, 0x0406000000000000, 0x0a0e000000000000, 0x141c000000000000,
    0x2838000000000000, 0x5070000000000000, 0x2060000000000000, 0x40c0000000000000,
];

/// The quadrant of each square as one bit, for the parity of the empty squares per quadrant. C#: `QuadrantBit`.
#[rustfmt::skip]
pub static QUADRANT_BIT: [i32; 64] = [
    1, 1, 1, 1, 2, 2, 2, 2,
    1, 1, 1, 1, 2, 2, 2, 2,
    1, 1, 1, 1, 2, 2, 2, 2,
    1, 1, 1, 1, 2, 2, 2, 2,
    4, 4, 4, 4, 8, 8, 8, 8,
    4, 4, 4, 4, 8, 8, 8, 8,
    4, 4, 4, 4, 8, 8, 8, 8,
    4, 4, 4, 4, 8, 8, 8, 8,
];

/// By number of discs: a null-window search at alpha this high or higher tries the stable-disc cutoff, because below
/// it the opponent's stable discs can rarely prove a fail low. C#: `StabilityThresholdNws`.
#[rustfmt::skip]
pub static STABILITY_THRESHOLD_NWS: [i32; 65] = [
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 64, 64, 64, 64, 64, 64, 64,
    64, 62, 62, 60, 60, 58, 58, 56,
    56, 54, 54, 52, 52, 50, 50, 48,
    48, 46, 44, 42, 40, 38, 36, 34,
    32, 30, 28, 26, 24, 22, 20, 16,
    14, 12, 10, 8, 6, 99, 99, 99,
    99,
];

/// The same for a search with a window: the test is on beta. C#: `StabilityThreshold`.
#[rustfmt::skip]
pub static STABILITY_THRESHOLD: [i32; 65] = [
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 62, 62, 60, 60, 58, 58, 56,
    56, 54, 54, 52, 52, 50, 50, 48,
    48, 46, 46, 44, 44, 42, 42, 40,
    40, 38, 36, 34, 32, 30, 28, 26,
    24, 22, 20, 18, 16, 14, 12, 8,
    6, 4, 2, 0, -2, 99, 99, 99,
    99,
];

/// The 8 neighbours of each square (fewer at the edges). C#: `AllNeighbours` (`CreateAllNeighbours`).
static ALL_NEIGHBOURS: [u64; 64] = create_all_neighbours();

/// For each square and direction, the squares from the next one to the edge. Directions 0..3 go towards higher bit
/// numbers, 4..7 towards lower ones. C#: `Rays` (`CreateRays`).
static RAYS: [[u64; 8]; 64] = create_rays();

/// The discs `own` flips by playing `square` (0 if the move is illegal; the square is not checked for being empty).
/// C#: `EndgameBits.Flips`.
#[inline]
pub fn flips(own: u64, opponent: u64, square: u32) -> u64 {
    let mut flipped = 0;
    let not_opponent = !opponent;
    let rays = &RAYS[square as usize];
    for &ray in &rays[..4] {
        let mut outflank = ray & not_opponent;
        outflank &= outflank.wrapping_neg();
        if outflank & own != 0 {
            flipped |= outflank.wrapping_sub(1) & ray;
        }
    }

    for &ray in &rays[4..] {
        // The highest square of the ray that is not the opponent's, without branches: if there is none, `| 1` makes
        // `top` square a1, and then `top & ray & own` is 0 (a1 is on the ray only as an opponent's disc).
        let top = 0x8000_0000_0000_0000u64 >> (ray & not_opponent | 1).leading_zeros();
        let outflanked = 0u64.wrapping_sub((top & ray & own != 0) as u64);
        flipped |= ray & (top.wrapping_neg() << 1) & outflanked;
    }

    flipped
}

/// The legal moves of `p` against `o` (Egaroucid `calc_legal`). C#: `EndgameBits.LegalMoves`; the same result as
/// [`crate::bits::legal_moves`].
#[inline]
pub fn legal_moves(p: u64, o: u64) -> u64 {
    let m_o = o & 0x7e7e_7e7e_7e7e_7e7e;
    let (mut flip1, mut flip7, mut flip9, mut flip8) = (m_o & (p << 1), m_o & (p << 7), m_o & (p << 9), o & (p << 8));
    flip1 |= m_o & (flip1 << 1);
    flip7 |= m_o & (flip7 << 7);
    flip9 |= m_o & (flip9 << 9);
    flip8 |= o & (flip8 << 8);
    let (mut pre1, mut pre7, mut pre9, mut pre8) = (m_o & (m_o << 1), m_o & (m_o << 7), m_o & (m_o << 9), o & (o << 8));
    flip1 |= pre1 & (flip1 << 2);
    flip7 |= pre7 & (flip7 << 14);
    flip9 |= pre9 & (flip9 << 18);
    flip8 |= pre8 & (flip8 << 16);
    flip1 |= pre1 & (flip1 << 2);
    flip7 |= pre7 & (flip7 << 14);
    flip9 |= pre9 & (flip9 << 18);
    flip8 |= pre8 & (flip8 << 16);
    let mut moves = (flip1 << 1) | (flip7 << 7) | (flip9 << 9) | (flip8 << 8);
    flip1 = m_o & (p >> 1);
    flip7 = m_o & (p >> 7);
    flip9 = m_o & (p >> 9);
    flip8 = o & (p >> 8);
    flip1 |= m_o & (flip1 >> 1);
    flip7 |= m_o & (flip7 >> 7);
    flip9 |= m_o & (flip9 >> 9);
    flip8 |= o & (flip8 >> 8);
    pre1 >>= 1;
    pre7 >>= 7;
    pre9 >>= 9;
    pre8 >>= 8;
    flip1 |= pre1 & (flip1 >> 2);
    flip7 |= pre7 & (flip7 >> 14);
    flip9 |= pre9 & (flip9 >> 18);
    flip8 |= pre8 & (flip8 >> 16);
    flip1 |= pre1 & (flip1 >> 2);
    flip7 |= pre7 & (flip7 >> 14);
    flip9 |= pre9 & (flip9 >> 18);
    flip8 |= pre8 & (flip8 >> 16);
    moves |= (flip1 >> 1) | (flip7 >> 7) | (flip9 >> 9) | (flip8 >> 8);
    moves & !(p | o)
}

/// The empty squares with no empty neighbour (Egaroucid `empty1_bb`). C#: `EndgameBits.LoneEmpties`.
#[inline]
pub fn lone_empties(own: u64, opponent: u64) -> u64 {
    let empty = !(own | opponent);
    let mut t0 = (empty & 0x7f7f_7f7f_7f7f_7f7f) << 1;
    let mut t1 = (empty & 0xfefe_fefe_fefe_fefe) >> 1;
    let e1 = t0 | t1;
    t0 = empty | e1;
    t1 = t0 << 8;
    t0 >>= 8;
    !(e1 | t0 | t1) & empty
}

/// Whether no square next to `square` is in `empties`. C#: `EndgameBits.IsLoneEmpty`.
#[inline]
pub fn is_lone_empty(square: u32, empties: u64) -> bool {
    ALL_NEIGHBOURS[square as usize] & empties == 0
}

/// The parity of the empty squares in each quadrant, one bit per quadrant as in [`QUADRANT_BIT`].
/// C#: `EndgameBits.Parity`.
#[inline]
pub fn parity(empty: u64) -> i32 {
    ((empty & 0x0000_0000_0F0F_0F0F).count_ones() & 1) as i32
        | ((((empty & 0x0000_0000_F0F0_F0F0).count_ones() & 1) as i32) << 1)
        | ((((empty & 0x0F0F_0F0F_0000_0000).count_ones() & 1) as i32) << 2)
        | ((((empty & 0xF0F0_F0F0_0000_0000).count_ones() & 1) as i32) << 3)
}

/// The number of moves with corners counted twice (Egaroucid `get_n_moves_cornerX2`).
/// C#: `EndgameBits.MovesCornersTwice`.
#[inline]
pub fn moves_corners_twice(legal: u64) -> i32 {
    (legal.count_ones() + (legal & 0x8100_0000_0000_0081).count_ones()) as i32
}

/// Twice the number of moves plus the corner moves (Egaroucid `get_weighted_n_moves`). C#: `EndgameBits.WeightedMoves`.
#[inline]
pub fn weighted_moves(legal: u64) -> i32 {
    (legal.count_ones() * 2 + (legal & 0x8100_0000_0000_0081).count_ones()) as i32
}

/// The empty squares next to `discs` (Egaroucid `get_potential_mobility`). C#: `EndgameBits.PotentialMobility`.
#[inline]
pub fn potential_mobility(discs: u64, empties: u64) -> i32 {
    let h = discs & 0x7E7E_7E7E_7E7E_7E7E;
    let v = discs & 0x00FF_FFFF_FFFF_FF00;
    let hv = discs & 0x007E_7E7E_7E7E_7E00;
    let around = (h << 1) | (h >> 1) | (v << 8) | (v >> 8) | (hv << 7) | (hv >> 7) | (hv << 9) | (hv >> 9);
    (empties & around).count_ones() as i32
}

/// The final score with `empties` empty squares for the side owning `own`, the empty squares going to the winner
/// (Egaroucid `end_evaluate(board, e)`). C#: `EndgameBits.ScoreWithEmpties`.
#[inline]
pub fn score_with_empties(own: u64, empties: i32) -> i32 {
    let mut score = own.count_ones() as i32 * 2 + empties;
    score += (((score >> 6) & 1) + (((score + 63) >> 7) & 1) - 1) * empties;
    score - 64
}

/// The same for an odd number of empty squares, where there is no draw (`end_evaluate_odd`).
/// C#: `EndgameBits.ScoreWithOddEmpties`.
#[inline]
pub fn score_with_odd_empties(own: u64, empties: i32) -> i32 {
    let mut score = own.count_ones() as i32 * 2 + empties;
    score += (((score >> 5) & 2) - 1) * empties;
    score - 64
}

const fn create_all_neighbours() -> [u64; 64] {
    let mut neighbours = [0u64; 64];
    let mut square = 0;
    while square < 64 {
        let (row, column) = ((square >> 3) as i32, (square & 7) as i32);
        let mut r = row - 1;
        while r <= row + 1 {
            let mut c = column - 1;
            while c <= column + 1 {
                if r >= 0 && r < 8 && c >= 0 && c < 8 && (r != row || c != column) {
                    neighbours[square] |= 1u64 << (r * 8 + c);
                }
                c += 1;
            }
            r += 1;
        }
        square += 1;
    }

    neighbours
}

const fn create_rays() -> [[u64; 8]; 64] {
    const STEPS: [(i32, i32); 8] = [(0, 1), (1, 0), (1, -1), (1, 1), (0, -1), (-1, 0), (-1, 1), (-1, -1)];
    let mut rays = [[0u64; 8]; 64];
    let mut square = 0;
    while square < 64 {
        let (row, column) = ((square / 8) as i32, (square % 8) as i32);
        let mut d = 0;
        while d < 8 {
            let mut ray = 0u64;
            let (mut r, mut c) = (row + STEPS[d].0, column + STEPS[d].1);
            while r >= 0 && r < 8 && c >= 0 && c < 8 {
                ray |= 1u64 << (r * 8 + c);
                r += STEPS[d].0;
                c += STEPS[d].1;
            }
            rays[square][d] = ray;
            d += 1;
        }
        square += 1;
    }

    rays
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn scores_with_empties_give_the_empties_to_the_winner() {
        // 40 own discs, 20 opponent's, 4 empty: 44 - 20 = +24.
        let own = (1u64 << 40) - 1;
        assert_eq!(score_with_empties(own, 4), 24);
        assert_eq!(score_with_odd_empties((1u64 << 30) - 1, 3), -4);
        // A draw: 30 + 30 discs and 4 empty squares.
        assert_eq!(score_with_empties((1u64 << 30) - 1, 4), 0);
    }

    #[test]
    fn neighbours_and_rays_are_on_the_board() {
        assert_eq!(ALL_NEIGHBOURS[0], 0x302);
        assert_eq!(ALL_NEIGHBOURS[9], 0x70507);
        assert_eq!(RAYS[0][0], 0xFE);
        assert_eq!(RAYS[0][4], 0);
        assert_eq!(RAYS[63][7], 0x0040_2010_0804_0201);
    }
}
