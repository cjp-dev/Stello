# Findings from improving Stello's playing strength (Stello 2.0)

What the work on Stello 2.0 teaches us, written down as we go: the decisions, the measurements, what worked and what did not, and the ideas for later.

- **The plan:** [improve-stello-strength-plan.md](improve-stello-strength-plan.md). This file is updated after every phase.
- **Where it starts:** the comparison with the Egaroucid port ([egaroucid-comparison-report.md](egaroucid-comparison-report.md), [egaroucid-port-findings.md](egaroucid-port-findings.md)).
- **The engines:**

  | Name | Project | Match side / benchmark | What it is |
  |---|---|---|---|
  | Stello 1 | `Stello.Net/Stello.Engine.V1` | `engine=stello-v1`, `--engine stello-v1` | The engine before 2.0 (commit a261a6d), frozen as the baseline |
  | Stello 2 | `Stello.Net/Stello.Engine` | `engine=stello` (the default), `--engine stello` | The engine being improved; the apps use it |
  | Egaroucid | `Stello.Net/Egaroucid.Engine` | `engine=egaroucid`, `--engine egaroucid` | The C# port of Egaroucid 7.9, the reference |

## Status

| Phase | State | What it showed |
|---|---|---|
| 0 Set-up and the frozen baseline | done | Stello 1 searches node for node like the engine it was copied from; a match against it is even |
| 1 Training data and trainer | done | 500 M positions in 2 minutes; a first model (11 patterns × 15 stages, 1.5 MB) has 5.55 discs error on held-out games, against Stello 1's 12.14 and Egaroucid's 5.05 |
| 2 Evaluation 2.0 | done | 15 patterns × 10 stages in 0.99 MB: 5.47 discs error; +400 Elo against Stello 1 at 100 and 500 ms, +67 against Egaroucid at 100 ms, −84 at 500 ms |
| 3 Endgame solver | done | FFO #40–49 in 47.1 s (Stello 1 369 s, Egaroucid port 25.6 s); in games as strong as the old solver (51.1 % at 500 ms) |
| 4 Release 2.0 in the apps | done | The engine is Stello 2 with a book recalculated by it; with books on it scores 88.9 % (+362) against Stello 1 at 500 ms, and the new book is as good as the old one (49.2 %); in the browser 22 empty squares solve in 0.6–1.3 s (Stello 1: 1.1–4.5 s) |
| 5 Search improvements (final 2.0) | done (released 2026-11-02) | Item 1 (time use): no gain, the 2/3 rule stays. Item 2: PVS in every node +28/+30 Elo, kept; a table with both bounds and two moves gave nothing; item 3 Multi-ProbCut +120 at 500 ms; item 4 ordering and ETC kept; item 5 rejected; the book recalculated with the phase 5 search: 51.7 % against the first 2.0 release's book; item 6 (book exit check) rejected |

## Stello 1, the frozen baseline

- **What was copied:** `SearchEngine`, `Search/*` (move ordering, time control, transposition table), `Evaluation/*` (evaluator, edge tables) and `ComputerPlayer`, under the namespace `Stello.Engine.V1`. Board, moves, limits, results and the opening book are shared with Stello 2. Stello 1 reaches the internal `Bitboards` through `InternalsVisibleTo`.
- **Proof that it is the same engine:**
  - on all 100 `midgame.txt` positions at depth 8, and on FFO #40–44, it gives the same moves, scores, kinds, depths and node counts as `Stello.Engine` at a261a6d;
  - `StelloV1Tests` keeps checking 30 of those searches and FFO #40–41 against the recorded `Data/stello-v1-signatures.txt`, so the copy can't drift.
- **A/B switches:** the plan put `SearchOptions` (`opt=` on a match side) in phase 0. There is nothing to switch yet, so it comes with the first alternative, the new evaluation in phase 2.
- **Its book:** Stello 1 plays from `Book/opening-book.txt`. When phase 4 recalculates the book for Stello 2, the current book must be kept for Stello 1.

## The training data

- **Converted** by `Stello.Trainer convert` into `Traning data/stello-bin` (git-ignored, never committed): 17-byte records (own discs, opponent's discs, final score), one file per split and disc count.
  - `0001`: 25.5 M positions with scores, all kept. Split by line, so positions of one game can fall in different splits.
  - `v0002`: all 52 M games replayed (no invalid game). The positions after the random opening moves are kept, thinned to about 10 M per disc count.
  - Result: 499.5 M positions, 8.0 GB: 479.6 M training, 10.0 M validation, 10.0 M test. From 20 discs on there are about 10 M per disc count; at 12 discs 1 M; below 12 almost none (the data starts after at least 8 random moves).
  - 1 m 53 s on 24 cores. The output does not depend on the number of workers (checked: the same SHA-256 with 24 and 7 workers).
- **The test split is harder than `eval-compare`'s sample.** Most test positions come from `v0002`, games between Egaroucid and Edax at level 11, whose results are noisier than Egaroucid's self-play at level 17. On it Egaroucid's own error is 5.05 discs (3.96 on `eval-compare`'s sample).

## The pattern evaluation

- **Patterns** (`Stello.Engine/Evaluation/Patterns.cs`): a pattern is a list of squares in its canonical place. Its other places are read from the position turned by one of the 8 symmetries, so all places share one table. A symmetry that maps the pattern onto itself (a row read backwards) makes two indices one weight ("folding").
  - The squares of one row are read with a shift; the squares of one diagonal with a multiplication by 0x0101…01 into the top byte. A table turns each byte into base-3 digits. This needs no BMI2 or AVX2, so it also runs in WebAssembly.
  - Tests: every index equals reading square by square, and the folded weights give the same sum for all 8 symmetric positions.
- **The first set, `classic11`** (Logistello / Edax-like): rows 2–4, diagonals of 4–8 squares, the edge with both X-squares, the 2 × 5 corner and the 3 × 3 corner. 46 features, 113,877 weights per stage.
- **Training** (`Stello.Trainer train`): full-batch Adam on the mean residual of each weight, rate 0.2, halved when validation stops improving. A stage trains on its own disc counts ± 2. One stage of 8 M positions converges in 300–500 loops, about 30 s on 24 cores.
  - Training and validation errors are equal (5.90 against 5.90 in stage 7), so the model is not overfitting: more data would not help, more patterns might.
  - The rate hardly matters (0.05 with 1,000 loops and 0.2 with 300 give the same 5.903); a stronger pull towards zero (prior 20) is slightly worse.
- **The file** (`EvalWeights`, version 2): gzip of the folded weights as int16, each stage stored as its difference from the stage before, low bytes first and then high bytes. 15 stages of `classic11` in 1/32 discs: 1.5 MB in version 1, 1.27 MB this way; in 1/16 discs 1.17 MB.

### Phase 2, experiment 1: stages and rare weights (`classic11`, 1 M positions per disc count, 1/16 discs)

