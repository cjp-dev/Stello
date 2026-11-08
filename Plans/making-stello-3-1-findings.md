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
| 2 The parity quirks | done (2026-11-07) | Items 1 and 3a–5 are kept; items 6–8 reviewed with no fix needed; item 9's counting is unified and its strict baseline regenerated. The combined fixes passed Gate P2 at 500 ms and 2 s without evidence of a loss |
| 3 The apps | done (2026-11-08) | Shared view-model foundation, Computer vs Computer in both apps, full WPF alignment (layout, board styles, 3D discs and flips, sound, Help), the Stello 1 archive stays on GitHub, mobile landscape checked; Gate P3 passed with the user's WPF check |
| 4 Tools and verification gaps | in progress | Step 1: old plan links repaired and GUI specification replaced by a GameShell pointer. Step 2: match workers default to physical cores divided by the larger side's thread count |
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

### Item 7: full windows for shallow root moves
- **What was questioned:** `search_root` uses a full window for every root move while `look <= 2` (search depths 1–3); principal-variation null-window searches with full-window re-search begin only at greater depths.
- **Trace:** C# 2.0 has the same `i == 0 || look <= 2` condition. In the C++ driver, `zero_findmax` is called only when `varlook > 2` and there is more than one root move; shallower searches use `findmax`/`findmax1` instead.
- **Verdict:** intentional shallow-search policy, not a bug. No change, A/B switch, benchmark, match or signature update was warranted.
- **Review note:** at depths 1–3 every root move is searched with the current `(alpha, beta)` window, where alpha rises as moves improve. This is plain alpha-beta, so the root scores are correct either way, and only the speed can differ.
- **Review spot check:** the reviewer temporarily used null-window PVS at every depth (`if i == 0`) and compared it with the current code, using the Rust CLI on one thread with cleared tables. The results go both ways:
  - 100 midgame positions at depth 4: 61,263 → 61,003 (−0.4%);
  - at depth 10: 3,716,389 → 3,712,087 (−0.12%);
  - at depth 12: 10,579,515 → 10,625,421 (+0.43%);
  - FFO #40–44: 92,556,457 → 92,556,466 (+9), all correct.

  There is no consistent gain, and a strength-only change must show one, so the policy stays. The experiment was reverted.

### Item 8: Multi-ProbCut and the transposition table
- **What was questioned:** the Multi-ProbCut probe searches the same board, side and ply as the main search, but at a shallower depth. That nested search can use the normal transposition-table get and put paths.
- **Trace:** a transposition-table cutoff requires sufficient depth and a bound compatible with the search window. A store protects a deeper same-position entry when it occupies slot 0; if it is in slot 1 behind a deeper unrelated entry, a shallower store can replace it. The older C++ `SELEXT` path also calls its shallow search on the same board and depth before the normal hash lookup.
- **Experiment:** disabling only stores at the Multi-ProbCut root, while keeping its reads, on the 100 midgame benchmark positions at depth 10 (three passes, hash bits 19) changed total nodes from 3,716,389 to 3,890,985 (+4.7%) and fastest total time from 0.371 s to 0.390 s (−4.8% speed). Disabling both reads and stores was also tested, but it conflated the suspected replacement effect with losing useful table hits, so it was not used for the decision.
- **Review spot check:** the reviewer rebuilt the same store-only experiment: no store at the ply where the Multi-ProbCut search starts while `in_probcut` is set. Using the Rust CLI on one thread with cleared tables, it reproduced the depth-10 total exactly (3,890,985). At depth 12 the total went from 10,579,515 to 11,239,221 (+6.2%). FFO #40–44 stayed identical at 92,556,457, all correct.

  The moves and scores were not all identical, contrary to the agent's report:
  - depth 10: every move is the same, but one score differs (g07e26, c1 −1287 → −1962);
  - depth 12: two moves differ (g01e36 b4 → b6, g07e42 d7 → f5), and one more score (g09e30, f7 +100 → −100).

  This is expected from a selective search whose cuts depend on the table's contents. It does not change the verdict.
- **Timing test:** the agent's run had one failure, `several_threads_keep_a_time_per_move` (467 ms against 190 ms). It is the known load-sensitive test (handoff, section 7). On review it passed three times out of three, and the full Rust workspace passed (84 tests).
- **Verdict:** the slot-1 replacement case exists, but suppressing the shallow stores made the fixed-depth search slower and gave no benefit (a few selective results differ either way). Keep the existing shared-table behavior; no code change or match was warranted, and the cleared-table signatures remain unchanged.

