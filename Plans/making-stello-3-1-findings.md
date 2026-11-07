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
| 2 The parity quirks | in progress | Items 1 and 3a–5 are kept; item 6 reviewed with no fix needed: hash-move swapping and the final-singleton guard are harmless ordering details |
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
- **Speed (review):** stello-cli `solve-bench` FFO #40–49, pinned to one CPU, fastest of 3 passes, 2 interleaved rounds: before 25.9 / 25.7 s, after 26.0 / 25.6 s, so the earlier +3.3 % was noise. Over all ten positions the fix uses 0.08 % fewer nodes (1,099,769,823 → 1,098,937,112).

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
| 2. Upper-bound hash moves as response killers | Harmless ordering heuristic; unchanged | depth 10: 3,716,389 → 3,716,538 (+0.004%); depth 12: 10,579,515 → 10,578,457 (−0.010%) | Not applicable (endgame ordering is separate) | — | No |
| 3. Transposition table never ages | Old-root entries crowd the current search; aging (3a) kept, tighter-bound retention (3b) dropped | Kept 3a: 3,716,389 → 3,716,389 | Kept 3a: 92,485,229 → 92,485,229 | Confirmation at 2 s: +11 (−2 to +24) | 3a yes: match 5 was not worse; 3b no: no demonstrated gain |
| 4. Endgame deep-node stored move and split timing | ETC fail-low now retains its best move; a searched hash move counts for YBWC splitting | 3,716,389 → 3,716,389 (identical) | 92,485,229 → 92,551,133 (+0.071%) | +2 (−10 to +14) | Yes: 50.3 % (48.6–52.0), not worse; switch removed |

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

### Item 2: upper-bound hash cutoffs and response killers (investigated 2026-11-06)
- **What happens:** a transposition-table upper bound can cut off a child search and return its stored move. The parent records that reply in the response table (`record_response`): +4 if the parent's move was refuted (its score at or below alpha), else +1. A child's upper-bound cutoff means the child failed low, so the parent's score is at or above beta: the stored move is recorded with +1, as a candidate reply. It was the best move found in an earlier all-node, but was not searched in this visit. (The +4 weight goes to replies from a lower-bound cutoff in the child, which are proven refutations.) *Corrected in review: the agent's first text said +4.*
- **Verdict:** harmless ordering heuristic, not a result-correctness bug. Response killers affect only the shallow midgame move ordering; deeper nodes use evaluation ordering, and the endgame solver has its own ordering.
- **A/B measurement:** disabling this response update on the 100 standard midgame positions, one thread, default Multi-ProbCut and cleared hash tables:
  - depth 10, three passes: 3,716,389 → 3,716,538 nodes (+149, +0.004%); best-per-position total 0.386 → 0.395 s (−2.3%);
  - depth 12, three passes: 10,579,515 → 10,578,457 nodes (−1,058, −0.010%); 1.148 → 1.123 s (+2.2%).

  All moves and scores matched at both depths. The node differences are tiny and reverse direction between depths; the short timing samples are noisy and show no reliable gain. The solver is unaffected, so a solve benchmark and playing-strength match are not warranted.
- **Decision:** leave the heuristic unchanged. The temporary A/B switch was removed, the ABI remains version 3, and the strict 3.0 node signatures remain unchanged because the default search did not change.
- **Checks:** 80 Rust workspace tests, 489 engine tests, and 42 Egaroucid/benchmark tests pass; the 100-position node/move/score comparisons were deterministic across three passes.

