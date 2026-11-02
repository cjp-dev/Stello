//! Pattern shapes and pattern sets. C#: `Stello.Engine.V2.Evaluation.PatternShape` and `PatternSet` (Patterns.cs).

use std::collections::{HashMap, HashSet};
use std::ops::Range;
use std::sync::OnceLock;

use super::symmetry;
use crate::square::Square;

const DIAGONAL_MULTIPLIER: u64 = 0x0101_0101_0101_0101;

/// One row of a pattern (read with a shift) or one whole diagonal pattern (gathered into the top byte with a
/// multiplication); its byte is turned into base-3 digits with a table. C#: `PatternShape.Part`.
#[derive(Clone, Debug)]
struct Part {
    mask: u64,
    multiplier: u64,
    shift: u32,
    /// For each extracted byte, the sum of 3^(position in the pattern) of its squares. At most (3^10 - 1) / 2.
    digits: [u16; 256],
}

impl Part {
    /// C#: `Part.Extract`.
    #[inline(always)]
    fn extract(&self, discs: u64) -> usize {
        (((discs & self.mask).wrapping_mul(self.multiplier) >> self.shift) & 0xFF) as usize
    }

    /// The digits of both players: own discs count 1, the opponent's 2.
    #[inline(always)]
    fn index(&self, own: u64, opponent: u64) -> u32 {
        self.digits[self.extract(own)] as u32 + 2 * self.digits[self.extract(opponent)] as u32
    }
}

/// A pattern of squares in its canonical place on the board, such as the second row. The squares of a position are
/// read in the pattern's order as base-3 digits: empty 0, own disc 1, opponent's disc 2; the first square is the
/// lowest digit. The same pattern in its other places (the second row, the seventh row, the b and g columns) is read
/// from the position turned by a symmetry, so every place shares one weight table.
///
/// C#: `PatternShape`.
#[derive(Clone, Debug)]
pub struct PatternShape {
    name: String,
    squares: Vec<u32>,
    size: usize,
    parts: Vec<Part>,
    placements: Vec<usize>,
    fold: Vec<u32>,
    representatives: Vec<u32>,
}

impl PatternShape {
    /// C#: `new PatternShape(name, squares)`. Panics on an invalid list of squares (more than 10, repeated or off
    /// the board) or squares that cannot be extracted (C# throws).
    pub fn new(name: &str, squares: &[u32]) -> PatternShape {
        assert!(squares.len() <= 10, "Pattern {name} has more than 10 squares.");
        let distinct: HashSet<u32> = squares.iter().copied().collect();
        assert!(
            distinct.len() == squares.len() && squares.iter().all(|&s| s < 64),
            "Pattern {name} has an invalid list of squares."
        );

        let size = 3usize.pow(squares.len() as u32);
        let (fold, representatives) = make_fold(squares, size);
        PatternShape {
            name: name.to_string(),
            squares: squares.to_vec(),
            size,
            parts: make_parts(squares),
            placements: find_placements(squares),
            fold,
            representatives,
        }
    }

    /// C#: `PatternShape.Name`.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// The squares in digit order (the first is the lowest digit). C#: `PatternShape.Squares`.
    pub fn squares(&self) -> &[u32] {
        &self.squares
    }

    /// 3 to the power of the number of squares: the number of raw indices. C#: `PatternShape.Size`.
    pub fn size(&self) -> usize {
        self.size
    }

    /// The symmetries whose turned positions are read: one for each different set of squares the pattern covers on
    /// the board. C#: `PatternShape.Placements`.
    pub fn placements(&self) -> &[usize] {
        &self.placements
    }

    /// For every raw index, the smallest raw index that must have the same weight because a symmetry maps the
    /// pattern's squares onto themselves in another order (for example a row read backwards). C#: `PatternShape.Fold`.
    pub fn fold(&self) -> &[u32] {
        &self.fold
    }

    /// The raw indices that are their own fold, in increasing order: the weights a file stores.
    /// C#: `PatternShape.Representatives`.
    pub fn representatives(&self) -> &[u32] {
        &self.representatives
    }

    /// The raw index of the canonical squares of a position (own and opponent's discs, already turned).
    /// C#: `PatternShape.Index`.
    #[inline]
    pub fn index(&self, own: u64, opponent: u64) -> u32 {
        self.parts.iter().map(|part| part.index(own, opponent)).sum()
    }

