# Improve Stello playing strength (Stello 2.0)

This plan gets a Status section per phase. Its findings document, `Plans/improve-stello-strength-findings.md`, is updated after every phase, as with the port.

## Context
The Egaroucid port (phases 0–5, last commit a261a6d) showed where Stello loses, on 1 thread (`Plans/egaroucid-comparison-report.md`):
- Egaroucid is 330–500 Elo stronger.
- **The evaluation is the main cause.** Egaroucid's error is 4.0 discs against Stello's 11.1. Stello's moves hardly improve with 20× more time.
- **The endgame solver is 14× faster:** FFO #40–49 takes 25.6 s against 369 s.

The user's order:
1. A new, smaller evaluation for Stello, trained on Egaroucid's training data and small enough for the web: **about 1 MB** (user's choice).
2. Egaroucid's endgame solver adapted as a replacement for Stello's.
3. Only then, improvements to Stello's search.

**Engines to compare:**
- the current engine, frozen as **Stello 1** (the baseline);
- the **Egaroucid port** (the reference);
- the new **Stello 2.0**.

All three must be able to play matches against each other.

**Decisions (user):**
- 2.0 replaces the engine in the WPF and web apps once evaluation and solver pass their gates. The search work then follows as 2.x releases.
- The opening book is recalculated with v2, so its values are v2 scores in discs.
- The port's YBWC phase (6) is postponed and can be resumed later.

**Answer to "can Egaroucid's solver just replace the current one?"** Mostly yes:
- **The last 13 empty squares** (`Egaroucid.Engine/EndSearch.cs`, 984 lines) need no midgame evaluation. That covers the last 1–4 squares, the fast search up to 6 squares, the search with a local table up to 10 squares, and the stability cutoff. The only evaluation it uses is a 4-pattern move-ordering table at 11–13 squares. Stello can use its own new evaluation there instead.
- **Above 13 squares,** Egaroucid's solver is its midgame search, which orders moves with its 38 MB evaluation. That part is rebuilt on Stello's search, ordered by the new evaluation. This is why the evaluation comes first.
- **No bit conversion is needed.** Reversing the 64 bits turns the board half a turn, and Othello is symmetric under that turn. Every table in the solver is geometric. Stello's bitboards can therefore be used as they are, and the move indices come out in Stello's numbering.
- **Licence:** the code is already GPL, which allows this.

## Facts that shape the plan
- **Current Stello search** (`Stello.Engine/SearchEngine.cs`, 834 lines, 1 thread):
  - fail-hard alpha-beta and a fixed-margin selective search (:267);
  - the solver starts after midgame depth empties−8 and runs a win/loss/draw pass, then the exact search (:147–167);
  - `Solve` (:423) uses PVS, an endgame transposition table, enhanced transposition cutoff from 10 empties, evaluation ordering from 18 empties, then `SolveShallow`/`SolveLast2`/`SolveLast`;
  - `CheckAbort` runs every 1024 nodes and sends a progress heartbeat every 100 ms. The **web app's Stop and Move Now only work through those reports** (`Stello.Web/Worker/EngineWorker.cs:90`), so any new search must keep calling `Report`.
- **Evaluation** (`Evaluation/Evaluator.cs`, `EdgeTables.cs`): hand-made edge tables, stable discs, mobility and potential mobility. It has no game phases and builds a 10×10 mailbox on every call.
- **Score units:** heuristic scores are in evaluation units, a game end is ±(32600 + discs), and the solver works in discs. `ScoreKind` keeps them apart. The book stores `short` values in evaluation units (`BookEntry.cs`). `BookEffort.EngineVersion` makes a recalculation redo older values.
- **Web** (Blazor WASM AOT, one Web Worker, no threads): the book (`wwwroot/data/OPENING.bin`, 193 KB) is fetched with `HttpClient` and passed to the worker's `Init`. WASM has no BMI2 or AVX2, so the code must use portable bit operations; the port already does. **Desktop:** `Stello.Net` (WPF) and `Stello.App` (`LocalEngineHost.cs:22,82`).
- **A frozen v1 is cheap.** The search uses only `SearchEngine.cs`, `Search/*` (3 files), `Evaluation/*` (2 files) and the internal `Bitboards`. Tests pin no node counts; only `EvaluatorTests` pins the old evaluation's internals.
- **Training data** (`Traning data/`, git-ignored; free to use for training, redistribution prohibited, trained weights are not covered by that rule):
  - `0001`: 25.5 M positions with final scores, 500 k per disc count from 14 to 63 discs;
  - `v0002_0/1`: 52 M game records (Egaroucid 7.8 against Edax). The folder name gives the number of random opening moves. There are no scores, so each game is replayed and its result used.
