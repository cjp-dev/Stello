# Stello step 2: port the Egaroucid engine and compare it with Stello

This is the working copy of the plan; its Status section is updated as phases finish. It replaces the earlier draft `pattern-evaluation-plan.md`, which only ported the evaluation.

## Context
Step 1 (`docs/brain/18-measuring-the-engine.md`) added the measuring tools. The planned step 2 was a pattern evaluation. The user widened it: port the **whole Egaroucid engine** (search, evaluation, endgame, book) to C#. It then becomes possible to:
- compare evaluation, midgame search and endgame search with Stello's, one component at a time;
- play test matches between the two engines.

The goal comes later: an improved Stello engine built on what is learnt. That is not part of this plan.

**Inputs**
- **Egaroucid 7.9 source** in `Stello/Egaroucid-main/` (git-ignored, GPL-3.0-or-later). Resources are in `bin/resources/`:
  - `eval.egev2` (38 MB)
  - `eval_move_ordering_end.egev` (0.5 MB)
  - `hash/hash25-27.eghs` (2 MB each; random numbers for the transposition table)
  - `book.egbk3` (32 MB)
- **Training data** in `Stello/Traning data/` (git-ignored; its terms forbid redistribution). It is used here only to compare the two evaluations on positions with known final scores.

**Decisions (user)**
- **Licence:** Stello becomes GPL-3.0-only.
- **Port scope:** the engine core plus Egaroucid's opening book. The book is for playing only, not for editing or expanding. GUI, human-like play, XOT and local strategy are left out.
- **Threads:** port single-threaded first and prove it gives the same nodes, scores and moves as the C++ original. YBWC multi-threading comes as a later phase.
- **Where it runs:** tools and tests only (benchmark tool, tests). The WPF and web apps keep the Stello engine.
- **Postponed:** the C# trainer for evaluation weights moves to the later improved-engine step. So does putting the pattern evaluation into Stello's own search, and recalculating Stello's book.
- **Resources stay outside git** (my choice). The port finds them through `--egaroucid-resources <dir>`, which defaults to `<repo>/Egaroucid-main/bin/resources`. Tests that need them are skipped when they are missing. Committing about 73 MB of resources can be decided later, when the improved engine needs them.

## Facts from the exploration (what is being ported)
**Files.** `src/engine/`, generic variants only. Skip `*_7_4*`, `*_7_7_beta*`, the SIMD files and the book editing code. About 10,000 C++ lines, of which roughly 5,000 are real logic plus about 550 in `ai.hpp`. Expect about 5,000 C# lines.

**Board.** `Board{player, opponent}` with **a1 = bit 63**; Stello uses a1 = bit 0. The port keeps Egaroucid's bit order inside, so the code can be compared line by line, and converts at the adapter.

**Pipeline.**
1. `ai()` calls `ai_common`, which handles pass, game over and the book.
2. `get_level` (`level.hpp`) gives depth and MPC level.
3. `tree_search_legal` first runs the clog search (`clogsearch.hpp`) when MPC < 100.
4. Then `iterative_deepening_search` (`ai.hpp:136`):
   - Depth goes +3 up to 15, then +1.
   - The endgame climbs through the MPC levels 74 → 88 → … → 100.
   - There is no aspiration window.
   - The midgame value is smoothed with `round((0.9·prev + 1.1·new)/2)`. C's `round` rounds halves away from zero, so .NET must use `MidpointRounding.AwayFromZero`.

**Midgame.** `midsearch.hpp` and `midsearch_nws.hpp`:
- PVS / NWS with a transposition-table cutoff.
- ETC from depth 16.
- MPC from depth 3. The error table is `mpc_error[7][65][61][61]`, a cubic sigma.
- Move ordering (`move_ordering.hpp`): mobility, potential mobility, and the value of a shallow search.
- `eval1` / `eval2` at the leaves.

**Transposition table** (`transposition_table.hpp`):
- 3-slot probe with first-fit replacement and an importance reset at 40%.
- Hash from table lookups using the `.eghs` random numbers.

