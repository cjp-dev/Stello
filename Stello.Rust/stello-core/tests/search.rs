//! The midgame search: the current engine's recorded signatures (Stello.Net/Stello.Engine.Tests/Data/
//! stello-v3-signatures.txt, the same file C#'s StelloV3BaselineTests checks; it started as Stello 2.0's and is updated
//! deliberately when a kept fix changes the search), and time control, progress, cancel and Move Now.

use std::sync::Arc;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::{Duration, Instant};

use stello_core::search::{NoStop, SearchInfo, SearchOptions};
use stello_core::{
    Board, EvalWeights, Player, ScoreKind, SearchEngine, SearchError, SearchLimits, SearchResult, SearchSignals, Square,
};

fn repository_file(path: &str) -> String {
    let full = format!("{}/../../{path}", env!("CARGO_MANIFEST_DIR"));
    std::fs::read_to_string(&full).unwrap_or_else(|e| panic!("{full}: {e}"))
}

/// `<id> <64 squares> <X|O>` lines of a benchmark position file (C#: `BenchmarkPosition`).
fn benchmark_positions(file: &str) -> Vec<(String, Board, Player)> {
    repository_file(&format!("Stello.Net/tools/Stello.Benchmark/Data/{file}"))
        .lines()
        .filter(|line| !line.trim().is_empty() && !line.starts_with('#'))
        .map(|line| {
            let head: Vec<&str> = line.split(';').next().unwrap().split_whitespace().collect();
            let player = if head[2] == "X" { Player::Black } else { Player::White };
            (head[0].to_string(), Board::parse(head[1]).unwrap(), player)
        })
        .collect()
}

fn position(file: &str, id: &str) -> (Board, Player) {
    let (_, board, player) = benchmark_positions(file).into_iter().find(|(i, _, _)| i == id).unwrap();
    (board, player)
}

/// Every 5th of the 100 `midgame.txt ... depth=10` signatures, node for node (all 100 are checked by C#'s
/// StelloV3BaselineTests). The `solve` lines are [`solve_signatures_match_the_baseline`].
#[test]
fn midgame_signatures_match_the_baseline() {
    let signatures = repository_file("Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt");
    let positions = benchmark_positions("midgame.txt");
    let mut engine = SearchEngine::new(19, SearchOptions::default());
    let mut checked = 0;
    for line in signatures.lines().filter(|l| l.starts_with("midgame.txt ")).step_by(5) {
        let fields: Vec<&str> = line.split(' ').collect();
        let (_, board, player) = positions.iter().find(|(id, _, _)| id == fields[1]).unwrap();
        let depth: i32 = fields[2].strip_prefix("depth=").unwrap().parse().unwrap();

        // C#'s StelloV3BaselineTests uses a new engine for each search; a cleared one is the same.
        engine.clear_hash();
        let result = engine.search_simple(board, *player, &SearchLimits::fixed_depth(depth));
        let actual = format!(
            "{} {} {} {} {} {} {} {}",
            fields[0], fields[1], fields[2], result.mv, result.score, result.kind, result.depth, result.nodes
        );
        assert_eq!(actual, line);
        checked += 1;
    }

    assert_eq!(checked, 20);
}

/// The 5 `ffo-40-59.txt ... solve` signatures (FFO #40–44, 92 M nodes; about 3 s in release, 15 s in debug).
#[test]
fn solve_signatures_match_the_baseline() {
    let signatures = repository_file("Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt");
    let positions = benchmark_positions("ffo-40-59.txt");
    let mut engine = SearchEngine::new(19, SearchOptions::default());
    let lines: Vec<&str> = signatures.lines().filter(|l| l.starts_with("ffo-40-59.txt ")).collect();
    assert_eq!(lines.len(), 5);
    for line in &lines {
        let fields: Vec<&str> = line.split(' ').collect();
        assert_eq!(fields[2], "solve");
        let (_, board, player) = positions.iter().find(|(id, _, _)| id == fields[1]).unwrap();
        engine.clear_hash();
        let result = engine.search_simple(board, *player, &SearchLimits::SOLVE);
        let actual = format!(
            "{} {} {} {} {} {} {} {}",
            fields[0], fields[1], fields[2], result.mv, result.score, result.kind, result.depth, result.nodes
        );
        assert_eq!(actual, *line);
    }
}

