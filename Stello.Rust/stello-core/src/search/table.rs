//! C#: `Search/TranspositionTable.cs` (`Bound`, `TranspositionTable`, `TranspositionTable.Entry`).
//!
//! The table is shared by all threads of a search (Plans/making-stello-3-plan.md, phase 5), so it is read and
//! written through `&self`. Each slot is guarded by a sequence lock: a writer takes the slot's lock, and a reader
//! checks that the slot's sequence did not change while it read the entry. A read that overlaps a write is a miss,
//! so a torn entry is never taken for a valid one. With one thread the lock is never contended, and the table
//! behaves exactly as C#'s: the same hits, the same replacements.

use std::sync::atomic::{AtomicU64, Ordering, fence};

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

impl Bound {
    #[inline]
    fn from_bits(bits: u64) -> Bound {
        match bits & 0xff {
            1 => Bound::Exact,
            2 => Bound::Lower,
            3 => Bound::Upper,
            _ => Bound::None,
        }
    }
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

// The entry's fields other than the position, packed into one word: value, depth, bound, move, tag, generation
// from the low bits up. A zero word is an empty entry (Bound::None, generation 0).
const DEPTH_SHIFT: u32 = 16;
const BOUND_SHIFT: u32 = 24;
const MOVE_SHIFT: u32 = 32;
const TAG_SHIFT: u32 = 40;
const GENERATION_SHIFT: u32 = 48;

impl Entry {
    #[inline]
    fn pack(&self) -> u64 {
        (self.value as u16 as u64)
            | (self.depth as u8 as u64) << DEPTH_SHIFT
            | (self.bound as u8 as u64) << BOUND_SHIFT
            | (self.mv as u8 as u64) << MOVE_SHIFT
            | (self.tag as u64) << TAG_SHIFT
            | (self.generation as u64) << GENERATION_SHIFT
    }

    #[inline]
    fn unpack(own: u64, opponent: u64, data: u64) -> Entry {
        Entry {
            own,
            opponent,
            value: data as u16 as i16,
            depth: (data >> DEPTH_SHIFT) as u8 as i8,
            bound: Bound::from_bits(data >> BOUND_SHIFT),
            mv: (data >> MOVE_SHIFT) as u8 as i8,
            tag: (data >> TAG_SHIFT) as u8,
            generation: (data >> GENERATION_SHIFT) as u16,
        }
    }

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

/// The sequence's lowest bit: a writer holds the slot.
const LOCKED: u64 = 1;

/// Two entries (own, opponent and the packed word each) and the sequence that guards them: one cache line.
#[repr(C, align(64))]
#[derive(Default)]
struct Slot {
    /// Even while the slot is free; a writer makes it odd, and adds 2 in all when it is done.
    sequence: AtomicU64,
    words: [AtomicU64; 6],
}

const _: () = assert!(size_of::<Slot>() == 64);

impl Slot {
    /// Entry `i` (0 or 1), read without the lock.
    #[inline]
    fn load(&self, i: usize) -> Entry {
        let words = &self.words[3 * i..3 * i + 3];
        Entry::unpack(
            words[0].load(Ordering::Relaxed),
            words[1].load(Ordering::Relaxed),
            words[2].load(Ordering::Relaxed),
        )
    }

    /// Writes entry `i`; only while the lock is held.
    #[inline]
    fn put(&self, i: usize, entry: &Entry) {
        let words = &self.words[3 * i..3 * i + 3];
        words[0].store(entry.own, Ordering::Relaxed);
        words[1].store(entry.opponent, Ordering::Relaxed);
        words[2].store(entry.pack(), Ordering::Relaxed);
    }

    /// Takes the slot's lock and returns the sequence to pass to [`Slot::unlock`]. Writes are a few stores, so a
    /// writer that finds the slot held waits for it.
    #[inline]
    fn lock(&self) -> u64 {
        loop {
            let sequence = self.sequence.load(Ordering::Relaxed);
            if sequence & LOCKED == 0
                && self
                    .sequence
                    .compare_exchange_weak(sequence, sequence | LOCKED, Ordering::Acquire, Ordering::Relaxed)
                    .is_ok()
            {
                // A reader that sees any of the stores that follow must also see the sequence as odd (or newer):
                // this fence pairs with the reader's acquire fence before it reads the sequence again.
                fence(Ordering::Release);
                return sequence;
            }

            std::hint::spin_loop();
        }
    }

