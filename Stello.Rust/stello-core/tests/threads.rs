//! Searches with more than one thread (Plans/making-stello-3-plan.md, phase 5): the threads share the hash tables, and
//! the results must stay right. Solves give the exact score of the single-threaded solver (which equals C# 2.0's);
//! midgame searches give a legal move and a heuristic score; stop signals stop all threads; and an engine that searched
//! with several threads searches with 1 exactly as before.

use std::sync::Arc;
use std::time::{Duration, Instant};

use stello_core::search::{SearchOptions, WIN_SCORE};
use stello_core::{Board, Player, ScoreKind, SearchEngine, SearchError, SearchLimits, SearchSignals, Square};

fn repository_file(path: &str) -> String {
    let full = format!("{}/../../{path}", env!("CARGO_MANIFEST_DIR"));
    std::fs::read_to_string(&full).unwrap_or_else(|e| panic!("{full}: {e}"))
}

/// `<id> <64 squares> <X|O>; <move>:<score>; ...` lines of a benchmark file, with the best score if there is one.
fn benchmark_positions(file: &str) -> Vec<(String, Board, Player, Option<i32>)> {
    repository_file(&format!("Stello.Net/tools/Stello.Benchmark/Data/{file}"))
        .lines()
        .filter(|line| !line.trim().is_empty() && !line.starts_with('#'))
        .map(|line| {
            let mut parts = line.split(';');
            let head: Vec<&str> = parts.next().unwrap().split_whitespace().collect();
            let player = if head[2] == "X" { Player::Black } else { Player::White };
            let best = parts
                .filter_map(|part| part.split_once(':'))
                .map(|(_, score)| score.trim().trim_start_matches('+').parse::<i32>().unwrap())
                .max();
            (head[0].to_string(), Board::parse(head[1]).unwrap(), player, best)
        })
        .collect()
}

/// A random position with `empties` empty squares and a legal move for the side to move, or `None`.
fn random_position(seed: &mut u64, empties: u32) -> Option<(Board, Player)> {
    let mut random = |n: u32| {
        *seed ^= *seed << 13;
        *seed ^= *seed >> 7;
        *seed ^= *seed << 17;
        (*seed % n as u64) as usize
    };
    let (mut board, mut player) = (Board::INITIAL, Player::Black);
    while board.empty_count() > empties && !board.is_game_over() {
        let moves: Vec<Square> = Square::in_mask(board.legal_moves(player)).collect();
        if !moves.is_empty() {
            board = board.play_unchecked(player, moves[random(moves.len() as u32)].index() as u32);
        }
        player = player.opponent();
    }

    (board.empty_count() == empties && board.legal_moves(player) != 0).then_some((board, player))
}

/// The exact score of `board` for `player`, whoever is to move (the opponent, or `player` after a pass), with a
/// single-threaded solve.
fn score_for(board: &Board, player: Player) -> i32 {
    let mut engine = SearchEngine::new(16, SearchOptions::default());
    if board.legal_moves(player.opponent()) != 0 {
        -engine.search_simple(board, player.opponent(), &SearchLimits::SOLVE).score
    } else {
        assert!(board.legal_moves(player) != 0, "the game is over");
        engine.search_simple(board, player, &SearchLimits::SOLVE).score
    }
}

fn threads(limits: SearchLimits, threads: u32) -> SearchLimits {
    limits.with_threads(threads).unwrap()
}

/// FFO #40–44 with 2, 4 and 8 threads: the published exact score, and a move with that score.
#[test]
fn several_threads_solve_ffo_exactly() {
    let positions = benchmark_positions("ffo-40-59.txt");
    let mut engine = SearchEngine::new(19, SearchOptions::default());
    for count in [2, 4, 8] {
        for (id, board, player, best) in positions.iter().filter(|p| ["40", "41", "42", "43", "44"].contains(&&*p.0)) {
            engine.clear_hash();
            let result = engine.search_simple(board, *player, &threads(SearchLimits::SOLVE, count));
            assert_eq!((result.score, result.kind), (best.unwrap(), ScoreKind::Exact), "#{id}, {count} threads");
            let after = board.play_unchecked(*player, result.mv.0.unwrap().index() as u32);
            assert_eq!(
                score_for(&after, *player),
                best.unwrap(),
                "#{id}, {count} threads: {} is not a best move",
                result.mv
            );
        }
    }
}