### Item 9: consistent node and evaluation counts
- **What changed:** every counted recursive node and explicit pass now goes through `Searcher::count_node`, which checks cancellation, Move Now and the hard time limit at the shared 1,024-node threshold. `ordering_value`, `solve_shallow`, `solve_last2` and `last4`–`last1` no longer have paths that can skip the check. Heuristic leaves and exact terminal-score evaluations are counted consistently across the midgame, endgame and ordering paths.
- **Correctness and counts:** the 100 depth-10 midgame benchmark positions remain at 3,716,389 nodes, with every move, score, kind and depth unchanged. FFO #40–44 also kept every move, score, kind and depth; their nodes changed from 92,556,457 to 93,697,164 (+1.23%). All results matched the pre-change snapshots.
- **Timing:** the one-thread release runs measured 370.1 → 380.8 ms for the 100 midgame positions and 2,177.2 → 2,262.8 ms for FFO #40–44. The latter includes the 1.23% increase in counted nodes; the timing samples are small and do not indicate a playing-strength change. No match is needed for a statistics-only correction.
- **Baseline:** only the five FFO solve signatures changed; the 100 midgame signatures are unchanged. The shared `stello-v3-signatures.txt` remains strict, including nodes, for C#, Rust and Node. The dated header records item 9.
- **Regression tests:** the core tests verify consistent terminal evaluation counts and cancellation at the shared threshold in `ordering_value`, `solve_shallow`, `solve_last2` and every last-square routine. The native Move Now test now checks a legal partial result and that the next search is unaffected; its depth can differ from frozen 2.0 because the newly reachable check may stop earlier. No search path increments the node counter outside `count_node`.
- **Checks:** all 85 Rust workspace tests except one timing guard pass; `several_threads_keep_a_time_per_move` fails alone at 399.9 ms against its 190 ms limit (the known load-sensitive test, also seen before item 9). Clippy passes with warnings denied; all 489 .NET engine tests pass; the web build has no warnings, and all 15 single- and multi-threaded WASM Node tests pass.
- **Review speed check:** the reviewer timed HEAD (`921fc9f`) against the candidate with the Rust CLI on one pinned core, alternating, 3 rounds each:
  - FFO #40–49: 27.4–27.8 s before and 27.9–28.1 s after, about +1.4%. The counted nodes went from 1,098,913,729 to 1,115,666,701 (+1.5%) for the same tree. The cost is the extra checks and error propagation in `last4`–`last1`.
  - Midgame depth 12 (100 positions, 3 passes): 1.099–1.117 s before and 1.104–1.112 s after, with identical nodes. No difference.

  The agent's larger midgame difference (370 → 381 ms) was noise between runs. About 1 % in the solver can't change the play measurably; phase 6 (speed) may win it back, for example by counting the last-square nodes in a batch.
- **Review checks:** formatting, and Clippy with warnings denied (native and wasm); all 86 Rust workspace tests, including `several_threads_keep_a_time_per_move`, which passed here; the .NET build with no warnings; all 489 engine tests (strict baseline); all 42 Egaroucid/benchmark tests; and all 15 Node tests. `Stello.Net.Tests` has only the 2 known score-display failures. `solve-bench` reproduces the new FFO #40–44 signatures (93,697,164 nodes, all correct), and no node increment remains outside `count_node`.
- **Verdict:** keep the consistent accounting and reachable abort checks. The change fixes statistics and cancellation responsiveness without changing any benchmark move or score.

### Gate P2: all kept fixes against 3.0 (2026-11-07)
- **Setup:** the candidate was 3.1 with kept phase 2 fixes 1, 3a, 4 and 5, plus item 9's count-only change; the opponent used the temporary `v30_search` switch. Both ran with 12 workers, one thread per engine, hash bits 19, and colors swapped within each opening.
- **Results:**

  | Time | Games | 3.1 points / score | 3.1 W–D–L | 3.1 score 95% interval | Elo (95% interval) | Duration |
  |---|---:|---:|---:|---:|---:|---:|
  | 500 ms/move | 1,180 (590 starts × 2 colors) | 603.5 / 51.1% | 527–153–500 | 49.5–52.8% | +8 (−3 to +19) | 21m 20s |
  | 2 s/move | 600 (300 starts × 2 colors) | 299 / 49.8% | 245–108–247 | 47.9–51.7% | −1 (−14 to +12) | 40m 36s |

  In the first match, 3.0 scored 576.5/1,180 (48.9%); in the second, it scored 301/600 (50.2%). The search controls were closely matched: average midgame depth was 18.21 vs. 18.20 and first solve 22.7 empties for both at 500 ms; at 2 s, depth was 20.59 for both and first solve 24.4 empties for both. Throughput was 8.46M vs. 8.48M nodes/s at 500 ms and 8.55M vs. 8.58M at 2 s (3.1 vs. 3.0).
