//! Move ordering from Sort.cpp: hash move, response killers (C++: humres/comres) and square values.
//! C#: `Search/MoveOrdering.cs`.

use crate::board::{Board, Player};

// C++: scores[100]; the squares next to the corners are recomputed for every position.
#[rustfmt::skip]
const BASE_SQUARE_SCORES: [i8; 64] = [
    127, -32, 22, 11, 11, 22, -32, 127,
    -32, -64, -8, -5, -5, -8, -64, -32,
    22, -8, 12, 1, 1, 12, -8, 22,
    11, -5, 1, 0, 0, 1, -5, 11,
    11, -5, 1, 0, 0, 1, -5, 11,
    22, -8, 12, 1, 1, 12, -8, 22,
    -32, -64, -8, -5, -5, -8, -64, -32,
    127, -32, 22, 11, 11, 22, -32, 127,
];

/// Corner, the two C-squares with the square beyond each, and the X-square.
struct CornerArea {
    corner: usize,
    c1: usize,
    beyond1: usize,
    c2: usize,
    beyond2: usize,
    x: usize,
}

const CORNER_AREAS: [CornerArea; 4] = [
    CornerArea { corner: 0, c1: 1, beyond1: 2, c2: 8, beyond2: 16, x: 9 },
    CornerArea { corner: 7, c1: 6, beyond1: 5, c2: 15, beyond2: 23, x: 14 },
    CornerArea { corner: 56, c1: 48, beyond1: 40, c2: 57, beyond2: 58, x: 49 },
    CornerArea { corner: 63, c1: 62, beyond1: 61, c2: 55, beyond2: 47, x: 54 },
];

/// C#: `MoveOrdering`.
pub struct MoveOrdering {
    /// `[replier * 4096 + previous move * 64 + reply]`.
    responses: Box<[i32; 2 * 64 * 64]>,
}

impl Default for MoveOrdering {
    fn default() -> MoveOrdering {
        MoveOrdering { responses: Box::new([0; 2 * 64 * 64]) }
    }
}

impl MoveOrdering {
    /// C#: `ClearResponses`.
    pub fn clear_responses(&mut self) {
        self.responses.fill(0);
    }

    /// A refutation (the reply caused a cutoff) counts 4, other best replies 1. C#: `RecordResponse` (whose `int`
    /// wraps on overflow, as `wrapping_add` does).
    #[inline]
    pub fn record_response(&mut self, replier: Player, mv: i32, reply: i32, refuted: bool) {
        if reply >= 0 {
            let cell = &mut self.responses[response_row(replier, mv) + reply as usize];
            *cell = cell.wrapping_add(if refuted { 4 } else { 1 });
        }
    }

    /// C++: sortlist. Hash move first, then moves with response values, then the rest by square value.
    /// C#: `Order`.
    pub fn order(&self, moves: &mut [i32], hash_move: i32, previous_move: i32, player: Player, board: &Board) {
        let mut start = 0;
        if hash_move >= 0
            && let Some(index) = moves.iter().position(|&m| m == hash_move)
        {
            moves.swap(0, index);
            start = 1;
        }

        let mut keys = [0i32; 64];
        if previous_move >= 0 {
            let row = response_row(player, previous_move);
            let rest = &mut moves[start..];
            for (key, &mv) in keys.iter_mut().zip(rest.iter()) {
                *key = self.responses[row + mv as usize];
            }

            let count = rest.len();
            sort_descending(rest, &mut keys[..count]);

            let mut i = 0;
            while moves.len() - start >= 2 && keys[i] != 0 {
                start += 1;
                i += 1;
            }
        }

        if moves.len() - start <= 1 {
            return;
        }

        let scores = square_scores(board, player);
        let remaining = &mut moves[start..];
        for (key, &mv) in keys.iter_mut().zip(remaining.iter()) {
            *key = scores[mv as usize] as i32;
        }

        let count = remaining.len();
        sort_descending(remaining, &mut keys[..count]);
    }
}

/// C#: `MoveOrdering.SquareScores`.
pub fn square_scores(board: &Board, player: Player) -> [i8; 64] {
    let mut scores = BASE_SQUARE_SCORES;
    let own = board.discs(player);
    let opponent = board.discs(player.opponent());

    for area in &CORNER_AREAS {
        if own & (1 << area.corner) != 0 {
            scores[area.c1] = 34;
            scores[area.c2] = 34;
            scores[area.x] = 24;
        } else if opponent & (1 << area.corner) != 0 {
            scores[area.c1] = if opponent & (1 << area.beyond1) != 0 { 32 } else { -32 };
            scores[area.c2] = if opponent & (1 << area.beyond2) != 0 { 32 } else { -32 };
            scores[area.x] = -16;
        } else {
            scores[area.c1] = -16;
            scores[area.c2] = -16;
            scores[area.x] = -64;
        }
    }

    scores
}

/// Stable insertion sort, highest key first (as the C++ linear sort). C#: `MoveOrdering.SortDescending`.
#[inline]
pub fn sort_descending(moves: &mut [i32], keys: &mut [i32]) {
    for i in 1..moves.len() {
        let mv = moves[i];
        let key = keys[i];
        let mut j = i;
        while j > 0 && keys[j - 1] < key {
            moves[j] = moves[j - 1];
            keys[j] = keys[j - 1];
            j -= 1;
        }

        moves[j] = mv;
        keys[j] = key;
    }
}

#[inline]
fn response_row(replier: Player, previous_move: i32) -> usize {
    (replier.index() * 64 + previous_move as usize) * 64
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sort_is_stable_and_descending() {
        let mut moves = [1, 2, 3, 4, 5];
        let mut keys = [0, 5, 0, 5, 7];
        sort_descending(&mut moves, &mut keys);
        assert_eq!(moves, [5, 2, 4, 1, 3]);
        assert_eq!(keys, [7, 5, 5, 0, 0]);
    }

    #[test]
    fn hash_move_then_responses_then_square_values() {
        let mut ordering = MoveOrdering::default();
        let board = Board::INITIAL;
        // White replies to d3 (19): c5 (34) once, e3 (20) refuted.
        ordering.record_response(Player::White, 19, 34, false);
        ordering.record_response(Player::White, 19, 20, true);
        let mut moves = [18, 20, 34, 44];
        ordering.order(&mut moves, 44, 19, Player::White, &board);
        assert_eq!(moves, [44, 20, 34, 18]);
    }
}
