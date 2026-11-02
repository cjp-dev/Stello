//! `solve-bench` and `mid-bench`: C#'s `Stello.Benchmark solve-bench` and `mid-bench`
//! (tools/Stello.Benchmark/BenchCommands.cs) for the Rust engine, with the same position files and the same JSON
//! (`BenchRun`), so that the C# tool's `--compare` checks parity.

use std::fmt::Write as _;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};

use stello_core::eval::EvalWeights;
use stello_core::search::{SearchEngine, SearchLimits, SearchOptions};
use stello_core::{Board, Player};

use crate::{Options, repository_folder};

/// A benchmark position: C#'s `BenchmarkPosition` (`<id> <64 squares> <X|O>[; A2:+38; ...]`).
pub(crate) struct Position {
    pub id: String,
    pub board: Board,
    pub player: Player,
    /// The best score of the move list, if there is one (C#: `Score`).
    pub score: Option<i32>,
    /// The moves with that score, lower case (C#: `BestMoves`).
    pub best_moves: Vec<String>,
}

/// C#: `BenchmarkPosition.Load`.
pub(crate) fn load_positions(path: &Path) -> Result<Vec<Position>, String> {
    let text = std::fs::read_to_string(path).map_err(|e| format!("{}: {e}", path.display()))?;
    let mut positions = Vec::new();
    for line in text.lines() {
        if line.trim().is_empty() || line.trim_start().starts_with('#') {
            continue;
        }

        let mut parts = line.split(';').map(str::trim).filter(|p| !p.is_empty());
        let head: Vec<&str> = parts.next().unwrap_or("").split_whitespace().collect();
        let [id, squares, side] = head[..] else { return Err(format!("Not a position: {line}")) };
        if squares.len() != 64 {
            return Err(format!("Not a position: {line}"));
        }

        let board = Board::parse(squares).map_err(|e| format!("{e}: {line}"))?;
        let player = match side {
            "X" | "x" => Player::Black,
            "O" | "o" => Player::White,
            _ => return Err(format!("The side to move must be X or O: {line}")),
        };
        let mut scored: Vec<(String, i32)> = Vec::new();
        for part in parts {
            let parsed = part.split_once(':').and_then(|(mv, score)| {
                let score = score.strip_prefix('+').unwrap_or(score).parse::<i32>().ok()?;
                stello_core::Square::parse(&mv.to_lowercase()).map(|m| (m.to_string(), score))
            });
            scored.push(parsed.ok_or(format!("Not a move with a score: {part}"))?);
        }

        let score = scored.iter().map(|&(_, s)| s).max();
        let best_moves = scored.iter().filter(|&&(_, s)| Some(s) == score).map(|(m, _)| m.clone()).collect();
        positions.push(Position { id: id.to_string(), board, player, score, best_moves });
    }

    Ok(positions)
}

/// C#: `BenchResult`.
struct BenchResult {
    id: String,
    empties: u32,
    mv: String,
    score: i32,
    kind: &'static str,
    depth: i32,
    nodes: u64,
    ms: f64,
}

fn number<T: std::str::FromStr + PartialOrd + std::fmt::Display>(
    options: &Options,
    name: &str,
    default: T,
    min: T,
    max: T,
) -> Result<T, String> {
    match options.text(name) {
        None => Ok(default),
        Some(text) => text
            .parse::<T>()
            .ok()
            .filter(|v| *v >= min && *v <= max)
            .ok_or(format!("--{name} must be a whole number from {min} to {max}.")),
    }
}