**Endgame** (`endsearch*.hpp`):
- More than 13 empties: midgame code at MPC 100.
- 10–13 empties: `nega_alpha_end_nws`, with a thread-local "lttable" and ordering by the move-ordering-end evaluation.
- ≤ 10 empties: simple NWS (parity).
- ≤ 6 empties: fast NWS.
- 4 or fewer: `last4` … `last1`.
- Stability cutoffs throughout.

**Book** (`book.hpp`, play-time part about 800 lines):
- File format: `"DICUORAGE"`, version 3, a uint32 count, then 25-byte records (player, opponent, value, level, n_lines, leaf).
- 1.3M positions stored in canonical orientation (smallest of the 8 symmetries).
- `get_random` looks up the children and picks among the best (acc 0 = uniform among the tied best). With acc 0, the moves not in the book are also checked with a null-window search.
- Egaroucid seeds its random generator from the clock. The port takes a seed, so matches can be reproduced.

**Determinism and parity traps**
- **Build variant.** The SIMD build counts nodes differently from the generic one (`last1_nws`), so compare against a **Generic** build (`-DHAS_NO_AVX2`).
- **`use_multi_thread`.** The console always passes `use_multi_thread = true`. Even at 1 thread that sends the search through the YBWC paths, which sort moves with `std::sort` (ties ordered by the STL). The reference driver below calls the engine with `use_multi_thread = false`, which uses the plain PVS path with `swap_next_best_move`.
- **Hash level.** Only hash levels 25–27 have a random-number file. Any other level uses clock-seeded random numbers.
- **State between searches.** Clear the transposition table and the lttable before each reference position.
- **MPC table.** Build the reference with `-ffp-contract=off`, so the floating-point MPC table is not computed with fused multiply-add. Also dump the table and compare it.
- **Time mode** is not reproducible. It is checked for behaviour, not parity.

**Console and reference.**
- The console builds with plain g++: one source file, no libraries (`g++ -O2 -std=c++20 -pthread src/Egaroucid_for_Console.cpp`).
- It finds its resources relative to the executable.
- `-solve <file> -l L -t 1 -nobook` prints nodes per position.
- GTP mode exists, but it returns only the move.

## Phase 0: Licence (GPL-3.0-only)
- **New files:**
  - `LICENSE` (GPLv3 text, copied from `Egaroucid-main/LICENSE`).
  - `Stello.Net/Directory.Build.props` with `PackageLicenseExpression GPL-3.0-only`, `Copyright` and `Authors`.
  - `THIRD-PARTY-NOTICES.md` crediting:
    - Egaroucid (Takuto Yamana, GPL-3.0-or-later): the engine port, used under GPLv3, and its resources, which are not redistributed;
    - Edax (GPL-3.0): ideas, plus the FFO file in the benchmark data;
    - the Egaroucid training data: used but not redistributed, credited as its terms ask.
  - A short root `README.md` with the licence and a link to the docs.
- **About box** (`Stello.App/Models/AboutInfo.cs`): add a licence line and the source URL `https://github.com/cjp-dev/Stello`. The repository is public, which covers GPL's source requirement for the web build.
- **Docs:** fix `docs/brain/15-gui-and-settings.md:105`, which claims About shows licence details.
- **Headers:** every ported file gets `// SPDX-License-Identifier: GPL-3.0-only` and "Ported from Egaroucid <file>, © Takuto Yamana, GPL-3.0-or-later". `external/GameShell` keeps its own licence.
- **Commit:** "License Stello under GPL-3.0-only".

## Phase 1: C++ reference harness
`tools/Egaroucid.Reference/` (committed): `reference.cpp`, `build.sh` and a README. The binaries go to a git-ignored `bin/`, together with a copy of the resources, because the console looks for them next to the executable.

- **`build.sh`** builds two binaries:
  - `egaroucid-generic`: the Generic console, `-O2 -std=c++20 -pthread -DHAS_NO_AVX2 -ffp-contract=off`;
  - `egaroucid-ref`: our own driver, `reference.cpp`, which includes `engine/engine_all.hpp` unchanged.
