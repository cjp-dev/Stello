# Findings from making Stello 3.1 and 3.2

What the work on Stello 3.1 and 3.2 teaches us, written down as we go: the decisions, the measurements, what worked and what did not, and the ideas for later.

- **The plan:** [making-stello-3-1-plan.md](making-stello-3-1-plan.md). This file is updated after every phase.
- **Where it starts:** Stello 3.0 as released on 4 November 2026 (main baa44e5, git tag `v3.0`). Its findings are in [making-stello-3-findings.md](making-stello-3-findings.md), and the docs are [chapter 25](../docs/25-phases-0-4-the-engine-in-rust.md) and [chapter 26](../docs/26-phases-5-9-threads.md).

## Status

| Phase | State | What it showed |
|---|---|---|
| **Part 1: Stello 3.1** | | |
| 0 Set-up and the new baseline | done (2026-11-04) | 3.0 one-thread JSON references, 105-signature baseline test, and Rust-engine `opt=` match switches; Stello tests pass |
| 1 Real bugs | in progress | Item 1: a stopped solve no longer reports a bound as Exact (nodes unchanged) |
| 2 The parity quirks | not started | |
| 3 The apps | not started | |
| 4 Tools and verification gaps | not started | |
| 5 The 3.1 release | not started | |
| **Part 2: Stello 3.2** | | |
| 6 Speed | not started | |
| 7 Threads | not started | |
| 8 Search | not started | |
| 9 The endgame | not started | |
| 10 Evaluation | not started | |
| 11 The opening book | not started | |
| 12 The 3.2 release | not started | |

## Phase 0: set-up and the new baseline (done 2026-11-04)
- **Branch and documents:**
  - the branch `feature/stello-3.1` starts from main baa44e5 (the 3.0 release);
  - the plan is [making-stello-3-1-plan.md](making-stello-3-1-plan.md) (a5b66e4), and this file holds the findings;
  - the git tag `v3.0` marks baa44e5.
