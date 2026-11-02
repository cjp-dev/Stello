//! Players and positions. C#: `Stello.Engine.Player` and `Stello.Engine.Board` (shared by all engines).

use std::fmt;

use crate::bits;
use crate::square::Square;

/// C#: `Player` (C++: DARK = Black, LIGHT = White). The discriminants are the C# enum values.
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
#[repr(u8)]
pub enum Player {
    Black = 0,
    White = 1,
}

impl Player {
    /// C#: `PlayerExtensions.Opponent`.
    #[inline]
    pub const fn opponent(self) -> Player {
        match self {
            Player::Black => Player::White,
            Player::White => Player::Black,
        }
    }

    /// The C# enum value (Black 0, White 1), as used in table indices such as the response table.
    #[inline]
    pub const fn index(self) -> usize {
        self as usize
    }
}

/// An Othello position stored as one bitboard per player (bit n = square index n). C#: `Board` (a readonly record
/// struct; equal when both bitboards are equal).
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct Board {
    black: u64,
    white: u64,
}

/// C#: `Board.Parse` failures (`FormatException`).
#[derive(Clone, PartialEq, Eq, Debug)]
pub enum BoardParseError {
    /// Not exactly 64 squares.
    WrongLength,
    /// A character other than X, O, '-' or '.' (or whitespace).
    InvalidCharacter(char),
}

impl fmt::Display for BoardParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            BoardParseError::WrongLength => f.write_str("A board must have exactly 64 squares."),
            BoardParseError::InvalidCharacter(c) => write!(f, "Invalid square character '{c}'."),
        }
    }
}

impl std::error::Error for BoardParseError {}

impl Board {
    /// C#: `Board.Initial` (C++ init_game(): d4 and e5 white, e4 and d5 black).
    pub const INITIAL: Board = Board { black: (1 << 28) | (1 << 35), white: (1 << 27) | (1 << 36) };

    /// C#: `new Board(black, white)`. Panics if a square holds both colours (C# throws `ArgumentException`).
    #[inline]
    pub const fn new(black: u64, white: u64) -> Board {
        assert!(black & white == 0, "A square cannot hold both a black and a white disc.");
        Board { black, white }
    }

    /// The position from the discs of the side to move (`own`) and its opponent.
    #[inline]
    pub const fn from_own(player: Player, own: u64, opponent: u64) -> Board {
        match player {
            Player::Black => Board::new(own, opponent),
            Player::White => Board::new(opponent, own),
        }
    }

    /// C#: `Board.Black`.
    #[inline]
    pub const fn black(&self) -> u64 {
        self.black
    }

    /// C#: `Board.White`.
    #[inline]
    pub const fn white(&self) -> u64 {
        self.white
    }

    /// C#: `Board.Empty`.
    #[inline]
    pub const fn empty(&self) -> u64 {
        !(self.black | self.white)
    }

    /// C#: `Board.EmptyCount`.
    #[inline]
    pub const fn empty_count(&self) -> u32 {
        self.empty().count_ones()
    }

    /// C#: `Board[square]`.
    pub const fn get(&self, square: Square) -> Option<Player> {
        if self.black & square.bit() != 0 {
            Some(Player::Black)
        } else if self.white & square.bit() != 0 {
            Some(Player::White)
        } else {
            None
        }
    }

    /// C#: `Board.Discs`.
    #[inline]
    pub const fn discs(&self, player: Player) -> u64 {
        match player {
            Player::Black => self.black,
            Player::White => self.white,
        }
    }

    /// C#: `Board.Count`.
    #[inline]
    pub const fn count(&self, player: Player) -> u32 {
        self.discs(player).count_ones()
    }

    /// C#: `Board.LegalMoves`.
    #[inline]
    pub fn legal_moves(&self, player: Player) -> u64 {
        bits::legal_moves(self.discs(player), self.discs(player.opponent()))
    }

    /// C#: `Board.HasLegalMove`.
    #[inline]
    pub fn has_legal_move(&self, player: Player) -> bool {
        self.legal_moves(player) != 0
    }

    /// C#: `Board.IsGameOver`.
    pub fn is_game_over(&self) -> bool {
        !self.has_legal_move(Player::Black) && !self.has_legal_move(Player::White)
    }

    /// C#: `Board.IsLegal`.
    pub fn is_legal(&self, player: Player, square: Square) -> bool {
        self.legal_moves(player) & square.bit() != 0
    }