- **Assessment:** neither result is statistically significant; both 3.1 intervals include 50%. Both pass the agreed not-worse gate (the upper confidence bound is above 50%), so the combined fixes are kept. This is not evidence of a playing-strength gain or a formal proof of non-inferiority.
- **Decision:** Gate P2 is complete. Keep items 1, 3a, 4 and 5.
- **Review:** the reviewer checked that the switch restored 3.0 exactly before the matches:
  - item 1's strict `value < alpha`;
  - 3a through a root of 64 empty squares, which reduces the table's probe and replacement to 3.0's (`root_empties` is used only there);
  - the old code of items 4 and 5, line for line.

  With the switch on, the 100 midgame signatures equalled the 3.0 baseline (`e0a6cf7`) node for node, 3,719,468 in total, also through the match tool. FFO #40–44 kept 3.0's moves and scores. The persistent 34-position game gave 2,260,501 nodes, the same as `17a27ff` (the commit before item 1). The brief's target of 2,274,056 was aging alone with item 1 kept, and the switch gives that too when only its table part is on. The match tool was rebuilt from the reviewed code before the matches.
- **Longest move:** in both reports 3.1 (side A) has the longer longest move (625 against 502 ms, 2,124 against 2,002 ms). This isn't 3.1: side A has the longer longest move in every phase 2 match (541–642 ms against 506–509 ms at 500 ms), also when A was 3.0. It looks like a start-up cost on side A in the match tool; phase 4 (tools) can look at it.
- **Cleanup:** the `v30_search` switch, its FFI setter (the ABI is back to 3), the C# property, the match key `opt=v30-search=`, `solve-bench --side`, the temporary tests and the chapter 14 ABI change are removed. The code is byte for byte that of `f3fdb8c`.
- **Reports and full game data:** [500 ms report](benchmark-results/match_p2-gate-500ms.md) ([JSON](benchmark-results/match_p2-gate-500ms.json)); [2 s report](benchmark-results/match_p2-gate-2s.md) ([JSON](benchmark-results/match_p2-gate-2s.json)).

## Phase 3: the apps

### Step 1: the two score-display tests (2026-11-07)
- **What was wrong:** `AnalysisViewModelTests.Update_ShowsTheSearchProgress` and `NewGuiFeaturesTests.AnalysisViewModel_SpeedAndUpdatedEvent` expected `+40` and `+20` for heuristic scores of 40 and 20. The app shows `+0.4 discs` and `+0.2 discs`, the intended format that the `FormatScore` theory pins (`+1.2 discs` for 123). The tests were stale, not the app.
- **The fix:** the two expected values are now `+0.4 discs` and `+0.2 discs`. No app code changed.
- **Checks:** the .NET Release build with 0 warnings; Stello.Engine.Tests 489/489, Egaroucid.Engine.Tests 42/42, and Stello.Net.Tests 91/91, the first full pass on Windows in 3.1.

### Step 2: shared App foundation (2026-11-07)
- **View-model state:** `MainViewModel` now raises `PropertyChanged` for the computed turn, game-over, undo/redo, clock-visibility and settings-badge properties. It also exposes `IsBlackActive`/`IsWhiteActive` and paired move-history rows with the latest row marked. `Home.razor` consumes those values instead of calculating them.
- **Shared settings and appearance:** `SettingsViewModel.TranspositionTableChoices` is the single list used by the WPF and web settings dialogs. `Appearance` and `BoardStyle` now live in `Stello.App`; the web keeps using its existing appearance local-storage key, while WPF stores the same appearance model in `settings.json`. `Sound` is saved in `AppSettings` by both settings stores. Older settings files with no sound field normalize to sound on.
- **Tests:** added coverage for active-player flags, paired/latest history rows, computed-property notifications, the shared table-choice list, and appearance/sound settings persistence and legacy defaults. Docs 14 and 15 now describe the shared model and persistence.
- **Checks:** Release solution build with 0 warnings/errors; Stello.Engine.Tests 489/489, Egaroucid.Engine.Tests 42/42, Stello.Net.Tests 98/98, and all 15 WASM Node tests pass.
- **Review fixes:**
  - The WPF slider's members (`TranspositionTablePower`, its minimum and maximum, `TranspositionTableEntriesText`) were used only by an old test after the slider became a combo box. They are removed, and the test checks the shared choices instead.
  - A saved capacity outside the five choices, for example 131,072 (an odd power of two, which `GameSettings.Normalize` keeps), would have left the selection empty in both dialogs. The old slider snapped such values instead. `SettingsViewModel` now shows the nearest smaller choice, and a theory test checks it.
  - After the fixes, Stello.Net.Tests passes 98/98, and the other suites are unchanged.
- **Review note resolved in step 5:** `Home.razor` now renders and changes `MainViewModel.Appearance`, initialized from the existing `AppearanceStore` key for compatibility. Changes save both `AppSettings.Appearance` and that key, so the web's saved settings no longer carry a stale/default appearance. A browser check confirms menu changes, reload and conflicting legacy settings keep the two stores synchronized.

