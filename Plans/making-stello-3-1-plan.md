# Stello 3.1 and 3.2: fixes, missing features, then the postponed search work

This plan is in `Plans/making-stello-3-1-plan.md`. Its findings document, `Plans/making-stello-3-1-findings.md`, is created at phase 0 and updated after every phase. Branch: `feature/stello-3.1`, from main baa44e5.

## Context
- **Where we are:** Stello 3.0 is released (2026-11-04). Its Rust engine is node-for-node identical to C# 2.0 on 1 thread, and it adds Lazy SMP and YBWC, browser threads and a Threads setting.
- **What was left behind:** the work kept many C#/C++ quirks so that parity would hold, and it postponed many search, evaluation, endgame, thread, speed and book ideas. The findings of 2.0, the Egaroucid port and 3.0 list them all.
- **Two real bugs** turned up while preparing this plan.
- **The user's open item:** "align the WPF GUI with the web apps".
- **What the user wants:**
  - **Part 1 (Stello 3.1):** the quirks investigated and fixed where they are bugs, plus the missing features.
  - **Part 2 (Stello 3.2):** the postponed ideas.

## Decisions (user, 2026-11-04)
- **Parity may break.** Stello 3.0 on 1 thread becomes the new baseline. Each quirk fix is one change, kept only if measurement shows it is not worse. Stello 2.0 (`stello-v2`) stays frozen.
- **The WPF alignment is a full alignment** with the web app:
  - the same menus, panels, wording, board styles, sound and help;
  - Connect-4's WPF app is the template;
  - WPF keeps what is desktop-only: Exit, native file dialogs, the window title, saved window placement and the Book learning menu.
- **ComputerVsComputer is implemented** in both apps.
- **Releases:** 3.1 after Part 1, and 3.2 after Part 2.
- **The Egaroucid port's time overruns** (a clog search without a time limit, twice the time when it leaves its book) are fixed behind a side key. The key is on by default in matches, and the C++-identical behaviour stays selectable.
- **Matches can be saved and resumed.**
- **Part 2 includes** the opening book work, the speed work and a larger evaluation. Book learning on the web is **not** included.

