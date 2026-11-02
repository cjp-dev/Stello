//! C#: `Search/TranspositionTable.cs` (`Bound`, `TranspositionTable`, `TranspositionTable.Entry`).

/// C#: `Bound`. The discriminants are the C# enum values.
#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
#[repr(u8)]
pub enum Bound {
    #[default]
    None = 0,
    Exact = 1,
    /// C++: HI_HASH (score >= beta).
    Lower = 2,
    /// C++: LO_HASH (score <= alpha).
    Upper = 3,
}

/// C#: `TranspositionTable.Entry`: the full position, so there are no false hits.
#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
pub struct Entry {
    pub own: u64,
    pub opponent: u64,
    pub value: i16,
    pub depth: i8,
    pub bound: Bound,
    /// The best move's square, or -1.
    pub mv: i8,
    pub tag: u8,
    /// The [`TranspositionTable::clear`] count when it was stored (not in C#; it fills the entry's padding).
    generation: u16,
}

impl Entry {
    /// Whether the entry is in use: stored since the last [`TranspositionTable::clear`]. An entry from before is
    /// empty, as all of C#'s entries are after its `Clear`.
    #[inline]
    fn is_live(&self, generation: u16) -> bool {
        self.bound != Bound::None && self.generation == generation
    }

    /// C#: `Entry.Matches`.
    #[inline]
    fn matches(&self, own: u64, opponent: u64, tag: i32, generation: u16) -> bool {
        self.is_live(generation) && self.own == own && self.opponent == opponent && self.tag as i32 == tag
    }
}

/// Hash table keyed on the full position. Each slot holds two entries: one keeps the deepest result, the other is
/// always replaced (C++ f_hash_put: one entry; a deeper entry for the same position is kept).
/// C#: `TranspositionTable`.
///
/// [`TranspositionTable::clear`] does not touch the entries: it starts a new generation, and entries of older ones
/// count as empty. Only every 65,535th clear writes the whole table.
pub struct TranspositionTable {
    entries: Vec<Entry>,
    shift: u32,
    /// Never 0, the generation of a never-written entry.
    generation: u16,
}

impl TranspositionTable {
    /// The table has 2^bits slots of two entries.
    pub fn new(bits: u32) -> TranspositionTable {
        TranspositionTable { entries: vec![Entry::default(); 2 << bits], shift: 64 - bits, generation: 1 }
    }

    /// The entry of a position, if it is stored. `tag` is anything else the value depends on, such as the side to
    /// move. C#: `TryGet`.
    #[inline]
    pub fn get(&self, own: u64, opponent: u64, tag: i32) -> Option<Entry> {
        let slot = self.slot(own, opponent, tag);
        let first = self.entries[slot];
        if first.matches(own, opponent, tag, self.generation) {
            return Some(first);
        }

        let second = self.entries[slot + 1];
        second.matches(own, opponent, tag, self.generation).then_some(second)
    }

    /// C#: `Store` (the value, depth and move are narrowed as C#'s casts do).
    #[inline]
    #[allow(clippy::too_many_arguments)]
    pub fn store(&mut self, own: u64, opponent: u64, tag: i32, depth: i32, bound: Bound, value: i32, mv: i32) {
        let slot = self.slot(own, opponent, tag);
        let generation = self.generation;
        let entry = Entry {
            own,
            opponent,
            value: value as i16,
            depth: depth as i8,
            bound,
            mv: mv as i8,
            tag: tag as u8,
            generation,
        };
        let deep = self.entries[slot];

        if deep.matches(own, opponent, tag, generation) {
            if depth >= deep.depth as i32 {
                self.entries[slot] = entry;
            }
        } else if !deep.is_live(generation) || depth >= deep.depth as i32 {
            // An empty first entry moves to the second place as C#'s does: it is still empty there.
            self.entries[slot + 1] = deep;
            self.entries[slot] = entry;
        } else {
            self.entries[slot + 1] = entry;
        }
    }

    /// Asks the processor to load the slot of a position into the cache, for a [`TranspositionTable::store`] after
    /// a search below it. Changes nothing else.
    #[inline]
    pub fn prefetch(&self, own: u64, opponent: u64, tag: i32) {
        #[cfg(target_arch = "x86_64")]
        {
            let slot = self.slot(own, opponent, tag);
            // SAFETY: the slot and the next entry are in the table; a prefetch has no other effect.
            unsafe {
                let entry = self.entries.as_ptr().add(slot);
                std::arch::x86_64::_mm_prefetch::<{ std::arch::x86_64::_MM_HINT_T0 }>(entry.cast());
            }
        }
        #[cfg(not(target_arch = "x86_64"))]
        let _ = (own, opponent, tag);
    }

    /// C#: `Clear`: afterwards every entry counts as empty.
    pub fn clear(&mut self) {
        self.generation = self.generation.wrapping_add(1);
        if self.generation == 0 {
            // SAFETY: an entry of zero bytes is `Entry::default()` (Bound::None is 0, generation 0), so this is
            // `fill(Entry::default())` as one memset.
            unsafe { std::ptr::write_bytes(self.entries.as_mut_ptr(), 0, self.entries.len()) };
            self.generation = 1;
        }
    }

    #[inline]
    fn slot(&self, own: u64, opponent: u64, tag: i32) -> usize {
        let mut hash = own.wrapping_mul(0x9E37_79B9_7F4A_7C15);
        hash ^= opponent.wrapping_mul(0xC2B2_AE3D_27D4_EB4F).rotate_left(31).wrapping_add(tag as u64);
        hash = hash.wrapping_mul(0xD6E8_FEB8_6659_FD93);
        (hash >> self.shift) as usize * 2
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn keeps_the_deeper_entry_and_replaces_the_other() {
        let mut table = TranspositionTable::new(10);
        table.store(1, 2, 0, 5, Bound::Exact, 100, 7);
        assert_eq!(table.get(1, 2, 0).map(|e| (e.value, e.depth, e.mv)), Some((100, 5, 7)));
        assert_eq!(table.get(1, 2, 1), None);

        // A shallower result for the same position does not replace it.
        table.store(1, 2, 0, 3, Bound::Lower, 50, 8);
        assert_eq!(table.get(1, 2, 0).map(|e| e.value), Some(100));

        table.clear();
        assert_eq!(table.get(1, 2, 0), None);
    }

    #[test]
    fn entries_from_before_a_clear_are_empty_also_for_replacement() {
        let mut table = TranspositionTable::new(10);
        table.store(1, 2, 0, 9, Bound::Exact, 100, 7);
        table.clear();
        // A shallower result replaces the old deep entry, as in an empty slot.
        table.store(1, 2, 0, 3, Bound::Lower, 50, 8);
        assert_eq!(table.get(1, 2, 0).map(|e| (e.value, e.depth, e.mv)), Some((50, 3, 8)));

        // Over 65,535 clears the generation wraps, and the table is written once.
        for _ in 0..70_000 {
            table.clear();
            assert_eq!(table.get(1, 2, 0), None);
            table.store(1, 2, 0, 3, Bound::Lower, 50, 8);
        }
        assert!(table.get(1, 2, 0).is_some());
        assert_eq!(std::mem::size_of::<Entry>(), 24);
    }
}