| Stages | Rare weights | File | Test error (MAE / RMSE) |
|---:|---|---:|---:|
| 10 | kept | 0.81 MB | 5.59 / 7.44 |
| 15 | kept | 1.5 MB (1/32 discs, version 1) | 5.55 / 7.40 |
| 15 | zeroed below 10 appearances | 0.97 MB | 5.55 / 7.41 |
| 20 | kept | 1.52 MB | 5.53 / 7.38 |
| 30 | kept | 2.20 MB | 5.51 / 7.36 |

- **More stages hardly help:** from 10 to 30 stages the error falls by 0.08 discs while the file grows 2.7×. Training on ± 2 disc counts already makes neighbouring stages share their data.
- **Weights seen fewer than 10 times can be zeroed** at no cost, and the file shrinks by a third. About half of all weights are never seen at all.
- So the budget of about 1 MB is better spent on more patterns than on more stages.

### Phase 2, experiment 2: pattern sets (10 stages, rare weights zeroed, 1 M positions per disc count)

| Set | Patterns / features | Weights per stage | File | Test error (MAE / RMSE) | Evaluations per second |
|---|---|---:|---:|---:|---:|
| `classic11` | 11 / 46 | 113,877 | 0.68 MB | 5.59 / 7.45 | 4.65 M |
| `plus15` | 15 / 62: `classic11` + Egaroucid's triangle, corner + block, cross, edge + y | 233,676 | 1.28 MB | 5.46 / 7.28 | 3.72 M |
| `egaroucid16` | Egaroucid 7.9's 16 / 64 | 320,193 | 1.62 MB | 5.37 / 7.16 | 2.17 M |

Egaroucid itself (the same 16 patterns, 60 stages, trained on more data) has 5.05 on this test.

**Matches between them** (1,180 games each, the same search):

| Match | 100 ms per move | 500 ms per move |
|---|---:|---:|
| `plus15` against `classic11` | 50.7 % (48.1–53.4), +5 | **53.2 % (50.6–55.8), +22** |
| `egaroucid16` against `classic11` | 49.4 % (47.0–51.9), −4 | — |

- At 100 ms, the better accuracy and the slower evaluation cancel out. At 500 ms `plus15` wins at the same thinking time (depth 13.4 against 13.6, 3.0 M against 3.55 M nodes per second): **accuracy matters more as the search gets deeper**.
- `egaroucid16` is too slow in this implementation (2.2 M evaluations per second): its diagonal patterns with extra squares are read row by row, up to 8 parts each.

### Phase 2, experiment 3: `plus15` within about 1 MB

| Variant (`plus15`, 1 M positions per disc count) | File | Test error (MAE / RMSE) |
|---|---:|---:|
| 10 stages, zeroed below 10 appearances, 1/16 discs | 1.28 MB | 5.46 / 7.28 |
| 10 stages, zeroed below 30 | 1.09 MB | 5.47 / 7.30 |
| 8 stages, zeroed below 10 | 1.06 MB | 5.49 / 7.32 |
| 10 stages, zeroed below 10, 1/8 discs | 1.16 MB | 5.46 / 7.28 |
| **10 stages, zeroed below 30, 1/8 discs: chosen** | **0.99 MB (990,568 bytes)** | **5.47 / 7.31** |
| 3 M positions per disc count | — | ran out of memory (24 M positions × 62 features as 32-bit indices); not needed, since training and validation errors are equal |

The chosen model is `Stello.Net/Book/stello-eval.bin` ([report](benchmark-results/v2-p2-eval-p15-s10-m30-u8.md)); `eval=default` and `--eval default` in the benchmark tool. 3.3 M evaluations per second from scratch. Its stage for 4–10 discs is copied from 11–16 discs: the data has almost no positions there, and the opening book covers them.

### Phase 2: tuning the old search to the new evaluation (100 ms per move, 1,180 games each, against the defaults)

| Change | Score | Elo |
|---|---:|---:|
| Selective margin \|beta\| × 25 % + 50 (default 50 % + 50) | 49.4 % (47.0–51.7) | −4 |
| Margin 50 % + 100 | 50.1 % (47.8–52.4) | +1 |
| Margin 25 % + 25 | 48.3 % (45.9–50.7) | −12 |
| A fixed margin of 2 discs (0 % + 200) | 51.7 % (49.2–54.1) | +11 |
| No "dangerous corner" extension | 49.4 % (47.2–51.6) | −4 |

- **None is significant.** The selective search stays as it is until phase 5 replaces it with a fitted Multi-ProbCut.
- **The corner extension is worth nothing** with the new evaluation. It is the only part of the search that still reads Stello 1's edge tables, so phase 4 can drop it together with the classic evaluation.

### The first match with the pattern evaluation

`classic11`, 10 stages (0.81 MB), in Stello's unchanged search (`eval=` on the match side): scores in 1/100 disc, the old selective-search margin and the old "dangerous corner" extension.

- **Against Stello 1 at 100 ms per move: 90.7 % (89.2–92.2 %), +395 Elo, +14.6 discs per game** (1,180 games). For comparison, the Egaroucid port scored 87.1 % against Stello 1 at 100 ms.
- The search also got faster and deeper: 3.44 M against 2.76 M nodes per second, midgame depth 11.4 against 10.3. On `mid-bench` at depth 8: 4.0 M nodes in 1.15 s against 4.65 M in 1.9 s (better move ordering from better values).
- The evaluation alone is worth more than the whole difference between Stello 1 and Egaroucid at this speed.

### The phase 2 gate: `stello-eval.bin` (`plus15`, 10 stages, 0.99 MB)

`eval=default` in Stello's search, with the old selective margin and corner extension. Matches have 1,180 games, no books, 1 thread per engine.

| Match | Score | Elo | Discs per game | Midgame depth (B / A) |
|---|---:|---:|---:|---:|
| Stello 2 against Stello 1, 100 ms | **90.9 %** (89.3–92.5) | **+400** | +15.1 | 11.10 / 10.38 |
| Stello 2 against Stello 1, 500 ms | **90.6 %** (89.0–92.2) | **+393** | +12.7 | 13.08 / 12.34 |
| Stello 2 against Egaroucid, 100 ms | **59.5 %** (57.0–62.1) | **+67** | +1.9 | 11.40 / 8.62 |
| Stello 2 against Egaroucid, 500 ms | 38.2 % (35.9–40.5) | −84 | −2.1 | 13.36 / 13.52 |

`mid-quality` (300 positions, loss in discs against the best move, lower is better; paired difference Stello 1 − Stello 2):

| ms/move | Stello 1 | Stello 2 | Egaroucid | Stello 1 − Stello 2 | Stello 2 − Egaroucid |
|---:|---:|---:|---:|---:|---:|
| 50 | 1.59 ± 0.31 | 1.33 ± 0.30 | 1.18 ± 0.30 | +0.26 ± 0.36 | +0.15 ± 0.26 |
| 200 | 1.59 ± 0.33 | 0.91 ± 0.22 | 0.94 ± 0.24 | +0.68 ± 0.36 | −0.03 ± 0.21 |
| 1000 | 1.39 ± 0.30 | 0.68 ± 0.18 | 0.54 ± 0.18 | +0.71 ± 0.30 | +0.14 ± 0.19 |