    /// The raw index read square by square: slow, for tests and checks. C#: `PatternShape.SlowIndex`.
    pub fn slow_index(&self, own: u64, opponent: u64) -> u32 {
        let (mut index, mut power) = (0u32, 1u32);
        for &square in &self.squares {
            let bit = 1u64 << square;
            index += power
                * if own & bit != 0 {
                    1
                } else if opponent & bit != 0 {
                    2
                } else {
                    0
                };
            power *= 3;
        }

        index
    }
}

/// Squares in one row are read with a shift; squares on one diagonal (column + row the same, or column − row the same)
/// are gathered into the top byte with a multiplication. Each part's byte is turned into base-3 digits with a table.
/// C#: `PatternShape.MakeParts`.
fn make_parts(squares: &[u32]) -> Vec<Part> {
    let row_of = |s: u32| s >> 3;
    let column_of = |s: u32| s & 7;
    let first = squares[0];
    let same_row = squares.iter().all(|&s| row_of(s) == row_of(first));
    let anti_diagonal = squares.iter().all(|&s| column_of(s) + row_of(s) == column_of(first) + row_of(first));
    let diagonal = squares
        .iter()
        .all(|&s| column_of(s) as i32 - row_of(s) as i32 == column_of(first) as i32 - row_of(first) as i32);

    // C# GroupBy keeps the order in which the rows first appear.
    let groups: Vec<Vec<u32>> = if !same_row && (anti_diagonal || diagonal) {
        vec![squares.to_vec()]
    } else {
        let mut groups: Vec<Vec<u32>> = Vec::new();
        for &square in squares {
            match groups.iter_mut().find(|group| row_of(group[0]) == row_of(square)) {
                Some(group) => group.push(square),
                None => groups.push(vec![square]),
            }
        }
        groups
    };

    let mut parts = Vec::with_capacity(groups.len());
    for group in groups {
        let row = group.iter().all(|&s| row_of(s) == row_of(group[0]));
        let mask = group.iter().fold(0u64, |m, &s| m | (1u64 << s));
        let mut part = Part {
            mask,
            multiplier: if row { 1 } else { DIAGONAL_MULTIPLIER },
            shift: if row { 8 * row_of(group[0]) } else { 56 },
            digits: [0; 256],
        };

        // Where each square's bit lands in the part's byte, and its digit value 3^(position in the pattern).
        let mut bit_of = Vec::with_capacity(group.len());
        for &square in &group {
            let value = part.extract(1u64 << square);
            assert!(value.count_ones() == 1, "Square {square} cannot be extracted.");
            bit_of.push(value.trailing_zeros());
        }

        let distinct: HashSet<u32> = bit_of.iter().copied().collect();
        assert!(distinct.len() == bit_of.len(), "Two squares of a part land on the same bit.");

        for subset in 0..1u32 << group.len() {
            let (mut discs, mut expected, mut digits) = (0u64, 0usize, 0u32);
            for (i, &square) in group.iter().enumerate() {
                if subset & (1 << i) != 0 {
                    discs |= 1u64 << square;
                    expected |= 1 << bit_of[i];
                    let position = squares.iter().position(|&s| s == square).unwrap() as u32;
                    digits += 3u32.pow(position);
                }
            }

            let extracted = part.extract(discs);
            assert!(extracted == expected, "A part's extraction is not carry-free.");
            part.digits[extracted] = digits as u16;
        }

        parts.push(part);
    }

    parts
}

/// C#: `PatternShape.FindPlacements`.
fn find_placements(squares: &[u32]) -> Vec<usize> {
    let mut seen = HashSet::new();
    let mut placements = Vec::new();
    for symmetry in 0..symmetry::COUNT {
        // The squares of the original position that the turned position has on the pattern's squares.
        let mut covered = 0u64;
        for square in 0..64 {
            if squares.contains(&symmetry::map_square(square, symmetry)) {
                covered |= 1u64 << square;
            }
        }

        if seen.insert(covered) {
            placements.push(symmetry);
        }
    }

    placements
}

