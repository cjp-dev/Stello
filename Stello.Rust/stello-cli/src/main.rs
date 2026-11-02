//! `stello`: parity and speed runs of the Rust engine without .NET (Plans/making-stello-3-plan.md, phase 1).

use std::path::Path;
use std::process::ExitCode;
use std::time::Instant;

use stello_core::eval::{self, EvalWeights};
use stello_core::{Board, Player, board};

const USAGE: &str = "\
Usage: stello <command> [options]

  version                   The engine version.
  perft [--depth 8]         Perft from the start position to every depth up to --depth (a pass counts as one move),
                            with the time of each depth.
  eval-split [<folder>] [--max-per-discs 20000|all] [--eval <file>]
                            Stello 2's evaluation over the trainer's test split (test-dNN.bin; default folder
                            Traning data/stello-bin above the current folder): the count and FNV-1a 64 checksums of
                            Evaluate and PatternScore in order, to compare with
                            `Stello.Benchmark parity-data --split`, then the evaluation speed and perft(8)'s time.
                            --max-per-discs takes evenly spread positions per number of discs, as the trainer's report.
";

const FNV_OFFSET: u64 = 14_695_981_039_346_656_037;
const FNV_PRIME: u64 = 1_099_511_628_211;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let result = match args.first().map(String::as_str) {
        Some("version") => {
            println!("stello {}", stello_core::VERSION);
            Ok(())
        }
        Some("perft") => perft(&args[1..]),
        Some("eval-split") => eval_split(&args[1..]),
        _ => Err(USAGE.to_string()),
    };

    match result {
        Ok(()) => ExitCode::SUCCESS,
        Err(message) => {
            eprintln!("{message}");
            ExitCode::FAILURE
        }
    }
}

/// `--name value` options and positional arguments.
struct Options {
    named: Vec<(String, String)>,
    positional: Vec<String>,
}

impl Options {
    fn parse(args: &[String], allowed: &[&str]) -> Result<Options, String> {
        let mut options = Options { named: Vec::new(), positional: Vec::new() };
        let mut rest = args.iter();
        while let Some(arg) = rest.next() {
            match arg.strip_prefix("--") {
                Some(name) => {
                    if !allowed.contains(&name) {
                        return Err(format!("Unknown option --{name}."));
                    }
                    let value = rest.next().ok_or(format!("--{name} needs one value."))?;
                    options.named.push((name.to_string(), value.clone()));
                }
                None => options.positional.push(arg.clone()),
            }
        }

        Ok(options)
    }

    fn text(&self, name: &str) -> Option<&str> {
        self.named.iter().find(|(n, _)| n == name).map(|(_, v)| v.as_str())
    }
}

fn perft(args: &[String]) -> Result<(), String> {
    let options = Options::parse(args, &["depth"])?;
    let depth: u32 = match options.text("depth") {
        Some(text) => text.parse().ok().filter(|d| (1..=14).contains(d)).ok_or("--depth must be 1-14.")?,
        None => 8,
    };

    for d in 1..=depth {
        let clock = Instant::now();
        let leaves = board::perft(&Board::INITIAL, Player::Black, d);
        println!("perft {d} {leaves} {:.1} ms", clock.elapsed().as_secs_f64() * 1000.0);
    }

    Ok(())
}

/// The repository folder: the first folder above the current one with Stello.Net/Stello.Net.slnx.
fn repository_folder() -> Option<std::path::PathBuf> {
    let current = std::env::current_dir().ok()?;
    current.ancestors().find(|folder| folder.join("Stello.Net").join("Stello.Net.slnx").exists()).map(Path::to_path_buf)
}

fn eval_split(args: &[String]) -> Result<(), String> {
    let options = Options::parse(args, &["max-per-discs", "eval"])?;
    let folder = match options.positional.as_slice() {
        [folder] => folder.into(),
        [] => repository_folder()
            .ok_or("No folder given and no repository found.")?
            .join("Traning data")
            .join("stello-bin"),
        _ => return Err(USAGE.to_string()),
    };
    let max_per_discs: u64 = match options.text("max-per-discs") {
        None => 20_000,
        Some("all") => u64::MAX,
        Some(text) => {
            text.parse().ok().filter(|&m| m >= 1).ok_or("--max-per-discs must be a whole number from 1, or all.")?
        }
    };
    let loaded;
    let weights = match options.text("eval") {
        Some(path) => {
            loaded =
                EvalWeights::load(path).map_err(|e| format!("{path}: {e}"))?.map_err(|e| format!("{path}: {e}"))?;
            &loaded
        }
        None => EvalWeights::built_in(),
    };

    // The test split's records: own u64, opponent u64, score i8 (tools/Stello.Trainer TrainingData.cs), the subset of
    // TrainingData.Load(folder, Split.Test, discs, max).
    let mut samples: Vec<(u64, u64)> = Vec::new();
    for discs in 4..=64 {
        let path = folder.join(format!("test-d{discs:02}.bin"));
        let Ok(bytes) = std::fs::read(&path) else { continue };
        let count = bytes.len() as u64 / 17;
        let take = count.min(max_per_discs);
        for i in 0..take {
            let source = if take == count { i } else { i * count / take };
            let record = &bytes[(source * 17) as usize..(source * 17 + 17) as usize];
            let own = u64::from_le_bytes(record[0..8].try_into().unwrap());
            let opponent = u64::from_le_bytes(record[8..16].try_into().unwrap());
            samples.push((own, opponent));
        }
    }

    if samples.is_empty() {
        return Err(format!("{} holds no test positions (test-dNN.bin).", folder.display()));
    }

    let fnv =
        |hash: u64, value: i32| value.to_le_bytes().iter().fold(hash, |h, &b| (h ^ b as u64).wrapping_mul(FNV_PRIME));
    let (mut eval_hash, mut score_hash) = (FNV_OFFSET, FNV_OFFSET);
    for &(own, opponent) in &samples {
        eval_hash = fnv(eval_hash, weights.evaluate(own, opponent));
        score_hash = fnv(score_hash, eval::pattern_score(weights, own, opponent));
    }

    println!("positions {}", samples.len());
    println!("eval-fnv {eval_hash:x}");
    println!("score-fnv {score_hash:x}");

    // Speed: the fastest of 3 passes over the positions, and of 3 perft(8) runs.
    let mut best = f64::MAX;
    let mut sink = 0i64;
    for _ in 0..3 {
        let clock = Instant::now();
        for &(own, opponent) in &samples {
            sink += weights.evaluate(std::hint::black_box(own), opponent) as i64;
        }
        best = best.min(clock.elapsed().as_secs_f64());
    }
    println!("eval-speed {:.2} M/s ({:.0} ms per pass, sum {sink})", samples.len() as f64 / best / 1e6, best * 1000.0);

    best = f64::MAX;
    let mut leaves = 0;
    for _ in 0..3 {
        let clock = Instant::now();
        leaves = board::perft(std::hint::black_box(&Board::INITIAL), Player::Black, 8);
        best = best.min(clock.elapsed().as_secs_f64());
    }
    println!("perft8 {leaves} in {:.1} ms", best * 1000.0);
    Ok(())
}
