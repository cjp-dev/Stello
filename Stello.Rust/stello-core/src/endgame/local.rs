//! A small table for the null-window solver at 7 to 13 empty squares: one row of 1024 slots per number of empty
//! squares, each slot a position with a lower and an upper bound. A new result simply replaces the old one.
//!
//! C#: `Stello.Engine.V2.Endgame.LocalTable`. Adapted from Egaroucid src/engine/transposition_table.hpp (the local
//! table of endsearch_nws.hpp) (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana, GPL-3.0-or-later.

const SIZE_BITS: u32 = 10;
const SIZE: usize = 1 << SIZE_BITS;
const FIRST_EMPTIES: i32 = 6;
const ROWS: usize = (13 - FIRST_EMPTIES) as usize;

/// C#: `LocalTable.Entry`.
#[derive(Clone, Copy, Default, PartialEq, Eq, Debug)]
pub(crate) struct LocalEntry {
    pub own: u64,
    pub opponent: u64,
    pub lower: i32,
    pub upper: i32,
}

impl LocalEntry {
    /// C#: `Entry.Matches`.
    #[inline]
    pub fn matches(&self, own: u64, opponent: u64) -> bool {
        self.own == own && self.opponent == opponent
    }
}

/// C#: `LocalTable`. C# hands out a `ref` to the slot; here [`LocalTable::slot`] gives its index, which stays valid
/// while the search below overwrites the slot (C# then writes through the same `ref`).
pub(crate) struct LocalTable {
    entries: Vec<LocalEntry>,
}

impl Default for LocalTable {
    fn default() -> LocalTable {
        LocalTable { entries: vec![LocalEntry::default(); ROWS * SIZE] }
    }
}

impl LocalTable {
    /// C#: `Clear`.
    pub fn clear(&mut self) {
        self.entries.fill(LocalEntry::default());
    }

    /// The slot of a position with `empties` (6 to 12) empty squares. C#: `Get`.
    #[inline]
    pub fn slot(&self, own: u64, opponent: u64, empties: i32) -> usize {
        let hash = (own.wrapping_mul(0x9dda_1c54_cfe6_b6e9) ^ opponent.wrapping_mul(0xa2e6_c030_0831_e05a))
            >> (64 - SIZE_BITS);
        (empties - FIRST_EMPTIES) as usize * SIZE + hash as usize
    }

    #[inline]
    pub fn get(&self, slot: usize) -> LocalEntry {
        self.entries[slot]
    }

    /// C#: `Entry.Set`.
    #[inline]
    pub fn set(&mut self, slot: usize, own: u64, opponent: u64, lower: i32, upper: i32) {
        self.entries[slot] = LocalEntry { own, opponent, lower, upper };
    }
}