/// C#: `PatternShape.MakeFold`.
fn make_fold(squares: &[u32], size: usize) -> (Vec<u32>, Vec<u32>) {
    let index_of = |list: &[u32], value: u32| list.iter().position(|&s| s == value);

    // The permutations of the pattern's squares by the symmetries that map the set of squares onto itself.
    let mut permutations: Vec<Vec<usize>> = Vec::new();
    for symmetry in 1..symmetry::COUNT {
        let mapped: Vec<Option<usize>> =
            squares.iter().map(|&s| index_of(squares, symmetry::map_square(s, symmetry))).collect();
        if mapped.iter().all(Option::is_some) {
            permutations.push(mapped.into_iter().map(Option::unwrap).collect());
        }
    }

    let mut fold = vec![0u32; size];
    let mut digits = vec![0usize; squares.len()];
    for (index, slot) in fold.iter_mut().enumerate() {
        let mut rest = index;
        for digit in digits.iter_mut() {
            *digit = rest % 3;
            rest /= 3;
        }

        let mut smallest = index;
        for permutation in &permutations {
            let (mut other, mut power) = (0usize, 1usize);
            for i in 0..squares.len() {
                // Square i's digit moves to the place of the square it is mapped onto.
                let from = permutation.iter().position(|&p| p == i).unwrap();
                other += digits[from] * power;
                power *= 3;
            }

            smallest = smallest.min(other);
        }

        *slot = smallest as u32;
    }

    let representatives = (0..size as u32).filter(|&i| fold[i as usize] == i).collect();
    (fold, representatives)
}

/// One pattern in one place: the symmetry its position is turned by, where its weights start, and its parts in
/// [`PatternSet`]'s flat part list.
#[derive(Clone, Debug)]
struct Feature {
    shape: usize,
    symmetry: usize,
    offset: u32,
    parts: Range<usize>,
}

/// The patterns of an evaluation, each in all its places on the board: a feature is one pattern in one place.
///
/// C#: `PatternSet`.
#[derive(Clone, Debug)]
pub struct PatternSet {
    name: String,
    shapes: Vec<PatternShape>,
    offsets: Vec<usize>,
    raw_size: usize,
    features: Vec<Feature>,
    symmetries: Vec<usize>,
    /// The parts of all features in feature order, so [`PatternSet::indices`] walks one array.
    parts: Vec<Part>,
    /// The features read straight from the position, for [`PatternSet::sum_weights`].
    direct: Direct,
}

impl PatternSet {
    /// C#: `new PatternSet(name, shapes)`.
    pub fn new(name: &str, shapes: Vec<PatternShape>) -> PatternSet {
        assert!(!shapes.is_empty(), "A pattern set needs a shape.");
        let mut offsets = vec![0usize; shapes.len()];
        for i in 1..shapes.len() {
            offsets[i] = offsets[i - 1] + shapes[i - 1].size;
        }

        let raw_size = offsets[shapes.len() - 1] + shapes[shapes.len() - 1].size;
        let mut features = Vec::new();
        let mut parts = Vec::new();
        for (i, shape) in shapes.iter().enumerate() {
            for &symmetry in &shape.placements {
                let start = parts.len();
                parts.extend(shape.parts.iter().cloned());
                features.push(Feature { shape: i, symmetry, offset: offsets[i] as u32, parts: start..parts.len() });
            }
        }

        let mut symmetries: Vec<usize> = features.iter().map(|f| f.symmetry).collect();
        symmetries.sort_unstable();
        symmetries.dedup();
        let direct = Direct::new(&shapes, &features);
        PatternSet { name: name.to_string(), shapes, offsets, raw_size, features, symmetries, parts, direct }
    }

    /// C#: `PatternSet.Name`.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// C#: `PatternSet.Shapes`.
    pub fn shapes(&self) -> &[PatternShape] {
        &self.shapes
    }

    /// Where each shape's raw indices start in one table of all shapes. C#: `PatternSet.Offsets`.
    pub fn offsets(&self) -> &[usize] {
        &self.offsets
    }

    /// The number of raw indices of all shapes together. C#: `PatternSet.RawSize`.
    pub fn raw_size(&self) -> usize {
        self.raw_size
    }

    /// C#: `PatternSet.FeatureCount`.
    pub fn feature_count(&self) -> usize {
        self.features.len()
    }

    /// The shape of each feature, by feature number. C#: `PatternSet.ShapeOf`.
    pub fn shape_of(&self, feature: usize) -> &PatternShape {
        &self.shapes[self.features[feature].shape]
    }

    /// The symmetry each feature reads its position through (C#: the `Symmetry` of the private feature tuple).
    pub fn symmetry_of(&self, feature: usize) -> usize {
        self.features[feature].symmetry
    }