- **Machine:** 24 cores, 15 GB RAM, no GPU, no Python libraries, and the projects are .NET only. **So the trainer is written in C# and runs on the CPU.**
- **Reference for a small evaluation:** Egaroucid for Web (`Egaroucid-main/src/web/`) has a 192 KB embedded evaluation with 12 patterns of 8–9 squares and 10 phases.

## Phase 0: Set-up and the frozen baseline
- **Branch:** `feature/stello-v2`, with the plan and findings documents.
- **Closing the port:**
  - the port plan's Status records phase 6 (YBWC) as postponed;
  - the port's docs chapters are written now (its planned phase 7), so the docs match the code: `docs/brain/19-the-egaroucid-port.md` and `20-engine-comparison.md`, built from the findings and the report, and linked from README and chapter 01.
- **`Stello.Net/Stello.Engine.V1`:** a new project in the namespace `Stello.Engine.V1`. It copies `SearchEngine`, `Search/*` and `Evaluation/*` unchanged and shares `Board`, `Move`, `SearchLimits` and `SearchResult`. It gets `InternalsVisibleTo` for `Bitboards`. Only tools and tests reference it.
- **`V1MatchPlayer : IMatchPlayer`** (`BookMatch.cs` pattern) with Stello 1's book choice.
- **Benchmark:**
  - `engine=stello-v1|stello|egaroucid` in `MatchCommands.ParseSide`, with the bool turned into an enum;
  - `--engine stello-v1` for `solve-bench`, `mid-bench` and `mid-quality` (`BenchSearcher`).
- **A/B switches:** an internal `SearchOptions` on Stello's `SearchEngine` (e.g. `eval=classic|pattern`, `endgame=classic|new`, plus later flags). Match sides set them with `opt=…`. The classic paths are removed at the 2.0 release, once v1 holds them.
- **Gate P0:**
  - v1 gives the same moves, scores and nodes as the current engine on `midgame.txt` at depth 8, and on FFO #40–44;
  - a 1,180-game match of v1 against the current engine scores 50 % within its interval;
  - all tests pass.

## Phase 1: Training data and the C# trainer
**New tool `Stello.Net/tools/Stello.Trainer`** (GPL-3.0-only).

- **`convert`** writes compact binary shards to `Traning data/stello-bin/`, which stays git-ignored and is never committed.
  - Each record is own u64, opponent u64 and the score as sbyte: 17 bytes.
  - Sources:
    - `0001` as it is;
    - `v0002`: the games are replayed and their random opening moves dropped. Each position gets the game's final disc difference from its side to move. Passes are handled, and empty squares go to the winner.
  - Up to about 10 M positions per disc count are kept, roughly 10 GB on disk, of 150 GB free.
  - Positions are split by game into training, validation (2 %) and test (2 %). The test split replaces `eval-compare`'s current sample, which overlaps Egaroucid's training data.
- **`train`** fits pattern tables with mean squared error on the final score, in discs.
  - The symmetric placements of a pattern share one table, and the mirror image of a pattern's shape shares indices.
  - The game is split into stages by disc count, and neighbouring stages are smoothed together.
  - The optimiser is Adam, with steps scaled by how often each weight is seen, as Egaroucid's trainer does, and early stopping on validation.
  - It runs on all 24 cores, one stage's data in memory at a time.
- **Output:** `stello-eval.bin` (magic, version, pattern-set id, stages, int8 or int16 tables with a scale). The `report` command gives validation error per stage and the file size, raw and gzip-compressed.
- **Gate P1:**
  - `convert` is deterministic: the same input gives the same hash;
  - a first model, about 11 patterns × 15 stages, beats Stello 1's 11.1-disc error by a wide margin on the test split.

## Phase 2: Evaluation 2.0 in Stello
- **Experiments with the trainer, chosen by numbers:**
  - pattern sets: a Logistello/Edax-like set (rows 2–4, diagonals 4–8, edge + 2X, corner 2×5, corner 3×3), plus selected Egaroucid shapes;
  - 10, 15, 20 or 30 stages;
  - extra features: mobility, potential mobility, parity;
  - int8 or int16.

  A table of test error against compressed size picks the best model under about 1 MB. Its error is reported next to Egaroucid (4.0) and Stello 1 (11.1).
- **`Stello.Engine/Evaluation/PatternEvaluator.cs` and `EvalWeights`** (load from a stream or from bytes):
  - the indices are computed from scratch, with byte and line extraction and base-3 lookup tables, because Stello's boards are immutable;
  - no BMI2 or AVX2 is needed, so it runs on WASM AOT;
  - if profiling shows the leaf evaluation dominates, incremental updates are added.
