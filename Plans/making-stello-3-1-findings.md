# Findings from making Stello 3.1 and 3.2

What the work on Stello 3.1 and 3.2 teaches us, written down as we go: the decisions, the measurements, what worked and what did not, and the ideas for later.

- **The plan:** [making-stello-3-1-plan.md](making-stello-3-1-plan.md). This file is updated after every phase.
- **Where it starts:** Stello 3.0 as released on 4 November 2026 (main baa44e5, git tag `v3.0`). Its findings are in [making-stello-3-findings.md](making-stello-3-findings.md), and the docs are [chapter 25](../docs/25-phases-0-4-the-engine-in-rust.md) and [chapter 26](../docs/26-phases-5-9-threads.md).

## Status

| Phase | State | What it showed |
|---|---|---|
| **Part 1: Stello 3.1** | | |
| 0 Set-up and the new baseline | done (2026-11-04) | 3.0 one-thread JSON references, 105-signature baseline test, and Rust-engine `opt=` match switches; Stello tests pass |
| 1 Real bugs | done (2026-11-06) | Six bugs fixed: an aborted solve reported as Exact, the natural delay charged to the clock, the clock restore after Back/Forward, the endgame soft limit (gate matches at 1 and 5 min per game not worse), web worker robustness (review found a restart loop) and doc errors; items 1 and 2 checked in the apps |
| 2 The parity quirks | in progress | Item 1: equality cutoff passed the 500 ms not-worse match gate; A/B switch removed |
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

### A separate fix: one `obj/` folder per operating system (2026-11-05)
- **The problem:** the repository is built both on Windows and in the Linux sandbox, in the same folders. `dotnet restore` writes the machine's NuGet paths into `obj/project.assets.json`, so after a restore on one machine the other machine's build failed (or skipped the tests) until it restored again.
- **The fix:**
  - a new [Directory.Build.props](../Directory.Build.props) at the repository root sets `BaseIntermediateOutputPath` to `obj/$(OS)/`, which gives `obj/Windows_NT/` and `obj/Unix/`. It also excludes `obj/**` from the default items, so the files left in the old shared `obj/` are not compiled;
  - `Stello.Net/Directory.Build.props` imports it;
  - it also covers the GameShell submodule, which has no `Directory.Build.props` of its own.
- **Unchanged:** `bin/` stays shared, so the tool paths are the same. The old files in `obj/` can be deleted, but are not used.
- **Checks (Linux):**
  - restore and build give 0 warnings, and the assets are in `obj/Unix/`;
  - Stello.Engine.Tests 488, Egaroucid.Engine.Tests 42 and GameShell.Tests 40 pass;
  - mid-bench depth 10 is identical to `v3.0-ref`.