    /// The 11 patterns of Logistello and Edax-like programs: rows 2–4, the diagonals of 4 to 8 squares, the edge with
    /// both X-squares, the 2 × 5 corner and the 3 × 3 corner; 46 features. C#: `PatternSet.Classic11`.
    pub fn classic11() -> &'static PatternSet {
        static SET: OnceLock<PatternSet> = OnceLock::new();
        SET.get_or_init(|| PatternSet::new("classic11", classic11_shapes()))
    }

    /// [`PatternSet::classic11`] plus four 10-square corner and edge patterns of Egaroucid 7.9 (triangle, corner +
    /// block, cross, edge + y); 62 features. The shipped evaluation uses it. C#: `PatternSet.Plus15`.
    pub fn plus15() -> &'static PatternSet {
        static SET: OnceLock<PatternSet> = OnceLock::new();
        SET.get_or_init(|| {
            let mut shapes = classic11_shapes();
            shapes.extend([
                shape("triangle", "a1 b1 c1 d1 a2 b2 c2 a3 b3 a4"),
                shape("corner+block", "a1 c1 d1 e1 f1 h1 c2 d2 e2 f2"),
                shape("cross", "a1 b2 c3 d4 b1 c2 d3 a2 b3 c4"),
                shape("edge+y", "c2 a1 b1 c1 d1 e1 f1 g1 h1 f2"),
            ]);
            PatternSet::new("plus15", shapes)
        })
    }

    /// The 16 patterns of Egaroucid 7.9 (evaluate_generic.hpp), in their first place; 64 features.
    /// C#: `PatternSet.Egaroucid16`.
    pub fn egaroucid16() -> &'static PatternSet {
        static SET: OnceLock<PatternSet> = OnceLock::new();
        SET.get_or_init(|| {
            PatternSet::new(
                "egaroucid16",
                vec![
                    shape("hv2", "a2 b2 c2 d2 e2 f2 g2 h2"),
                    shape("d6+2c+x", "b1 c1 d2 e3 g2 f4 g5 h6 h7"),
                    shape("hv3", "a3 b3 c3 d3 e3 f3 g3 h3"),
                    shape("d7+2corner", "a1 b1 c2 d3 e4 f5 g6 h7 h8"),
                    shape("hv4", "a4 b4 c4 d4 e4 f4 g4 h4"),
                    shape("corner9", "a1 b1 c1 a2 b2 c2 a3 b3 c3"),
                    shape("d5+2x", "b2 d1 e2 f3 g4 h5 g7"),
                    shape("d8+2c", "a1 b2 c3 d4 e5 f6 g7 h8 a2 b1"),
                    shape("edge+2x", "b2 a1 b1 c1 d1 e1 f1 g1 h1 g2"),
                    shape("triangle", "a1 b1 c1 d1 a2 b2 c2 a3 b3 a4"),
                    shape("corner+block", "a1 c1 d1 e1 f1 h1 c2 d2 e2 f2"),
                    shape("cross", "a1 b2 c3 d4 b1 c2 d3 a2 b3 c4"),
                    shape("edge+y", "c2 a1 b1 c1 d1 e1 f1 g1 h1 f2"),
                    shape("narrow-triangle", "a1 b1 c1 d1 e1 a2 b2 a3 a4 a5"),
                    shape("fish", "a1 b1 a2 b2 c2 d2 b3 c3 b4 d4"),
                    shape("anvil", "c6 d6 d7 d8 c8 f8 e8 e7 e6 f6"),
                ],
            )
        })
    }

    /// The pattern sets by name, for files. C#: `PatternSet.ByName` (`None` where C# throws `ArgumentException`).
    pub fn by_name(name: &str) -> Option<&'static PatternSet> {
        match name {
            "classic11" => Some(PatternSet::classic11()),
            "plus15" => Some(PatternSet::plus15()),
            "egaroucid16" => Some(PatternSet::egaroucid16()),
            _ => None,
        }
    }

    /// The positions turned by every symmetry the features read (the others stay 0). C#: the start of `Indices`.
    #[inline(always)]
    fn turned(&self, own: u64, opponent: u64) -> ([u64; symmetry::COUNT], [u64; symmetry::COUNT]) {
        let mut turned_own = [0u64; symmetry::COUNT];
        let mut turned_opponent = [0u64; symmetry::COUNT];
        for &s in &self.symmetries {
            turned_own[s] = symmetry::apply(own, s);
            turned_opponent[s] = symmetry::apply(opponent, s);
        }

        (turned_own, turned_opponent)
    }

    /// Writes the raw index of every feature into `indices` (at least [`PatternSet::feature_count`] long), each plus
    /// its shape's offset in [`PatternSet::offsets`], so that it indexes one table of all shapes.
    /// C#: `PatternSet.Indices`.
    #[inline]
    pub fn indices(&self, own: u64, opponent: u64, indices: &mut [u32]) {
        let (turned_own, turned_opponent) = self.turned(own, opponent);
        for (feature, slot) in self.features.iter().zip(indices.iter_mut()) {
            let (o, p) = (turned_own[feature.symmetry], turned_opponent[feature.symmetry]);
            *slot = feature.offset + self.parts[feature.parts.clone()].iter().map(|part| part.index(o, p)).sum::<u32>();
        }
    }

    /// The sum of `weights` (one stage, by raw index plus offset) over the indices of all features: the fused form of
    /// [`PatternSet::indices`] and the sum in C#'s `EvalWeights.Evaluate`. Integer addition, so the order does not
    /// change the result.
    ///
    /// The features are read straight from the position, not from its turned copies: with BMI2 `pext` where it is
    /// fast ([`crate::cpu::fast_pext`]), otherwise with shifts and multiplications. Both give C#'s indices exactly.
    #[inline]
    pub(crate) fn sum_weights(&self, own: u64, opponent: u64, weights: &[i16]) -> i32 {
        // Every index is below `raw_size` by construction, which the unchecked reads rely on.
        assert!(weights.len() == self.raw_size, "one stage's weights");
        #[cfg(target_arch = "x86_64")]
        if crate::cpu::fast_pext() {
            // SAFETY: the processor has BMI2 (checked by fast_pext).
            return unsafe { self.direct.sum_pext(own, opponent, weights) };
        }

        self.direct.sum_portable(own, opponent, weights)
    }
}

