//! Trained pattern weights and the evaluation file. C#: `Stello.Engine.V2.Evaluation.EvalWeights`.

use std::fmt;
use std::io::Read;
use std::sync::OnceLock;

use flate2::read::MultiGzDecoder;

use super::patterns::PatternSet;

const MAGIC: &[u8; 8] = b"STELLOEV";
const VERSION: u16 = 2;

/// Stello 2's evaluation, `Stello.Net/Book/stello-eval.bin` (C#: the resource `Stello.Engine.V2.stello-eval.bin`).
static BUILT_IN_FILE: &[u8] = include_bytes!("../../../../Stello.Net/Book/stello-eval.bin");

/// Why an evaluation file could not be read (C#: `InvalidDataException` from `EvalWeights.Read`).
#[derive(Clone, PartialEq, Eq, Debug)]
pub enum EvalError {
    /// The gzip stream is broken.
    Gzip(String),
    /// The magic "STELLOEV" or the version 2 is missing.
    NotAnEvaluationFile,
    /// The pattern set's name is not one of [`PatternSet::by_name`].
    UnknownPatternSet(String),
    /// The file ends early.
    CutShort,
    /// The stage table, the number of stages or the units per disc do not fit.
    DoesNotFit,
}

impl fmt::Display for EvalError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            EvalError::Gzip(message) => write!(f, "The evaluation file is not valid gzip: {message}"),
            EvalError::NotAnEvaluationFile => f.write_str("Not a Stello evaluation file (version 2)."),
            EvalError::UnknownPatternSet(name) => write!(f, "Unknown pattern set {name}."),
            EvalError::CutShort => f.write_str("The evaluation file is cut short."),
            EvalError::DoesNotFit => f.write_str("The weights do not fit the pattern set."),
        }
    }
}

impl std::error::Error for EvalError {}

/// Trained pattern weights: for every stage of the game (a range of disc counts), one weight per raw index of every
/// pattern, in 1/[`EvalWeights::units_per_disc`] discs, for the side to move.
///
/// The file (made by tools/Stello.Trainer) is gzip-compressed. Inside, all numbers are little-endian: `"STELLOEV"`,
/// version (u16, 2), pattern set name (u8 length and ASCII), number of stages (u8), the stage of every disc count 0–64
/// (65 × u8), units per disc (u16), then for every stage its folded weights: one i16 per
/// [`super::PatternShape::representatives`] entry of every pattern, minus the same weight of the stage before (so
/// neighbouring stages, which are alike, give small numbers), written as all low bytes and then all high bytes.
///
/// C#: `EvalWeights`.
#[derive(Clone, Debug)]
pub struct EvalWeights {
    set: &'static PatternSet,
    stage_of_discs: [u8; 65],
    units_per_disc: i32,
    stages: usize,
    /// All stages' weights, stage after stage, each [`PatternSet::raw_size`] long.
    weights: Vec<i16>,
}

impl EvalWeights {
    /// C#: `new EvalWeights(set, stageOfDiscs, unitsPerDisc, weights)`, with one weight table per stage.
    pub fn new(
        set: &'static PatternSet,
        stage_of_discs: &[u8],
        units_per_disc: i32,
        weights: &[Vec<i16>],
    ) -> Result<EvalWeights, EvalError> {
        if stage_of_discs.len() != 65
            || weights.is_empty()
            || weights.iter().any(|w| w.len() != set.raw_size())
            || stage_of_discs.iter().any(|&s| s as usize >= weights.len())
            || !(1..=1024).contains(&units_per_disc)
        {
            return Err(EvalError::DoesNotFit);
        }

        Ok(EvalWeights {
            set,
            stage_of_discs: stage_of_discs.try_into().unwrap(),
            units_per_disc,
            stages: weights.len(),
            weights: weights.concat(),
        })
    }