// Plain minimax to the end, the empty squares going to the winner.
fn exact_score(board: &Board, player: Player, passed: bool) -> i32 {
    let moves = board.legal_moves(player);
    if moves == 0 {
        if passed {
            let (own, other) = (board.count(player) as i32, board.count(player.opponent()) as i32);
            let empty = board.empty_count() as i32;
            return match own.cmp(&other) {
                std::cmp::Ordering::Greater => own - other + empty,
                std::cmp::Ordering::Less => own - other - empty,
                std::cmp::Ordering::Equal => 0,
            };
        }

        return -exact_score(board, player.opponent(), true);
    }

    Square::in_mask(moves)
        .map(|square| -exact_score(&board.play_unchecked(player, square.index() as u32), player.opponent(), false))
        .max()
        .unwrap()
}

/// The solver's score and move are exact on random endings with 2–11 empty squares (every layer from the shallow
/// solver to EndLocal), against a plain minimax. Node parity with C# is checked by `stello solve-bench` and
/// `Stello.Benchmark solve-bench --compare`.
#[test]
fn solver_finds_the_exact_score_of_random_endings() {
    let mut engine = SearchEngine::new(14, SearchOptions::default());
    let mut seed = 0x9E37_79B9_7F4A_7C15u64;
    let mut random = move |n: u32| {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        (seed % n as u64) as u32
    };
    let max_empties = if cfg!(debug_assertions) { 9 } else { 11 };
    let mut checked = 0;
    for game in 0..120 {
        let target = 2 + game % (max_empties - 1);
        let (mut board, mut player) = (Board::INITIAL, Player::Black);
        while board.empty_count() > target && !board.is_game_over() {
            let moves = board.legal_moves(player);
            if moves != 0 {
                let squares: Vec<Square> = Square::in_mask(moves).collect();
                board = board.play_unchecked(player, squares[random(squares.len() as u32) as usize].index() as u32);
            }
            player = player.opponent();
        }

        if board.legal_moves(player) == 0 {
            continue;
        }

        engine.clear_hash();
        let result = engine.search_simple(&board, player, &SearchLimits::SOLVE);
        let score = exact_score(&board, player, false);
        assert_eq!((result.score, result.kind), (score, ScoreKind::Exact), "{board}");
        let square = result.mv.0.unwrap();
        let after = board.play_unchecked(player, square.index() as u32);
        assert_eq!(-exact_score(&after, player.opponent(), false), score, "{board} {square}");
        checked += 1;
    }

    assert!(checked > 100, "{checked}");
}

/// Move Now and cancel in the middle of a solve (FFO #49, 26 empty squares, about 12 s): Move Now returns a legal
/// move at once, cancel returns no result.
#[test]
fn move_now_and_cancel_stop_a_solve() {
    let (board, player) = position("ffo-40-59.txt", "49");
    // Loading the built-in evaluation takes a while in a debug build; not in the search's time.
    EvalWeights::built_in();
    for move_now in [true, false] {
        let signals = Arc::new(SearchSignals::new());
        let clock = Instant::now();
        let setter = {
            let signals = Arc::clone(&signals);
            std::thread::spawn(move || {
                std::thread::sleep(Duration::from_millis(300));
                if move_now { signals.move_now() } else { signals.cancel() }
            })
        };

        let mut engine = SearchEngine::new(16, SearchOptions::default());
        let mut nodes = 0;
        let mut progress = |info: &SearchInfo| nodes = info.nodes;
        let result = engine.search(&board, player, &SearchLimits::SOLVE, Some(&mut progress), signals.as_ref(), None);
        let elapsed = clock.elapsed();
        setter.join().unwrap();
        assert!(elapsed >= Duration::from_millis(300) && elapsed < Duration::from_secs(3), "{elapsed:?}");
        // The solver was running (the 4-ply presearch takes a few hundred nodes); the heartbeats report its nodes.
        assert!(nodes > 100_000, "{nodes}");
        if move_now {
            let result = result.unwrap();
            assert!(board.legal_moves(player) & result.mv.0.unwrap().bit() != 0);
            assert!(result.nodes > 0);
        } else {
            assert_eq!(result, Err(SearchError::Cancelled));
        }
    }
}

/// Move Now from the progress callback at the first report of the solver that `stop_at` accepts.
fn solve_with_move_now_at(board: &Board, player: Player, stop_at: impl Fn(&SearchInfo) -> bool) -> SearchResult {
    let signals = SearchSignals::new();
    let mut engine = SearchEngine::new(16, SearchOptions::default());
    let mut stopped = false;
    let mut progress = |info: &SearchInfo| {
        if !stopped && stop_at(info) {
            stopped = true;
            signals.move_now();
        }
    };
    let result = engine.search(board, player, &SearchLimits::SOLVE, Some(&mut progress), &signals, None).unwrap();
    assert!(stopped, "the search never reached the report to stop at");
    result
}