- **Search fitted to the new scale:**
  - the new evaluation's own resolution (for example 1/32 disc) is used inside the search;
  - the selective-search margin is refitted;
  - the evaluation ordering from 18 empties uses the new evaluation;
  - `IsDangerous` at the horizon is checked;
  - heuristic `SearchResult.Score` values are reported in discs.
- **Gate P2:**
  - test error and size are in the findings, and the load and evaluation speed are measured;
  - `mid-quality` is better than v1 at 50, 200 and 1000 ms;
  - matches of `opt=eval=pattern` against v1 at 100 ms and 500 ms, 1,180 games each, have a lower bound above 50 %;
  - the same matches against Egaroucid, for the record.

## Phase 3: The new endgame solver
- **New files in `Stello.Engine/Endgame/`, adapted from `Egaroucid.Engine`:**
  - `EndSearch.cs` (`Last1`–`Last4`, the fast search up to 6 squares, the search with a local table at 7–10 squares, and the one at 11–13 squares);
  - the stability cutoff and `Stability.cs`, the local transposition table, the parity tables, `Flip`.
  - They use Stello's bitboards directly, with no conversion. The ordering at 11–13 squares uses the new evaluation instead of `eval_move_ordering_end.egev`. If it measurably helps, a small ordering table is trained instead.
- **Above 13 squares,** Stello's `Solve` is rebuilt as Egaroucid's null-window/PVS structure:
  - transposition entries with bounds and best moves, and ETC;
  - ordering by mobility (corners counted double), potential mobility and a shallow search with the new evaluation;
  - no selectivity yet.
- **The win/loss/draw pass** first, or exact at once with a window: `solve-bench` decides.
- **Cancellation:** abort checks and progress reports in the new code too.
- **Gate P3:**
  - FFO #40–49 are all correct in at most about 2× the port's 25.6 s, against Stello 1's 369 s. The scores equal the port's;
  - `EndgameTests` pass, including the minimax comparison and the endgame suite;
  - Move Now during a solve answers within 100 ms;
  - a 500 ms match of `endgame=new` against `endgame=classic` (both on the new evaluation) is not worse, with an upper bound above 50 %;
  - the time to solve a position with 20–22 empty squares in the browser's Release build is noted.

## Phase 4: Release Stello 2.0 in the apps
- **Clean-up:** the classic evaluation and solver are removed from `Stello.Engine`, because v1 keeps them. `EvaluatorTests` are rewritten for the new evaluation.
- **Weights file:**
  - shipped as `Book/stello-eval.bin`, linked into WPF like `OPENING`;
  - on the web, copied to `wwwroot/data` by the same build step as the book, fetched by `StartupBook` beside it and passed to the worker's `Init`;
  - the transferred size and load time are checked.
- **Book:**
  - `BookEffort.EngineVersion` is bumped, and the book is recalculated with v2 through `Stello.BookTool recalc` (`BookRecalculator`), with its values in discs;
  - the GUI shows scores in discs;
  - About shows version 2.0.
- **Gate P4:**
  - with books on, v2 with the new book beats v1 with the v1 book at 500 ms (1,180 games);
  - v2 with the new book is at least as good as v2 with the old book's moves;
  - Stello.Engine and Egaroucid tests pass;
  - WPF builds with `-p:EnableWindowsTargeting=true`;
  - after the deploy, the live site plays, stops and solves (screenshot via the Playwright set-up).
- **Docs:** a chapter `docs/brain/21-stello-2-evaluation-and-solver.md`, plus updates to 01, 18 and README.
- Fast-forward main, push and check the deploy run.

## Phase 5: Search improvements (2.x)
Each change is a separate commit behind a `SearchOptions` flag and is measured on its own. The order follows the findings' expected gain:
1. **Time use:** no new iteration after 2/3 of the time, so Stello uses 313 of 500 ms. Use a cost estimate of the next iteration instead.
2. PVS throughout, and a transposition table with bounds and two moves, cleared by importance instead of two tables.
3. Multi-ProbCut with an error model fitted on v2's own shallow and deep searches of training positions.
4. Move ordering by a shallow search, and ETC in the midgame.
5. A selective endgame (solve earlier at 74 % → 100 %), and a clog search with a depth or time limit.
6. Checking the moves outside the book when leaving it.
7. Optional: Lazy SMP or YBWC.

- **Gate per change:**
  - a 1,180-game match against the previous best v2 at 100 ms with a lower bound above 50 %, confirmed at 500 ms;
  - a speed-only change may pass on `mid-bench` / `solve-bench --compare` instead, with no match regression.
- Kept changes become the default and their flags are removed; 2.x releases follow phase 4's release steps.