## Working rules (as for 3.0)
- **Matches and long runs:** start only on the user's go, from a numbered list with time estimates ([[feedback-long-matches-on-go]]). Check uptime first, run nothing heavy at the same time, and keep the per-game lines in the log.
- **A/B switches:** each engine change sits behind a `SearchOptions` switch (Rust `SearchOptions`, C# internal `SearchOptions`, match key `opt=`/named keys), as in 2.0. **User decision (2026-11-05):** as soon as a change's match is approved, its switch is removed again: the old code path, the option, the FFI setter (and its ABI bump, as long as no release had it), the C# property and the match key. A kept change stays as the only code. A rejected change is removed the same way, with the old code kept. **User decision (2026-11-06):** a fix is kept unless the measurement shows that it makes the engine play worse; "not worse" is enough. A change that only claims to make the engine stronger (not a fix) still has to show a gain. When a kept change alters node counts, `Stello.Engine.Tests/Data/stello-v3-signatures.txt` is regenerated with a note in its header and the findings, and the signature tests (C#, Rust, Node) stay node for node.
- **Gates:**
  - For a change that alters play: a 500 ms match of 1,180 games against the previous default, not worse (upper bound > 50 %). A gain is confirmed at 2 s.
  - For a change that only alters counting or speed: `mid-bench`/`solve-bench --compare`, with correct results.
  - All tests pass: cargo, Stello.Engine.Tests, Egaroucid tests, Node, and Stello.Net.Tests on Windows (by the user).
- **Git:**
  - identity `cjp-dev`, with the Co-Authored-By trailer;
  - `git fsck` after each commit, and `env -u GH_TOKEN` for pushes;
  - main is fast-forwarded only at the releases, with the user's OK.
- **Never commit:** training data, Egaroucid resources or binaries.
- **Findings:** updated after every phase, and memory too.

---

# Part 1: Stello 3.1 — bug fixes and missing features

## Phase 0: set-up and the new baseline
- **Branch and documents:** create the branch, the plan and findings copies, and a git tag `v3.0` on baa44e5.
- **The 3.0 baseline:**
  - record the 1-thread signatures (FFO #40–49, mid-bench depths 10 and 12, the random positions, handover, endrand) as reference JSONs in `Plans/benchmark-results/v3.0-ref/`;
  - a `StelloV3Baseline` test pins the signatures of the 105 positions for 3.0, independently of the frozen Stello 2.0 signatures. It is updated deliberately, with a note in the findings, whenever a kept fix changes nodes.
- **A/B in matches:** repeat `opt=<key>=<value>` for Rust engine search switches, such as `opt=selective=off,opt=ybwc=off,opt=soft-pct=80`. The pattern already exists: `selective`, `ybwc` and `soft_time_percent` in `Stello.Rust/stello-core/src/search/options.rs` and `tools/Stello.Benchmark/MatchCommands.cs`.

## Phase 1: real bugs (wrong results or wrong UX)
1. **An aborted solve is reported as Exact.**
   - **The cause:** `solve_root` (`stello-core/src/search/engine.rs:1185-1188`) overwrites `best` with kind Exact after every root move. A stop or Move Now in the middle of a pass then returns a bound as the exact value: the first move's score in pass 2, or a draw in pass 1. `BookSearch` then stores such a result as `BookOrigin.Exact` and never searches it again.
   - **The fix:** a stopped pass keeps the last *completed* pass's result. In pass 2 that is the win/loss/draw result with its move. A draw in pass 1 counts as Exact only once the pass has finished.
   - **Also:** check whether the master book has entries stored this way, by looking at the `recalc` reports with time limits. If it does, mark them for the phase 11 recalculation.
   - **A test:** Move Now set from the progress callback during pass 2 must give a non-Exact result.
2. **The 250 ms "natural delay" is charged to the computer's clock.**
   - **The cause:** `MainViewModel.cs:720-723` uses `elapsed` after `WaitForNaturalDelayAsync`, and with less than 250 ms left the computer can lose on time.
   - **The fix:** charge `searchTime` only. Docs 09 and 14 already say so.
3. **The clock restore after Back/Forward** (C++ heritage, `MainViewModel.RestoreClock`):
   - **The faults:** snapshots are taken only before computer moves, Forward does not charge a move's time again, and with two humans Back restores no clock.
   - **The fix:** a snapshot of both clocks at every ply, restored exactly, with tests. Today none exist (doc 14:446).
4. **The endgame soft limit is not offset by the time already used** (`search/time.rs:56-57`).
   - **The fault:** pass 2 gets less than its window, or never starts. `with_soft_percent` also disagrees with the default.
   - **The fix:** make it consistent, and fix the sentence in doc 09:32. This is a play change for time per game, so it is gated by a match at 5 min per game.
5. **Web worker robustness:**
   - a pool worker that fails to start is never retried (`engine-worker.js:61-73`); retry it once, then report the reduced thread count in `info()`;
   - check that a late `Progress<T>` report cannot overwrite the final analysis (doc 14 design note), in both apps.
6. **Wrong statements in the docs:**
   - perft(7) is 55,092, not 55,010 (audit findings:62);
   - Stello 1's search is called both fail-hard and fail-soft; check which is right and fix the other;
   - doc 15:77 gives the wrong web wording for Add Game to Book;
   - doc 25:284 says the findings cover "phases 0–3", but they cover 0–9;
   - the audit plan has inconsistent numbers.

**Gate P1:** the new tests pass, nothing regresses in the existing tests, and items 1 and 2 are verified in the app. For item 4, run the 1-minute 1,180-game gate first, then the 5-minute 600-game confirmation (300 start positions, two games each); the fix must not be worse than 3.0.

## Phase 2: the parity quirks, investigated one by one
Each item: read the code and decide whether it is a bug or a harmless quirk. If it is worth fixing: a switch, `mid-bench`/`solve-bench` nodes and time, then a 500 ms match against 3.0 for anything that changes play. The findings get a table of item, verdict, nodes, Elo and kept or not.

1. **Asymmetric hash cutoffs** (`engine.rs:921-926`): an upper bound cuts only if strictly below alpha.
2. **Hash cutoffs on an upper bound feed the response killers** (`engine.rs:923, 1014, 1024`). Investigated: harmless ordering heuristic; left unchanged (see findings).
3. **The transposition table never ages:**
   - deep entries from earlier positions can never be replaced, so slot 0 slowly becomes useless during a game;
   - a store for the same position at a lower depth is dropped even when its bound is tighter (`search/table.rs:228-241`).

   Completed: 3a (aging) is kept unconditionally; entries with more empty squares than the root count as empty on probe and replace. 3b (tighter-bound retention) is dropped for lack of a demonstrated gain. After the first four matches, independent-opening match 5 scored 51.6% (49.7–53.4) at 2 s and passed the agreed not-worse confirmation rule, superseding the initial rejection of 3a.
   The temporary switches and 3b code are removed, the ABI is back to 3, and the strict cleared-table baseline is unchanged from `bbe2a60`. A persistent 4,096-slot, 34-position depth-12 regression checks aging under real slot competition. See the item 3 findings for all five matches and the decision.
4. **The stored move in `end_deep`** (`endgame/search.rs:342-355, 377, 407-408`):
   - after an ETC fail-low the stored best move can be −1, or a move that was not the best;
   - the hash move does not count as searched, so the node splits one move later.

   Completed: both are fixed unconditionally. An ETC fail-low keeps its move with its value, and a searched hash move counts for splitting. The 1,180-game 500 ms match scored 50.3% (48.6–52.0), so it is not worse and is kept. The switch is removed, and the ABI stays at 3. The baseline is regenerated for FFO #43 and #44 (nodes only), and the tests stay strict. See the item 4 findings.
5. **Moves decided by the local table stay in the sort** (`endgame/search.rs:535-540, 642-647`).

   Completed: `end_local` and `end_simple` sort only the moves still to search; decided moves no longer change their order. The 1,180-game 500 ms match scored 49.6% (48.0–51.1), so it is not worse and is kept. The switch is removed, and the ABI stays at 3. The baseline is regenerated for FFO #40–44 (nodes only), and the tests stay strict. See the item 5 findings.
6. **Response ordering** (`search/ordering.rs:68-72, 87`): reviewed; no fix. Swapping the hash move to the front is the same move-to-front operation used by C++ and C# 2.0; it can change only tie order among the remaining moves. The response-score loop leaves a final singleton, which needs no further ordering. No defect found; see the findings.
7. **A full window for every root move up to depth 3** (`search/engine.rs:850`): reviewed; no fix. C++ selects `zero_findmax` only for `varlook > 2` with multiple root moves, and C# 2.0 uses the same full-window condition for shallow roots. This is the intended shallow-search policy; see the findings.
8. **The Multi-ProbCut shallow search** reads and writes the hash table at the same ply (findings 3.0:167): reviewed; no fix. Turning off the stores alone used 4.7% more nodes (6.2% at depth 12) and was 4.8% slower, with no benefit, so the existing table access stays. See the findings.
9. **Counting** (statistics only):
   - two abort-check schemes share one node counter;
   - evaluations are counted at final scores in some layers and not in others;
   - `ordering_value` counts nodes without an abort check;
   - `last4`–`last1` count each pass as a node;
   - `solve_shallow`/`solve_last2` can skip abort checks.

   Make the counting consistent, and every abort check reachable. The nodes per second change, so the findings note the new counting.

   **Completed (2026-11-07):** recursive nodes and explicit passes now use one 1,024-node abort-check path; ordering, shallow-solve and last-square paths can no longer skip cancellation checks; terminal evaluations are counted consistently. The 100 depth-10 midgame searches kept their 3,716,389 nodes. FFO #40–44 kept every move and score while nodes changed from 92,556,457 to 93,697,164 (+1.23%). The strict baseline was updated for those five solve signatures only; see the findings.
10. **Left as they are, and documented:** Stello 1's corner-stability bug (V1 is frozen), and the C++-identical quirks in the Egaroucid port (`last3` reads the wrong board, `bit_around` and so on). The port must stay identical to C++.

**Gate P2 — complete (2026-11-07):** all kept fixes together played against 3.0 for 1,180 games at 500 ms and 600 games at 2 s. The 3.1 candidate scored 51.1% (95% interval 49.5–52.8, +8 Elo) at 500 ms and 49.8% (47.9–51.7, −1 Elo) at 2 s. Neither result was statistically significant; both meet the agreed not-worse criterion. Keep items 1, 3a, 4 and 5. See the [Gate P2 findings](making-stello-3-1-findings.md#gate-p2-all-kept-fixes-against-30-2026-11-07) and [500 ms report](benchmark-results/match_p2-gate-500ms.md) / [2 s report](benchmark-results/match_p2-gate-2s.md).

## Phase 3: the apps
1. **Align the WPF GUI with the web app** (`Stello.Net/Stello.Net/MainWindow.xaml`, `Views/*`; template `Connect-4/Connect4.Net/Connect4.WPF/MainWindow.xaml`):
   - **Menus as on the web:**
     - File: New, Open, Save, Save As, Add Game to Book, Exit;
     - Game: vs Computer (Black/White), vs Human, vs Computer (both sides), Switch sides, Settings F2;
     - Options: Analysis, Sound, the board styles, 3D Pieces, Animate Flips;
     - Help: User guide F1, Rules & Documentation Shift+F1, About.
   - **The Book menu stays,** for learning: Evaluate Book, Self-play, Stop Learning.
   - **Move Now** becomes a button plus Ctrl+M. Undo and Redo become buttons plus Ctrl+Z/Ctrl+Y, keeping the Ctrl+T/Ctrl+F aliases.
   - **The status panel:**
     - a STATUS header that turns crimson when the game is modified;
     - the "Othello" and time-limit badges, and a thinking bar;
     - two player cards with glyph, name, role, clock, discs, "Moves: N" and the active-turn highlight, so both clocks are shown;
     - the turn-style `StatusText`.
   - **A move-history panel:** "MOVE HISTORY (N moves)", paired rows, the latest move highlighted, auto-scroll.
   - **Analysis:** upper-case titles, and "-" for empty fields.
   - **The board:** the three styles, with the textures from `Stello.Web/wwwroot/textures`; 3D discs and the flip animation; the hint and last-move colours of `app.css`.
   - **Sound:** a WPF `ISoundService` on `SoundWaves`, like Connect-4's `SoundService`.
   - **Help:** opens the live site's `/docs` chapters in the browser.
   - **Wording:** the web settings wording. The transposition-table choices come from one list in `SettingsViewModel` that both apps use (today they are duplicated in `SettingsViewModel.cs:88-95` and `SettingsForm.razor:32-36`).
   - **Remembered settings:** `Appearance`/`BoardStyle` move from `Stello.Web/Services/AppearanceStore.cs` to `Stello.App`, so both apps remember the appearance and the Sound setting.
   - **View-model plumbing:** `MainViewModel` raises `PropertyChanged` for `ToMove`, `IsGameOver`, `CanUndo`, `CanRedo`, `IsClockVisible` and `SettingsBadgeText`, and gets `IsBlackActive`/`IsWhiteActive` and paired history rows. The web computes those today in `Home.razor`.
2. **Computer vs Computer in both apps** (`Stello.App/Models/GameMode.cs`, `MainViewModel`):
   - `IsHumanTurn`, the player labels, both clocks and the timeouts handle the mode, and the computer plays both sides with the current settings;
   - Move Now, stop (Switch sides or New Game), Undo and Save all work;
   - a short pause between moves for watching.
3. **The docs viewer and archive pages:** decide how `docs/stello-1/` is shown. Today it is raw markdown, because GameShell renders only top-level chapters. Either render it, or link it to GitHub.
4. **Mobile landscape:** check the GameShell layout on a phone-sized landscape viewport with Playwright, and fix it if it clips.

**Gate P3:** the Windows build and Stello.Net.Tests pass (new tests for the CvC mode, clocks, history rows and settings lists), the user checks WPF on Windows, and a headless Chromium check of the web app passes. Docs 14 and 15 describe both apps.

**Progress:**
- **Step 1 done (2026-11-07):** the two score-display tests now expect the app's `+0.4 discs`/`+0.2 discs` format.
- **Step 2 done (2026-11-07):** shared game-state notifications, active-player flags and paired history rows; one transposition-table choice list in `SettingsViewModel` used by both UIs; shared appearance models and persisted appearance/sound preferences. Stello.Net.Tests passes 94/94; see the findings.

## Phase 4: tools and verification gaps
1. **Matches save and resume** (`tools/Stello.Benchmark/MatchCommands.cs`): each finished game is appended to `<name>.games.jsonl`. The same command with `--resume` checks that the settings and start positions are the same and plays only the missing games. The `.md`/`.json` are written at the end as now.
2. **Matches with threads:** the default `--workers` becomes max(1, cores / max threads of the two sides), so workers × threads ≤ cores without setting it by hand.
3. **The Egaroucid port's time** (`Egaroucid.Engine`: `ClogSearch.cs:61`, `Ai.cs:107,128`):
   - a time limit for the clog search, and a book exit within the move's time, behind the side key `time-strict=on|off`;
   - on by default in matches, off for the parity tests;
   - a 100 ms check that the port no longer overruns its time.
4. **FFO #50–59 in `solve-bench`** (planned in the port, never run), with Egaroucid C++ times for comparison.
5. **Platform checks, by the user and recorded:**
   - on Windows, the Stello.Net.Tests run, `cargo test` with MSVC, and a Stello.Web build with the nightly (the threaded package);
   - Firefox and WebKit with Playwright (nested workers, shared memory, fallback), if the browsers can be installed; otherwise the user checks them by hand.
6. **Old plan files:**
   - fix the broken links in `Plans/Opening book improvment.md`, `Plans/stello-exhaustive-docs-and-code-audit-plan.md`/`-findings.md` and `Plans/general-game-gui-specification.md`;
   - replace the old spec copy with a pointer to `external/GameShell/docs/general-game-gui-specification.md`.

**Gate P4:**
- a match interrupted on purpose resumes and gives the same result format;
- the port meets its time at 100 ms;
- FFO #50–59 are all correct;
- the link check finds 0 broken links in `Plans/` too.

## Phase 5: the 3.1 release
- **The final matches** (a numbered list with estimates, each on the user's go):
  - 3.1 against 3.0 on 1 thread at 100 ms, 500 ms and 2 s;
  - 8 threads against 3.0 with 8 threads at 500 ms;
  - against the port with `time-strict` on, at 100 and 500 ms.
- **Docs:**
  - Part 1 chapters updated;
  - a new Part 3 chapter, "27 Stello 3.1: fixes and the desktop app", with glossary and references renumbered;
  - About shows 3.1.
- **The release:** fast-forward main with the user's OK, follow the deploy, and do the live check (1 thread, threads, Computer vs Computer).

---

# Part 2: Stello 3.2 — better search, threads, evaluation, speed and book

The order puts speed first (it changes no nodes), then the threads, the search, the endgame and the evaluation. The book comes last, so that it is recalculated with the final engine. Baseline: 3.1.

## Phase 6: speed (no change in nodes)
1. **AVX2 flips and legal moves in the solver:**
   - the solver is compiled twice, a generic build and `#[target_feature(enable = "avx2")]`, with runtime dispatch as in `stello-core/src/cpu.rs`;
   - that is about 1,200 lines through a macro or generic code;
   - flips are 38 % of the endgame time.
2. **Incremental pattern indices** through make/unmake in the midgame search. The evaluation is about 58 % of the midgame time, mostly waiting on weight loads.
3. **WebAssembly `simd128`** (+2 %, no new browser requirement).

**Gate P6:** node-identical to 3.1 on all the parity sets, and faster through the wrapper (one pinned CPU, fastest of 3) and in Chromium. `--cpu portable` stays identical.

## Phase 7: threads
1. **Root splitting** in `search_root` and `solve_root`. With 8 threads FFO is 3.29× faster today, against Egaroucid's 4.15×.
2. **Interrupting the splitting thread's own brother** when a task fails high: the local search observes the current split point, and a cut is not reported as an abort.
3. **Progress heartbeats while a thread waits at a split,** from the polling callback of `scope_polling`.
4. **The `searched` flag in `end_deep`,** if phase 2 did not settle it. (Settled in phase 2 item 4.)
5. **Tuning on two machines:**
   - the midgame YBWC split depth (today tasks average 415 nodes) and the helper/YBWC split at 16 threads;
   - Lazy SMP helpers in the solve once more;
   - measured on the laptop and on the user's Ryzen 7800X3D;
   - the default Threads (Automatic) is chosen from those numbers.

**Gate P7:**
- FFO #40–49 is faster with 8 and 16 threads, and correct;
- 8 threads against 3.1 with 8 threads at 500 ms is not worse, and better at 2 s;
- ThreadSanitizer is clean.

## Phase 8: search
1. **Multi-ProbCut:**
   - adopt the refitted model (without single-move rows), or fit one per depth pair;
   - t by game phase, and retune t (`mpc=`).
2. **The second-best move in the table, alone:** it was only tried together with bounds.
3. **The soft time limit again** (`soft-pct=`, +4 to +14 Elo before).
4. **An aspiration window or MTD(f)** with the previous iteration as the first guess.
5. **The table size at 2 s with 8 threads:** hash bits 19 against 21.
6. **The clog search,** with a depth or time limit.

**Gate per item:** a 500 ms match of 1,180 games against the previous default, lower bound above 50 % (or not worse for simplifications), confirmed at 2 s.

## Phase 9: the endgame
1. **A small move-ordering table for 11–13 empty squares,** like Egaroucid's 4-pattern table updated move by move. It must be trained, by the trainer, with a weights file.
2. **Start the solve from an estimated solve time,** and solve selectively (`mpc-end`) at longer time controls (2 s, 5 min per game).

**Gate:** FFO correct and not slower, and the matches as in phase 8.

## Phase 10: evaluation
1. **A larger evaluation for the desktop:**
   - egaroucid16 × 60 stages on 10 M positions, about 8 MB, with its own Multi-ProbCut fit;
   - at 3.0's speed its 17 % lower nodes per second may matter less;
   - the web keeps the 1 MB model unless the larger one also pays there (a desktop/web split of the weights file).
2. **Extra features** (mobility, potential mobility, parity) and better training at the same size: the rest of the 5.47 → 5.05 gap to Egaroucid.

**Gate:**
- the error on the test split is lower;
- 500 ms and 2 s matches against 3.2-so-far pass;
- the web first visit and memory are measured and stay acceptable (for example ≤ 1.5 MB more).

## Phase 11: the opening book
1. **A recalculation with the final 3.2 engine:**
   - `BookEffort.EngineVersion` 4 → 5, which also redoes the 5 proven draws that version 4 made heuristic, and any entries found in phase 1.1;
   - from phase 1.1: the 2,988 `X` (exact) values with a time limit (`X:60s`, 17–31 empty squares after the move) may be bounds from a stopped solve and cannot be told apart in the book. `recalc` keeps exact values, so it must be made to re-solve these (for example: treat a timed `X` from engine version ≤ 4 as unproven). The 1,091 `X` values without a limit (solve or older) are not affected;
   - 1 thread per worker, run on the user's go (hours).
2. **Search the moves the book lacks,** as C++ `calclib` did: each book position gets a search of its moves that are not in the book (`BookSearch` already has the `onlyMoves` search), and good ones are added.
3. **Drop-Out Expansion (`Plans/Opening book improvment.md` phase 3):** decide with the user at that point:
   - the web size limit (today localStorage, about 5 MB, so maybe IndexedDB);
   - the effort per position and the DOE depth.

**Gate:** with books, 3.2 with the new book beats 3.2 with the 3.1 book at 500 ms, and the web load size is measured.

## Phase 12: the 3.2 release
- **Final matches** on the user's go: against 3.1, 3.0, 2.0 and the port, at 100 ms, 500 ms and 2 s, with 1 and 8 threads, plus FFO #40–59.
- **Docs:** a Part 3 chapter for phases 6–11, and About 3.2.
- **The release:** fast-forward main with the user's OK, the deploy and the live check.

---

## Verification summary
| Phase | Check |
|---|---|
| 0 | 3.0 reference JSONs recorded; baseline signature test green |
| 1 | New bug tests (aborted solve kind, clock charge, clock restore); a match at 5 min per game not worse; doc errors fixed |
| 2 | A verdict per quirk; kept fixes not worse at 500 ms and 2 s; signatures updated with notes |
| 3 | Windows build and tests; the user's WPF check; web check in Chromium; Computer vs Computer works in both apps |
| 4 | Match resume works; the port meets its time; FFO 50–59 correct; Plans/ links 0 broken |
| 5 | Final matches; docs; live check (1 thread, threads, CvC) |
| 6 | Node-identical and faster (native and browser) |
| 7 | FFO faster with 8 and 16 threads; 8-thread matches; TSan clean |
| 8–10 | Per item, a 500 ms match with lower bound > 50 %, confirmed at 2 s; evaluation test error; web size |
| 11 | New book beats old book; web size |
| 12 | Final matches; docs; live check |

**Tools:**
- `Stello.Benchmark` `mid-bench`/`solve-bench --compare`, `match` (with `opt=`, `threads=`, `time-strict=`);
- `cargo test`/clippy/fmt (also wasm32 single- and multi-threaded);
- `dotnet test` Stello.Engine.Tests and Egaroucid.Engine.Tests;
- `node --test` on the WASM packages;
- Playwright in `~/.venvs/pw`;
- the link checker `scratchpad/linkcheck.py`.

## Status
- Plan written on 4 November 2026, with the user's decisions above.
- **Phase 0: done (2026-11-04).** Branch, plan, findings and the tag `v3.0` on baa44e5 were already in place. The seven 1-thread reference JSONs are in `Plans/benchmark-results/v3.0-ref/`; `StelloV3BaselineTests` pins the 105 signatures independently of frozen Stello 2.0; the match tool accepts repeated `opt=<key>=<value>` search switches.
- **Phase 1: done (2026-11-06).** Items 1–6 fixed: an aborted solve reported as Exact, the natural delay charged to the computer clock, the clock restore after Back and Forward, the endgame soft limit (both gate matches not worse; its A/B switch removed after approval), web worker startup retry and thread reporting plus late progress reports (review fixed a restart loop on a failed pool worker), and the doc errors. Gate P1: item 1 checked in the web app against live 3.0, item 2 checked by the user in WPF.
- **Phase 2: done (2026-11-07).** Items 1 and 3a–5 are kept; 2 and 6–8 needed no fix; item 9 unified counting. The combined kept search fixes passed Gate P2 at 500 ms and 2 s without evidence of a loss. The temporary comparison switch is removed again (ABI 3).
- **Phase 3: in progress (2026-11-07).** Steps 1 and 2 are done; shared state, settings choices and appearance/sound persistence are in place. Step 2 passes the Release build, all .NET test projects and the 15 WASM Node tests.
