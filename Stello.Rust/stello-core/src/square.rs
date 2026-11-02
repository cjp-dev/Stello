//! Squares and moves. C#: `Stello.Engine.Square` and `Stello.Engine.Move` (shared by all engines).

use std::fmt;

/// A board square. [`Square::index`] = row * 8 + column, so a1 = 0 and h8 = 63 (bit n of a bitboard).
///
/// C#: `Square`.
#[derive(Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord, Debug)]
pub struct Square(u8);

impl Square {
    /// C#: `new Square(index)`. Panics if the index is not 0–63.
    #[inline]
    pub const fn new(index: u8) -> Square {
        assert!(index < 64, "a square index is 0-63");
        Square(index)
    }

    /// C#: `Square.At(column, row)`, both 0-based.
    #[inline]
    pub const fn at(column: u8, row: u8) -> Square {
        assert!(column < 8 && row < 8, "column and row are 0-7");
        Square(row * 8 + column)
    }

    /// C#: `Square.Index`.
    #[inline]
    pub const fn index(self) -> u8 {
        self.0
    }

    /// C#: `Square.Column` (0 = a).
    #[inline]
    pub const fn column(self) -> u8 {
        self.0 & 7
    }

    /// C#: `Square.Row` (0 = row 1).
    #[inline]
    pub const fn row(self) -> u8 {
        self.0 >> 3
    }

    /// C#: `Square.Bit`.
    #[inline]
    pub const fn bit(self) -> u64 {
        1u64 << self.0
    }

    /// C#: `Square.ToLegacy`: Stello C++'s index 10 * row + column, both 1-based (a1 = 11, h8 = 88).
    pub const fn to_legacy(self) -> i32 {
        (self.row() as i32 + 1) * 10 + self.column() as i32 + 1
    }

    /// C#: `Square.FromLegacy`; `None` if the number is not a legacy square (11–88).
    pub fn from_legacy(legacy: i32) -> Option<Square> {
        let (row, column) = (legacy / 10 - 1, legacy % 10 - 1);
        (legacy >= 0 && (0..8).contains(&row) && (0..8).contains(&column)).then(|| Square::at(column as u8, row as u8))
    }

    /// C#: `Square.TryParse`: "a1"–"h8", the column letter in either case.
    pub fn parse(text: &str) -> Option<Square> {
        let bytes = text.as_bytes();
        if bytes.len() != 2 {
            return None;
        }

        let column = bytes[0].to_ascii_lowercase().wrapping_sub(b'a');
        let row = bytes[1].wrapping_sub(b'1');
        (column < 8 && row < 8).then(|| Square::at(column, row))
    }

    /// C#: `Square.InMask`: the squares of a bitboard, lowest index first.
    pub fn in_mask(mask: u64) -> impl Iterator<Item = Square> {
        let mut rest = mask;
        std::iter::from_fn(move || {
            if rest == 0 {
                return None;
            }

            let square = Square(rest.trailing_zeros() as u8);
            rest &= rest - 1;
            Some(square)
        })
    }
}

impl fmt::Display for Square {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}{}", (b'a' + self.column()) as char, self.row() + 1)
    }
}

/// A move in a game: a disc placed on a square, or a pass. C#: `Move` (`Square` is null for a pass).
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug, Default)]
pub struct Move(pub Option<Square>);

impl Move {
    /// C#: `Move.Pass`.
    pub const PASS: Move = Move(None);

    /// C#: `Move.IsPass`.
    #[inline]
    pub const fn is_pass(self) -> bool {
        self.0.is_none()
    }

    /// C#: `Move.TryParse`: a square or "pass" (any case).
    pub fn parse(text: &str) -> Option<Move> {
        if text.eq_ignore_ascii_case("pass") {
            Some(Move::PASS)
        } else {
            Square::parse(text).map(|square| Move(Some(square)))
        }
    }
}

impl From<Square> for Move {
    fn from(square: Square) -> Move {
        Move(Some(square))
    }
}

impl fmt::Display for Move {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.0 {
            Some(square) => square.fmt(f),
            None => f.write_str("pass"),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn squares_parse_and_print() {
        assert_eq!(Square::parse("a1"), Some(Square::new(0)));
        assert_eq!(Square::parse("H8"), Some(Square::new(63)));
        assert_eq!(Square::parse("i1"), None);
        assert_eq!(Square::parse("a9"), None);
        assert_eq!(Square::new(12).to_string(), "e2");
        assert_eq!(Square::new(63).to_legacy(), 88);
        assert_eq!(Square::from_legacy(11), Some(Square::new(0)));
        assert_eq!(Square::from_legacy(19), None);
        assert_eq!(Move::parse("PASS"), Some(Move::PASS));
        assert_eq!(Move::PASS.to_string(), "pass");
        let squares: Vec<u8> = Square::in_mask(0x8000_0000_0000_0011).map(Square::index).collect();
        assert_eq!(squares, [0, 4, 63]);
    }
}