## Working rules (unchanged)
- **Git:**
  - one commit per phase (phase 5: per change), with identity `-c user.name=cjp-dev -c user.email=cjp230364@gmail.com` and the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`;
  - `git fsck --full --no-dangling` after each commit;
  - `env -u GH_TOKEN` for push; fetch and rebase first;
  - fast-forward main after each green gate, then check the deploy.
- **Tools:** `bash -l -c` for dotnet; `EGAROUCID_RESOURCES=…/Egaroucid-main/bin/resources` for the tools.
- **Never commit:** training data, binary shards, `Egaroucid-main`, Egaroucid's resources.
- **The trained `stello-eval.bin` is committed** (derived weights are allowed) and credited in `THIRD-PARTY-NOTICES.md`.
- **Machine:** no heavy work runs during timed matches. Training runs when no match runs.
- **Docs and memory:** findings updated after every phase, and memory `stello-engine-improvement.md` updated.

## Verification summary
| Gate | Check |
|---|---|
| P0 | v1 identical to the current engine (moves, scores, nodes); v1 against current ≈ 50 %; tests pass; docs 19/20 written |
| P1 | Deterministic conversion; first model far below 11.1 discs on the test split |
| P2 | Error against size table, model ≤ ~1 MB; mid-quality better than v1; matches against v1 at 100 and 500 ms with lower bound > 50 % |
| P3 | FFO #40–49 correct, ≤ ~2× the port's time; endgame tests; Move Now < 100 ms; match not worse |
| P4 | v2 with its book beats v1 with its book; apps build; live site plays, stops and solves |
| P5 | Each change: match lower bound > 50 % (or a speed gain with no regression) |

## Status
- **Phase 0: done (2026-10-31).**
  - `Stello.Net/Stello.Engine.V1` (Stello 1) and `V1MatchPlayer`; `engine=stello-v1` for match sides, `--engine stello-v1` for `solve-bench` and `mid-bench`, and `--engines` takes any of `stello`, `stello-v1` and `egaroucid` in `mid-quality` (paired differences for every pair).
  - Stello 1 equals the engine at a261a6d: the same moves, scores and nodes on 100 `midgame.txt` positions at depth 8 and on FFO #40–44. `StelloV1Tests` checks 32 recorded searches.
  - Match against the current engine at 100 ms: 51.8 % (49.5–54.1 %), not significant.
  - `SearchOptions` and `opt=` moved to phase 2, where the first alternative appears.
  - Port closed: phase 6 postponed, chapters 19 and 20 written (README, 01, 17 and 18 updated).
  - Tests: Stello.Engine.Tests 238, Egaroucid.Engine.Tests 34, all green.
- **Phase 1: done (2026-10-31).**
  - `tools/Stello.Trainer` with `convert`, `train` and `report`; `Stello.Engine/Evaluation/Symmetry.cs`, `Patterns.cs` (pattern shapes, places, folding, the `classic11` set) and `EvalWeights.cs` (the gzip file, version 1). Tests: `PatternTests` (7).
  - `convert`: 52 M games and 25.5 M positions → 499.5 M positions (8 GB, git-ignored) in 1 m 53 s; deterministic (the same hash with 24 and 7 workers).
  - First model, `classic11` × 15 stages, 1.5 MB: 5.55 discs error on the test split, against Stello 1's 12.14 and Egaroucid's 5.05; 4.8 M evaluations per second.

- **Phase 2: done (2026-10-31).**
  - Pattern sets `plus15` (62 features) and `egaroucid16` (64) beside `classic11`. `EvalWeights` version 2 stores each stage as the difference from the previous one, which compresses better.
  - Chosen model `Stello.Net/Book/stello-eval.bin`: `plus15` × 10 stages, rare weights (seen < 30 times) zeroed, 1/8-disc units, 0.99 MB. Test error 5.47 discs (Stello 1 12.14, Egaroucid 5.05). 3.3 M evaluations per second.
  - `SearchOptions` (internal): `Pattern`, `SelectiveMargin`, `SelectivePercent`, `DangerousCorners`. Match sides: `eval=<file>|default|classic`, `sel-margin=`, `sel-percent=`, `danger=on|off`; `--eval` for the benchmarks; `mid-quality --sides`.
  - The search uses the pattern score in 1/100 disc, for leaves and for the solver's ordering from 18 empties. Tuning the selective margin and the corner extension gave nothing significant, so they stay until phase 5.
  - Gate: against Stello 1, 90.9 % (+400 Elo) at 100 ms and 90.6 % (+393) at 500 ms. `mid-quality` loss 1.33 / 0.91 / 0.68 discs against Stello 1's 1.59 / 1.59 / 1.39 at 50 / 200 / 1000 ms; the gap at 50 ms is within its margin. Against Egaroucid: 59.5 % (+67) at 100 ms, 38.2 % (−84) at 500 ms.
  - Tests: Stello.Engine.Tests 250, Egaroucid.Engine.Tests 34, all green.
