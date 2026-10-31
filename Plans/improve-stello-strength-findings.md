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
| 3 Endgame solver | | |
| 4 Release 2.0 in the apps | | |
| 5 Search improvements (2.x) | | |

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

## Measurements

All on 1 thread, on the same Intel i7-12850HX laptop VM as the port's measurements.

| What | Stello 1 | Stello 2 | Egaroucid | Note |
|---|---:|---:|---:|---|
| `mid-bench`, depth 8, 100 positions | 4,650,177 nodes, 1.9 s | the same | — | phase 0: identical |
| FFO #40–44, exact | 329,535,633 nodes, 21.1 s | the same | 4.3 s | phase 0: identical |
| Evaluation error on the test split (1.04 M positions), all | 12.14 discs (scale fitted) | 5.55 discs (`classic11`, 15 stages) | 5.05 discs | phase 1 ([report](benchmark-results/v2-p1-eval-c11-s15.md)) |
| Evaluations per second from scratch, 1 thread | 2.14 M | 4.80 M | 0.33 M | phase 1; Stello 1 and Egaroucid from `eval-compare` |
| Match against Stello 1, 100 ms per move, no books | — | 90.7 % (89.2–92.2), +395 Elo | 87.1 % (phase 5 of the port) | phase 2, `classic11` × 10 stages ([result](benchmark-results/match_v2-p2-c11-s10-vs-v1-100ms.md)) |
| Match Stello 2 (= Stello 1 so far) against Stello 1, 100 ms per move, 1,180 games | — | 51.8 % (49.5–54.1), +13 Elo | — | phase 0: no significant difference; depth 10.36 / 10.35, 2.5M nodes/s both ([result](benchmark-results/match_v2-p0-v1-vs-current-100ms.md)) |
| Evaluation error on the test split, chosen model | — | 5.47 discs (RMSE 7.31), `plus15` × 10 stages, 0.99 MB | 5.05 discs | phase 2 ([report](benchmark-results/v2-p2-eval-p15-s10-m30-u8.md)); 3.3 M evaluations per second |
| Match against Stello 1, 100 / 500 ms per move, no books | — | 90.9 % (+400) / 90.6 % (+393) | — | phase 2, `stello-eval.bin` ([100 ms](benchmark-results/match_v2-p2-eval-vs-v1-100ms.md), [500 ms](benchmark-results/match_v2-p2-eval-vs-v1-500ms.md)) |
| Stello 2 against Egaroucid, 100 / 500 ms per move | — | 59.5 % (+67) / 38.2 % (−84) | — | phase 2 ([100 ms](benchmark-results/match_v2-p2-eval-vs-egaroucid-100ms.md), [500 ms](benchmark-results/match_v2-p2-eval-vs-egaroucid-500ms.md)) |
| `mid-quality`, loss in discs at 50 / 200 / 1000 ms | 1.59 / 1.59 / 1.39 | 1.33 / 0.91 / 0.68 | 1.18 / 0.94 / 0.54 | phase 2 ([result](benchmark-results/v2-p2-mid-quality.md)) |

## Ideas for later

- (filled in as the work goes on)