- **The driver's commands**, reading positions as `setboard` lines:
  - `eval`: the 64 feature indices, `mid_evaluate`, and the move-ordering-end evaluation;
  - `search -l L`: `ai(board, L, book=false, …, use_multi_thread=false)` with the transposition table and lttable cleared per position. It prints move, value, depth, probability and nodes, with nodes per iteration from the log;
  - `perft d`, `stability` (stable-disc count), `mpc-table` (dump);
  - `book`: the book's children and values for a position.
- **Fixtures** in `Egaroucid.Engine.Tests/Data/`, all small text files:
  - eval and features: about 2,000 training positions across all 60 phases, plus FFO positions and positions where a side passes;
  - stability and perft;
  - search: `midgame.txt` (100 positions) at levels 1, 5, 11, 15 and 21, plus FFO #40–44 at level 60;
  - book: about 200 positions.
- **Gate P1:** both binaries build, and the fixtures are generated twice with identical output, which shows the reference is deterministic.

## Phase 2: Port the foundations (`Stello.Net/Egaroucid.Engine`)
A new class library (net10.0, namespace `Stello.Egaroucid`), added to `Stello.Net.slnx`. It has its own types and does not depend on `Stello.Engine`. Files follow Egaroucid's names, so the C++ and C# can be compared side by side.

- **Board, move generation, hashing:**
  - `Board`, `Flip`, mobility, `LastFlip`, `BitUtil` (popcount, first/next bit, mirror helpers). These may use `System.Numerics.BitOperations` and `Bmi2`/`Avx2` intrinsics wherever the results are identical.
  - `Hash` loads the `.eghs` files.
- **Stability:** stable discs from an edge table (`stability.hpp`).
- **Evaluation:**
  - `Evaluate` with the 16 patterns × 4 instances and 60 phases, loaded from `egev2`, plus the move-ordering-end evaluation (4 patterns).
  - Incremental features follow the generic code, but store **one** weight table and two index views (side to move and opponent). That gives the same values as the generic double table at about 74 MB instead of 147 MB.
- **`Resources`:** finds the resource directory and loads each file once. The data is read-only and shared, so it is thread-safe.
- **Tests** (`Stello.Net/Egaroucid.Engine.Tests`):
  - perft matches;
  - features and evaluations equal the fixture exactly;
  - incremental features equal a from-scratch computation along random games;
  - stability matches.
- **Gate P2:** 100% exact against the fixtures. Throughput of eval and perft is reported next to the C++ reference.
- **Commit:** "Port Egaroucid's board, evaluation and stability".

## Phase 3: Port the single-threaded search
- **Search core:**
  - `Search` (state: board, discs, parity, MPC level, nodes, eval features);
  - `TranspositionTable` (exact replacement and reset rules, `get`/`reg`, moves);
  - `TranspositionCutoff` and ETC;
  - `MoveOrdering`, `MultiProbCut` (with the precomputed error table, checked against the dump);
  - `MidSearch` and `MidSearchNws`;
  - `EndSearch` (PV `last4`…`last1`, NWS end/simple/fast, the lttable as a per-`Search` or `[ThreadStatic]` array);
  - `StabilityCutoff`, `ClogSearch`, `Level` (the level table).
- **Driver:** `Ai.Search(board, level)` follows `ai_common` → `tree_search_legal` → `iterative_deepening_search`. It returns `SearchResult(move, value, depth, probability, isEndSearch, nodes, time)`.
- **Code paths:** the YBWC branches are ported only where `use_multi_thread = false` still reaches them. The serial `std::sort` paths are left for Phase 6.
- **Tests:**
  - move, value, depth and **nodes** equal the reference for the `midgame.txt` levels and for FFO #40–44 at level 60;
  - FFO #40–49 are all solved correctly.
- **Gate P3:**
  - 100% parity on the search fixtures. On a mismatch, the nodes-per-iteration log shows which iteration first differs.
  - Speed against the C++ reference (same nodes, so NPS can be compared directly) is reported for midgame and endgame.
- **Commit:** "Port Egaroucid's single-threaded search".