- **On Windows (checked by the agent on the user's machine):**
  - after deleting all 14 `obj\Windows_NT` folders, `dotnet build Stello.Net.slnx` without a manual restore restored all 14 projects by itself and built with 0 warnings;
  - `project.assets.json` there points only to `C:\Users\Z6CJP\.nuget\packages\`;
  - `Stello.Net.Tests` really ran this time (88 of 90; the 2 known score-display tests fail, for phase 3), and `StelloV3BaselineTests` passes 105 of 105.

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

- **Checked in the web app (2026-11-06, gate P1):**
  - **The setup:** a Release AOT publish of the branch and the live 3.0 site as the control; headless Chromium, 1 thread, 60 s per move, no book; games opened from a file, with Move Now pressed at a chosen moment.
  - **The results:**

    | Case | Live 3.0 | 3.1 |
    |---|---|---|
    | Move Now in the exact pass (26 empty squares, a win; interim "+2 discs") | "to the end (26 empty), +2 discs": reported as proven | "to the end (26 empty), Win": the completed win/loss/draw pass |
    | Move Now right after a draw is found in the win/loss/draw pass (28 empty squares) | "to the end (28 empty), 0 discs": reported as a proven draw | "20 plies, 0.0 discs": the draw move as an estimate |

  - **Unchanged:** the computer moved 0.03 s after Move Now in all four runs, and there were no page errors.
  - The script is `scratchpad/item1/item1.py`.

### Item 2: natural delay charged to the computer clock (fixed 2026-11-05)
- **What was wrong:** `ComputerMoveAsync` measured `elapsed` after `WaitForNaturalDelayAsync`, so the 250 ms UX delay (or its remainder) was deducted from the computer's game clock. Clock ticks during the delay could also time out a nearly exhausted computer clock.
- **The fix:** set the remaining clock from `searchTime`, captured immediately after the engine returns and before the natural delay; pause clock ticks during that delay. Chapters 09 and 14 already specify that the natural delay is not chargeable.
- **Test:** `TimePerGame_NaturalDelayIsNotChargedToComputerClock` in [MainViewModelTests.cs](../Stello.Net/Stello.Net.Tests/MainViewModelTests.cs) waits for a 1,200 ms natural delay and checks clock charge against the displayed search time. It failed before the fix (1.21 s charged for a 0.6 s search) and passes after.
- **Baseline:** unchanged. `StelloV3BaselineTests` passes, 105/105.
- **Windows tests:** `Stello.Net.Tests` passes 86/88, including the new test. The same two existing score-display tests still fail (`+40` vs `+0.4 discs` and `+20` vs `+0.2 discs`); this clock change does not affect them.

- **Checked in the WPF app by the user on Windows (2026-11-06, gate P1): passed.**

### Item 3: clock restore after Back and Forward (fixed 2026-11-05)
- **What was wrong:** snapshots were recorded only when the computer started a move, so two-human Back had no clock state to restore. After Redo, the computer's clock also kept the pre-move value instead of restoring the time already spent on that move.
- **The fix:** record both clocks for every ply when `RunAsync` reaches that position, and restore the exact destination-ply snapshot after Back or Forward. Opening a saved game initializes its existing positions with full clocks because the file contains no clock history.
- **Tests:** `TimePerGame_UndoInTwoHumanGameRestoresBothClocks` verifies that Back restores both clocks in a two-human timed game; `TimePerGame_RedoRestoresTheComputerTimeAfterItsMove` verifies that Redo restores the computer clock after its original search time was charged. Both failed before the fix: the two-human clock remained at 0:58 after Back, and the computer's clock returned to 1:00 instead of 0:59.122 after Redo.
- **Baseline:** unchanged. `StelloV3BaselineTests` passes, 105/105.
- **Windows tests:** `Stello.Net.Tests` passes 88/90, including both new tests. The same two existing score-display tests still fail (`+40` vs `+0.4 discs` and `+20` vs `+0.2 discs`).

### Item 4: endgame soft limit ignores elapsed search time (fixed 2026-11-05; both matches complete)
- **What was wrong:** the endgame soft limit was calculated as a duration from when the solver started, but the search compares it with elapsed time from the start of the whole search. If the solver started after 1,000 ms, with 10,000 ms left, the hard deadline was 5,500 ms while the soft deadline was incorrectly 3,000 ms instead of 4,000 ms. Pass 2 could therefore get a shortened window or never start. `with_soft_percent` also applied a custom percentage to the absolute hard deadline instead of the endgame window.
- **The fix:** offset the default and custom endgame soft limits by the time already used. Time-per-move percentages remain relative to the start of the search. Chapter 09 now documents both the offset and custom-percentage behavior.
- **Tests:** `endgame_soft_limit_includes_time_already_used` failed before the fix (3,000 ms rather than 4,000 ms); `endgame_soft_percent_applies_to_the_remaining_budget` checks the window at 1%, 50% and 100%. `solver_budget_preserves_other_time_control_modes` checks time per move, fixed depth and solve. Rust core passes 70/70 and FFI 5/5 in release; fmt and clippy are clean. The 8-thread time-per-move timing test previously exceeded its 190 ms ceiling in debug, but passes in release.
- **Baseline:** unchanged. `StelloV3BaselineTests` and `NativeEngineTests` pass 123/123 together, including all 105 baseline signatures.
- **Windows tests:** `Stello.Net.Tests` passes 88/90. The same two existing score-display tests fail (`+40` vs `+0.4 discs` and `+20` vs `+0.2 discs`); this Rust time-budget change does not affect the app clock tests.
- **A/B comparison:** a temporary switch reproduced 3.0's calculation for the matches (3,000 ms and 2,750 ms at 50%, versus 4,000 and 3,250 with the fix). After both matches and approval, the switch and its Rust, FFI, C# and match-tool plumbing were removed; unreleased ABI 4 returned to ABI 3. The corrected calculation is now the only path.
- **Switch-removal checks:** the native DLL reports ABI 3 and no longer exports the setter; the match tool rejects both removed switch values, and its parser tests pass 15/15. The .NET solution builds with 0 warnings using `StelloWasmEngine=false`; the WebAssembly target is not installed on this machine.
- **1-minute gate match:** [report](./benchmark-results/match_p1-item4-endgame-soft-60s-2026-11-05.md). 1,180 games, fixed 591 points vs legacy 589 (49.9%; 95% interval 48.6%–51.3%), Elo -1 (-10 to +9); no significant difference.
- **5-minute confirmation:** [report](./benchmark-results/match_p1-item4-endgame-soft-300s-300starts-2026-11-05.md). 600 games (300 starts, two games each), fixed 298 points vs legacy 302 (50.3%; 95% interval 48.9%–51.8%), Elo +2 (-8 to +12) for legacy; no significant difference. Wall time 7h 28m.
- **Gate: passed.** The gate is "not worse than 3.0": the fix's upper bound must be above 50 %. Its score was 50.1 % (48.7–51.4) at 1 minute and 49.7 % (48.2–51.1) at 5 minutes, so both upper bounds are above 50 %. The fix is kept for correctness: it makes the time control do what chapter 09 describes. As the identical depths and first solves show, it changes little in play.
- **Interpretation:** neither match found a significant difference; this does not establish equivalence or rule out a small regression. The 300 starts in the 5-minute match were also used in the 1-minute match, so the matches do not use independent sets of openings.


### Item 5: web worker robustness and late progress reports (fixed 2026-11-06)
- **What was wrong:**
  - a pool worker that failed to start was never retried, and `info()` reported `navigator.hardwareConcurrency` threads whatever had started;
  - a late `Progress<T>` report could be handled after the final result and overwrite the analysis (without a `SynchronizationContext`, or one posted just before the result).
- **The fix (the agent on Windows):**
  - `engine-worker.js` starts each pool worker in `startPoolWorker`, retries a failed start once, and stops trying after a second failure (`poolWorkerLimit`);
  - `info()` reports the threads that really started (`poolWorkers.length + 1`), updated through an `info` message when that changes;
  - searches are capped at the threads available;
  - `init` now waits until the pool workers are ready;
  - in `MainViewModel`, a per-search gate (`searchCompleted`, under a lock) closes the progress callback when the result arrives, so a late report is dropped in both apps.
- **Found in review and fixed: a restart loop when a pool worker fails.**
  - **The cause:** the worker's `error` event was not cancelled, so the error also reached the engine worker's global scope and the page. `engine-client.js` treats an error from the engine worker as a failed engine: it rejected the pending `init`, and the page started the engine again, which failed again.
  - **What was seen:** in headless Chromium, with a test copy whose pool workers fail to start, the engine never became ready and the computer never moved. The same happened when half of the starts failed.
  - **The fix:** `e.preventDefault()` in `startPoolWorker`'s error handler.
  - **After the fix:**
    - with every pool worker failing, the engine runs single-threaded in the threaded package, `info()` says 1 thread and the computer plays (3.9 M nodes/s);
    - with half of the starts failing, it plays on the threads that started (1–2), and reports them.
- **Measured in review** (Release AOT publish, headless Chromium, COOP/COEP, Automatic = 24 threads):
  - **The engine is ready about 150 ms later at page load** (960–1,020 ms against 780–920 ms before), because `init` now waits for the pool workers, which start one after another;
  - **the first computer move after the click** takes as long as before (850–1,360 ms in both).
- **Tests:**
  - `Play_LateProgressDoesNotOverwriteFinalAnalysis` (Stello.Net.Tests, Windows) sends a report after the result while the move is still pending, and checks that the analysis is unchanged. The agent reports all 50 `MainViewModelTests` passing;
  - the worker changes have no automated test (the Node tests use their own pool worker). They were checked by the browser runs above.


### Item 6: wrong statements in the docs (fixed 2026-11-06)
- **Corrected:**
  - perft(7) is 55,092, not 55,010 (audit findings);
  - Stello 1's search is fail-soft: ordinary nodes return their best score, and only the selective-search cutoffs return the window bound (`Stello.Engine.V1/SearchEngine.cs:278, 287`, checked in review). The 2.0 plan said "fail-hard";
  - chapter 15 now gives the web's real wording for Add Game to Book ("Who won the game?": Black won, White won, Cancel);
  - chapter 25 now says the 3.0 findings cover phases 0–9;
  - the audit plan's file, block and test counts and its memory figures are brought up to date (31 files and 91 fenced blocks for the linter; 4 MiB–1 GiB of tables, 64 MiB by default; 621 tests).
- **The audit plan's test gate is now marked as not met.** A full Windows run found 621 tests, of which 4 failed: the 2 known score-display tests (phase 3) and 2 timing-threshold tests.
- **Checks:** the docs snippet linter passes (31 files, 91 blocks), and the link check finds 0 broken links.

## Phase 2: the parity quirks

| Item | Verdict | Mid-bench nodes | Solve-bench nodes | Elo | Kept |
|---|---|---:|---:|---:|---|
| 1. Asymmetric hash cutoffs | Missed a valid equality cutoff; fixed | 3,719,468 → 3,716,389 (−0.083%) | 92,485,229 → 92,485,229 | +1 (−10 to +12) | Yes; 50.2 % (48.6–51.7), not worse |

### Item 1: an upper bound equal to alpha (2026-11-06)
- **What was wrong:** a stored upper bound equal to alpha still proves the position cannot exceed alpha. The old strict comparison missed that cutoff and searched unnecessarily; it was conservative rather than an incorrect score.
- **The fix:** an upper bound cuts off when its value is at or below alpha. The old strict comparison and its temporary match/benchmark switches have been removed after the match gate; the native ABI remains version 3. The frozen C# engines remain references, not a reason to retain the quirk.
- **A/B:** Release, one thread, cleared tables, strict → inclusive:
  - the default 100 midgame positions at depth 10, three interleaved passes: 3,719,468 → 3,716,389 nodes (−0.083%); best-per-position total 375.5 → 374.6 ms;
  - the same positions at depth 12, one pass: 10,582,535 → 10,579,515 nodes (−0.029%); 1,151.1 → 1,163.5 ms;
  - depth 10 with Multi-ProbCut off, one pass: 58,772,944 → 58,742,160 nodes (−0.052%); 5,954.6 → 6,020.3 ms;
  - FFO #40–44, one pass: 92,485,229 nodes in both, 2,247.9 → 2,269.7 ms; all known scores and moves are correct.

  Every tested move, score, kind and depth matched. The timing differences are small and noisy, not evidence of a speedup. The .NET `--compare` also reproduced all 100 strict midgame signatures and all five solve signatures; the inclusive midgame run differed in nodes at 29 positions.
- **Match gate:** the 1,180-game, 500 ms/move match against the strict cutoff finished in 21m 16s. Inclusive scored 592/1,180 (50.2 %, 95 % interval 48.6–51.7), with 517 wins, 150 draws and 513 losses; +1 Elo (−10 to +12). Average midgame depth was 18.45 for both, and nodes per second were 9.59M strict versus 9.62M inclusive. This meets the plan's not-worse criterion (upper bound above 50 %), but establishes no playing-strength gain. The [match report](benchmark-results/match_p2i1-upper-cutoff-20261106-094405.md) and [full result data](benchmark-results/match_p2i1-upper-cutoff-20261106-094405.json) contain the results; the local run log records the per-game lines.
- **Tests:** a direct table-entry regression searched 41 nodes before the fix and one after it. Boundary tests cover values below and above alpha, depth, move legality and other bound types; the native wrapper test pins the default node count.
- **Checks:** 80 Rust workspace tests, formatting and Clippy with warnings denied; 489 engine tests and 42 Egaroucid/benchmark tests; a web build with threads required and all 15 Node tests on the rebuilt packages. The Windows app tests passed 89 of 91: the two score-display expectation mismatches from item 6 still fail. The docs snippet linter passes, and the changed documents' local file links resolve.
- **The baseline (review):**
  - the agent had changed `StelloV3BaselineTests`, the Rust signature test and the Node tests to ignore midgame node counts; that would no longer catch an unintended change in the search;
  - instead, `stello-v3-signatures.txt` was regenerated: the node counts of the 29 changed midgame positions, with a note in its header. The script checked that no move, score, kind or depth changed;
  - the C# baseline is strict again (node for node), and the Rust and Node signature tests now read `stello-v3-signatures.txt` with full comparison, not the frozen 2.0 file;
  - the 2.0 file stays for `StelloV2Tests`, which pins only the frozen C# engine;
  - after the review: 80 Rust tests, 489 engine tests and 15 Node tests pass.
- **Verdict:** keep the correctness fix. The match found no significant playing-strength difference, so a 2 s gain confirmation is not warranted. The 500 ms gate is passed; the old code path and A/B switches have been removed.