/// Move Now in the exact pass (FFO #40, +38): the first move's score is only a bound, so the result is the completed
/// win/loss/draw pass's, not an Exact score.
#[test]
fn move_now_in_the_exact_pass_keeps_the_win_loss_draw_result() {
    let (board, player) = position("ffo-40-59.txt", "40");
    let empties = board.empty_count() as i32;
    let result = solve_with_move_now_at(&board, player, |info| info.depth == empties && info.kind == ScoreKind::Exact);
    assert_eq!(result.kind, ScoreKind::WinLossDraw);
    assert!(result.score > 0, "{}", result.score);
    assert_eq!(result.depth, empties);
    assert!(board.legal_moves(player) & result.mv.0.unwrap().bit() != 0);
}

/// Move Now in the win/loss/draw pass after a draw is found (FFO #41, a draw): a later move could still win, so the
/// draw is not reported as Exact.
#[test]
fn move_now_after_a_draw_in_the_win_loss_draw_pass_is_not_exact() {
    let (board, player) = position("ffo-40-59.txt", "41");
    let empties = board.empty_count() as i32;
    let result = solve_with_move_now_at(&board, player, |info| {
        info.depth == empties && info.kind == ScoreKind::WinLossDraw && info.score == 0
    });
    assert_ne!(result.kind, ScoreKind::Exact);
    assert_ne!(result.kind, ScoreKind::WinLossDraw);
    assert!(board.legal_moves(player) & result.mv.0.unwrap().bit() != 0);
}

/// Move Now in the win/loss/draw pass after a losing move, with no midgame result before it (8 empties: no ordering
/// searches; a lost position, -2, from FFO #41 by random play): the loss is only a bound until every move is
/// searched, so the result is a legal move without a solved score. Had the pass completed, it would be WinLossDraw.
#[test]
fn move_now_after_a_loss_in_the_win_loss_draw_pass_keeps_a_move_without_a_score() {
    let board = Board::parse("XXXXXXXO--OOOXXXOOOOXOOXXOXXXOOX-XOXOX-XOOXOOO---XOXOOO-XOOOOOOO").unwrap();
    let player = Player::Black;
    let result = solve_with_move_now_at(&board, player, |info| {
        info.depth == 8 && info.kind == ScoreKind::WinLossDraw && info.score < 0
    });
    assert_eq!(result.kind, ScoreKind::None);
    assert!(board.legal_moves(player) & result.mv.0.unwrap().bit() != 0);
}

#[test]
fn trivial_positions_need_no_search() {
    let mut engine = SearchEngine::new(10, SearchOptions::default());

    // One legal move: played without a search.
    let board = Board::parse(&format!("XO{}", "-".repeat(62))).unwrap();
    let result = engine.search_simple(&board, Player::Black, &SearchLimits::fixed_depth(8));
    assert_eq!((result.mv.to_string(), result.kind, result.nodes), ("c1".to_string(), ScoreKind::None, 0));

    // No legal move: a pass.
    let result = engine.search_simple(&board, Player::White, &SearchLimits::fixed_depth(8));
    assert!(result.mv.is_pass());
    assert_eq!(result.kind, ScoreKind::None);
}

#[test]
fn only_moves_restrict_the_root() {
    let mut engine = SearchEngine::new(12, SearchOptions::default());
    let (board, player) = position("midgame.txt", "g01e48");
    let legal = board.legal_moves(player);
    let only = 1u64 << legal.trailing_zeros();
    let limits = SearchLimits::fixed_depth(6);

    // A single allowed move is still searched.
    let result = engine.search(&board, player, &limits, None, &NoStop, Some(only)).unwrap();
    assert_eq!(result.mv.0.map(Square::bit), Some(only));
    assert_eq!((result.kind, result.depth), (ScoreKind::Heuristic, 6));
    assert!(result.nodes > 0);

    assert_eq!(engine.search(&board, player, &limits, None, &NoStop, Some(!legal)), Err(SearchError::NoLegalOnlyMove));
}