### Step 3: Computer vs Computer (2026-11-07)
- **Mode and game loop:** CvC is selectable from the Game menu in both apps. The game loop treats neither side as human and continues searching after each move; both player labels show Computer, and the active side's clock and timeout are used. The existing 250 ms natural delay applies between moves, and Move Now advances the game immediately.
- **Controls:** New Game cancels and restarts the CvC loop; Switch Sides cancels it and changes to vs Computer (White), giving the human White; Undo cancels the current search and undoes one move. Save serializes the played moves as a valid game record. CvC timeouts end without a human continue prompt.
- **Tests:** verified both sides move, both clocks run only for the side to move, Switch Sides/New Game/Undo stop safely, timeouts do not prompt, and a saved CvC game round-trips through the game-record parser.
- **Checks:** Release solution build with 0 warnings/errors; Stello.Engine.Tests 489/489, Egaroucid.Engine.Tests 42/42, Stello.Net.Tests 104/104, and all 15 WASM Node tests pass.
- **Review:**
  - **Menu check fixed:** the WPF item had `IsCheckable="True"` with a one-way `IsChecked` binding. A click set the check mark locally, which replaces the binding, so a cancelled "Abandon current match?" question left it ticked and later mode changes no longer updated it. `IsCheckable` is removed; a WPF menu item still shows the tick when `IsChecked` is true.
  - **Switch Sides is consistent:** starting CvC makes Black the game's human player, and Switch Sides flips it to White as the mode becomes vs Computer (White).
  - **Live check:** the Release WPF app was driven through UI Automation, with the user's `settings.json` backed up and restored. The Game menu's "vs Computer (both sides)" is enabled; invoking it shows "Computer vs Computer", the clock runs, and the discs went from 2–2 to 10–14 in 16 s.
  - **The pause between moves** is the existing 250 ms natural delay. That is short for watching fast settings (a low fixed depth); step 4 or the user's check may want a longer one.
  - **Undo** takes back one move, and the computer then plays on from there at once. **Redo** is not available while CvC runs, because the computer is always thinking.