/// The engine options of `--eval`, `--selective` and `--probcut-pct`, and their text for the report.
pub(crate) fn engine_options(options: &Options) -> Result<(SearchOptions, String), String> {
    let mut search = SearchOptions::default();
    let mut text = String::new();
    // Not in the report's text: the results are the same either way.
    match options.text("cpu") {
        None | Some("auto") => {}
        Some("portable") => stello_core::cpu::set_portable_only(true),
        Some(_) => return Err("--cpu must be auto or portable.".to_string()),
    }

    if let Some(path) = options.text("eval").filter(|&p| p != "default") {
        let weights =
            EvalWeights::load(path).map_err(|e| format!("{path}: {e}"))?.map_err(|e| format!("{path}: {e}"))?;
        search.pattern = Some(Arc::new(weights));
        let name = Path::new(path).file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
        write!(text, ", evaluation {name}").unwrap();
    }

    match options.text("selective") {
        None | Some("on") => {}
        Some("off") => {
            search.selective = false;
            text.push_str(", selective off");
        }
        Some(_) => return Err("--selective must be on or off.".to_string()),
    }

    if options.text("probcut-pct").is_some() {
        let percent = number(options, "probcut-pct", 100, 1, 1000)?;
        search.probcut_percent = Some(percent);
        write!(text, ", probcut {percent} %").unwrap();
    }

    Ok((search, text))
}

/// The positions file: `--positions`, or `default` in the benchmark's data folder.
fn positions_path(options: &Options, default: &str) -> Result<PathBuf, String> {
    match options.text("positions") {
        Some(path) => Ok(path.into()),
        None => Ok(repository_folder()
            .ok_or("No --positions given and no repository found.")?
            .join("Stello.Net/tools/Stello.Benchmark/Data")
            .join(default)),
    }
}

pub(crate) fn solve_bench(args: &[String]) -> Result<(), String> {
    let options = Options::parse(args, &["positions", "from", "to", "passes", "hash-bits", "eval", "out", "cpu"])?;
    let all = load_positions(&positions_path(&options, "ffo-40-59.txt")?)?;
    let from = number(&options, "from", 40, 0, i32::MAX)?;
    let to = number(&options, "to", 44, 0, i32::MAX)?;
    // C#: ids that are not numbers are always searched.
    let positions: Vec<Position> =
        all.into_iter().filter(|p| p.id.parse::<i32>().map_or(true, |id| id >= from && id <= to)).collect();
    let passes = number(&options, "passes", 1, 1, 100)?;
    let hash_bits = number(&options, "hash-bits", 19u32, 10, 26)?;
    let (search_options, options_text) = engine_options(&options)?;
    let limits_text = format!("solve{options_text}");
    let mut engine = SearchEngine::new(hash_bits, search_options);
    let results = run("solve-bench", &positions, &mut engine, &SearchLimits::SOLVE, &limits_text, hash_bits, passes)?;

    let mut wrong = 0;
    println!();
    println!("| # | Empty | Score | Move | Nodes | Time | Nodes/s |");
    println!("|---|---:|---:|---|---:|---:|---:|");
    for (position, result) in positions.iter().zip(&results) {
        let score_ok = position.score.is_none_or(|s| result.kind == "Exact" && result.score == s);
        let move_ok = position.best_moves.is_empty() || position.best_moves.contains(&result.mv);
        if !score_ok || !move_ok {
            wrong += 1;
        }

        println!(
            "| {} | {} | {}{} | {}{} | {} | {} | {} |",
            result.id,
            result.empties,
            signed(result.score),
            if score_ok { String::new() } else { format!(" (should be {})", signed(position.score.unwrap())) },
            result.mv,
            if move_ok { "" } else { " (wrong)" },
            result.nodes,
            seconds(result.ms),
            speed(result.nodes, result.ms)
        );
    }

    let nodes: u64 = results.iter().map(|r| r.nodes).sum();
    let ms: f64 = results.iter().map(|r| r.ms).sum();
    println!("| **Total** | | | | **{nodes}** | **{}** | **{}** |", seconds(ms), speed(nodes, ms));
    println!(
        "{}",
        if wrong == 0 {
            "All scores and moves are correct.".to_string()
        } else {
            format!("{wrong} positions have a wrong score or move.")
        }
    );
    save(&options, "solve-bench", &limits_text, hash_bits, passes, &results)?;
    if wrong == 0 { Ok(()) } else { Err(format!("{wrong} wrong.")) }
}