/// A part of a feature read straight from the untouched position: the squares of one row (a shift), one column or
/// one diagonal (gathered into the top byte by a multiplication). The byte is turned into base-3 digits by a table
/// in [`Direct::digits`].
#[derive(Clone, Copy, Debug)]
struct DirectPart {
    mask: u64,
    multiplier: u64,
    shift: u32,
    /// Where the part's 256 digit values start in [`Direct::digits`].
    table: u32,
}

/// The features with the same number of parts: their offsets, and their parts one feature after the other.
#[derive(Clone, Debug)]
struct PartGroup {
    count: usize,
    offsets: Vec<u32>,
    parts: Vec<DirectPart>,
}

/// A feature for BMI2 (x86-64 only): `pext` gathers its squares from the position in board order, and a table of 2^squares
/// entries in [`Direct::pext_tables`] turns them into base-3 digits.
#[cfg(target_arch = "x86_64")]
#[derive(Clone, Copy, Debug)]
struct PextFeature {
    mask: u64,
    table: u32,
    offset: u32,
}

/// The features of a [`PatternSet`] read without turning the position. A feature's squares on the turned position
/// are other squares of the original one, so each feature gets its own masks and digit tables; features with the same
/// tables share them.
#[derive(Clone, Debug)]
struct Direct {
    /// The features by their number of parts, so that each group's loop has a fixed length.
    groups: Vec<PartGroup>,
    digits: Vec<u16>,
    #[cfg(target_arch = "x86_64")]
    pext: Vec<PextFeature>,
    #[cfg(target_arch = "x86_64")]
    pext_tables: Vec<u16>,
}