### Step 4: WPF layout and controls (2026-11-07)
- **Menus:** WPF now has the File, Game, Options, Book and Help menus aligned with the web wording, including all four game modes, appearance/sound options and the F2 settings shortcut. Add Game to Book is in File; the Book menu keeps Evaluate Book, Self-play and Stop Learning.
- **Side panels:** the status card has the modified-game heading, Othello/time-limit badges, thinking bar and Black/White player cards with role, both clocks, disc and legal-move counts, and active-turn highlights. Undo, Redo and Move Now are buttons; the Ctrl+Z/Ctrl+Y shortcuts and Ctrl+T/Ctrl+F aliases remain.
- **History and analysis:** WPF now shows paired, highlighted move rows, a move-count heading, an empty-state message and auto-scroll. Analysis uses the uppercase heading and displays `-` for empty fields. The settings dialog now uses the web wording.
- **Staged for step 5:** the board-style and piece-animation choices update and persist the shared appearance, but the renderer is still the old green board. Sound is stored but WPF still uses `NullSoundService`. User guide and documentation menu items are visible but disabled until their browser links are added.
- **Tests:** added checks for the move-history heading and empty state, plus the empty-analysis-field converter.
- **Checks:** Release solution build with 0 warnings/errors; Stello.Engine.Tests 489/489, Egaroucid.Engine.Tests 42/42 and Stello.Net.Tests 106/106. The web code was not changed, so the 15 Node tests were not rerun. WPF visual behavior remains for the user's Phase 3 hand check.
- **Review adjustment (2026-11-08):** the WPF status header now spaces STATUS, Othello and the thinking limit across the row like the web app. A WPF layout test checks normal and modified games, two widths, the longest time limit and the hidden-limit state; Stello.Net.Tests passes 107/107.
- **Settings crash fixed (2026-11-08):** opening Settings threw an unhandled `InvalidOperationException`: the WPF `Run.Text` binding defaulted to two-way, but `SettingsViewModel.DeviceThreads` is read-only. The binding is now explicitly one-way. A regression test reproduces the original exception by showing the real modal dialog, then verifies the thread label and changes to time, table capacity and threads after the fix. Earlier tests exercised the settings view model but did not show the dialog.
- **Analysis visibility fixed (2026-11-08):** the Analysis card bound its visibility to `IsAnalysisVisible` after replacing its own data context with `AnalysisViewModel`, which has no such property. Its outer border now keeps `MainViewModel` as its context and only the inner fields use `AnalysisViewModel`. A real WPF window test exercises the menu repeatedly, the Ctrl+A command binding, saved visibility across a new window, and live analysis fields. It reproduced the visible-when-disabled bug before the fix.
- **Review (screenshots of the Release app driven through UI Automation, with the user's settings backed up and restored):**
  - **Start of a game:** the menus, the STATUS card with its badges, the player cards, the disabled Move Now button, ANALYSIS with "-" fields and the empty MOVE HISTORY all look right at the default 860 × 640 window.
  - **Fixed, the side panel scrolled away during play:** at the default size the side panel is taller than the window, so it has an outer scroll bar. The history list's auto-scroll asked WPF to bring the latest row into view, and WPF passed that request on to the outer panel. During a Computer vs Computer game this scrolled the STATUS card and the player names off the top. `MainWindow` now marks the history list's request as handled, so only the list scrolls. A new screenshot shows the status and the cards staying in place.
  - **Still for the user's check:** at the default size the move history is partly below the window, behind the side panel's scroll bar. At 640 × 480 the board shrinks and its coordinate labels get small. Whether the side panel should fit without scrolling is a layout choice for the Gate P3 check.

### Step 5: WPF board, sound and Help (2026-11-08)
- **Board:** all three styles follow `MainViewModel.Appearance`; Golden Oak and Reddish Wood are embedded as linked resources from the existing web textures, without duplicate image files. Green cells, coordinates, hint/last-move colors and sizes follow the web CSS. 3D faces use radial gradients, contrasting rims and shadows; flat faces remain available.
- **Animation:** actual human/computer moves increment a shared move version. WPF animates placement and distance-staggered flips, changing faces at the midpoint; navigation and appearance changes cancel/redraw without flips. Windows' client-area animation preference is respected.
- **Sound:** WPF now injects an `ISoundService` built on `SoundWaves` and a background FIFO `SoundPlayer` queue, following the Connect-4 pattern. Options → Sound controls playback and remembers the setting. Muting invalidates queued sounds even if sound is immediately reenabled; an already-playing sound can finish. Targeted playback failures are traced and notified, and shutdown disposes the service.
- **Help:** enabled menu commands and F1/Shift+F1 open the live user-guide chapter and documentation index in the default browser. Launch failures show an error dialog.
- **Web appearance:** the step 2 review note above is settled by using the shared view model as the sole rendered/menu state while preserving the existing appearance key as startup authority.
- **Tests:** six new tests cover the embedded textures, flat/3D faces, marker colors, actual-move animation and navigation/appearance cancellation, Help commands/routes, queued generated waves, muting and explicit playback errors. The first board run found frozen template transforms; each square now clones its transform before animation. The animation test pumps the WPF dispatcher to verify both the old face and the completed new face.
- **Checks:** Release solution build with 0 warnings/errors; Stello.Net.Tests 113/113, Stello.Engine.Tests 489/489, Egaroucid.Engine.Tests 42/42, Node 15/15. Browser smoke checks of the Release web build verify board/toggle restoration and matching appearance entries even with conflicting legacy AppSettings. The temporary server was stopped. No engine/search/signature changes; no commit or push.
- **Settings follow-up:** after the step 4 Settings crash fix above, the Release build has 0 warnings/errors and Stello.Net.Tests passes 114/114; Stello.Engine.Tests 489/489 and Egaroucid.Engine.Tests 42/42 still pass.
- **Analysis follow-up:** after the visibility fix above, the Debug WPF tests and Release build pass with 0 warnings/errors; Stello.Net.Tests passes 115/115, Stello.Engine.Tests 489/489 and Egaroucid.Engine.Tests 42/42. The web was unchanged, so Node tests were not rerun.
- **User check:** inspect all three styles and 3D/flip toggles, play and undo/redo, listen to move/pass/result sounds and mute, restart to check saved choices, and try both Help menu entries and keyboard shortcuts. Stop after step 5; the docs-viewer decision and mobile landscape remain steps 6–7.
- **Review:**
  - **The step 5 report the agent gave first was wrong:** it described this work before anything was in the working tree, which was clean at `0eb00ea`. The work was then done on a second request.
  - **The two errors the user found are real fixes:**
    - Settings crashed because `Run.Text` binds two-way by default and `DeviceThreads` is read-only; `Mode=OneWay` fixes it, and `SettingsWindowTests` opens the real dialog.
    - The Analysis card's `DataContext` was the analysis view model, so its `IsAnalysisVisible` binding found nothing and the card never hid; the `DataContext` is now on the inner panel.
  - **Screenshots of the Release app** (UI Automation, settings backed up and restored): the Green and Reddish Wood boards with the web's textures, coordinates and green hint dots; the 3D discs with shading and shadow, and flat discs after Options → 3D Pieces; Options → Analysis hides and shows the card. The saved file then held `{"Board": "ReddishWood", "Pieces3D": false, "AnimateFlips": true}`.
  - **The Help links** point at the live site's `/docs/15-gui-and-settings` and `/docs/`, the same chapters as the web's F1 and Shift+F1. The address is the one in `README.md`; if the site moves, `MainWindow.UserGuideUri` must change with it.
  - **The flip animation and the sounds** can't be judged from screenshots; they are for the user's check.

### Step 6: the Stello 1 archive (2026-11-08)
- **Decision:** keep linking `docs/stello-1/` to GitHub; do not render it in the GameShell viewer. The archive is a frozen record, and GitHub renders its formulas, Mermaid diagrams and images.
- **Reason:** rendering the archive in the viewer would mean a GameShell change shared with Connect-4, Checkers and Gomoku for consistency of look only. The existing GitHub links already serve the archive; no Stello code or GameShell change is needed.
- **Existing behavior:** GameShell `b1cd813` already handles Markdown links into docs subfolders in `BrainDocs.RewriteUrl` (since `10525ab`). Stello registers `https://github.com/cjp-dev/Stello/blob/main/docs/` as `RepositoryFolder`, so the archive opens from `main`, preserving anchors, with `target="_blank"` and `rel="noopener"`. The plan's former "Today it is raw markdown" statement was out of date.
- **Verification:** a throwaway console check used the existing GameShell assembly and `BrainDocs.GetPageAsync` to render the actual top-level docs. All **24 link occurrences in 13 files**, covering **14 distinct targets**, had the expected GitHub URL, retained anchor and safe new-tab attributes. HTTP HEAD requests to the **nine distinct URLs without anchors** all returned **200**. The temporary check was removed; no app build or existing test suites were needed for these docs-only changes.

| Distinct target | HTTP status (without anchor) |
|---|---|
| [stello-1/01-overview.md](https://github.com/cjp-dev/Stello/blob/main/docs/stello-1/01-overview.md) | 200 |
| [stello-1/04-evaluation.md](https://github.com/cjp-dev/Stello/blob/main/docs/stello-1/04-evaluation.md) | 200 |
| [stello-1/04-evaluation.md#dangerous-corner-moves](https://github.com/cjp-dev/Stello/blob/main/docs/stello-1/04-evaluation.md#dangerous-corner-moves) | 200 |
| [stello-1/04-evaluation.md#the-edge-tables](https://github.com/cjp-dev/Stello/blob/main/docs/stello-1/04-evaluation.md#the-edge-tables) | 200 |
| [stello-1/06-midgame-search.md](https://github.com/cjp-dev/Stello/blob/main/docs/stello-1/06-midgame-search.md) | 200 |
| [stello-1/06-midgame-search.md#selective-search](https://github.com/cjp-dev/Stello/blob/main/docs/stello-1/06-midgame-search.md#selective-search) | 200 |
| [stello-1/07-transposition-table.md](https://github.com/cjp-dev/Stello/blob/main/docs/stello-1/07-transposition-table.md) | 200 |
| [stello-1/08-endgame-solver.md](https://github.com/cjp-dev/Stello/blob/main/docs/stello-1/08-endgame-solver.md) | 200 |
| [stello-1/08-endgame-solver.md#worked-example-the-ffo-positions](https://github.com/cjp-dev/Stello/blob/main/docs/stello-1/08-endgame-solver.md#worked-example-the-ffo-positions) | 200 |
| [stello-1/10-opening-book.md](https://github.com/cjp-dev/Stello/blob/main/docs/stello-1/10-opening-book.md) | 200 |
| [stello-1/12-book-tool.md](https://github.com/cjp-dev/Stello/blob/main/docs/stello-1/12-book-tool.md) | 200 |
| [stello-1/12-book-tool.md#recalculating-the-master-book](https://github.com/cjp-dev/Stello/blob/main/docs/stello-1/12-book-tool.md#recalculating-the-master-book) | 200 |
| [stello-1/16-glossary.md](https://github.com/cjp-dev/Stello/blob/main/docs/stello-1/16-glossary.md) | 200 |
| [stello-1/README.md](https://github.com/cjp-dev/Stello/blob/main/docs/stello-1/README.md) | 200 |

- **Next:** step 7, mobile landscape, then Gate P3. No commit or push in this step.

### Step 7: mobile landscape (2026-11-08)
- **Publish and server:** `Stello.Web` published in Release and served from the publish output with `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`.
- **Headless Chromium:** the app loaded at **844×390**; `crossOriginIsolated` was true, with no console errors, page errors or failed requests.
- **Layout:** document width is 844 px, matching the viewport, with no horizontal overflow. The board is 312×312 px and the side panels are 280 px wide. The document height is 676 px, so analysis and move history extend below the first viewport; browser scrolling reaches them without clipping. No `Stello.Web` CSS change was needed.
- **Screenshots:** the initial viewport, full page and scrolled view are saved outside the repository in the session files directory.
- **Validation:** the Release solution build succeeded with 0 warnings and 0 errors. Tests passed: Stello.Engine **489/489**, Egaroucid.Engine **42/42**, Stello.Net.Tests **115/115**. Node tests were not rerun because no web source changed; the 15 tests passed in step 5. Chapters 14 and 15 already describe both apps.
- **Gate P3:** the build, test and browser checks are complete. The reviewer looked at the three screenshots: at 844×390 the board and the status column fit side by side, and the analysis and history follow below.

### Gate P3 (2026-11-08)
- **Passed.** The user checked the Release WPF app on Windows (board styles, 3D pieces, flip animation, sound, Help F1/Shift+F1, Settings and Analysis), after the two problems found earlier in that check (the Settings crash and the Analysis toggle) were fixed in step 5. With the Release build at 0 warnings, Stello.Net.Tests 115/115, engine 489/489, Egaroucid 42/42, Node 15/15, the headless Chromium check and docs 14/15 describing both apps, every Gate P3 condition is met. Phase 3 is done.

## Phase 4: tools and verification gaps

### Step 1: old plan links (2026-11-08)
- **Initial check (clean worktree of 25c2c30):** Plans had 125 broken local links: 20 in [Opening book improvment.md](Opening%20book%20improvment.md), 86 in the [audit plan](stello-exhaustive-docs-and-code-audit-plan.md), 18 in its [findings](stello-exhaustive-docs-and-code-audit-findings.md) and 1 in [making-stello-3-plan.md](making-stello-3-plan.md). Docs had 4, all in [chapter 06](../docs/06-midgame-search.md). Those last 5 point into the git-ignored `Egaroucid-main/` checkout, so they resolve only on a machine that has it.
- **Repairs:** corrected paths relative to Plans, renamed book chapters and section anchors, and links to the separate Checkers repository. Historical algorithm references now point to the Stello 1 archive and frozen V1 C# source. The obsolete Markdown `#L...` fragments now use section anchors. `DialogHost.razor` points to its GameShell location; the removed `EngineWorker.cs` links became plain text with a short note and a link to the replacement JavaScript worker. The rest of the old-plan text was preserved.
- **One GUI specification:** [general-game-gui-specification.md](general-game-gui-specification.md) is now a short pointer to [GameShell's specification](../external/GameShell/docs/general-game-gui-specification.md), not another copy.
- **Egaroucid links:** the 5 machine-only links now go to upstream GitHub: the four files in chapter 06 to `Nyanyan/Egaroucid/blob/main/src/engine/…`, and the folder in the 3.0 plan to `…/tree/main/src/engine`.
- **External URLs (HTTP HEAD):** the 7 Checkers URLs and the 4 Egaroucid file URLs answer 200. The Egaroucid folder page and the Edax repository page answered 504 to both HEAD and GET from this machine (GitHub's directory pages time out); they are the standard GitHub URLs but are unverified here.
- **Validation:** Plans: 150 Markdown files, 289 local links, **0 broken**; docs (including the archive): 48 files, 1,750 local links, **0 broken**. Release solution build: 0 warnings/errors. Tests: Stello.Engine **489/489**, Egaroucid.Engine **42/42**, Stello.Net.Tests **115/115**. No Rust or web source changed, so their test suites were not rerun; the baseline signatures are unchanged.
- **Next:** step 2, default match workers. No matches or long solves were run; Gate P4 remains open.

### Step 2: default match workers (2026-11-08)
- **Decision:** the user clarified that cores means physical cores, not logical processors. The Windows machine has 16 physical cores and 24 logical processors; `Environment.ProcessorCount / 2` was only an approximation and gave 12.
- **Behavior:** without `--workers`, the number of games at a time is `max(1, physical cores / max(A threads, B threads))`, with integer division. The two sides of each game search in turn. An explicit `--workers` (1–256) still wins and does not require core detection. Detection failures give an error that asks for an explicit value.
- **Core detection:** Windows uses the active processor topology, Linux the online CPUs and thread-sibling lists, and macOS `hw.physicalcpu`. The count is capped at .NET's available logical processors; process affinity is also taken into account on 64-bit Windows with one processor group and on Linux.
- **Validation:** on Windows, the detector reports 16 physical cores and 24 logical processors, giving **16 workers at one thread** and **2 workers at eight threads**. Restricted to two sibling logical processors, it reports one physical core and one worker. Unit tests cover both sides' thread counts, rounding and the minimum, explicit and invalid overrides, and synthetic Windows/Linux hybrid topologies and affinity. Linux and macOS detection were not run on those operating systems here; WSL is not installed.
- **Build and tests:** Release solution build **0 warnings, 0 errors**; Stello.Engine **489/489**, Egaroucid.Engine **66/66** (42 existing and 24 new cases), Stello.Net.Tests **115/115**. The invalid `--workers 0` command is rejected before a game starts. No Rust or web source changed; the baseline signatures are unchanged. No standalone benchmark matches or long solves were run.
- **Next:** step 3, match save and resume; Gate P4 remains open.
- **Review:**
  - The Windows topology parsing matches the documented record layout of `GetLogicalProcessorInformationEx` (the group count at byte 30, then 16-byte group masks on 64-bit). The Release tool, run live without `--workers`, printed "16 games at a time" with one thread and "2 games at a time" with 8 threads for side A.
  - **Comparability:** the default at one thread rises from 12 to 16 games at a time on this machine. With 8 performance and 8 efficiency cores, some games now run on efficiency cores. That stays fair within a match, because both sides of a game share its worker, but the per-move depth and the time estimates in HANDOFF section 4 are for 12 workers. Matches meant to compare with earlier ones (all phase 2 matches used `--workers 12`) should keep passing `--workers 12` explicitly.

### Step 3: match save and resume (2026-11-08)
- **Behavior:** `Stello.Benchmark match` appends every finished game to `match_<name>.games.jsonl`, flushing it to disk before continuing. `--resume` validates the exact side settings (including limits and seeds) and the ordered start positions, recovers an incomplete final line, and plays only missing games. The final Markdown and JSON reports are regenerated from all saved games; `--workers` may change on resume.
- **Validation:** a tiny-depth test interrupts after one game, resumes the other game, and compares the resumed reports with an uninterrupted run while excluding timing-dependent fields. Tests also reject changed sides, limits, seeds, or starts; reject empty logs and accidental overwrite; and cover recovery from a partial final line. No real matches were run.
- **Build and tests:** Release solution build **0 warnings, 0 errors**; Stello.Engine **489/489**, Egaroucid.Engine **72/72**, Stello.Net.Tests **115/115**. `git diff --check` passes. No Rust or web source changed; baseline signatures are unchanged.
- **Next:** step 4, the Egaroucid port's strict time limit; Gate P4 remains open.
- **Review:**
  - `BookMatch.cs` (in Stello.Engine) changed only the match driver: games already in the log are skipped, and each new game is reported. The search is not touched, and the strict baseline still passes 489/489. Games rebuilt from the log have no move list, which is fine because the reports never held moves, only each start's discs.
  - **Live check:** 6 starts at depth 2 against depth 3. The log was cut to 5 of 12 games and resumed; the pairs and points equalled an uninterrupted run, and the log ended with 12 games. Re-running without `--resume` is refused, and so is resuming with different sides.
  - **Wall time:** a resumed report's wall time covers only the resumed session, because the clock starts when the command starts.
  - **The `.games.jsonl` logs are git-ignored** (user decision, 2026-11-08): they are only for `--resume`, and the committed `.md`/`.json` reports hold the results.

### Step 4: Egaroucid port time (2026-11-08)
- **Behavior:** `time-strict=on|off` is an Egaroucid match-side key, on by default. Strict mode charges book checks against the move's remaining time and caps the clog search at one quarter of that budget, reserving time for iterative deepening and a legal move. If the search is cut off before an iteration completes, strict mode returns a legal move without an unbounded fallback search. Direct AI calls remain non-strict by default, so the C++ parity behavior is unchanged.
- **Validation:** the adapter-level 100 ms test warms up first, then checks eight starts without a book and a book exit against a 175 ms ceiling (125% of the limit plus 50 ms). All return legal moves and pass. Parser tests check the match default, `time-strict=off`, and rejection for non-Egaroucid sides. The Egaroucid suite, including search parity fixtures, passes **76/76**.
- **Build and tests:** Release solution build **0 warnings, 0 errors**; Stello.Engine **489/489**, Egaroucid.Engine **76/76**, Stello.Net.Tests **115/115**. `git diff --check` passes. No Rust or web source changed; baseline signatures are unchanged. No matches were run.
- **Proposed matches (not run; 590 starts / 1,180 games each, `--workers 12` for comparability):**

  | # | Match | Estimate |
  |---|---|---|
  | 1 | `match --a time-ms=100,name=stello --b engine=egaroucid,time-ms=100,time-strict=on,name=egaroucid --workers 12 --name p4-time-strict-100ms` | About 6 minutes; the previous 100 ms comparison took 5 m 25 s |
  | 2 | `match --a time-ms=500,name=stello --b engine=egaroucid,time-ms=500,time-strict=on,name=egaroucid --workers 12 --name p4-time-strict-500ms` | About 24 minutes; the previous 500 ms comparison took 23 m 57 s |

- **Next:** step 5, FFO #50–59; first measure #50 alone and get the user's go before the long solves. Gate P4 remains open.
- **Review:**
  - With `timeStrict` false, every path is the old one: `RemainingTimeLimit()` returns the full limit, the clog search runs unbounded, and the fallback is the level-1 search. The C++ parity tests pass (Egaroucid.Engine.Tests 76/76).
  - In strict mode the clog search gets a quarter of the move's time, and iterative deepening gets the rest minus the clog time, as before. If nothing finished, the first legal move is played instead of an unbounded search.
  - **Spot check, not a gate match:** Stello against the port at 100 ms, 8 starts (16 games), 4 workers. The port's longest move was **212 ms** with `time-strict=off` and **108 ms** with `on`; the averages were 68 and 65 ms. The scores of 16 games are noise. The test's ceiling of 175 ms at 100 ms is a timing test, so it can fail on a loaded machine like the other timing tests.
  - **The two port matches** (100 ms about 6 min, 500 ms about 24 min) are postponed to phase 5's final matches by the user's decision (2026-11-08); the spot check and the test cover Gate P4's time condition.