- **The gate passes.** Both matches against Stello 1 are far above 50 %. Stello 2 loses less than Stello 1 at all three times, clearly at 200 and 1000 ms. At 50 ms the difference (+0.26 ± 0.36) is inside its margin: in 50 ms both engines are mostly at the same shallow depth.
- **Unlike Stello 1, Stello 2 gets better with time** (1.33 → 0.91 → 0.68 discs lost), as Egaroucid does. Stello 1 hardly improves (1.59 → 1.39).
- **At 100 ms Stello 2 beats the Egaroucid port.** Egaroucid's 38 MB evaluation is slow to compute, and at 100 ms it only reaches depth 8.6. At 500 ms the depths are equal, and Egaroucid's better evaluation and search are worth about 84 Elo. Phases 3 and 5 go after that gap.
- `plus15` against `classic11` at 500 ms: 53.2 % (50.6–55.8), +22 Elo. At 100 ms it was even (50.7 %): the bigger set is 20 % slower and only pays off with more time.

## The new endgame solver (phase 3)

`Stello.Engine/Endgame/` (`SearchEngine.Endgame.cs`, `EndgameBits.cs`, `StableDiscs.cs`, `LocalTable.cs`). It is switched on with `SearchOptions.NewEndgame`, `endgame=new` on a match side and `--endgame new` in `solve-bench`/`mid-bench`.

- **The last 13 empty squares are Egaroucid's.** This part is adapted from the port's `EndSearch.cs`:
  - `Last4` … `Last1`, and the fast search at 5–6 squares, which plays squares alone in their region first;
  - at 7–10 squares, parity and the opponent's mobility, with the local table;
  - at 11–13 squares, mobility only;
  - the stable-disc cutoff, the 4 empty-square tricks and the wipe-out test.
- **It uses Stello's bitboards as they are.** Egaroucid numbers the squares the other way round. Reversing 64 bits turns the board half a turn, and every table in the solver is the same after that turn, so nothing is converted.
- **Above 13 empty squares the search is new, but shaped like Egaroucid's:**
  - null-window searches with fail-soft bounds, stored in Stello's endgame transposition table;
  - the stored best move is searched first, before the others are ordered;
  - ETC (enhanced transposition cutoff) from 12 empty squares;
  - move ordering by the opponent's weighted mobility and the pattern evaluation of the child, after a 1-ply search from 16 empty squares;
  - at principal-variation nodes, also potential mobility and a 2-ply search.