impl Direct {
    fn new(shapes: &[PatternShape], features: &[Feature]) -> Direct {
        let mut direct = Direct {
            groups: Vec::new(),
            digits: Vec::new(),
            #[cfg(target_arch = "x86_64")]
            pext: Vec::new(),
            #[cfg(target_arch = "x86_64")]
            pext_tables: Vec::new(),
        };
        let mut digit_tables: HashMap<Vec<u16>, u32> = HashMap::new();
        #[cfg(target_arch = "x86_64")]
        let mut pext_tables: HashMap<Vec<u16>, u32> = HashMap::new();
        let share = |pool: &mut Vec<u16>, tables: &mut HashMap<Vec<u16>, u32>, table: Vec<u16>| -> u32 {
            *tables.entry(table).or_insert_with_key(|table| {
                let start = pool.len() as u32;
                pool.extend_from_slice(table);
                start
            })
        };

        for feature in features {
            let shape = &shapes[feature.shape];
            // The square of the original position that lands on each of the pattern's squares.
            let original: Vec<u32> = shape
                .squares
                .iter()
                .map(|&target| (0..64).find(|&q| symmetry::map_square(q, feature.symmetry) == target).unwrap())
                .collect();
            let power = |square: u32| 3u16.pow(original.iter().position(|&q| q == square).unwrap() as u32);

            let groups = direct_groups(&original);
            let at = match direct.groups.iter().position(|g| g.count == groups.len()) {
                Some(at) => at,
                None => {
                    direct.groups.push(PartGroup { count: groups.len(), offsets: Vec::new(), parts: Vec::new() });
                    direct.groups.len() - 1
                }
            };
            direct.groups[at].offsets.push(feature.offset);
            for group in groups {
                let (mask, multiplier, shift) = line_extraction(&group);
                let extract = |discs: u64| ((discs & mask).wrapping_mul(multiplier) >> shift & 0xFF) as usize;
                let mut table = vec![0u16; 256];
                for subset in 0..1u32 << group.len() {
                    let (mut discs, mut digits) = (0u64, 0u16);
                    for (i, &square) in group.iter().enumerate() {
                        if subset & (1 << i) != 0 {
                            discs |= 1u64 << square;
                            digits += power(square);
                        }
                    }

                    table[extract(discs)] = digits;
                }

                let table = share(&mut direct.digits, &mut digit_tables, table);
                direct.groups[at].parts.push(DirectPart { mask, multiplier, shift, table });
            }

            // pext: bit k of the gathered value is the k-th lowest square of the mask.
            #[cfg(target_arch = "x86_64")]
            {
                let mask = original.iter().fold(0u64, |m, &q| m | (1u64 << q));
                let mut sorted = original.clone();
                sorted.sort_unstable();
                let table: Vec<u16> = (0..1usize << sorted.len())
                    .map(|bits| (0..sorted.len()).filter(|k| bits & (1 << k) != 0).map(|k| power(sorted[k])).sum())
                    .collect();
                let table = share(&mut direct.pext_tables, &mut pext_tables, table);
                direct.pext.push(PextFeature { mask, table, offset: feature.offset });
            }
        }

        direct
    }

    /// The portable form of [`PatternSet::sum_weights`] (also WebAssembly's).
    #[inline]
    fn sum_portable(&self, own: u64, opponent: u64, weights: &[i16]) -> i32 {
        let mut sum = 0i32;
        for group in &self.groups {
            sum += match group.count {
                1 => self.sum_group::<1>(group, own, opponent, weights),
                2 => self.sum_group::<2>(group, own, opponent, weights),
                3 => self.sum_group::<3>(group, own, opponent, weights),
                4 => self.sum_group::<4>(group, own, opponent, weights),
                _ => self.sum_any(group, own, opponent, weights),
            };
        }

        sum
    }

    #[inline(always)]
    fn sum_group<const N: usize>(&self, group: &PartGroup, own: u64, opponent: u64, weights: &[i16]) -> i32 {
        let mut sum = 0i32;
        for (parts, &offset) in group.parts.as_chunks::<N>().0.iter().zip(&group.offsets) {
            let mut index = offset;
            for part in parts {
                index += self.part_digits(part, own, opponent);
            }

            // SAFETY: a feature's index is below its shape's offset plus size, at most raw_size = weights.len().
            sum += unsafe { *weights.get_unchecked(index as usize) } as i32;
        }

        sum
    }

    #[inline(never)]
    fn sum_any(&self, group: &PartGroup, own: u64, opponent: u64, weights: &[i16]) -> i32 {
        let mut sum = 0i32;
        for (parts, &offset) in group.parts.chunks_exact(group.count).zip(&group.offsets) {
            let index = offset + parts.iter().map(|part| self.part_digits(part, own, opponent)).sum::<u32>();
            // SAFETY: as in sum_group.
            sum += unsafe { *weights.get_unchecked(index as usize) } as i32;
        }

        sum
    }

    /// The base-3 digits of one part: own discs count 1, the opponent's 2.
    #[inline(always)]
    fn part_digits(&self, part: &DirectPart, own: u64, opponent: u64) -> u32 {
        let own_byte = ((own & part.mask).wrapping_mul(part.multiplier) >> part.shift) & 0xFF;
        let opponent_byte = ((opponent & part.mask).wrapping_mul(part.multiplier) >> part.shift) & 0xFF;
        // SAFETY: every part's table has 256 entries in `digits`.
        let (own_digits, opponent_digits) = unsafe {
            (
                *self.digits.get_unchecked(part.table as usize + own_byte as usize),
                *self.digits.get_unchecked(part.table as usize + opponent_byte as usize),
            )
        };
        own_digits as u32 + 2 * opponent_digits as u32
    }