- **The one-thread baseline JSONs** are in [benchmark-results/v3.0-ref](benchmark-results/v3.0-ref/). All runs used the Rust engine through `Stello.Benchmark`, 24 logical processors available, and one search thread:

  | Run | Searches | Nodes | Recorded time |
  |---|---:|---:|---:|
  | [FFO #40–49 exact](benchmark-results/v3.0-ref/ffo-40-49.json) | 10 | 1,099,769,823 | 27.88 s |
  | [Midgame depth 10](benchmark-results/v3.0-ref/midgame-depth10.json) | 100 | 3,719,468 | 0.387 s |
  | [Midgame depth 12](benchmark-results/v3.0-ref/midgame-depth12.json) | 100 | 10,582,535 | 1.081 s |
  | [1,000 random midgames, depth 8](benchmark-results/v3.0-ref/midgame-random-depth8.json) | 1,000 | 9,624,573 | 0.923 s |
  | [Handover, depth 12](benchmark-results/v3.0-ref/midgame-handover-depth12.json) | 300 | 320,339,863 | 7.372 s |
  | [Handover, depth 14](benchmark-results/v3.0-ref/midgame-handover-depth14.json) | 300 | 1,842,840,550 | 38.7 s |
  | [Random endgames, 14–24 empties](benchmark-results/v3.0-ref/endgame-random-14-24.json) | 500 | 27,720,283,198 | 567.0 s |

  Midgame JSONs use three passes and retain the fastest time per position; exact solves use one pass. These timings are machine-specific; the references are the per-position search results.
- **The baseline test:** [StelloV3BaselineTests.cs](../Stello.Net/Stello.Engine.Tests/StelloV3BaselineTests.cs) pins 100 midgame and 5 FFO signatures in `Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt`. It is independent of the frozen Stello 2.0 signatures. The existing 2.0 test now pins only the frozen 2.0 engine.
- **A/B match switches:** `MatchCommands` supports repeatable `opt=<key>=<value>` for `selective`, `soft-pct`, `mpc` and `ybwc`; existing direct named switches remain supported. Invalid, duplicate and non-Stello switches are rejected.
- **Checks:** all 488 `Stello.Engine.Tests` pass, including the 105 Stello 3.0 and 105 frozen Stello 2.0 signatures; all 15 focused match-parser tests pass. The full Egaroucid test run had 40 passes and two timing-bound failures (700 ms against a 500 ms ceiling; 352 ms against a 200 ms limit). All FFO scores/moves matched the position fixture; the 500 random-endgame results are recorded for later comparison.
- **Review in the sandbox (Linux):**
  - **the Windows references are node for node identical to the 2.0 references** recorded on Linux in the 3.0 work: FFO #40–49, depths 10 and 12, the 1,000 random positions, handover at depth 12 and the 500 random endgames. This also shows that the MSVC build of the Rust engine searches exactly like the Linux build. Handover at depth 14 is new and has no 2.0 reference;
  - the 3.0 signatures equal the 2.0 ones, with only the header comment different;
  - the solution builds with 0 warnings; 488 engine tests and all 42 Egaroucid tests pass. The two timing failures on Windows did not happen here, so they were load on that machine;
  - a smoke match with `opt=selective=off,opt=ybwc=off` plays and shows both switches in the header.

## Phase 1: real bugs

### Item 1: an aborted solve was reported as Exact (fixed 2026-11-04)
- **What was wrong:** `solve_root` ([engine.rs](../Stello.Rust/stello-core/src/search/engine.rs)) wrote each better root move into `best` with the pass's kind. A Move Now or the hard time limit in the middle of a pass then returned a bound as solved:
  - in pass 2 (exact), the best score so far is only a lower bound until every root move is searched, but it came back as `Exact`;
  - in pass 1 (win/loss/draw), a move found to draw came back as `Exact` 0, though a later move could still win; and with no midgame result before the solve, a first move that lost came back as a `WinLossDraw` loss.

  `BookSearch` stores `Exact` as `BookOrigin.Exact`, and `recalc` never searches an exact value again.
- **The fix:** a stopped pass keeps the last completed pass's result.
  - In pass 2, `solve_root` no longer touches `best`, so a stop returns pass 1's win/loss/draw result with its move.
  - In pass 1, a win still counts at once (one winning move proves it); a draw keeps its move as `Heuristic` 0; nothing else changes `best`. A loss leaves `best` as it was, which always holds a legal move: the midgame result or, at 8 empty squares or fewer (no ordering searches before the solve), the first ordered move with kind `None` and no score, as a single forced move is reported.
  - The completed passes set the result as before. `best` only feeds the reports and the result, so the search is unchanged.
  - The frozen C# engines (`Stello.Engine.V1`/`V2`) keep the old behaviour; they are references.
- **Tests:** three new Rust tests in [tests/search.rs](../Stello.Rust/stello-core/tests/search.rs) set Move Now from the progress callback. All three failed before the fix:
  - in the exact pass of FFO #40, the result must be `WinLossDraw`, a win, depth 20 (was `Exact`);
  - after the first draw report in the win/loss/draw pass of FFO #41, the result must not be `Exact` or `WinLossDraw` (was `Exact`);
  - after the first losing report in the win/loss/draw pass of a lost 8-empty position (−2, from FFO #41 by random play; no midgame result before the solve), the result must be a legal move with kind `None` (was a `WinLossDraw` loss). Had the pass completed, the result would be `WinLossDraw`, so the test also fails if the stop comes too late.
- **Baseline:** unchanged. `StelloV3BaselineTests` passes (105 signatures). `solve-bench` FFO #40–49 against `v3.0-ref/ffo-40-49.json` is identical: all 10 searches have the same nodes, move and score.
- **Checks:**
  - `cargo test --workspace --release` and clippy pass;
  - all 488 `Stello.Engine.Tests` pass, with the rebuilt native DLL;
  - `Stello.Net.Tests`: 85 of 87 pass. The two failures (`AnalysisViewModelTests.Update_ShowsTheSearchProgress` and `NewGuiFeaturesTests.AnalysisViewModel_SpeedAndUpdatedEvent`, expecting `+40` but getting `+0.4 discs`) fail the same way without this change. They are pre-existing, a heuristic-score display mismatch, and are left for phase 3.
- **The master book:**
  - The recalculation report (`Stello.Net/Book/opening-book.report.txt`, 60 s per position, 2026-11-02) came from Stello 2.0, which has the same fault.
  - The book holds 2,988 exact values found with a time limit (`X:60s`), at 17–31 empty squares after the move; 258 of them are draws.
  - A stopped solve stores the same depth (all empty squares) as a finished one, so the book cannot tell them apart. The risk grows with the empty squares: 933 have 26 or more.
  - They are marked for the phase 11 recalculation in the plan. The 1,091 exact values without a time limit are not affected.