- **Solving still starts with win/loss/draw.** Solving exactly at once with a full window was slower (50.0 s against 45.6 s on FFO #40–49).

### What made it fast (`solve-bench`, FFO #40–44, all correct)

| Version | Nodes | Time |
|---|---:|---:|
| First working version | 151 M | 10.5 s |
| No pattern evaluation in the ordering at 11–13 empty squares | 179 M | 7.9 s |
| … and ETC from 12 instead of 16 empty squares | 173 M | 7.6 s |
| … and in solve mode a 4-ply midgame search first, to order the root moves | 94 M | 4.1 s |
| Egaroucid port, for comparison | 84 M | 4.0 s |

- **The root order matters most.** Without the short midgame search, #43 took 94 M nodes instead of 33 M. In games the midgame iterations come first anyway, so this only changes solve mode (`SearchLimits.Solve`).
- **The pattern evaluation is too slow for the ordering at 11–13 empty squares.** It saves about 20 % of the nodes, but each node becomes about 40 % slower. Egaroucid uses a small 4-pattern table updated move by move there instead.
- **The midgame table's best moves hurt deeper in the solver.** Used as the hash move where the endgame table has none, they made FFO #40–44 take 6.9–10.6 s instead of 4.1 s. The endgame ordering is better than a shallow midgame search's choice.
- **Little else mattered.** The depth of the ordering search (1 ply from 14, 16, 18 or 20 empty squares) changed the time by under 15 %.

### The phase 3 gate

- **FFO #40–49: all correct, 47.1 s, 1.10 G nodes, 23 M nodes/s** ([result](benchmark-results/v2-p3-solve-bench-40-49.json)). That is 1.8× the Egaroucid port's 25.6 s, within the gate of about 2×, and 7.8× faster than Stello 1's 369 s. The slow ones are #45 (11.1 s), #48 (12.0 s) and #49 (16.3 s).
- **Tests:** `EndgameTests` run on both solvers:
  - FFO #40–44;
  - minimax on small endgames and the last 1–4 squares;
  - the endgame-C suite.

  New tests check that:
  - the new solver agrees with Stello 1's on 30 positions with 14–18 empty squares;
  - Move Now during a solve of FFO #49 answers within 100 ms;
  - the flips, the legal moves and the final score equal Stello's own, and stable discs are never flipped in random games (`EndgameBitsTests`).
- **Matches, new against Stello 1's solver, both with `stello-eval.bin`, 1,180 games:**

  | Time per move | Score | Elo | First solve (empty squares, old → new) |
  |---:|---:|---:|---:|
  | 100 ms | 49.7 % (47.5–51.9) | −2 | 18.8 → 19.4 |
  | 500 ms | 51.1 % (49.2–53.0) | +8 | 20.7 → 21.2 |

  The solver is much faster, but the games hardly change. The search only solves after the midgame search reaches empty squares − 8 (`EndgameDistance`), so the faster solver starts only about half a square earlier. Egaroucid instead solves selectively (Multi-ProbCut at 74 %, then 88 … 100 %) from 24–28 empty squares; that is phase 5, item 5.
- **Not measured yet:** the time to solve 20–22 empty squares in the browser. The apps get the new solver in phase 4, so it is measured there.

## Releasing Stello 2.0 (phase 4)

- **The engine is Stello 2** (commit 720f224 on `feature/stello-v2`):
  - `SearchEngine` always uses the pattern evaluation and the new solver;
  - Stello 1's evaluator, edge tables, corner extension and solver are gone from `Stello.Engine` (`Stello.Engine.V1` keeps them);
  - the match keys `eval=classic`, `danger=` and `endgame=` are gone.
- **The evaluation is built into `Stello.Engine`** as an embedded resource (`EvalWeights.Default`), instead of a separate file that WPF links and the web app fetches, as the plan said. Every app and tool gets it without loading code, and the browser downloads the same bytes inside the engine assembly. That assembly is now 1.15 MB, 1.03 MB compressed.
- **The book:**
  - `OpeningBook.EngineVersion` is 3, so `recalc` searches every heuristic leaf again (8,397 of 11,613 leaves; the 3,216 exact ones are kept). The heuristic values are in 1/100 disc and the solved ones stay ±(32600 + discs). A first try with 30 s per position already reached depth 17–18, as deep as Stello 1's last recalculation in 60 s; the run uses 60 s, like Stello 1's.
  - The recalculation ran on a second computer (Ryzen 7 7800X3D, 8 workers, about 16 hours), so it did not disturb timed matches here. 11,322 values changed (5,634 leaves, 5,688 backed up), 219 more leaves were solved exactly (3,435 in all) and 1,307 positions got another first move. White's replies to d3: c5 −0.25, c3 −0.62, e3 −1.50 discs. The main line from the start is 15 moves, as before up to g5. `verify` passes.
  - One leaf had −65 discs, as the midgame evaluation was not capped; the final 2.0 caps it at ±64.
  - **Flaw found afterwards:** 2,718 leaves carry the C++ book's `INEXACT` flag, imported as win/loss/draw, with evaluation-sized values (e.g. −18). `recalc` kept every win/loss/draw value it could not solve, so these kept their C++-era values in this recalculation too (`WinLossDraw -> WinLossDraw 2,744 (0)`). Fixed on `feature/stello-v2.x` (d21e42f): only values beyond ±32,600 are kept. The second recalculation replaces them.
  - `Book/opening-book-stello1.txt` keeps the old book for Stello 1's match sides (`engine=stello-v1,book=on`).
- **The GUI** shows heuristic scores in discs with one decimal ("+1.2 discs"). The About box describes the new evaluation and solver and credits Egaroucid. Its version text already said "Stello Version 2.0" (from the C# port) and is unchanged.
- **The book-learning worked example** (chapter 11, `BookLearnerTests`) has new values: with Stello 2, c5 (−62) stays ahead of e3 (−275) at the root.

### The book gates (500 ms per move, books on, 1,180 games from `starts.txt`)

| Match | Result |
|---|---|
| Stello 2 + new book against Stello 1 + its book | **88.9 %** (87.2–90.7 %), +362 Elo, +12.6 discs per game; depth 13.05 against 12.25 ([result](benchmark-results/match_v2-p4-book-vs-v1-500ms.md)) |
| Stello 2 + new book against Stello 2 + the version 2 book (`book-file=`) | **49.2 %** (47.3–51.2 %), −5 Elo, −0.1 discs per game: not worse ([result](benchmark-results/match_v2-p4-newbook-vs-oldbook-500ms.md)) |

The games start at plies 10–12, so the books only differ in the lines they hold beyond that; an even result is what to expect there. The first match is about as strong as without books (+393), so the book costs Stello 2 nothing.

### The live site after the deploy

Checked with Playwright (headless Chromium) on https://polite-sky-0e9899910.3.azurestaticapps.net after deploy run 41290149765:
- **About** shows the Stello 2.0 text (pattern evaluation, Egaroucid's solver).
- **Plays:** with the book on and 1 s per move, the computer played 15 book moves (Score: "Book"), then searched depth 14–15 at about 1.8 M nodes per second, with scores such as "−0.4 discs".
- **Stops:** during a 60 s search, Move Now played the best move so far within 0.11 s, including the browser driver's own delay.
- **Solves:** a position with 22 empty squares was solved in 0.59 s (6.5 M nodes, +12 discs), as in the local build.
- No page errors.

### Solving in the browser

Eight positions from Stello 2 games (depth 6), with 20 and 22 empty squares, opened in the web app (AOT Release build, headless Chromium, 60 s per move, no book). The computer played the move ("Switch sides").

| | 20 empty squares | 22 empty squares |
|---|---:|---:|
| Stello 2, local AOT build | 0.1–0.3 s | 0.6–1.3 s (6.5–23 M nodes) |
| Stello 1, the live site | 0.3–0.7 s | 1.1–4.5 s (10–60 M nodes) |

All scores are the same. In the browser Stello 2 runs at 8–22 M nodes per second, about native speed. Most of the gain comes from needing fewer nodes.

## Search improvements (phase 5)

Phase 5 runs on `feature/stello-v2.x`, so the first 2.0 release keeps exactly the engine that recalculated the book. Each change is behind a `SearchOptions` switch and plays 1,180 games at 100 ms per move against the current Stello 2. To be kept it needs a lower bound above 50 %, confirmed at 500 ms.

### Item 1: time use

Today Stello starts no new midgame iteration after 2/3 of the time (soft limit) and stops at the time (hard limit). An iteration cut off by the hard limit is not wasted: each better root move it has found is kept.

| Variant (match key) | Time per move | Score | Elo |
|---|---:|---:|---:|
| Soft limit 80 % (`soft-pct=80`), 100 ms | 65 ms (62) | 51.6 % (49.4–53.8) | +11 |
| Soft limit 100 % (`soft-pct=100`), 100 ms | 66 ms (62) | 51.9 % (49.8–54.1) | +14 |
| … the same again | 67 ms (62) | 49.2 % (47.0–51.5) | −5 |
| … both runs, 2,360 games | | 50.6 % (about 49.5–51.6) | +4 |
| Soft limit 100 %, 500 ms | 308 ms (289) | 51.9 % (49.9–54.0) | +14 |
| Soft limit 85 %, 500 ms | 304 ms (289) | 51.5 % (49.4–53.6) | +11 |
| Predicted next iteration within the time (`predict=on`), 100 ms | 42 ms (62) | 44.7 % (42.3–47.2) | −37 |
| Predicted next iteration within 120 % of the time, 100 ms | 46 ms (62) | 46.4 % (44.0–48.7) | −25 |

The time in brackets is the default side's.

- **Rejected: none passes the gate.** Using more of the time is worth a few Elo at most. The average time per move rises only 5–7 %, because the endgame moves end early anyway, and one more iteration is about one ply. The 2/3 rule stays.
- **Predicting the next iteration is clearly worse.** It does not start iterations that would not finish, but a cut-off iteration still improves the root move. The code was removed.
- `SearchOptions.SoftTimePercent` (`soft-pct=`) stays as a switch, so it can be tried again once the search is deeper and more selective.

### Item 2: principal variation search and the hash table

**2a, PVS in every midgame node.** Only the root searched its later moves with a null window. Now every node does: the first move with the full window, the others with a null window and a re-search if they turn out better.

| Against the old search | Score | Elo | Midgame depth (old → new) |
|---|---:|---:|---:|
| 100 ms | 54.0 % (51.8–56.3) | +28 | 11.53 → 11.68 |
| 500 ms | 54.4 % (52.1–56.6) | +30 | 13.34 → 13.67 |

Kept, now the default (commit 53644bd). At a fixed depth of 10, `mid-bench` needs a little more (17.5 M nodes against 16.3 M), but in the same time the search gets deeper. The null windows probably let the selective search cut more often.

**2b, a midgame table with both bounds and two moves.** Each entry kept a lower and an upper bound, the best move and the one before it, and the search that stored it. All matches are against Stello 2 with PVS, at 100 ms:

| Variant | Score | Elo | Midgame depth |
|---|---:|---:|---:|
| Both bounds, two moves, entries of earlier searches replaced first | 47.8 % (45.5–50.0) | −16 | 11.54 (11.62) |
| … without the replacement by search | 48.5 % (46.1–50.8) | −11 | 11.49 (11.64) |
| … and without the second move | 44.7 % (42.5–47.0) | −37 | 11.29 (11.64) |
| A shallower result no longer changes the move of a deeper one, with replacement by search | 48.3 % (45.9–50.6) | −12 | 11.52 (11.64) |
| … without the replacement by search | 50.6 % (48.5–52.8) | +4 | 11.68 (11.61) |

- **Rejected; the code was removed.** The best version is only even with the old table.
- **What was learned:**
  - The selective search's shallow searches at depth 3 store results for the same positions just before the deep searches. A table that lets those results change a deeper entry's move loses depth.
  - Entries from the previous move's search are worth keeping: the tree two plies later is mostly the same. Replacing them first costs about 15 Elo.
  - The second move helps (−37 → −11). It may be worth adding to the old table on its own later.

### Item 3: Multi-ProbCut

Multi-ProbCut replaces the old selective search. That search used a fixed margin, |β|/2 + 50, at `look` 7 and 5. The new one follows Egaroucid's `multi_probcut.hpp`:
- from depth 3, the shallow depth is about half the depth with the same parity (3 → 1, 7 → 3, 13 → 7);
- a cut needs the shallow null-window search to be t × σ outside the window;
- the static evaluation must first be within 4 discs of that margin;
- inside the shallow search Multi-ProbCut is off, and it is not used near won and lost scores.

**The error model.** The trainer's new `mpc` command (`tools/Stello.Trainer/MpcCommand.cs`):
- It searched 11,700 test-split positions, 300 per disc count from 14 to 52, without selectivity, at every depth up to min(13, empties − 8). That took 2 hours on 24 threads.
- It wrote every iteration's score to `Traning data/mpc-values.csv` (git-ignored).
- It printed σ of deep − shallow per group of 4 disc counts and fitted σ = c0 + c1·x + c2·x² + c3·s + c4·d by weighted least squares, with x = discs / 64.

σ of deep − shallow in discs, for the shallow depth that is used:

| Discs | 1→3 | 2→4 | 3→5 | 2→6 | 3→7 | 4→8 | 5→9 | 4→10 | 5→11 | 6→12 | 7→13 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 16–19 | 2.39 | 2.26 | 2.23 | 2.69 | 2.48 | 2.35 | 2.28 | 2.47 | 2.22 | 2.23 | 2.11 |
| 24–27 | 3.11 | 2.68 | 2.41 | 3.11 | 2.98 | 2.80 | 2.55 | 2.88 | 2.68 | 2.70 | 2.49 |
| 32–35 | 3.44 | 2.71 | 2.33 | 3.53 | 3.27 | 2.94 | 2.60 | 3.16 | 2.96 | 2.74 | 2.74 |
| 40–43 | 3.39 | 3.12 | 3.03 | 3.92 | 3.69 | 3.32 | 3.24 | 3.63 | 3.61 | 3.12 | 3.11 |
| 48–51 | 3.70 | 3.22 | 3.02 | 3.96 | 3.65 | 3.25 | | | | | |

- **Fit:** σ = 1.8871 + 2.9543·x + 0.0660·x² − 0.6749·s + 0.2778·d discs. It is off by 10.2 % on average and 38 % at worst, over 117 cells.
- **The spread grows with the number of discs and shrinks as the shallow search gets closer to the deep one.** The mean of deep − shallow is −0.19 discs: no bias worth correcting.
- **Game ends had to be filtered out.** In 25 positions the deep search found a forced game end (score ±32 6xx). Those few values blew σ up to 7–20 discs at 36–47 discs, and the first fit was off by 47 %. They are now skipped, as Multi-ProbCut skips such scores.
- **The static evaluation (0→2) is much worse** than any search (2.8–7.7 discs, growing towards the end). That is why Multi-ProbCut starts at depth 3, as in Egaroucid.

**Matches against Stello 2 with the old selective search**, 1,180 games each:

| t (`mpc=`) | 100 ms: score | Elo | depth (old → new) | 500 ms: score | Elo | depth (old → new) |
|---:|---:|---:|---:|---:|---:|---:|
| 0.8 | 69.2 % (67.0–71.5) | +141 | 11.54 → 14.16 | | | |
| 1.0 | 67.3 % (65.0–69.6) | +126 | 11.59 → 13.81 | 66.6 % (64.4–68.7) | +120 | 13.61 → 16.55 |
| 1.15 | 68.9 % (66.7–71.2) | +139 | 11.55 → 13.51 | | | |
| 1.3 | 70.0 % (67.7–72.2) | +147 | 11.55 → 13.22 | 65.8 % (63.6–68.1) | +114 | 13.62 → 15.92 |
| 1.6 | 64.9 % (62.7–67.1) | +107 | 11.54 → 12.62 | | | |
| 2.0 | 60.2 % (57.7–62.7) | +72 | 11.57 → 12.00 | | | |

- **t = 1.0 beat t = 1.3 directly at 500 ms:** 52.2 % (50.1–54.3), +15 Elo, depth 16.55 against 15.83.
- **Kept, with t = 1.0 as the default.** This is the largest gain of phase 5 so far, about +120 Elo at 500 ms. The old selective search, its `SelectiveMargin`/`SelectivePercent` options and the `sel-margin=`/`sel-percent=` keys were removed.
  - `SearchOptions.ProbCutPercent` (`mpc=`) stays to retune t when the search changes.
  - `SearchOptions.Selective = false` turns Multi-ProbCut off for the `mpc` command.
- **Everything between t = 0.8 and 1.3 is within noise at 100 ms.** At 500 ms the lower t wins: there the extra depth is worth more than the cuts it gets wrong.
- **Speed:**
  - at a fixed depth of 10, `mid-bench` needs 3.8 M nodes instead of 17.5 M (4.6× fewer);
  - the first solve starts about 1 empty square earlier (21.3 → 22.5 at 500 ms);
  - FFO #40–44 are still all correct, in 4.0 s.
- **Next ideas for Multi-ProbCut:**
  - Egaroucid also uses it in the endgame (shallow depth d/3, item 5);
  - a fit per depth pair instead of the linear model;
  - t by game phase.

### Item 4: move ordering and midgame ETC

The midgame ordered its moves by the hash move, the response killers and the square values. Five additions were tried behind switches, against Stello 2 with Multi-ProbCut:
- **Evaluation ordering from look L:** Egaroucid's depth-0 key, 35 × (38 − opponent's weighted mobility) + 17 × (38 − potential mobility) − 2.69 × the child's evaluation in 1/100 disc. It was tried with the killers before the evaluated moves, or without them.
- **ETC from look L:** a child whose stored upper bound already reaches β ends the node.
- **IID:** a node without a hash move first searches itself to half the depth.
- **Shallow-search keys:** the evaluation in the key replaced by a search of each child to a quarter of the depth.

`mid-bench` got `--side <keys>` (any Stello side's switches) to count nodes at a fixed depth. At depth 12, 100 positions, 1 pass:

| Variant | Nodes | Time |
|---|---:|---:|
| Current (Multi-ProbCut, old ordering) | 11,015,727 | 3.26 s |
| Evaluation ordering from look 2 / 4 / 8 | 11.87 M / 10.93 M / 11.04 M | 3.58 / 3.28 / 3.32 s |
| … from look 2 / 4, without killers | 12.10 M / 10.68 M | 3.73 / 3.24 s |
| ETC from look 4 | 10.92 M | 3.31 s |
| IID from look 3 / 5 / 7 | 10.88 M / 10.97 M / 11.01 M | about 3.27 s |
| Evaluation ordering from look 4 without killers, children searched to look / 4 from look 4 / 6 / 8 | 28.6 M / 19.0 M / 15.6 M | 8.17 / 5.47 / 4.55 s |
| **Evaluation ordering from look 4 without killers, plus ETC from look 4** | **10,582,393** (−3.9 %) | 3.24 s |

Matches, 1,180 games each:

| Variant | 100 ms | 500 ms |
|---|---:|---:|
| Evaluation ordering from look 2 / 4 / 6 | 49.0 % (−7) / 49.6 % (−3) / 49.4 % (−4) | |
| … from look 4, without killers | 51.7 % (49.5–53.8), +11 | |
| … from look 3, without killers | 51.4 % (49.3–53.5), +10 | |
| ETC from look 4 / 8 | 49.3 % (−5) / 50.1 % (+1) | |
| **Evaluation ordering from look 4 without killers, plus ETC from look 4** | **53.3 % (51.1–55.5), +23** | **51.9 % (49.9–53.9), +13**, depth 16.49 → 16.78 |

- **Kept: evaluation ordering from look 4 without killers, plus ETC from look 4.** It is now the default, with no switch.
  - It passed at 100 ms. At 500 ms the lower bound is 49.9 %, just short of the gate.
  - It is kept under the speed rule: 3.9 % fewer nodes at a fixed depth, 0.3 plies more depth at 500 ms, and no loss in either match.
- **The old ordering was already good.** The hash move is almost always there:
  - iterative deepening and Multi-ProbCut's shallow searches store a move in nearly every deep node, so IID finds nothing to add;
  - the evaluation only orders the moves after the hash move.
- **The killers hurt the evaluation ordering.** Placed before the evaluated moves, they push better-evaluated moves back.
- **A real shallow search per child costs far more than it saves** at the depths Stello reaches (at most about 17 plies). Egaroucid only does it from depth 8 on PV nodes and 16 on null-window nodes (depth >> 3 and >> 4), and its searches are deeper.
- **Removed:** IID, shallow-search keys and the switches (`order=`, `killers=`, `etc=`, `iid=`, `order-shallow=`).

### Item 5: a selective endgame and when to start solving

**The selective pass.** It ran between the midgame and the exact solve: the solver with Multi-ProbCut at every node from 14 empty squares.
- The shallow test was a midgame search (Multi-ProbCut off) to Egaroucid's endgame depth, about a third of the empty squares with the same parity.
- The pre-check used the static evaluation within 4 discs, as in the midgame.
- Its results were stored in the endgame table with a negative depth. The exact solve only used their moves.
- Its root score became the move's heuristic score, and the win/loss/draw and exact solves followed as before.
- The switch `end-dist=` let the midgame hand over earlier.

**The error model** comes from the trainer's new `mpc-end` command:
- It solved 1,950 test positions (150 per disc count from 38 to 50, that is 14–26 empty squares) exactly, with a limit of 60 s each. 111 positions were left out because they took longer, mostly at 24–26 empty squares, so the sample leans slightly towards easier positions.
- It searched each position without selectivity to every depth up to min(10, empties − 8). That took 15 minutes on 24 threads.
- **σ of exact − shallow in discs:** 6–8 for the static evaluation; at Egaroucid's shallow depth 3.7–5.6 (4.9 at 14 empties, 4.2 at 20, 4.4 at 24). That is larger than in the midgame, because the deep side is now the exact result.
- **Fit:** σ = 17.0938 − 25.4434·x + 14.9928·x² − 0.2715·s. It is off by 8.5 % on average, over 120 cells. The mean of exact − shallow is −0.49 discs.
- **A bug in both data commands:** positions with a single legal move are played without a search, so their search values stayed 0.
  - In the first `mpc-end` fit this gave σ of 8–11 discs at 16–19 and 23 empty squares, flat over the depths.
  - Both commands now skip such positions, and the fits drop such rows from older files.
  - Refitted without them, the midgame model of item 3 becomes σ = 1.4547 + 4.0093·x − 1.6181·x² − 0.4457·s + 0.2048·d, off by 5.0 % on average. That is within 10 % of the fit in use at typical points (3.08 against 3.06 discs at 32 discs, 5→11). The matches found t = 0.8–1.3 equally good, so the tuned constants stay.

**Matches at 100 ms, 1,180 games each, against Stello 2 after item 4:**

| Variant | Score | Elo | First solve (empty squares, default → variant) |
|---|---:|---:|---:|
| Selective pass, t = 1.0, at the usual hand-over (`end-mpc=100`) | 49.7 % (47.7–51.8) | −2 | 20.1 → 19.3 |
| … handing over 2 squares earlier (`end-dist=9`) | 49.9 % (47.9–52.0) | −1 | 20.1 → 19.4 |
| … 4 squares earlier (`end-dist=11`) | 47.9 % (45.6–50.2) | −14 | 20.2 → 19.5 |
| … t = 0.7, `end-dist=9` | 49.7 % (47.7–51.7) | −2 | 20.1 → 19.7 |
| … t = 1.5, `end-dist=9` | 48.9 % (46.8–51.0) | −7 | 20.2 → 19.1 |
| No selective pass, exact solve when the midgame would reach 5 empty squares (`end-dist=5`) | 50.0 % (47.9–52.1) | 0 | 20.2 → 19.8 |
| … 6 (`end-dist=6`) | 51.1 % (48.9–53.2) | +7 | 20.1 → 20.0 |
| … 8 (`end-dist=8`) | 49.0 % (46.9–51.1) | −7 | 20.2 → 20.3 |
| … 9 (`end-dist=9`) | 48.6 % (46.6–50.6) | −10 | 20.1 → 20.4 |

- **Rejected; the engine code was removed.** No variant is better.
  - The selective pass costs time and pushes the exact solve about one square later.
  - Its result is no better than the midgame's. When the solve starts, the last midgame iteration has already searched (selectively) to 8 empty squares from the end.
- **When to start solving is flat** around today's rule (the midgame stops when its next iteration would leave 7 or fewer empty squares). The rule stays.
- **The clog search was not tried.** It finds forced early game ends, mostly wipeouts. Egaroucid needs it because its selective endgame can miss them. With no selective endgame there is nothing for it to fix, and wipeouts are rare in these games.
- **Kept:** the trainer's `mpc-end` command, with the single-move fix in both commands, for a later try, for example with Egaroucid's staged levels (74 % … 100 %) or at longer time controls.

### The book recalculated again for the final 2.0 (2026-11-02)

The user recalculated the book on the other PC with the phase 5 search: engine version 4, 60 s per position, 8 workers, about 14 hours (merged as abb5160).

- **The searches went deeper:** 22–29 plies, mostly 24–26, against 17–19 with the 2.0 search.
- **Every leaf without a proof was searched,** including the 2,718 old `WinLossDraw` leaves that the 2.0 recalculation had kept (the bug fixed in d21e42f).
- **What changed:** 15,776 values. 606 heuristic leaves became exact and 595 became proven wins or losses, and the first move changed in 1,694 positions.
- **The values look right.** The Egaroucid port searched sampled leaves at level 21; the table gives each book's average distance from its scores.

  | Leaves | first 2.0 book | final 2.0 book |
  |---|---:|---:|
  | 200 random leaves, heuristic in both books | 2.19 discs (correlation 0.93) | 2.02 discs (0.94) |
  | 100 random former stale `WinLossDraw` leaves | 3.80 discs (0.80) | 2.02 discs (0.93) |

  The average heuristic leaf grew from 3.2 to 4.9 discs. This comes from the stale leaves: their old values averaged 0.77 discs, against 4.39 from Egaroucid. It is not a bias of the deeper search: on the leaves that were heuristic in both books, the averages are 5.08 and 5.10 discs.
  - Both books rate the move that leads to the leaf about 0.7–0.8 discs higher than Egaroucid does. Book moves are the ones Stello liked best, so some optimism is expected.
- **The opening:** the replies to d3 are c5 −0.87, c3 −2.62 and e3 −7.75. Egaroucid's level 21 gives 0, 0 and −6.
  - The main line now leaves the tiger at move 3: c5 rather than c3 after f5 d6, rated +0.87 against +0.75.
  - It follows "Rotating Flat (Kling Continuation)" for 18 moves, out of 19 in all.
- **In play** (books on, 500 ms per move, 1,180 games from `starts.txt`), the final book against the first 2.0 release's book scored 51.7 % (49.7–53.6), +11 Elo. That is not significant, but not worse ([result](benchmark-results/match_v2x-book2x-vs-book20-500ms.md)). As in phase 4, the start positions at ply 10–12 leave the book little room. Item 6, checking the moves outside the book when leaving it, comes next.

### Side experiment: a larger evaluation for the desktop (2026-11-01)

The question: does an evaluation well over 1 MB, for the desktop app only, play better?

- **Trainer change first (8cf2341).** Rows are now stored as 16-bit numbers relative to each feature's first index, in blocks of 2^22 rows. Before, they were 32-bit indices in one array. This halves the memory, so 3 M positions per disc count fit on the 15 GB laptop. The change reproduces `Book/stello-eval.bin` byte for byte.
- **Settings:** all runs use 1/16 discs and zero weights seen fewer than 10 times. The errors are on the usual test split (1.04 M positions).

| Model | Positions per disc count | File (gzip) | Test error (MAE / RMSE) | Peak memory | Training time |
|---|---:|---:|---:|---:|---:|
| `plus15` × 10 stages (the shipped model) | 1 M | 0.99 MB | 5.47 / 7.31 | 5.8 GB | 8 min |
| `plus15` × 30 stages | 3 M | 3.6 MB | 5.35 / 7.16 | 11.1 GB | 36 min |
| `plus15` × 60 stages | 1 M | 6.0 MB | 5.35 / 7.17 | 3.1 GB | 21 min |
| `plus15` × 60 stages | 3 M | 6.5 MB | 5.31 / 7.12 | 8.6 GB | 56 min |
| `egaroucid16` × 30 stages | 3 M | 4.6 MB | 5.27 / 7.05 | 10.4 GB | 45 min |
| `egaroucid16` × 60 stages | 1 M | 7.6 MB | 5.28 / 7.07 | 3.6 GB | 25 min |
| **`egaroucid16` × 60 stages** | **3 M** | **8.3 MB** | **5.23 / 7.01** | 10.0 GB | 70 min |
| `plus15` / `egaroucid16` × 10 stages | 3 M | — | out of memory | > 13 GB | — |

Egaroucid's evaluation has a test error of 5.05 for comparison. The 10-stage runs failed because each stage spans 6 disc counts, so it holds 18 M rows plus its neighbours'. The reports are `benchmark-results/grid-*.md`.

- **The gains on paper are small.** The best model is 0.24 discs better than the shipped one, at 8 times the size.
  - Roughly 2/3 of the gain comes from more stages and the larger pattern set, and 1/3 from three times the data.
  - Three times the data gives only −0.04 to −0.05, so 10 M per disc count on the 32 GB PC would gain perhaps another 0.05.
- **In play it is no better.** The best model (`egaroucid16` × 60 stages, 3 M) played the shipped evaluation at 500 ms per move, with no books and 1,180 games. Both used the shipped model's Multi-ProbCut fit.
  - It scored 51.2 % (49.0–53.4), +9 Elo (−7 to +24). The difference is not significant.
  - `egaroucid16` has more patterns, so the search ran at 3.66 M nodes per second against 4.41 M (−17 %). The midgame depth fell from 16.75 to 16.39.
  - The more accurate evaluation roughly pays for its slower speed, and no more ([result](benchmark-results/match_v2x-eval-e16-s60-3m-500ms.md)).
- **Not pursued.** The final 10 M model, its own Multi-ProbCut fit (about 2 h) and a 2 s match could add a few Elo at most, for an 8 MB file. Size alone does not close the gap to Egaroucid's 5.05: with Egaroucid's own 16 pattern shapes and 60 stages, Stello's trainer reaches 5.23. The rest must come from Egaroucid's other inputs or from its training.

### Item 6: checking the moves outside the book (2026-11-02)

A book move costs no time. So in a book position, `ComputerPlayer` used that move's normal time to search the moves the book does not have, the same search as for any other move, restricted to those moves. It played the best of them if its score beat the book move's value by a margin, 1 disc in these matches.
- **Two modes:**
  - `exit`: only when the book move leads out of the book;
  - `all`: in every book position.
- **Matches:** against the default, both with the final book, 100 ms per move, 1,180 games from `starts.txt`.

| Variant | Score (95 %) | Elo |
|---|---|---:|
| `exit`, margin 1 disc | 49.6 % (47.5–51.6) | −3 |
| `all`, margin 1 disc | 50.4 % (48.2–52.6) | +3 |

- **Why nothing shows:** the games start at ply 10–12, so each side plays only about 1.2 book moves per game. A count over 100 start positions (200 games):
  - `exit` searched 101 times, found a better move than the book's 26 times, and switched (more than 1 disc better) 14 times;
  - `all` searched 245 times and switched 24 times.

  A change in 7–12 % of the games, each worth a fraction of a disc, is far below what 1,180 games can measure.
- **Rejected.** Neither mode passes the 100 ms gate, so the code was removed; it is in history as 124cf2d ([exit](benchmark-results/match_v2x-p5-item6-exit-m100-100ms.md), [all](benchmark-results/match_v2x-p5-item6-all-m100-100ms.md)).
  - In the apps, games start from the first move and stay in the book much longer, so the check would run more often. But a 0.1–1 s search that overrules a 60 s book value is more likely noise than a find.
  - A better use of the idea is in the book itself: the book tool could search the missing moves of each book position for 60 s and add the good ones, as C++'s calclib did ("the best move not yet in the book", the `onlyMoves` search that `BookSearch` already uses).

## Measurements

All on 1 thread, on the same Intel i7-12850HX laptop VM as the port's measurements. That CPU is hybrid: 8 performance cores with 2 threads each and 8 efficiency cores, 24 threads in all. Matches play 12 games at a time, so their depths and times per move are averages over both kinds of core. A/B results are fair, because both engines in a game share a thread, but absolute numbers differ on other machines.

| What | Stello 1 | Stello 2 | Egaroucid | Note |
|---|---:|---:|---:|---|
| `mid-bench`, depth 8, 100 positions | 4,650,177 nodes, 1.9 s | the same | — | phase 0: identical |
| FFO #40–44, exact | 329,535,633 nodes, 21.1 s | the same | 4.3 s | phase 0: identical |
| Evaluation error on the test split (1.04 M positions), all | 12.14 discs (scale fitted) | 5.55 discs (`classic11`, 15 stages) | 5.05 discs | phase 1 ([report](benchmark-results/v2-p1-eval-c11-s15.md)) |
| Evaluations per second from scratch, 1 thread | 2.14 M | 4.80 M | 0.33 M | phase 1; Stello 1 and Egaroucid from `eval-compare` |
| Match against Stello 1, 100 ms per move, no books | — | 90.7 % (89.2–92.2), +395 Elo | 87.1 % (phase 5 of the port) | phase 2, `classic11` × 10 stages ([result](benchmark-results/match_v2-p2-c11-s10-vs-v1-100ms.md)) |
| FFO #40–49, exact | 369 s | 47.1 s, 1.10 G nodes (new solver) | 25.6 s | phase 3 ([result](benchmark-results/v2-p3-solve-bench-40-49.json)) |
| New against Stello 1's solver, both with the pattern evaluation, 100 / 500 ms | — | 49.7 % (−2) / 51.1 % (+8) | — | phase 3 ([100 ms](benchmark-results/match_v2-p3-endgame-new-vs-classic-100ms.md), [500 ms](benchmark-results/match_v2-p3-endgame-new-vs-classic-500ms.md)) |
| Match Stello 2 (= Stello 1 so far) against Stello 1, 100 ms per move, 1,180 games | — | 51.8 % (49.5–54.1), +13 Elo | — | phase 0: no significant difference; depth 10.36 / 10.35, 2.5M nodes/s both ([result](benchmark-results/match_v2-p0-v1-vs-current-100ms.md)) |
| Evaluation error on the test split, chosen model | — | 5.47 discs (RMSE 7.31), `plus15` × 10 stages, 0.99 MB | 5.05 discs | phase 2 ([report](benchmark-results/v2-p2-eval-p15-s10-m30-u8.md)); 3.3 M evaluations per second |
| Match against Stello 1, 100 / 500 ms per move, no books | — | 90.9 % (+400) / 90.6 % (+393) | — | phase 2, `stello-eval.bin` ([100 ms](benchmark-results/match_v2-p2-eval-vs-v1-100ms.md), [500 ms](benchmark-results/match_v2-p2-eval-vs-v1-500ms.md)) |
| Stello 2 against Egaroucid, 100 / 500 ms per move | — | 59.5 % (+67) / 38.2 % (−84) | — | phase 2 ([100 ms](benchmark-results/match_v2-p2-eval-vs-egaroucid-100ms.md), [500 ms](benchmark-results/match_v2-p2-eval-vs-egaroucid-500ms.md)) |
| `mid-quality`, loss in discs at 50 / 200 / 1000 ms | 1.59 / 1.59 / 1.39 | 1.33 / 0.91 / 0.68 | 1.18 / 0.94 / 0.54 | phase 2 ([result](benchmark-results/v2-p2-mid-quality.md)) |
| Stello 2.0 after phase 5 items 1–4 against Egaroucid, 100 / 500 / 2000 ms per move, no books | — | 76.4 % (+204) / 57.7 % (55.4–59.9, +54) / 40.8 % (38.7–43.0, −64) | — | phase 5; depth 13.9 / 16.8 / 19.2 against Egaroucid's 8.7 / 13.4 / 21.5 nominal; first solve at 2 s: 24.0 empties against 22.9. Egaroucid's longest move at 100 ms took 590 ms. The engines are even somewhere between 500 ms and 2 s; Egaroucid's better evaluation pays off with depth. At 2 s Stello's 2^19-entry table (Egaroucid: 2^21) may also be too small ([100 ms](benchmark-results/match_v2x-p5-item4-vs-egaroucid-100ms.md), [500 ms](benchmark-results/match_v2x-p5-item4-vs-egaroucid-500ms.md), [2000 ms](benchmark-results/match_v2x-p5-item4-vs-egaroucid-2000ms.md)) |
| Stello 2.0 (phase 5) against itself with twice the time, no books | — | 200 against 100 ms: 61.8 % (59.5–64.1), +84; 1 s against 500 ms: 55.4 % (53.3–57.6), +38 | — | what a 2× faster engine is worth; depth +1.4 / +1.3 plies ([100/200](benchmark-results/match_v2x-timeodds-100-vs-200ms.md), [500/1000](benchmark-results/match_v2x-timeodds-500-vs-1000ms.md)). The C++/C# comparison is in [cpp-vs-csharp-speed.md](cpp-vs-csharp-speed.md) |

## Ideas for later

- **A small move-ordering table for 11–13 empty squares** (phase 3), like Egaroucid's 4-pattern table updated move by move. The full pattern evaluation saves 20 % of the nodes there but is too slow.
- **When to start solving** (phase 5, items 1 and 5): start from an estimate of the solve time instead of the fixed "midgame depth reaches empty squares − 8". Then solve selectively, as Egaroucid does.

## Closed

This work ended with the final 2.0 on 2 November 2026 (final measurements in [docs chapter 24](../docs/24-results-and-what-comes-next.md)). Item 7, parallel search, and the move to Rust continue in [making-stello-3-plan.md](making-stello-3-plan.md) and [making-stello-3-findings.md](making-stello-3-findings.md).