    #[inline]
    fn unlock(&self, sequence: u64) {
        self.sequence.store(sequence.wrapping_add(2), Ordering::Release);
    }
}

/// Hash table keyed on the full position. Each slot holds two entries: one keeps the deepest result, the other is
/// always replaced (C++ f_hash_put: one entry; a deeper entry for the same position is kept).
/// C#: `TranspositionTable`.
///
/// [`TranspositionTable::clear`] does not touch the entries: it starts a new generation, and entries of older ones
/// count as empty. Only every 65,535th clear writes the whole table.
///
/// Any number of threads may [`get`](TranspositionTable::get) and [`store`](TranspositionTable::store) at the same
/// time (see the module documentation); `clear` needs the table to itself.
pub struct TranspositionTable {
    slots: Box<[Slot]>,
    shift: u32,
    /// Never 0, the generation of a never-written entry.
    generation: u16,
}

impl TranspositionTable {
    /// The table has 2^bits slots of two entries.
    pub fn new(bits: u32) -> TranspositionTable {
        TranspositionTable { slots: zeroed_slots(1 << bits), shift: 64 - bits, generation: 1 }
    }

    /// The entry of a position, if it is stored. `tag` is anything else the value depends on, such as the side to
    /// move. C#: `TryGet`.
    #[inline]
    pub fn get(&self, own: u64, opponent: u64, tag: i32) -> Option<Entry> {
        let slot = &self.slots[self.slot(own, opponent, tag)];
        let sequence = slot.sequence.load(Ordering::Acquire);
        if sequence & LOCKED != 0 {
            // A writer holds the slot: a miss, which is always safe.
            return None;
        }

        let first = slot.load(0);
        let found = if first.matches(own, opponent, tag, self.generation) {
            first
        } else {
            let second = slot.load(1);
            if !second.matches(own, opponent, tag, self.generation) {
                // A miss read during a write is a miss either way.
                return None;
            }
            second
        };

        // A hit counts only if no writer changed the slot while it was read (the words may come from two writes).
        fence(Ordering::Acquire);
        (slot.sequence.load(Ordering::Relaxed) == sequence).then_some(found)
    }

    /// C#: `Store` (the value, depth and move are narrowed as C#'s casts do).
    #[inline]
    #[allow(clippy::too_many_arguments)]
    pub fn store(&self, own: u64, opponent: u64, tag: i32, depth: i32, bound: Bound, value: i32, mv: i32) {
        let slot = &self.slots[self.slot(own, opponent, tag)];
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

        let sequence = slot.lock();
        let deep = slot.load(0);
        if deep.matches(own, opponent, tag, generation) {
            if depth >= deep.depth as i32 {
                slot.put(0, &entry);
            }
        } else if !deep.is_live(generation) || depth >= deep.depth as i32 {
            // An empty first entry moves to the second place as C#'s does: it is still empty there.
            slot.put(1, &deep);
            slot.put(0, &entry);
        } else {
            slot.put(1, &entry);
        }
        slot.unlock(sequence);
    }

    /// Asks the processor to load the slot of a position into the cache, for a [`TranspositionTable::store`] after
    /// a search below it. Changes nothing else.
    #[inline]
    pub fn prefetch(&self, own: u64, opponent: u64, tag: i32) {
        #[cfg(target_arch = "x86_64")]
        {
            let slot = &self.slots[self.slot(own, opponent, tag)];
            // SAFETY: a prefetch of a valid address has no other effect.
            unsafe {
                std::arch::x86_64::_mm_prefetch::<{ std::arch::x86_64::_MM_HINT_T0 }>((slot as *const Slot).cast());
            }
        }
        #[cfg(not(target_arch = "x86_64"))]
        let _ = (own, opponent, tag);
    }

    /// C#: `Clear`: afterwards every entry counts as empty.
    pub fn clear(&mut self) {
        self.generation = self.generation.wrapping_add(1);
        if self.generation == 0 {
            // SAFETY: the table is not shared during `&mut self`, and zero bytes are a valid `Slot` (atomics of 0):
            // every entry empty (Bound::None, generation 0) and the sequence free.
            unsafe { std::ptr::write_bytes(self.slots.as_mut_ptr(), 0, self.slots.len()) };
            self.generation = 1;
        }
    }

