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
| 2 Evaluation 2.0 | | |
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
- **The file** (`EvalWeights`): gzip, the folded weights as int16 in 1/32 discs. 15 stages: 1.5 MB.

## Measurements

All on 1 thread, on the same Intel i7-12850HX laptop VM as the port's measurements.

| What | Stello 1 | Stello 2 | Egaroucid | Note |
|---|---:|---:|---:|---|
| `mid-bench`, depth 8, 100 positions | 4,650,177 nodes, 1.9 s | the same | — | phase 0: identical |
| FFO #40–44, exact | 329,535,633 nodes, 21.1 s | the same | 4.3 s | phase 0: identical |
| Evaluation error on the test split (1.04 M positions), all | 12.14 discs (scale fitted) | 5.55 discs (`classic11`, 15 stages) | 5.05 discs | phase 1 ([report](benchmark-results/v2-p1-eval-c11-s15.md)) |
| Evaluations per second from scratch, 1 thread | 2.14 M | 4.80 M | 0.33 M | phase 1; Stello 1 and Egaroucid from `eval-compare` |
| Match Stello 2 (= Stello 1 so far) against Stello 1, 100 ms per move, 1,180 games | — | 51.8 % (49.5–54.1), +13 Elo | — | phase 0: no significant difference; depth 10.36 / 10.35, 2.5M nodes/s both ([result](benchmark-results/match_v2-p0-v1-vs-current-100ms.md)) |

## Ideas for later

- (filled in as the work goes on)