    /// C#: `EvalWeights.Set`.
    pub fn set(&self) -> &'static PatternSet {
        self.set
    }

    /// C#: `EvalWeights.Stages`.
    pub fn stages(&self) -> usize {
        self.stages
    }

    /// C#: `EvalWeights.UnitsPerDisc`.
    #[inline]
    pub fn units_per_disc(&self) -> i32 {
        self.units_per_disc
    }

    /// C#: `EvalWeights.StageOf`.
    #[inline]
    pub fn stage_of(&self, discs: u32) -> usize {
        self.stage_of_discs[discs as usize] as usize
    }

    /// The weights of one stage, by raw index plus the shape's offset ([`PatternSet::offsets`]).
    /// C#: `EvalWeights.StageWeights`.
    #[inline]
    pub fn stage_weights(&self, stage: usize) -> &[i16] {
        let size = self.set.raw_size();
        &self.weights[stage * size..(stage + 1) * size]
    }

    /// The evaluation for the side to move, in 1/[`EvalWeights::units_per_disc`] discs. C#: `EvalWeights.Evaluate`.
    #[inline]
    pub fn evaluate(&self, own: u64, opponent: u64) -> i32 {
        let stage = self.stage_of((own | opponent).count_ones());
        self.set.sum_weights(own, opponent, self.stage_weights(stage))
    }

    /// The stage of each disc count when the game (4 to 64 discs) is cut into `stages` equal parts (1–61).
    /// C#: `EvalWeights.EvenStages`.
    pub fn even_stages(stages: i32) -> [u8; 65] {
        assert!((1..=61).contains(&stages), "stages must be 1-61");
        let mut stage_of = [0u8; 65];
        for (discs, stage) in stage_of.iter_mut().enumerate() {
            *stage = ((discs as i32 - 4) * stages / 61).clamp(0, stages - 1) as u8;
        }

        stage_of
    }

    /// Stello 2's evaluation, `Book/stello-eval.bin`, built into the library so that every app has it without loading
    /// a file. It is read on first use. C#: `EvalWeights.Default`.
    pub fn built_in() -> &'static EvalWeights {
        static BUILT_IN: OnceLock<EvalWeights> = OnceLock::new();
        BUILT_IN.get_or_init(|| EvalWeights::read(BUILT_IN_FILE).expect("the built-in evaluation is valid"))
    }

    /// The bytes of the built-in evaluation file (gzip), for tools that pass it on.
    pub fn built_in_file() -> &'static [u8] {
        BUILT_IN_FILE
    }

    /// C#: `EvalWeights.Load`. Not for WebAssembly, which has no file system: read the bytes and call
    /// [`EvalWeights::read`].
    pub fn load(path: impl AsRef<std::path::Path>) -> std::io::Result<Result<EvalWeights, EvalError>> {
        Ok(EvalWeights::read(&std::fs::read(path)?))
    }

    /// Reads an evaluation file (the gzip-compressed bytes). C#: `EvalWeights.Read`.
    pub fn read(file: &[u8]) -> Result<EvalWeights, EvalError> {
        // C#'s GZipStream also reads files of several gzip members one after the other.
        let mut data = Vec::new();
        MultiGzDecoder::new(file).read_to_end(&mut data).map_err(|e| EvalError::Gzip(e.to_string()))?;
        let mut reader = Reader { data: &data, at: 0 };

        // C# ReadBytes returns fewer bytes at the end of the stream, which fails the magic test; ReadUInt16 throws.
        if reader.take_up_to(MAGIC.len()) != MAGIC || reader.u16()? != VERSION {
            return Err(EvalError::NotAnEvaluationFile);
        }

        let name_length = reader.u8()? as usize;
        let name = String::from_utf8_lossy(reader.take_up_to(name_length)).into_owned();
        let set = PatternSet::by_name(&name).ok_or(EvalError::UnknownPatternSet(name))?;
        let stages = reader.u8()? as usize;
        let stage_of_discs = reader.take_up_to(65).to_vec();
        let units_per_disc = reader.u16()? as i32;
        let count: usize = set.shapes().iter().map(|shape| shape.representatives().len()).sum();
        let mut weights = Vec::with_capacity(stages);
        let mut previous = vec![0i16; count];
        for _ in 0..stages {
            let low = reader.take(count)?;
            let high = reader.take(count)?;
            for i in 0..count {
                previous[i] = previous[i].wrapping_add((low[i] as u16 | (high[i] as u16) << 8) as i16);
            }

            let mut table = vec![0i16; set.raw_size()];
            let mut next = 0;
            for (s, shape) in set.shapes().iter().enumerate() {
                let mut folded = vec![0i16; shape.size()];
                for &representative in shape.representatives() {
                    folded[representative as usize] = previous[next];
                    next += 1;
                }

                let offset = set.offsets()[s];
                for (raw, &fold) in shape.fold().iter().enumerate() {
                    table[offset + raw] = folded[fold as usize];
                }
            }

            weights.push(table);
        }

        EvalWeights::new(set, &stage_of_discs, units_per_disc, &weights)
    }
}

/// A little-endian reader over the decompressed file (C#: `BinaryReader`).
struct Reader<'a> {
    data: &'a [u8],
    at: usize,
}

impl<'a> Reader<'a> {
    /// Up to `count` bytes, fewer at the end (C#: `ReadBytes`).
    fn take_up_to(&mut self, count: usize) -> &'a [u8] {
        let end = (self.at + count).min(self.data.len());
        let bytes = &self.data[self.at..end];
        self.at = end;
        bytes
    }

    /// Exactly `count` bytes (C#: `ReadExactly`).
    fn take(&mut self, count: usize) -> Result<&'a [u8], EvalError> {
        let bytes = self.take_up_to(count);
        if bytes.len() == count { Ok(bytes) } else { Err(EvalError::CutShort) }
    }

    fn u8(&mut self) -> Result<u8, EvalError> {
        Ok(self.take(1)?[0])
    }

    fn u16(&mut self) -> Result<u16, EvalError> {
        let bytes = self.take(2)?;
        Ok(u16::from_le_bytes([bytes[0], bytes[1]]))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_built_in_file_is_plus15() {
        let weights = EvalWeights::built_in();
        assert_eq!(weights.set().name(), "plus15");
        assert!(weights.stages() >= 1);
        assert!(weights.units_per_disc() >= 1);
    }

    #[test]
    fn bad_files_are_rejected() {
        assert!(matches!(EvalWeights::read(b"not gzip"), Err(EvalError::Gzip(_))));
        let file = EvalWeights::built_in_file();
        // Cut the gzip stream: the decoder fails, or the content ends early.
        assert!(EvalWeights::read(&file[..file.len() / 2]).is_err());
    }

    #[test]
    fn even_stages_cut_the_game_in_equal_parts() {
        let stages = EvalWeights::even_stages(10);
        assert_eq!(stages[0], 0);
        assert_eq!(stages[4], 0);
        assert_eq!(stages[64], 9);
        assert!(stages.windows(2).all(|w| w[0] <= w[1]));
    }
}