pub(crate) fn mid_bench(args: &[String]) -> Result<(), String> {
    let options = Options::parse(
        args,
        &["positions", "depth", "passes", "hash-bits", "eval", "selective", "probcut-pct", "out", "cpu"],
    )?;
    let positions = load_positions(&positions_path(&options, "midgame.txt")?)?;
    let depth = number(&options, "depth", 10, 1, 60)?;
    let passes = number(&options, "passes", 3, 1, 100)?;
    let hash_bits = number(&options, "hash-bits", 19u32, 10, 26)?;
    let (search_options, options_text) = engine_options(&options)?;
    let limits_text = format!("depth {depth}{options_text}");
    let limits = SearchLimits::fixed_depth(depth);
    let mut engine = SearchEngine::new(hash_bits, search_options);
    let results = run("mid-bench", &positions, &mut engine, &limits, &limits_text, hash_bits, passes)?;

    println!();
    println!("| Empty squares | Positions | Nodes | Time | Nodes/s |");
    println!("|---:|---:|---:|---:|---:|");
    let mut empties: Vec<u32> = results.iter().map(|r| r.empties).collect();
    empties.sort_unstable_by(|a, b| b.cmp(a));
    empties.dedup();
    for e in empties {
        let group: Vec<&BenchResult> = results.iter().filter(|r| r.empties == e).collect();
        let nodes: u64 = group.iter().map(|r| r.nodes).sum();
        let ms: f64 = group.iter().map(|r| r.ms).sum();
        println!("| {e} | {} | {nodes} | {} | {} |", group.len(), seconds(ms), speed(nodes, ms));
    }

    let nodes: u64 = results.iter().map(|r| r.nodes).sum();
    let ms: f64 = results.iter().map(|r| r.ms).sum();
    println!("| **Total** | {} | **{nodes}** | **{}** | **{}** |", results.len(), seconds(ms), speed(nodes, ms));
    save(&options, "mid-bench", &limits_text, hash_bits, passes, &results)
}

/// C#: `BenchCommands.Run`. Interleaved passes, the fastest time of each position counts; each search starts with
/// empty hash tables.
fn run(
    command: &str,
    positions: &[Position],
    engine: &mut SearchEngine,
    limits: &SearchLimits,
    limits_text: &str,
    hash_bits: u32,
    passes: u32,
) -> Result<Vec<BenchResult>, String> {
    if positions.is_empty() {
        return Err("No positions to search.".to_string());
    }

    println!(
        "{command}: {} positions, stello-rust, {limits_text}, hash bits {hash_bits}, {passes} pass(es).",
        positions.len()
    );
    let mut best: Vec<Option<BenchResult>> = (0..positions.len()).map(|_| None).collect();
    for pass in 1..=passes {
        let clock = std::time::Instant::now();
        let mut nodes = 0;
        for (position, slot) in positions.iter().zip(best.iter_mut()) {
            engine.clear_hash();
            let result = engine.search_simple(&position.board, position.player, limits);
            let current = BenchResult {
                id: position.id.clone(),
                empties: position.board.empty_count(),
                mv: result.mv.to_string(),
                score: result.score,
                kind: result.kind.name(),
                depth: result.depth,
                nodes: result.nodes,
                ms: result.elapsed.as_secs_f64() * 1000.0,
            };
            nodes += result.nodes;

            if let Some(previous) = slot.as_ref()
                && (previous.nodes != current.nodes || previous.mv != current.mv || previous.score != current.score)
            {
                println!(
                    "  Warning: {} gave another result in pass {pass} ({} nodes, {}).",
                    position.id, current.nodes, current.mv
                );
            }

            if command == "solve-bench" && positions.len() <= 100 {
                println!(
                    "  pass {pass} #{}: {} {} in {} ({} nodes)",
                    current.id,
                    signed(current.score),
                    current.mv,
                    seconds(current.ms),
                    current.nodes
                );
            }

            if slot.as_ref().is_none_or(|old| old.ms > current.ms) {
                *slot = Some(current);
            }
        }

        println!("  pass {pass}: {nodes} nodes in {:.2} s", clock.elapsed().as_secs_f64());
    }

    Ok(best.into_iter().map(|r| r.expect("every position was searched")).collect())
}