/// Random endings with 14–20 empty squares, solved with 4 threads on one engine without clearing its tables
/// between them (as in a game): the score of the single-threaded solver.
#[test]
fn several_threads_solve_random_endings_like_one() {
    let mut threaded = SearchEngine::new(16, SearchOptions::default());
    let mut single = SearchEngine::new(16, SearchOptions::default());
    let mut seed = 0x2545_F491_4F6C_DD1Du64;
    let mut checked = 0;
    while checked < 40 {
        let Some((board, player)) = random_position(&mut seed, 14 + checked % 7) else { continue };
        single.clear_hash();
        let expected = single.search_simple(&board, player, &SearchLimits::SOLVE);
        let result = threaded.search_simple(&board, player, &threads(SearchLimits::SOLVE, 4));
        assert_eq!(
            (result.score, result.kind, result.depth),
            (expected.score, expected.kind, expected.depth),
            "{board}"
        );
        checked += 1;
    }
}

/// Midgame searches with 3 and 6 threads finish at the asked depth with a legal move and a heuristic score.
#[test]
fn several_threads_search_the_midgame() {
    let positions = benchmark_positions("midgame.txt");
    let mut engine = SearchEngine::new(18, SearchOptions::default());
    for (i, (id, board, player, _)) in positions.iter().step_by(4).enumerate() {
        let count = if i % 2 == 0 { 3 } else { 6 };
        let result = engine.search_simple(board, *player, &threads(SearchLimits::fixed_depth(9), count));
        assert_eq!((result.kind, result.depth), (ScoreKind::Heuristic, 9), "{id}");
        assert!(board.legal_moves(*player) & result.mv.0.unwrap().bit() != 0, "{id}: {}", result.mv);
        assert!(result.score.abs() < WIN_SCORE + 64, "{id}: {}", result.score);
    }
}

/// The nodes of a search with several threads are those of all of them: more than the main thread's alone.
#[test]
fn several_threads_count_the_nodes_of_all() {
    let (_, board, player, _) = benchmark_positions("midgame.txt").into_iter().next().unwrap();
    let mut engine = SearchEngine::new(18, SearchOptions::default());
    let one = engine.search_simple(&board, player, &SearchLimits::fixed_depth(10));
    engine.clear_hash();
    let four = engine.search_simple(&board, player, &threads(SearchLimits::fixed_depth(10), 4));
    // The helpers search the same position until the main thread is done; only a very fast main search could stop
    // them before they searched as much as it.
    assert!(four.nodes > one.nodes, "{} nodes with 4 threads, {} with 1", four.nodes, one.nodes);
}

/// After searches with several threads, one thread searches exactly as a new engine (node for node).
#[test]
fn one_thread_after_several_is_unchanged() {
    let positions = benchmark_positions("midgame.txt");
    let mut engine = SearchEngine::new(16, SearchOptions::default());
    for (id, board, player, _) in positions.iter().step_by(10) {
        engine.search_simple(board, *player, &threads(SearchLimits::fixed_depth(8), 4));
        engine.clear_hash();
        let after = engine.search_simple(board, *player, &SearchLimits::fixed_depth(8));
        let fresh = SearchEngine::new(16, SearchOptions::default()).search_simple(
            board,
            *player,
            &SearchLimits::fixed_depth(8),
        );
        assert_eq!(
            (after.mv, after.score, after.nodes, after.evaluations),
            (fresh.mv, fresh.score, fresh.nodes, fresh.evaluations),
            "{id}"
        );
    }
}