    /// [`PatternSet::sum_weights`] with BMI2 `pext`.
    #[cfg(target_arch = "x86_64")]
    #[target_feature(enable = "bmi2")]
    fn sum_pext(&self, own: u64, opponent: u64, weights: &[i16]) -> i32 {
        use std::arch::x86_64::_pext_u64;

        let mut sum = 0i32;
        for feature in &self.pext {
            let table = feature.table as usize;
            // SAFETY: a feature's table has 2^(squares in its mask) entries, and pext returns fewer bits than that;
            // its index is below raw_size = weights.len() as in sum_portable.
            unsafe {
                let own_digits = *self.pext_tables.get_unchecked(table + _pext_u64(own, feature.mask) as usize);
                let opponent_digits =
                    *self.pext_tables.get_unchecked(table + _pext_u64(opponent, feature.mask) as usize);
                let index = feature.offset + own_digits as u32 + 2 * opponent_digits as u32;
                sum += *weights.get_unchecked(index as usize) as i32;
            }
        }

        sum
    }
}

/// The parts of a feature on the original position. Squares in different columns can share a part (one
/// multiplication adds up the rows, see [`line_extraction`]), and so can squares of one column; a feature gets the
/// fewer parts of the two ways (columns on a tie). Within a part the squares keep the feature's order.
fn direct_groups(original: &[u32]) -> Vec<Vec<u32>> {
    let column_of = |s: u32| s & 7;
    // Each square goes into the first part without a square of its column.
    let mut across: Vec<Vec<u32>> = Vec::new();
    for &square in original {
        match across.iter_mut().find(|group| group.iter().all(|&s| column_of(s) != column_of(square))) {
            Some(group) => group.push(square),
            None => across.push(vec![square]),
        }
    }

    let mut columns: Vec<Vec<u32>> = Vec::new();
    for &square in original {
        match columns.iter_mut().find(|group| column_of(group[0]) == column_of(square)) {
            Some(group) => group.push(square),
            None => columns.push(vec![square]),
        }
    }

    if columns.len() < across.len() { columns } else { across }
}

/// The mask, multiplier and shift that bring the squares of one row, column or diagonal into the low byte, each on
/// its own bit and without carries (checked on every subset of the squares).
fn line_extraction(group: &[u32]) -> (u64, u64, u32) {
    let (row_of, column_of) = (|s: u32| s >> 3, |s: u32| s & 7);
    let mask = group.iter().fold(0u64, |m, &s| m | (1u64 << s));
    let first = group[0];
    let (multiplier, shift) = if group.len() > 1 && group.iter().all(|&s| column_of(s) == column_of(first)) {
        // One column: square (column c, row r) times 2^(56 - c - 7r) lands on bit 56 + r; every other product lands
        // on a bit of its own below 56 or beyond 63, so nothing carries into the top byte.
        let c = column_of(first);
        ((0..8).fold(0u64, |m, r| m | (1u64 << (56 - c - 7 * r))), 56)
    } else if group.iter().all(|&s| row_of(s) == row_of(first)) {
        (1, 8 * row_of(first))
    } else {
        // Squares in different columns: multiplying by 0x0101...01 adds up the rows, and as no two squares share a
        // column the sums have no carries; the top byte has each square on the bit of its column.
        (DIAGONAL_MULTIPLIER, 56)
    };

    let extract = |discs: u64| ((discs & mask).wrapping_mul(multiplier) >> shift) & 0xFF;
    let bits: Vec<u64> = group.iter().map(|&s| extract(1u64 << s)).collect();
    assert!(bits.iter().all(|b| b.count_ones() == 1), "A square of a direct part cannot be extracted.");
    for subset in 0..1u32 << group.len() {
        let (mut discs, mut expected) = (0u64, 0u64);
        for (i, &square) in group.iter().enumerate() {
            if subset & (1 << i) != 0 {
                discs |= 1u64 << square;
                expected |= bits[i];
            }
        }

        assert!(extract(discs) == expected, "A direct part's extraction is not carry-free.");
    }

    (mask, multiplier, shift)
}