fn save(
    options: &Options,
    command: &str,
    limits_text: &str,
    hash_bits: u32,
    passes: u32,
    results: &[BenchResult],
) -> Result<(), String> {
    if let Some(out) = options.text("out") {
        let json = bench_run_json(command, limits_text, hash_bits, passes, results);
        std::fs::write(out, json).map_err(|e| format!("{out}: {e}"))?;
        println!("Saved to {out}.");
    }

    Ok(())
}

fn signed(score: i32) -> String {
    if score > 0 { format!("+{score}") } else { score.to_string() }
}

/// C#: `BenchRun` as `JsonSerializer.Serialize(run, new() { WriteIndented = true })` writes it.
fn bench_run_json(command: &str, limits: &str, hash_bits: u32, passes: u32, results: &[BenchResult]) -> String {
    let processors = std::thread::available_parallelism().map(|n| n.get()).unwrap_or(1);
    let mut json = String::new();
    json.push_str("{\n");
    writeln!(json, "  \"Command\": {},", quote(command)).unwrap();
    writeln!(json, "  \"Date\": {},", quote(&utc_now())).unwrap();
    writeln!(json, "  \"ProcessorCount\": {processors},").unwrap();
    writeln!(json, "  \"Limits\": {},", quote(limits)).unwrap();
    writeln!(json, "  \"HashBits\": {hash_bits},").unwrap();
    writeln!(json, "  \"Passes\": {passes},").unwrap();
    json.push_str("  \"Results\": [\n");
    for (i, r) in results.iter().enumerate() {
        json.push_str("    {\n");
        writeln!(json, "      \"Id\": {},", quote(&r.id)).unwrap();
        writeln!(json, "      \"Empties\": {},", r.empties).unwrap();
        writeln!(json, "      \"Move\": {},", quote(&r.mv)).unwrap();
        writeln!(json, "      \"Score\": {},", r.score).unwrap();
        writeln!(json, "      \"Kind\": {},", quote(r.kind)).unwrap();
        writeln!(json, "      \"Depth\": {},", r.depth).unwrap();
        writeln!(json, "      \"Nodes\": {},", r.nodes).unwrap();
        writeln!(json, "      \"Ms\": {}", r.ms).unwrap();
        json.push_str(if i + 1 < results.len() { "    },\n" } else { "    }\n" });
    }
    json.push_str("  ],\n");
    json.push_str("  \"Engine\": \"stello-rust\"\n");
    json.push('}');
    json
}

fn quote(text: &str) -> String {
    let mut quoted = String::from("\"");
    for c in text.chars() {
        match c {
            '"' => quoted.push_str("\\\""),
            '\\' => quoted.push_str("\\\\"),
            c if (c as u32) < 0x20 => write!(quoted, "\\u{:04x}", c as u32).unwrap(),
            c => quoted.push(c),
        }
    }
    quoted.push('"');
    quoted
}

/// "yyyy-MM-dd HH:mm" in UTC (C# writes local time; there is no time zone database here).
fn utc_now() -> String {
    let seconds = SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_secs() as i64).unwrap_or(0);
    let (days, rest) = (seconds.div_euclid(86_400), seconds.rem_euclid(86_400));
    // Howard Hinnant's civil_from_days.
    let z = days + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z - era * 146_097;
    let yoe = (doe - doe / 1460 + doe / 36_524 - doe / 146_096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let day = doy - (153 * mp + 2) / 5 + 1;
    let month = if mp < 10 { mp + 3 } else { mp - 9 };
    let year = yoe + era * 400 + i64::from(month <= 2);
    format!("{year:04}-{month:02}-{day:02} {:02}:{:02}", rest / 3600, rest % 3600 / 60)
}

fn seconds(ms: f64) -> String {
    if ms >= 10_000.0 { format!("{:.1} s", ms / 1000.0) } else { format!("{:.3} s", ms / 1000.0) }
}

fn speed(nodes: u64, ms: f64) -> String {
    if ms > 0.0 { format!("{:.2}M", nodes as f64 / ms / 1000.0) } else { "-".to_string() }
}