#[test]
fn progress_reports_every_root_move_and_heartbeats() {
    // A clock that jumps 200 ms at every reading, so every abort check sends a heartbeat.
    static NOW: AtomicU64 = AtomicU64::new(0);
    fn jumping_clock() -> Duration {
        Duration::from_millis(NOW.fetch_add(200, Ordering::Relaxed))
    }

    let mut engine = SearchEngine::new(16, SearchOptions::default());
    engine.set_clock(jumping_clock);
    let (board, player) = position("midgame.txt", "g01e36");
    let root_moves = board.legal_moves(player).count_ones() as usize;
    let mut reports: Vec<SearchInfo> = Vec::new();
    let mut progress = |info: &SearchInfo| reports.push(*info);
    let result =
        engine.search(&board, player, &SearchLimits::fixed_depth(8), Some(&mut progress), &NoStop, None).unwrap();

    assert_eq!(reports.iter().filter(|r| r.depth == 1).count(), root_moves);
    assert!(reports.windows(2).all(|w| w[0].depth <= w[1].depth && w[0].nodes <= w[1].nodes));
    // A heartbeat repeats the last report with more nodes.
    let heartbeats = reports
        .windows(2)
        .filter(|w| (w[0].depth, w[0].current_move, w[0].best_move) == (w[1].depth, w[1].current_move, w[1].best_move))
        .count();
    assert!(heartbeats > 0);
    let last = reports.last().unwrap();
    assert_eq!(
        (last.depth, last.best_move, last.score, last.kind),
        (8, result.mv.0, result.score, ScoreKind::Heuristic)
    );
    assert!(reports.iter().all(|r| board.legal_moves(player) & r.current_move.bit() != 0));
}

/// The soft rule: no new iteration once 2/3 of the time (or `SoftTimePercent`) has passed. A clock that the test
/// moves by hand makes it deterministic.
#[test]
fn soft_time_rule_stops_before_a_new_iteration() {
    static NOW_MS: AtomicU64 = AtomicU64::new(0);
    fn manual_clock() -> Duration {
        Duration::from_millis(NOW_MS.load(Ordering::Relaxed))
    }

    let (board, player) = position("midgame.txt", "g02e42");
    let limits = SearchLimits::time_per_move_ms(3000);

    // 2/3 of 3000 = 2000 ms: past it after iteration 3.
    fn past_after_3(depth: i32) -> u64 {
        if depth >= 3 { 2001 } else { 0 }
    }
    // At 2000 ms exactly another iteration starts; after 5 it is past.
    fn past_after_5(depth: i32) -> u64 {
        match depth {
            3 | 4 => 2000,
            d if d >= 5 => 2500,
            _ => 0,
        }
    }
    // SoftTimePercent 50: 1500 ms.
    fn past_half_after_2(depth: i32) -> u64 {
        if depth >= 2 { 1600 } else { 0 }
    }

    // (soft percent, the clock after each iteration, the last iteration)
    type Case = (Option<i32>, fn(i32) -> u64, i32);
    let cases: [Case; 3] = [(None, past_after_3, 3), (None, past_after_5, 5), (Some(50), past_half_after_2, 2)];
    for (percent, time_after, expected) in cases {
        NOW_MS.store(0, Ordering::Relaxed);
        let options = SearchOptions { soft_time_percent: percent, ..SearchOptions::default() };
        let mut engine = SearchEngine::new(16, options);
        engine.set_clock(manual_clock);
        engine.set_iteration_done(Some(Box::new(move |depth, _| NOW_MS.store(time_after(depth), Ordering::Relaxed))));
        let mut deepest = 0;
        let mut progress = |info: &SearchInfo| deepest = deepest.max(info.depth);
        let result = engine.search(&board, player, &limits, Some(&mut progress), &NoStop, None).unwrap();
        assert_eq!((result.depth, result.kind), (expected, ScoreKind::Heuristic));
        assert_eq!(deepest, expected, "no move of a new iteration was searched");
    }
}

/// The hard limit stops an iteration in the middle; the best move so far is kept.
#[test]
fn hard_time_limit_stops_within_an_iteration() {
    static NOW_MS: AtomicU64 = AtomicU64::new(0);
    fn manual_clock() -> Duration {
        Duration::from_millis(NOW_MS.load(Ordering::Relaxed))
    }

    let mut engine = SearchEngine::new(16, SearchOptions::default());
    engine.set_clock(manual_clock);
    let (board, player) = position("midgame.txt", "g01e42");
    let mut reports = Vec::new();
    let mut progress = |info: &SearchInfo| {
        // Time runs out after the first move of iteration 7.
        if info.depth == 7 {
            NOW_MS.store(1000, Ordering::Relaxed);
        }
        reports.push(*info);
    };
    let result = engine
        .search(&board, player, &SearchLimits::time_per_move_ms(1000), Some(&mut progress), &NoStop, None)
        .unwrap();

    assert_eq!((result.depth, result.kind), (7, ScoreKind::Heuristic));
    let sevens: Vec<&SearchInfo> = reports.iter().filter(|r| r.depth == 7).collect();
    assert!(sevens.len() < board.legal_moves(player).count_ones() as usize);
    assert_eq!(result.mv.0, sevens.last().unwrap().best_move);
}