fn classic11_shapes() -> Vec<PatternShape> {
    vec![
        PatternShape::new("row2", &[8, 9, 10, 11, 12, 13, 14, 15]),
        PatternShape::new("row3", &[16, 17, 18, 19, 20, 21, 22, 23]),
        PatternShape::new("row4", &[24, 25, 26, 27, 28, 29, 30, 31]),
        PatternShape::new("diagonal4", &[3, 10, 17, 24]),
        PatternShape::new("diagonal5", &[4, 11, 18, 25, 32]),
        PatternShape::new("diagonal6", &[5, 12, 19, 26, 33, 40]),
        PatternShape::new("diagonal7", &[6, 13, 20, 27, 34, 41, 48]),
        PatternShape::new("diagonal8", &[7, 14, 21, 28, 35, 42, 49, 56]),
        PatternShape::new("edge+2x", &[0, 1, 2, 3, 4, 5, 6, 7, 9, 14]),
        PatternShape::new("corner2x5", &[0, 1, 2, 3, 4, 8, 9, 10, 11, 12]),
        PatternShape::new("corner3x3", &[0, 1, 2, 8, 9, 10, 16, 17, 18]),
    ]
}

/// C#: `PatternSet.Shape(name, "a1 b1 ...")`.
fn shape(name: &str, squares: &str) -> PatternShape {
    let squares: Vec<u32> = squares
        .split(' ')
        .map(|text| Square::parse(text).unwrap_or_else(|| panic!("'{text}' is not a square")).index() as u32)
        .collect();
    PatternShape::new(name, &squares)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sets() -> [&'static PatternSet; 3] {
        [PatternSet::classic11(), PatternSet::plus15(), PatternSet::egaroucid16()]
    }

    #[test]
    fn sets_have_the_documented_feature_counts() {
        assert_eq!(PatternSet::classic11().feature_count(), 46);
        assert_eq!(PatternSet::plus15().feature_count(), 62);
        assert_eq!(PatternSet::egaroucid16().feature_count(), 64);
        assert_eq!(PatternSet::by_name("plus15").unwrap().name(), "plus15");
        assert!(PatternSet::by_name("plus16").is_none());
    }

    #[test]
    fn fast_indices_equal_the_slow_ones() {
        // A fixed pseudo-random sequence of positions with every density of discs.
        let mut state = 0x9E37_79B9_7F4A_7C15u64;
        let mut next = || {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            state
        };
        for set in sets() {
            let mut indices = vec![0u32; set.feature_count()];
            for round in 0..2000 {
                let (a, b) = (next(), next());
                let occupied = match round % 3 {
                    0 => a | b,
                    1 => a,
                    _ => a & b,
                };
                let own = occupied & next();
                let opponent = occupied & !own;
                set.indices(own, opponent, &mut indices);
                for (feature, &index) in indices.iter().enumerate() {
                    let shape = set.shape_of(feature);
                    let s = set.symmetry_of(feature);
                    let slow = shape.slow_index(symmetry::apply(own, s), symmetry::apply(opponent, s));
                    let offset = set.offsets()[set.shapes().iter().position(|x| std::ptr::eq(x, shape)).unwrap()];
                    assert_eq!(index as usize, offset + slow as usize, "{} feature {feature}", set.name());
                }
            }
        }
    }

    #[test]
    fn direct_sums_equal_the_sum_over_the_indices() {
        let mut state = 0x2545_F491_4F6C_DD1Du64;
        let mut next = || {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            state
        };
        for set in sets() {
            // Weights that differ for every raw index.
            let weights: Vec<i16> = (0..set.raw_size()).map(|i| (i.wrapping_mul(2_654_435_761) >> 7) as i16).collect();
            let mut indices = vec![0u32; set.feature_count()];
            for round in 0..3000 {
                let occupied = match round % 3 {
                    0 => next() | next(),
                    1 => next(),
                    _ => next() & next(),
                };
                let own = occupied & next();
                let opponent = occupied & !own;
                set.indices(own, opponent, &mut indices);
                let expected: i32 = indices.iter().map(|&i| weights[i as usize] as i32).sum();
                assert_eq!(set.direct.sum_portable(own, opponent, &weights), expected, "{}", set.name());
                #[cfg(target_arch = "x86_64")]
                if std::arch::is_x86_feature_detected!("bmi2") {
                    // SAFETY: BMI2 is there.
                    assert_eq!(unsafe { set.direct.sum_pext(own, opponent, &weights) }, expected, "{}", set.name());
                }
            }
        }
    }

    #[test]
    fn folds_are_idempotent() {
        for set in sets() {
            for shape in set.shapes() {
                for &f in shape.fold() {
                    assert_eq!(shape.fold()[f as usize], f, "{}", shape.name());
                }
            }
        }
    }
}