/// Move Now and cancel from another thread stop a solve with 8 threads at once (FFO #49 takes several seconds).
#[test]
fn stop_signals_stop_all_threads() {
    let (_, board, player, _) = benchmark_positions("ffo-40-59.txt").into_iter().find(|p| p.0 == "49").unwrap();
    let mut engine = SearchEngine::new(18, SearchOptions::default());
    for move_now in [true, false] {
        let signals = Arc::new(SearchSignals::new());
        let setter = {
            let signals = Arc::clone(&signals);
            std::thread::spawn(move || {
                std::thread::sleep(Duration::from_millis(200));
                if move_now {
                    signals.move_now()
                } else {
                    signals.cancel()
                }
                Instant::now()
            })
        };

        engine.clear_hash();
        let result = engine.search(&board, player, &threads(SearchLimits::SOLVE, 8), None, signals.as_ref(), None);
        let returned = Instant::now();
        let signalled = setter.join().unwrap();
        assert!(returned.duration_since(signalled) < Duration::from_millis(200), "took {:?}", returned - signalled);
        if move_now {
            assert!(board.legal_moves(player) & result.unwrap().mv.0.unwrap().bit() != 0);
        } else {
            assert_eq!(result, Err(SearchError::Cancelled));
        }
    }

    // The engine still searches afterwards, with its threads.
    let result = engine.search_simple(&Board::INITIAL, Player::Black, &threads(SearchLimits::fixed_depth(6), 8));
    assert_eq!(result.depth, 6);
}

/// A time per move with 8 threads is kept (the helpers stop with the main thread's iterations and solves), and the
/// result is a legal move from a search that reached some depth.
#[test]
fn several_threads_keep_a_time_per_move() {
    let positions = benchmark_positions("midgame.txt");
    let mut engine = SearchEngine::new(18, SearchOptions::default());
    for (id, board, player, _) in positions.iter().step_by(25) {
        let clock = Instant::now();
        let result = engine.search_simple(board, *player, &threads(SearchLimits::time_per_move_ms(150), 8));
        let elapsed = clock.elapsed();
        assert!(elapsed < Duration::from_millis(150 + 40), "{id}: {elapsed:?}");
        assert!(result.depth >= 4, "{id}: depth {}", result.depth);
        assert!(board.legal_moves(*player) & result.mv.0.unwrap().bit() != 0, "{id}");
    }
}

/// Midgame searches that hand over to the solver (18–26 empty squares at depth 12), with 4 threads and with Multi-ProbCut
/// on and off: a win/loss/draw or exact result is that of 1 thread.
#[test]
fn several_threads_hand_over_to_the_solver_like_one() {
    let positions = benchmark_positions("midgame-handover.txt");
    for selective in [true, false] {
        let options = SearchOptions { selective, ..SearchOptions::default() };
        let (mut single, mut threaded) = (SearchEngine::new(16, options.clone()), SearchEngine::new(16, options));
        let mut solved = 0;
        for (id, board, player, _) in positions.iter().filter(|p| p.1.empty_count() <= 20).take(24) {
            single.clear_hash();
            threaded.clear_hash();
            let one = single.search_simple(board, *player, &SearchLimits::fixed_depth(12));
            let four = threaded.search_simple(board, *player, &threads(SearchLimits::fixed_depth(12), 4));
            assert_eq!(one.kind, four.kind, "{id}");
            match one.kind {
                ScoreKind::Exact => assert_eq!(one.score, four.score, "{id}"),
                ScoreKind::WinLossDraw => assert_eq!(one.score.signum(), four.score.signum(), "{id}"),
                _ => continue,
            }
            solved += 1;
        }
        assert!(solved >= 6, "{solved} solved");
    }
}

/// Positions with too few empty squares for a midgame iteration (the search goes straight to the solver): with a
/// depth limit and a time per move, several threads give the single-threaded score.
#[test]
fn several_threads_with_few_empty_squares_solve_like_one() {
    let mut seed = 0x2545_F491_4F6C_DD1Du64;
    let (mut single, mut threaded) =
        (SearchEngine::new(14, SearchOptions::default()), SearchEngine::new(14, SearchOptions::default()));
    for empties in 1..=10 {
        for _ in 0..4 {
            let Some((board, player)) = random_position(&mut seed, empties) else { continue };
            for limits in [SearchLimits::fixed_depth(12), SearchLimits::time_per_move_ms(100)] {
                let one = single.search_simple(&board, player, &limits);
                let eight = threaded.search_simple(&board, player, &threads(limits, 8));
                assert_eq!((eight.score, eight.kind), (one.score, one.kind), "{board} ({empties} empties)");
            }
        }
    }
}