    #[inline]
    fn slot(&self, own: u64, opponent: u64, tag: i32) -> usize {
        let mut hash = own.wrapping_mul(0x9E37_79B9_7F4A_7C15);
        hash ^= opponent.wrapping_mul(0xC2B2_AE3D_27D4_EB4F).rotate_left(31).wrapping_add(tag as u64);
        hash = hash.wrapping_mul(0xD6E8_FEB8_6659_FD93);
        (hash >> self.shift) as usize
    }
}

/// `count` empty slots. Zeroed memory comes from the system without being written, as C#'s array does.
fn zeroed_slots(count: usize) -> Box<[Slot]> {
    let layout = std::alloc::Layout::array::<Slot>(count).expect("the table fits in memory");
    // SAFETY: the layout has a non-zero size (count ≥ 1024). Zero bytes are a valid `Slot` (atomics of 0), so the
    // memory holds `count` initialised slots, allocated with the layout `Box<[Slot]>` frees it with.
    unsafe {
        let memory = std::alloc::alloc_zeroed(layout).cast::<Slot>();
        if memory.is_null() {
            std::alloc::handle_alloc_error(layout);
        }
        Box::from_raw(std::ptr::slice_from_raw_parts_mut(memory, count))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn keeps_the_deeper_entry_and_replaces_the_other() {
        let table = TranspositionTable::new(10);
        table.store(1, 2, 0, 5, Bound::Exact, 100, 7);
        assert_eq!(table.get(1, 2, 0).map(|e| (e.value, e.depth, e.mv)), Some((100, 5, 7)));
        assert_eq!(table.get(1, 2, 1), None);

        // A shallower result for the same position does not replace it.
        table.store(1, 2, 0, 3, Bound::Lower, 50, 8);
        assert_eq!(table.get(1, 2, 0).map(|e| e.value), Some(100));

        let mut table = table;
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
    }

    #[test]
    fn packing_keeps_every_field() {
        for (value, depth, bound, mv, tag) in [
            (-32768i16, -128i8, Bound::Upper, -1i8, 255u8),
            (32767, 127, Bound::Exact, 63, 0),
            (-1, 0, Bound::Lower, 0, 1),
            (0, 60, Bound::None, -1, 0),
        ] {
            let entry =
                Entry { own: 0x8000_0000_0000_0001, opponent: 42, value, depth, bound, mv, tag, generation: 65535 };
            assert_eq!(Entry::unpack(entry.own, entry.opponent, entry.pack()), entry);
        }
        assert_eq!(Entry::default().pack(), 0);
    }

    // A value, depth, bound and move made from the position alone, so that any entry read can be checked against its
    // own position: an entry torn between two writes would mix the fields of two positions.
    fn fields(own: u64, opponent: u64) -> (i32, i32, Bound, i32) {
        let hash = (own ^ opponent.rotate_left(17)).wrapping_mul(0x9E37_79B9_7F4A_7C15);
        let bound = [Bound::Exact, Bound::Lower, Bound::Upper][(hash >> 7) as usize % 3];
        ((hash >> 16) as i16 as i32, (hash >> 40) as i32 & 63, bound, (hash >> 50) as i32 & 63)
    }

    #[test]
    fn many_threads_never_read_a_torn_entry() {
        // Miri (`cargo +nightly miri test -p stello-core --lib search::table`) checks far fewer rounds.
        const THREADS: u64 = if cfg!(miri) { 4 } else { 8 };
        const ROUNDS: u64 = if cfg!(miri) { 300 } else { 1_000_000 };
        // Positions shared by all threads: twice as many as slots, or few under Miri so that its rounds find some.
        const KEYS: u64 = if cfg!(miri) { 64 } else { 2048 };
        // A small table, so that the threads write the same slots all the time.
        let table = TranspositionTable::new(10);
        let hits = std::sync::atomic::AtomicU64::new(0);
        std::thread::scope(|scope| {
            for thread in 0..THREADS {
                let (table, hits) = (&table, &hits);
                scope.spawn(move || {
                    let mut seed = 0x2545_F491_4F6C_DD1D ^ thread.wrapping_mul(0x9E37_79B9_7F4A_7C15);
                    for _ in 0..ROUNDS {
                        seed ^= seed << 13;
                        seed ^= seed >> 7;
                        seed ^= seed << 17;
                        let key = seed % KEYS;
                        let (own, opponent) = (key.wrapping_mul(0x0101_0101_0101_0101), key << 1 | 1 << 63);
                        let tag = (seed >> 20) as i32 & 1;
                        let (value, depth, bound, mv) = fields(own, opponent ^ tag as u64);
                        if (seed >> 33) & 1 == 0 {
                            table.store(own, opponent, tag, depth, bound, value, mv);
                        } else if let Some(entry) = table.get(own, opponent, tag) {
                            assert_eq!((entry.own, entry.opponent, entry.tag as i32), (own, opponent, tag));
                            assert_eq!(
                                (entry.value as i32, entry.depth as i32, entry.bound, entry.mv as i32),
                                (value, depth, bound, mv)
                            );
                            hits.fetch_add(1, Ordering::Relaxed);
                        }
                    }
                });
            }
        });
        let hits = hits.load(Ordering::Relaxed);
        assert!(hits > THREADS * ROUNDS / 20, "{hits} hits: too few to test anything");
    }
}