    /// The discs that would be flipped if `player` plays on `square`; 0 if the move is illegal. C#: `Board.Flips`.
    #[inline]
    pub fn flips(&self, player: Player, square: Square) -> u64 {
        if self.empty() & square.bit() == 0 {
            0
        } else {
            bits::flips(self.discs(player), self.discs(player.opponent()), square.index() as u32)
        }
    }

    /// C#: `Board.Play`; `None` if the move is illegal (C# throws `InvalidOperationException`).
    pub fn play(&self, player: Player, square: Square) -> Option<Board> {
        let flips = self.flips(player, square);
        (flips != 0).then(|| self.play_flips(player, square.index() as u32, flips))
    }

    /// The position after `player` plays `square` with the given flips, unchecked: C#'s private
    /// `SearchEngine.Play(board, player, move)` with the flips computed by the caller.
    #[inline]
    pub const fn play_flips(&self, player: Player, square: u32, flips: u64) -> Board {
        let placed = flips | (1u64 << square);
        match player {
            Player::Black => Board { black: self.black | placed, white: self.white & !flips },
            Player::White => Board { black: self.black & !flips, white: self.white | placed },
        }
    }

    /// C#: `SearchEngine.Play(board, player, move)`: plays a move known to be legal.
    #[inline]
    pub fn play_unchecked(&self, player: Player, square: u32) -> Board {
        let flips = bits::flips(self.discs(player), self.discs(player.opponent()), square);
        self.play_flips(player, square, flips)
    }

    /// C#: `Board.Parse`: 64 squares in index order (a1, b1, ... h8). 'X' = black, 'O' = white, '-' or '.' = empty.
    /// Whitespace is ignored.
    pub fn parse(text: &str) -> Result<Board, BoardParseError> {
        let (mut black, mut white, mut index) = (0u64, 0u64, 0u32);
        for c in text.chars() {
            if c.is_whitespace() {
                continue;
            }

            if index == 64 {
                return Err(BoardParseError::WrongLength);
            }

            match c {
                'X' | 'x' => black |= 1 << index,
                'O' | 'o' => white |= 1 << index,
                '-' | '.' => {}
                _ => return Err(BoardParseError::InvalidCharacter(c)),
            }

            index += 1;
        }

        if index == 64 { Ok(Board { black, white }) } else { Err(BoardParseError::WrongLength) }
    }
}

/// C#: `Board.ToString`: eight lines, row 1 first, in the format read by [`Board::parse`].
impl fmt::Display for Board {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        for index in 0..64u8 {
            let c = match self.get(Square::new(index)) {
                Some(Player::Black) => 'X',
                Some(Player::White) => 'O',
                None => '-',
            };
            write!(f, "{c}")?;
            if index & 7 == 7 && index != 63 {
                writeln!(f)?;
            }
        }

        Ok(())
    }
}

/// The number of leaves `depth` moves from a position, as `Stello.Engine.Tests.PerftTests`: a pass counts as one
/// move, a finished game as one leaf.
pub fn perft(board: &Board, player: Player, depth: u32) -> u64 {
    if depth == 0 {
        return 1;
    }

    let moves = board.legal_moves(player);
    if moves == 0 {
        return if board.has_legal_move(player.opponent()) { perft(board, player.opponent(), depth - 1) } else { 1 };
    }

    let own = board.discs(player);
    let opponent = board.discs(player.opponent());
    let mut nodes = 0;
    let mut rest = moves;
    while rest != 0 {
        let square = rest.trailing_zeros();
        rest &= rest - 1;
        let flips = bits::flips(own, opponent, square);
        nodes += perft(&board.play_flips(player, square, flips), player.opponent(), depth - 1);
    }

    nodes
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn initial_position_round_trips() {
        let text = Board::INITIAL.to_string();
        assert_eq!(Board::parse(&text), Ok(Board::INITIAL));
        assert_eq!(Board::INITIAL.get(Square::parse("e4").unwrap()), Some(Player::Black));
        assert_eq!(Board::INITIAL.get(Square::parse("d4").unwrap()), Some(Player::White));
        assert_eq!(Board::INITIAL.legal_moves(Player::Black).count_ones(), 4);
        assert_eq!(Board::parse("X"), Err(BoardParseError::WrongLength));
    }

    #[test]
    fn play_flips_and_rejects_illegal_moves() {
        let board = Board::INITIAL;
        let d3 = Square::parse("d3").unwrap();
        let next = board.play(Player::Black, d3).unwrap();
        assert_eq!(next.count(Player::Black), 4);
        assert_eq!(next.count(Player::White), 1);
        assert_eq!(board.play(Player::Black, Square::parse("a1").unwrap()), None);
        assert_eq!(next.play(Player::White, d3), None);
    }
}