## Phase 4: Time management and book
- **Time management** (`time_management.hpp`, `ai_time_limit`, `iterative_deepening_search_time_limit`):
  - Each iteration has a deadline and is stopped by a cancellation flag polled in the search, instead of `std::async`.
  - Two modes: time per game (Egaroucid's own `calc_time_limit_ply`) and a fixed time per move, for matches against Stello's `time-ms`.
- **Book:**
  - egbk3 loader;
  - canonical board via the 8 symmetries;
  - `get_random(acc)` with a seeded `Random`;
  - the acc-0 extra search on moves outside the book, as in `ai_common`.
- **Tests:**
  - the book's children and values equal the book fixture;
  - the chosen move is one of the tied best;
  - in time mode, every move stays within its limit (about 10% tolerance on this VM) and the search returns a legal move when stopped.
- **Gate P4:** tests pass. A 20-game smoke match of the port against itself at 100 ms per move gives no exceptions and no time losses.
- **Commit:** "Port Egaroucid's time management and opening book".

## Phase 5: Comparison tooling and the first comparison
All of this lives in `tools/Stello.Benchmark`, which references `Egaroucid.Engine`. Stello's engine is not changed.

- **One player abstraction.**
  - `EngineMatch` (`Stello.Engine/BookMatch.cs`) today plays `ComputerPlayer`s. It gets an internal `IMatchPlayer` (`ChooseMove(Board, Player, remaining time) → SearchResult`, `NewGame()`).
  - `ComputerPlayer` implements it directly. `EgaroucidPlayer` in Benchmark converts Stello's `Board` (a1 = bit 0, black/white) to Egaroucid's (a1 = bit 63, player/opponent) and maps the result to Stello's `SearchResult`. The value is in discs, `ScoreKind` is Exact, WinLossDraw or Heuristic, plus depth and nodes.
  - `SideStats` keeps working: midgame depth, first solve, nodes.
- **Match sides.**
  - New key `engine=stello|egaroucid`. For Egaroucid there are also `level=L`, `book=on|off` and `seed=`. The existing `time-ms`, `game-s`, `depth` and `hash-bits` stay.
  - `hash-bits` maps to Egaroucid's hash level 25–27 (other levels are not reproducible).
- **Example match:**
  `match --a engine=stello,time-ms=100 --b engine=egaroucid,time-ms=100,book=off` (1,180 games from `starts.txt`, 1 thread each).
- **Component benchmarks:**
  - **`eval-compare`** puts both evaluations on held-out training positions (a fixed sample, about 20k per phase, from `Traning data`):
    - mean absolute and mean squared error against the final score, per phase;
    - for Stello's classic evaluation, a fitted scale k (units per disc) and the error after scaling;
    - evaluations per second.
  - **`mid-bench --engine egaroucid [--level L | --depth D]`** runs fixed depth (MPC 100% and the level's own MPC) on `midgame.txt`: nodes, time, NPS, next to Stello's fixed depth.
  - **`mid-quality`** builds positions with 24–28 empties from the match games and solves them exactly once with the port (level 60, cached in `Data/midgame-exact.txt`). It then measures each engine's average disc loss and best-move rate at 50, 200 and 1,000 ms. This measures what the midgame search delivers, independently of style.
  - **`solve-bench --engine egaroucid`** runs FFO #40–49 (later up to 59) exactly at 1 thread. It reports time and nodes, next to Stello's 369 s and next to the C++ reference.
- **First comparison run** (no other heavy work on the machine meanwhile):
  - `eval-compare`, `mid-bench`, `mid-quality` and `solve-bench` for both engines;
  - matches Stello against Egaroucid at 100 ms and 500 ms per move, with books off;
  - one match at 500 ms with both books on.
- **Not in scope:** hybrids, such as Stello's search with Egaroucid's evaluation. They belong to the improved-engine step.
- **Results** go to `Plans/benchmark-results/` and `Plans/egaroucid-comparison-report.md`. The report has one section per component and points out where the strength difference comes from.
- **Gate P5:** all commands work, and the report is written with confidence intervals.
- **Commit:** "Compare Stello with the Egaroucid port".

## Phase 6: YBWC multi-threading
- **What is ported:**
  - `ybwc.hpp` (split points, young-brothers search for the PV, NWS and root), with the serial `std::sort` paths that the threaded search uses;
  - the Lazy-SMP-like sub-searches in iterative deepening (`ai.hpp:175-217`);
  - the parallel clog search;
  - locking in the transposition table.
- **Threads:** `thread_pool.hpp` is replaced by a small pool of `LongRunning` threads whose `TryPush` fails when no thread is idle. That copies Egaroucid's "if no idle thread, search it yourself" behaviour.
- **Tests:**
  - with N threads, FFO #40–49 give the same exact scores;
  - repeated runs give no exceptions and leave no threads hanging;
  - Move Now and cancel stop the search promptly.
- **Gate P6:**
  - `solve-bench --engine egaroucid --threads 1,2,4,8,12` shows the speed-up, next to the C++ reference with `-t N` (SIMD and Generic);
  - a match of the port at 1 thread against N threads at 500 ms (400 games) gives a 95% lower bound above 50%.
- **Commit:** "Add YBWC multi-threading to the Egaroucid port".

## Phase 7: Docs, report and memory
- **New chapters:**
  - `docs/brain/19-the-egaroucid-port.md`: architecture, a map from C++ files to C# files, the parity method and fixtures, the differences from the original (bit order at the adapter, one weight table, seeded book), and how to run it;
  - `docs/brain/20-engine-comparison.md`: the comparison results per component, linking the report.
- **Updated chapters:**
  - 01 (overview diagram and table: the new projects);
  - 17 (references: Egaroucid and the training data credit);
  - 18 (the `engine=` side key and the new commands);
  - README (index and reading path).
- **Memory:** update `stello-engine-improvement.md`.
- **Push:** push main after fetch and rebase, then check the deploy run. Docs changes trigger the deploy.

## Working rules
- **Branch:** `feature/egaroucid-port`, one commit per phase. After each green gate, fast-forward main and push.
- **Commits:**
  - identity: `git -c user.name=cjp-dev -c user.email=cjp230364@gmail.com commit`;
  - trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`;
  - run `git fsck --full --no-dangling` after each commit.
- **Tools:**
  - `env -u GH_TOKEN` for gh and push;
  - run dotnet through `bash -l -c`;
  - build WPF with `-p:EnableWindowsTargeting=true`.
- **Never commit:** training data, `Egaroucid-main`, Egaroucid's resources or reference binaries.
- **Long runs** (matches, `mid-quality` solving) run in the background with nothing else heavy on the machine.

## Verification summary
| Gate | Check |
|---|---|
| P1 | Reference builds; fixtures deterministic |
| P2 | Perft, features, evaluations and stability exactly equal to the reference |
| P3 | Move, value, depth and nodes equal to the reference at levels 1–21 and on FFO #40–44 at level 60; FFO #40–49 correct; speed against C++ |
| P4 | Book choices equal the reference; time limits kept; smoke match clean |
| P5 | `eval-compare`, `mid-bench`, `mid-quality`, `solve-bench` and matches run for both engines; report with confidence intervals |
| P6 | Same exact scores with N threads; speed-up table against C++; 1T vs NT match lower bound > 50% |
| P7 | Docs, report and memory updated; deploy checked |

## Status
- **Phase 0: done (2026-10-30)**, commit f8b2d01 on main. Pushed; the deploy succeeded. GitHub detects the licence as GPL-3.0. The live About box shows the licence line. Build: 0 warnings, 0 errors. Engine tests: 206/206 green.
- **Phase 1: done (2026-10-30).** `Stello.Net/tools/Egaroucid.Reference`: `build.sh` (Generic build, `-ffp-contract=off`, about 35 s), the driver `reference.cpp` and `make-fixtures.sh`. Fixtures are in `Stello.Net/Egaroucid.Engine.Tests/Data/egaroucid`:
  - eval and features for 2,150 positions;
  - perft;
  - the MPC table checksum;
  - book values for 221 positions;
  - searches of the 100 `midgame.txt` positions at levels 1/5/11/15/21 and FFO #40–44 at level 60, with per-iteration node logs.

  Two runs give identical files once times are dropped from the logs. Reference checks: perft 9 = 3,005,288, and FFO #40–44 give +38/0/+6/−12/−14. Fixture positions come from seeded random games, book walks, `midgame.txt` and FFO, not from the training data.
