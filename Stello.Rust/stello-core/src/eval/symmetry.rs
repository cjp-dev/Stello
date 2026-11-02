//! The 8 symmetries of the board on bitboards (bit n = square n, a1 = 0, h1 = 7, a8 = 56). Symmetry number s mirrors
//! the columns if bit 0 is set, then the rows if bit 1 is set, then transposes (a2 ↔ b1) if bit 2 is set.
//!
//! C#: `Stello.Engine.V2.Evaluation.Symmetry`.

/// C#: `Symmetry.Count`.
pub const COUNT: usize = 8;

/// C#: `Symmetry.Apply`.
#[inline]
pub fn apply(mut x: u64, symmetry: usize) -> u64 {
    if symmetry & 1 != 0 {
        x = mirror_columns(x);
    }

    if symmetry & 2 != 0 {
        x = x.swap_bytes();
    }

    if symmetry & 4 != 0 {
        x = transpose(x);
    }

    x
}

/// The square that `square` moves to under `symmetry`. C#: `Symmetry.MapSquare`.
pub fn map_square(square: u32, symmetry: usize) -> u32 {
    apply(1u64 << square, symmetry).trailing_zeros()
}

/// a1 ↔ h1: reverses the bits of every byte. C#: `Symmetry.MirrorColumns`.
#[inline]
pub fn mirror_columns(mut x: u64) -> u64 {
    x = ((x >> 1) & 0x5555_5555_5555_5555) | ((x & 0x5555_5555_5555_5555) << 1);
    x = ((x >> 2) & 0x3333_3333_3333_3333) | ((x & 0x3333_3333_3333_3333) << 2);
    ((x >> 4) & 0x0F0F_0F0F_0F0F_0F0F) | ((x & 0x0F0F_0F0F_0F0F_0F0F) << 4)
}

/// Mirrors in the a1–h8 diagonal: the square at (column c, row r) moves to (column r, row c). C#: `Symmetry.Transpose`.
#[inline]
pub fn transpose(mut x: u64) -> u64 {
    let mut t = 0x0F0F_0F0F_0000_0000 & (x ^ (x << 28));
    x ^= t ^ (t >> 28);
    t = 0x3333_0000_3333_0000 & (x ^ (x << 14));
    x ^= t ^ (t >> 14);
    t = 0x5500_5500_5500_5500 & (x ^ (x << 7));
    x ^ t ^ (t >> 7)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn symmetries_move_squares_as_documented() {
        // b1 (1): mirrored columns g1 (6), mirrored rows b8 (57), transposed a2 (8).
        assert_eq!(map_square(1, 1), 6);
        assert_eq!(map_square(1, 2), 57);
        assert_eq!(map_square(1, 4), 8);
        for symmetry in 0..COUNT {
            let mut seen = 0u64;
            for square in 0..64 {
                seen |= 1u64 << map_square(square, symmetry);
            }
            assert_eq!(seen, u64::MAX, "symmetry {symmetry} is a permutation");
        }
    }
}