/// With the real clock: a time per move ends within the limit (the abort check comes every 1024 nodes).
#[test]
fn time_per_move_stops_within_the_limit() {
    let mut engine = SearchEngine::new(16, SearchOptions::default());
    EvalWeights::built_in();
    for limit in [30, 200] {
        engine.clear_hash();
        let clock = Instant::now();
        let result = engine.search_simple(&Board::INITIAL, Player::Black, &SearchLimits::time_per_move_ms(limit));
        let elapsed = clock.elapsed();
        assert!(elapsed < Duration::from_millis(limit as u64 + 30), "{limit} ms took {elapsed:?}");
        assert!(result.elapsed <= elapsed);
        assert_eq!(result.kind, ScoreKind::Heuristic);
        assert!(result.depth >= 1);
        assert!(Board::INITIAL.legal_moves(Player::Black) & result.mv.0.unwrap().bit() != 0);
    }
}

fn search_until_signal(signal: fn(&SearchSignals)) -> (Result<SearchResult, SearchError>, Duration) {
    let signals = Arc::new(SearchSignals::new());
    // The clock starts before the signal's thread, so the search runs at least its 100 ms.
    let clock = Instant::now();
    let setter = {
        let signals = Arc::clone(&signals);
        std::thread::spawn(move || {
            std::thread::sleep(Duration::from_millis(100));
            signal(&signals);
        })
    };

    let mut engine = SearchEngine::new(16, SearchOptions::default());
    let (board, player) = position("midgame.txt", "g03e48");
    // 40 plies would take far longer than the test.
    let result = engine.search(&board, player, &SearchLimits::fixed_depth(40), None, signals.as_ref(), None);
    let elapsed = clock.elapsed();
    setter.join().unwrap();
    (result, elapsed)
}

#[test]
fn cancel_from_another_thread_stops_without_a_result() {
    let (result, elapsed) = search_until_signal(SearchSignals::cancel);
    assert_eq!(result, Err(SearchError::Cancelled));
    assert!(elapsed >= Duration::from_millis(100) && elapsed < Duration::from_secs(5), "{elapsed:?}");

    // A search that starts cancelled does not run.
    let signals = SearchSignals::new();
    signals.cancel();
    let mut engine = SearchEngine::new(10, SearchOptions::default());
    let limits = SearchLimits::fixed_depth(4);
    assert_eq!(
        engine.search(&Board::INITIAL, Player::Black, &limits, None, &signals, None),
        Err(SearchError::Cancelled)
    );
    signals.reset();
    assert!(engine.search(&Board::INITIAL, Player::Black, &limits, None, &signals, None).is_ok());
}

#[test]
fn move_now_from_another_thread_returns_the_best_move_so_far() {
    let (result, elapsed) = search_until_signal(SearchSignals::move_now);
    let result = result.unwrap();
    assert!(elapsed >= Duration::from_millis(100) && elapsed < Duration::from_secs(5), "{elapsed:?}");
    assert_eq!(result.kind, ScoreKind::Heuristic);
    assert!(result.depth >= 2 && result.depth < 40);
    let (board, player) = position("midgame.txt", "g03e48");
    assert!(board.legal_moves(player) & result.mv.0.unwrap().bit() != 0);
}

/// Multi-ProbCut off (the trainer's plain search) searches more nodes; the other switches are checked against C#
/// with `stello mid-bench --selective off|--probcut-pct N|--eval <file>`.
#[test]
fn selective_search_cuts_nodes() {
    let (board, player) = position("midgame.txt", "g01e42");
    let limits = SearchLimits::fixed_depth(8);
    let selective = SearchEngine::new(16, SearchOptions::default()).search_simple(&board, player, &limits);
    let plain = SearchEngine::new(16, SearchOptions { selective: false, ..SearchOptions::default() })
        .search_simple(&board, player, &limits);
    assert!(plain.nodes > selective.nodes);
}