### Item 3: aging transposition-table entries (2026-11-06; 3a kept, 3b dropped)
- **What was wrong:** entries for positions with more empty squares than the current root cannot occur in the current search, but their deep results stayed live during a game and could crowd out useful entries.
- **Kept fix (3a):** aging is unconditional in both midgame and endgame tables. Every search records the root's empty-square count and passes it to every thread. Probes ignore entries from earlier positions; replacement overwrites a stale first entry in place, leaving a live second entry alone. All other replacement rules remain Stello 3.0's: the deepest result takes the first place, a shallower result for that position is dropped, and the second place is otherwise always replaced. A probe returns one entry, not two. See [chapter 07](../docs/07-transposition-table.md#aging-across-moves).
- **Dropped strategy (3b):** keeping a tighter, shallower bound in the second place changed the replacement strategy rather than fixing aging. It showed no statistically supported gain in either match. Its replacement logic, two-bound probes and tests have been removed completely.
- **First combined version (historical, not the five split matches):** aging plus tighter-bound retention scored 579/1,180 (49.1%, 95% interval 47.2–50.9), −6 Elo (−19 to +6), at 500 ms/move. The implementation then changed, including a branch that could discard another live colliding position. Review required splitting the fix from the strategy and matching the actual proposed code. The [old report](benchmark-results/match_p2i3-tt-replacement.md) and [JSON](benchmark-results/match_p2i3-tt-replacement.json) remain as historical evidence, not measurements of the final code.
- **A/B before cleanup (Release, one thread, cleared tables between positions):**

  | Variant | Mid-bench depth 10, 3 passes | FFO #40–44 |
  |---|---:|---:|
  | 3.0 (current) | 3,716,389 | 92,485,229 |
  | 3a | 3,716,389 (identical) | 92,485,229 (identical) |
  | 3a+3b | 3,644,279 (−1.94%) | 92,485,227 |

  3a changes nothing in the benches: they clear the tables for each position, and no entry becomes stale inside one search. The removed 3b variant used fewer nodes in 94 midgame positions and more in 6; its only score change was `g18e30` (3,650 → 4,125, move `f1` unchanged). None of these 3b differences belongs in the kept baseline.
- **Matches (candidate was side B throughout; 12 games at a time, hash bits 19, one thread per engine, colours swapped per opening):**

  | # | Comparison | Games | Candidate score (95% interval) | Elo (95% interval) | Result |
  |---|---|---:|---:|---:|---|
  | 1 | 3a vs 3.0, 500 ms/move, starts 1–590 | 1,180 | 50.6% (49.0–52.2) | +4 (−7 to +15) | No significant difference; not worse under the fix rule |
  | 2 | 3a vs 3.0, 2 s/move, starts 1–300 | 600 | 47.5% (45.6–49.4) | −17 (−31 to −4) | Initial loss; independent confirmation requested |
  | 3 | 3a+3b vs 3a, 500 ms/move, starts 1–590 | 1,180 | 50.3% (48.6–52.0) | +2 (−9 to +14) | No significant gain |
  | 4 | 3a+3b vs 3a, 2 s/move, starts 1–300 | 600 | 51.3% (49.1–53.6) | +9 (−6 to +25) | No significant gain |
  | 5 | 3a vs 3.0, 2 s/move, starts 301–590 | 580 | 51.6% (49.7–53.4) | +11 (−2 to +24) | No significant difference; passes the agreed confirmation rule |

  Reports and full game data: [match 1](benchmark-results/match_p2i3a-500ms.md) ([JSON](benchmark-results/match_p2i3a-500ms.json)); [match 2](benchmark-results/match_p2i3a-2s.md) ([JSON](benchmark-results/match_p2i3a-2s.json)); [match 3](benchmark-results/match_p2i3b-500ms.md) ([JSON](benchmark-results/match_p2i3b-500ms.json)); [match 4](benchmark-results/match_p2i3b-2s.md) ([JSON](benchmark-results/match_p2i3b-2s.json)); [match 5](benchmark-results/match_p2i3a-2s-301-590.md) ([JSON](benchmark-results/match_p2i3a-2s-301-590.json)). Match 5 used the source opening-list header plus data positions 301–590, disjoint from match 2, and finished in 39m 43s with 256 wins, 86 draws and 238 losses.
- **Follow-up review:** persistent depth-12 searches over 34 game positions reproduced committed nodes exactly with aging off. The reviewer measured little difference while the table had room and about 9% fewer nodes with aging when the small table filled. This confirmed that aging was exercised, and motivated match 5 rather than immediately rejecting the correct fix.
- **Decision:** keep 3a and drop 3b. The agreed confirmation rule was to keep aging if match 5's upper confidence bound exceeded 50%; otherwise drop it if it lost again. Match 5 passes that rule. The earlier conclusion that neither qualified is superseded, and the initial 2 s loss is treated as a false alarm for this decision. This is not a statistically proven strength gain or proof of non-inferiority: the score interval still includes 50%. For context, the three aging-only matches total 1,181/2,360 points (50.04%, about +0.3 Elo from the pooled raw score), not +2 to +4 Elo; pooling different time controls is only descriptive.
- **Final cleanup:** aging is the only behaviour; the Rust/C#/match policy switches and native setter are gone, and the ABI is back to 3. The item-3-only `solve-bench --side` option is removed. `stello-v3-signatures.txt` is byte-for-byte identical to commit `bbe2a60`; strict move, score, kind, depth and node checks remain unchanged.
- **Regression:** `persistent_engine_ages_a_full_table_across_game_positions` keeps a default engine's 4,096-slot tables over 34 deterministic game positions at depth 12, pinning 2,156,127 nodes. Temporarily disabling replacement aging makes it fail at 2,274,056 nodes: aging saves 5.19% on this game. Aging was restored before final validation. The table unit test also checks that a stale first entry is replaced while a live second entry survives.
- **Final checks:** 82 Rust workspace tests, formatting and Clippy with warnings denied; all 489 engine tests (including the strict cleared-table baseline), all 42 Egaroucid/benchmark tests, and all 15 Node tests on rebuilt single-threaded and threaded WASM packages pass. The web build with threads required has no warnings or errors. The docs snippet linter passes (31 files, 91 blocks); all 77 local file links in the three changed documents resolve. All five split-match reports and JSON files, plus the historical first report, are retained for the reviewer's commit; no commit was made during cleanup.

### Item 4: endgame deep-node stored moves and split timing
- **What was wrong:** after an enhanced-transposition-cutoff (ETC) fail-low, `end_deep` kept the best cached score without retaining the move that supplied it. Separately, a hash move searched without improving that score did not reliably count as a searched move for the YBWC split decision.
- **A/B benchmarks:** with cleared tables and one thread, the 100 standard depth-10 midgame positions were identical at 3,716,389 nodes. FFO #40–44 kept every score and move; nodes changed from 92,485,229 to 92,551,133 (+0.071%), only #43 (33,415,521 → 33,446,615) and #44 (11,480,338 → 11,515,148) changed. The fastest-per-position timing went from 2.374 s without the fix to 2.297 s with it (3.3% faster, not slower; the review read the figure as a slowdown). The cleaned-up code took 2.228 s. With +0.07% nodes, both differences are noise between runs. The two-thread solve check also kept all scores and moves; its node counts varied under parallel search.
- **500 ms match gate:** 590 openings, two colors per opening (1,180 games), 12 games at a time, hash bits 19, one thread per engine. The candidate was side B and scored 593.5/1,180 points (50.3%): 509 wins, 169 draws and 502 losses. Its 95% score interval was 48.6%–52.0%, Elo +2 (−10 to +14), and likelihood of superiority 63.1%. Average depth was 18.46 versus 18.45, first solve 23.0 empties for both, and nodes per second 9.62M versus 9.61M. The match finished in 21m 14s.
- **Decision:** the result passes the agreed 500 ms not-worse criterion, but is not statistically significant and does not demonstrate a playing-strength gain. The review accepted the fix as a correctness fix, so it is kept under the not-worse rule.
- **Cleanup:** the fix is now the only code. The `end_deep_tt_move` option, the FFI setter (the ABI is back to 3), the C# property, the match key `opt=end-deep-tt-move=` and `solve-bench --side` are removed. The cleaned-up code reproduces the switch-on benchmarks node for node (midgame 3,716,389; FFO #40–44 92,551,133, all correct). `stello-v3-signatures.txt` is regenerated for FFO #43 and #44 with a note in its header; no move, score, kind or depth changed, the midgame lines are unchanged, and the C#, Rust and Node signature tests stay strict. The regression test `end_deep_stores_the_move_with_the_best_etc_fail_low_value` checks the stored move in the ETC case. Chapters [08](../docs/08-endgame-solver.md#threads-in-the-solver) and [26](../docs/26-phases-5-9-threads.md) describe the fix.
- **Checks after the cleanup:** formatting and Clippy with warnings denied; 83 Rust workspace tests; all 489 engine tests, including the strict baseline; and all 15 Node tests on the rebuilt Stello.Web packages, with and without threads.
- **Report and full game data:** [match report](benchmark-results/match_p2-item4-end-deep-500ms.md) and [JSON](benchmark-results/match_p2-item4-end-deep-500ms.json).

### Item 5: local-table-decided moves stay in the sort
- **What was changed in the candidate:** `end_local` and `end_simple` partition moves already marked done by the local-table prepass out of the active prefix, then sort only the unresolved moves. This can change search order in tied cases as well as reduce sorting work.
- **A/B benchmarks:** the 100 standard depth-10 midgame positions stayed node-for-node identical at 3,716,389. FFO #40–44 kept every move and score; nodes changed from 92,551,133 to 92,556,457 (+5,324, +0.0058%). On the 300-position depth-12 handover set, all moves and scores matched; 62 node counts changed, with a total increase from 320,286,428 to 320,291,282 (+4,854, +0.0015%). Midgame and FFO timings varied with run order; the candidate was 2.4% slower in the handover timing comparison. The samples do not establish a reliable speedup.
- **500 ms match:** 590 openings, two colors per opening (1,180 games), hash bits 19, one thread per engine. Candidate B scored 585 points (49.6%): 508 wins, 154 draws and 518 losses. Its 95% score interval was 48.0%–51.1%, Elo −3 (−14 to +8), and likelihood of superiority 29.6%. Average depth was 18.23 versus 18.22, first solve 22.8 empties for both, and nodes per second 8.38M versus 8.45M. The match finished in 21m 22s.
- **Assessment:** there was no significant playing-strength difference. The upper confidence bound exceeds 50%, so the match passes the agreed not-worse threshold; it does not demonstrate a playing-strength gain. Along with the inconclusive timing samples, this leaves the optimization's practical benefit unproven.
- **Decision:** kept under the not-worse rule. Decided moves no longer reorder the rest: the selection sort swaps the best remaining move to the front, so a decided move with a high value moved an undecided one out of its place. With the decided moves removed, `swap_next_best` sorts only the moves still to search, in their original order on ties.
- **Cleanup:** the fix is now the only code. `undone_first` moves the undecided moves to the front, and the loop runs over those only, so the `done` check inside it is gone. The `local_table_skip_done` option, the FFI setter `stello_engine_set_local_table_skip_done` (the ABI is back to 3), the C# property, the match key `opt=local-table-skip-done=` and `solve-bench --side` are removed. The cleaned-up code reproduces the switch-on benchmarks node for node: midgame 3,716,389 (unchanged) and FFO #40–44 92,556,457, all correct. `stello-v3-signatures.txt` is regenerated for FFO #40–44 with a note in its header; no move, score, kind or depth changed, the midgame lines are unchanged, and the C#, Rust and Node signature tests stay strict. The unit test `undone_first_keeps_decided_moves_out_of_the_sort` checks the partition and the sort that follows. [Chapter 08](../docs/08-endgame-solver.md#7-to-13-empty-squares) describes the fix.
- **Review speed check:** FFO #40–49 on one pinned core (Windows, alternating, 2 rounds): 27.3 s before and 27.3–27.4 s after; 1,098,937,112 against 1,098,913,729 nodes (−0.002%), all scores and moves correct. No speed difference.
- **Checks after the cleanup:** formatting and Clippy with warnings denied (native and `wasm32-unknown-unknown`); 84 Rust workspace tests; all 489 engine tests, including the strict baseline; all 42 Egaroucid/benchmark tests; and all 15 Node tests on the rebuilt Stello.Web packages, with and without threads. `Stello.Net.Tests` has only the 2 known score-display failures (phase 3).
- **Report and full game data:** [match report](benchmark-results/match_p2-item5-local-table-500ms.md) and [JSON](benchmark-results/match_p2-item5-local-table-500ms.json).

### Item 6: response ordering
- **What was questioned:** `MoveOrdering::order` swaps a legal hash move to the front, then promotes the prefix with nonzero response scores before sorting the rest by square value. Its `moves.len() - start >= 2` condition stops before consuming the final move.
- **Assessment:** no fix is needed. The swap is the same move-to-front operation used in the C++ `sortlist` and frozen C# 2.0 implementation; it can affect tie order among non-hash moves, but that is an ordering heuristic rather than a correctness issue. After the stable response-score sort, the loop skips entries with nonzero response scores so the square-value sort cannot disturb them. If only one move remains, leaving it untouched is equivalent: a singleton cannot be reordered. The Rust key array stays aligned with its sorted move slice.
- **Verdict:** harmless ordering details; no A/B switch, benchmark, match or signature change.
- **Review spot check:** the reviewer temporarily replaced the swap with a rotation that keeps the other moves in their original order (`moves[..=index].rotate_right(1)`) and compared it with the current code, using the Rust CLI on one thread with cleared tables. The rotation searched slightly more nodes in the midgame and the same in the solves:
  - 100 midgame positions at depth 10: 3,716,389 → 3,717,242 (+0.02%);
  - at depth 12: 10,579,515 → 10,584,497 (+0.05%);
  - FFO #40–44: identical at 92,556,457, all correct.

  The swap only breaks ties differently, and keeping it costs nothing. The experiment was reverted.
