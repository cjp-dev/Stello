# 23 – Phase 5: the search

[Back to the index](README.md)

## In short

Phase 5 is the last phase of the work on Stello 2.0. Phases 0–4 gave Stello a pattern evaluation, Egaroucid's endgame solver and a recalculated book, and the first 2.0 release (2026-11-01) put them in the apps. The search around them was still Stello 1's. Phase 5 improved the search, one change at a time, and the result is the final 2.0 (2026-11-02).

Six items were tried. Two made the engine clearly stronger, one made it faster, and the rest were measured and dropped:

- **Kept:**
  - **PVS in every node** (item 2a): +28 Elo at 100 ms per move, +30 at 500 ms.
  - **Multi-ProbCut** with an error model fitted on Stello's own searches (item 3): about +120 Elo at both time controls, and 3 plies more depth at 500 ms. It is by far the largest gain of the phase.
  - **Evaluation ordering and midgame ETC from `look` 4** (item 4): 3.9 % fewer nodes and +23 Elo at 100 ms, kept as a speed change.
- **Rejected:** using more of the time (item 1), a table with both bounds and two moves (item 2b), a selective endgame and another solve start (item 5), and checking the moves outside the book (item 6).
- **The book was recalculated again** with the phase 5 search, 22–29 plies deep. It scored 51.7 % against the first 2.0 release's book: not significant, but not worse.

After item 4, Stello 2.0 beat the Egaroucid port at 100 ms (76.4 %, +204 Elo) and at 500 ms (57.7 %, +54), and lost at 2 s (40.8 %, −64). Twice the time is worth +84 Elo at 100 ms and +38 at 500 ms, which is the background for the question of chapter 24: more threads, or a faster language.

How the search works now is explained in Part 1: [05 Move ordering](05-move-ordering.md), [06 Midgame search](06-midgame-search.md), [07 Transposition table](07-transposition-table.md) and [09 Time control](09-time-control.md). This chapter tells how it got there. The full record is in the findings, [improve-stello-strength-findings.md](../../Plans/improve-stello-strength-findings.md), section "Search improvements (phase 5)", and the plan is [improve-stello-strength-plan.md](../../Plans/improve-stello-strength-plan.md).

## How phase 5 worked

- **A branch of its own.** Phase 5 ran on `feature/stello-v2.x`, branched from `feature/stello-v2`. So the first 2.0 release kept exactly the engine that had recalculated its book ([chapter 22](22-phase-4-the-release.md)), while the search changed beside it. Items 1–4 were done on 2026-10-31, before that release; items 5 and 6 and the second book followed on 2026-11-01 and 2026-11-02.
- **One switch per change.** Each change was written behind a switch in the internal record [SearchOptions](../../Stello.Net/Stello.Engine/Search/SearchOptions.cs), with a match key for the benchmark tool (for example `soft-pct=100` or `mpc=130`). One side of a match got the switch, the other played the current engine.
- **The gate** ([chapter 17](17-measuring-the-engine.md) explains the tool):
  - a 1,180-game match against the current Stello 2.0, 100 ms per move, no books, with the lower end of the 95 % interval above 50 %;
  - confirmed by a second match at 500 ms;
  - a change that only makes the search faster may pass on `mid-bench` (nodes at a fixed depth) instead, if no match shows a loss.
- **After the decision** a kept change became the default and its switch was removed. A rejected change was removed too; its code stays in git history. Only two switches are left, `SoftTimePercent` and `ProbCutPercent`, because their best value may change when the search changes.
- **Each match takes about 5 minutes at 100 ms and 22 minutes at 500 ms** on the laptop, playing 12 games at a time on 1 thread each. The 95 % interval of 1,180 games is about ±2 % of score, or ±15 Elo, so a change worth less than about 10 Elo cannot be seen.

The plan listed the items in the order of the gain the comparison with Egaroucid suggested ([chapter 19](19-stello-1-against-egaroucid.md#what-explains-the-difference)). Item 7, Lazy SMP or YBWC, was optional and is left for later ([chapter 24](24-results-and-what-comes-next.md)).

## Item 1: time use

**The question.** With a time per move, Stello starts no new midgame iteration after 2/3 of the time (the soft limit) and stops at the full time (the hard limit, [chapter 09](09-time-control.md#how-the-search-uses-the-limits)). At 500 ms Stello 1 used only 313 ms on average ([chapter 19](19-stello-1-against-egaroucid.md#matches)). Would it gain by using more?

An iteration cut off by the hard limit is not wasted: each root move that beat the best move so far, at the new depth, is kept. So there were two ways to try:

- **A higher soft limit** (`SearchOptions.SoftTimePercent`, match key `soft-pct=`): start a new iteration up to 80, 85 or 100 % of the time.
- **Predicting the next iteration** (`predict=`): start a new iteration only if its predicted time fits within the time per move (or within 120 % of it).

| Variant (match key) | Time per move | Score | Elo | Result |
|---|---:|---:|---:|---|
| Soft limit 80 % (`soft-pct=80`), 100 ms | 65 ms (62) | 51.6 % (49.4–53.8) | +11 | [result](../../Plans/benchmark-results/match_v2x-p5-time-soft-pct_80-100ms.md) |
| Soft limit 100 % (`soft-pct=100`), 100 ms | 66 ms (62) | 51.9 % (49.8–54.1) | +14 | [result](../../Plans/benchmark-results/match_v2x-p5-time-soft-pct_100-100ms.md) |
| … the same again | 67 ms (62) | 49.2 % (47.0–51.5) | −5 | [result](../../Plans/benchmark-results/match_v2x-p5-time-soft-pct_100-100ms-run2.md) |
| … both runs, 2,360 games | | 50.6 % (about 49.5–51.6) | +4 | |
| Soft limit 100 %, 500 ms | 308 ms (289) | 51.9 % (49.9–54.0) | +14 | [result](../../Plans/benchmark-results/match_v2x-p5-time-soft-pct_100-500ms.md) |
| Soft limit 85 %, 500 ms | 304 ms (289) | 51.5 % (49.4–53.6) | +11 | [result](../../Plans/benchmark-results/match_v2x-p5-time-soft-pct_85-500ms.md) |
| Predicted next iteration within the time (`predict=on`), 100 ms | 42 ms (62) | 44.7 % (42.3–47.2) | −37 | [result](../../Plans/benchmark-results/match_v2x-p5-time-predict_on-100ms.md) |
| Predicted next iteration within 120 % of the time, 100 ms | 46 ms (62) | 46.4 % (44.0–48.7) | −25 | [result](../../Plans/benchmark-results/match_v2x-p5-time-predict_120-100ms.md) |

The time per move is the average over the searched moves; the time in brackets is the default side's.

- **Rejected: no variant passes the gate.** Using more of the time is worth a few Elo at most. The time per move rises only 5–7 %, because the endgame moves end early anyway, and one more iteration is about one ply. The two runs of `soft-pct=100` at 100 ms (+14 and −5) show how far one match can be from the truth.
- **Predicting the next iteration is clearly worse.** It saves the time of iterations that would not finish, but it also throws away what an unfinished iteration finds. The prediction code was removed.
- **The 2/3 rule stays.** `SoftTimePercent` (`soft-pct=`) stays as a switch, so the question can be asked again once the search is deeper or more selective. The apps never set it.

Commit: 299bcb7.

## Item 2a: PVS in every node

**Before.** Stello 1, and the first 2.0 release, used principal variation search only at the root: the first root move with the full window, the others with a null window and a re-search if they turned out better. Inside the tree the search was plain alpha-beta, as in the C++ original.

**The change.** Every midgame node now does the same: the first move with the full window, the others with a null window first ([chapter 06](06-midgame-search.md#one-node)).

| Against the old search | Score | Elo | Midgame depth (old → new) | Result |
|---|---:|---:|---:|---|
| 100 ms | 54.0 % (51.8–56.3) | +28 | 11.53 → 11.68 | [result](../../Plans/benchmark-results/match_v2x-p5-pvs-100ms.md) |
| 500 ms | 54.4 % (52.1–56.6) | +30 | 13.34 → 13.67 | [result](../../Plans/benchmark-results/match_v2x-p5-pvs-500ms.md) |

- **Kept**, and the default from commit 53644bd. Both matches pass the gate.
- **A surprise at a fixed depth.** At depth 10, `mid-bench` needed a little more with PVS (17.5 M nodes against 16.3 M). Yet in the same time the search got deeper. The null windows probably let the old selective search cut more often.

## Item 2b: a table with both bounds and two moves

**The idea.** Stello's midgame table stores one bound and one move per entry, with two entries per slot: the deepest and the newest ([chapter 07](07-transposition-table.md)). Egaroucid's table stores more. Phase 5 tried an entry with:

- a lower and an upper bound;
- the best move and the one before it, both tried first by the move ordering;
- the search that stored it, so that entries of earlier searches could be replaced first.

The switch was `SearchOptions.BoundTable` (match key `bounds=on`), added with item 2a in 53644bd. All matches are against Stello 2.0 with PVS, at 100 ms:

| Variant | Score | Elo | Midgame depth (variant, default) | Result |
|---|---:|---:|---:|---|
| Both bounds, two moves, entries of earlier searches replaced first | 47.8 % (45.5–50.0) | −16 | 11.54 (11.62) | [result](../../Plans/benchmark-results/match_v2x-p5-bounds-100ms.md) |
| … without the replacement by search | 48.5 % (46.1–50.8) | −11 | 11.49 (11.64) | [result](../../Plans/benchmark-results/match_v2x-p5-bounds-nogen-100ms.md) |
| … and without the second move | 44.7 % (42.5–47.0) | −37 | 11.29 (11.64) | [result](../../Plans/benchmark-results/match_v2x-p5-bounds-only-100ms.md) |
| A shallower result no longer changes the move of a deeper one, with replacement by search | 48.3 % (45.9–50.6) | −12 | 11.52 (11.64) | [result](../../Plans/benchmark-results/match_v2x-p5-bounds-v2-gen1-100ms.md) |
| … without the replacement by search | 50.6 % (48.5–52.8) | +4 | 11.68 (11.61) | [result](../../Plans/benchmark-results/match_v2x-p5-bounds-v2-gen0-100ms.md) |

- **Rejected; the code was removed** (65ccc87). The best version is only even with the old table, so there was no reason to keep the larger entry.
- **What was learned:**
  - **Shallow results must not change a deep entry's move.** The selective search's shallow test searches (at depth 3 then) store results for the same positions just before the deep searches. A table that lets them overwrite a deeper entry's move loses depth.
  - **The previous move's entries are worth keeping.** The tree two plies later is mostly the same. Replacing them first cost about 15 Elo.
  - **The second move helps** (−37 → −11). It might be worth adding to the old table on its own; that was not tried.

## Item 3: Multi-ProbCut

### The selective search it replaced

Stello 1 and the first 2.0 release used the selective search of the C++ original (`SELEXT`). At nodes with `look` 7 and 5 (8 and 6 plies to go), a 4-ply null-window search (`look` 3) tested whether the position was clearly outside the window (α, β):

$$\beta' = \beta + \left\lfloor \frac{|\beta|}{2} \right\rfloor + 50, \qquad \alpha' = \alpha - \left\lfloor \frac{|\alpha|}{2} \right\rfloor - 50$$

If the test search scored at least β′, the node returned β; if at most α′, it returned α. It was skipped near won and lost scores. The full description is in the [Stello 1 documentation](stello-1/06-midgame-search.md#selective-search).

This is the idea of ProbCut (Buro) without its statistics:

- **The margin was fixed:** half the bound plus 50 units. It did not depend on how good the evaluation was, on the stage of the game, or on how far the shallow search was from the deep one.
- **It ran at two depths only,** with one test depth, whatever the depth of the iteration.
- **Phase 2 kept it.** With the pattern evaluation in 1/100 disc, the margin's 50 is half a disc. Phase 2 tried other margins (`sel-margin=`, `sel-percent=`: 25 % + 50, 50 % + 100, 25 % + 25, a fixed 2 discs), and none was significantly better (−12 to +11 Elo), so the old search stayed until phase 5 could replace it ([chapter 20](20-phases-0-2-the-evaluation.md)).

### The new search

Multi-ProbCut follows Egaroucid's `multi_probcut.hpp` ([chapter 18](18-the-egaroucid-port.md#multi-probcut)), on Stello's search. Its rules are in [chapter 06](06-midgame-search.md#selective-search). In short:

- it runs at every node from depth 3, with a shallow depth of about half the depth with the same parity (3 → 1, 7 → 3, 13 → 7);
- a cut needs the shallow null-window search to be t × σ outside the window, where σ is the measured spread of deep minus shallow results;
- the static evaluation must first be within 4 discs of that margin, so most nodes pay only for one evaluation;
- it is off inside its own shallow searches and near won and lost scores.

### The error model: the trainer's `mpc` command

σ must come from Stello's own evaluation and search, not from Egaroucid's. The trainer got a new command, `mpc` ([MpcCommand.cs](../../Stello.Net/tools/Stello.Trainer/MpcCommand.cs)):

- **The data:** 11,700 positions from the test split of the training data, 300 per disc count from 14 to 52.
- **The searches:** each position without selectivity (`SearchOptions.Selective = false`) at every depth up to min(13, empties − 8). The engine's `IterationDone` callback records the score of every iteration, so one search gives all depths. This took 2 hours on 24 threads.
- **The output:** every score in `Traning data/mpc-values.csv` (git-ignored), then σ of deep − shallow per group of 4 disc counts, and a weighted least-squares fit of σ = c0 + c1·x + c2·x² + c3·s + c4·d, with x = discs / 64, s the shallow depth and d the deep depth.

σ of deep − shallow in discs, for the shallow depth Multi-ProbCut uses:

| Discs | 1→3 | 2→4 | 3→5 | 2→6 | 3→7 | 4→8 | 5→9 | 4→10 | 5→11 | 6→12 | 7→13 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 16–19 | 2.39 | 2.26 | 2.23 | 2.69 | 2.48 | 2.35 | 2.28 | 2.47 | 2.22 | 2.23 | 2.11 |
| 24–27 | 3.11 | 2.68 | 2.41 | 3.11 | 2.98 | 2.80 | 2.55 | 2.88 | 2.68 | 2.70 | 2.49 |
| 32–35 | 3.44 | 2.71 | 2.33 | 3.53 | 3.27 | 2.94 | 2.60 | 3.16 | 2.96 | 2.74 | 2.74 |
| 40–43 | 3.39 | 3.12 | 3.03 | 3.92 | 3.69 | 3.32 | 3.24 | 3.63 | 3.61 | 3.12 | 3.11 |
| 48–51 | 3.70 | 3.22 | 3.02 | 3.96 | 3.65 | 3.25 | | | | | |

- **The fit:** σ = 1.8871 + 2.9543·x + 0.0660·x² − 0.6749·s + 0.2778·d discs. It is off by 10.2 % on average and 38 % at worst, over the 117 cells.
- **The spread grows with the number of discs** and shrinks as the shallow search gets closer to the deep one. The mean of deep − shallow is −0.19 discs: no bias worth correcting.
- **The static evaluation is much worse** than any search as a predictor (0→2: 2.8–7.7 discs, growing towards the end). That is why Multi-ProbCut starts at depth 3, as in Egaroucid.

Two kinds of positions had to be filtered out:

- **Game-end scores.** In 25 positions the deep search found a forced game end, with a score of ±32,6xx. Those few values blew σ up to 7–20 discs at 36–47 discs, and the first fit was off by 47 %. They are now skipped, just as Multi-ProbCut itself is not used near such scores.
- **Single-move positions.** Found later, during item 5: a position with only one legal move is played without a search, so its search values stayed 0. Both data commands now skip such positions, and the fits drop such rows from older files. Refitted without them, the midgame model becomes σ = 1.4547 + 4.0093·x − 1.6181·x² − 0.4457·s + 0.2048·d, off by 5.0 % on average. At typical points it is within 10 % of the fit in use (3.08 against 3.06 discs at 32 discs, 5→11). The matches below found t = 0.8–1.3 equally good, so a change that small is lost in the choice of t, and the constants in use stay.

### Choosing t

t says how many standard deviations outside the window the shallow result must be. A small t cuts more and searches deeper, but makes more wrong cuts. The sweep, against Stello 2.0 with the old selective search, 1,180 games each (match key `mpc=`, t × 100):

| t | 100 ms: score | Elo | depth (old → new) | 500 ms: score | Elo | depth (old → new) |
|---:|---:|---:|---:|---:|---:|---:|
| 0.8 | [69.2 %](../../Plans/benchmark-results/match_v2x-p5-mpc-mpc_80-100ms.md) (67.0–71.5) | +141 | 11.54 → 14.16 | | | |
| **1.0** | [67.3 %](../../Plans/benchmark-results/match_v2x-p5-mpc-mpc_100-100ms.md) (65.0–69.6) | +126 | 11.59 → 13.81 | [66.6 %](../../Plans/benchmark-results/match_v2x-p5-mpc-mpc_100-500ms.md) (64.4–68.7) | **+120** | 13.61 → 16.55 |
| 1.15 | [68.9 %](../../Plans/benchmark-results/match_v2x-p5-mpc-mpc_115-100ms.md) (66.7–71.2) | +139 | 11.55 → 13.51 | | | |
| 1.3 | [70.0 %](../../Plans/benchmark-results/match_v2x-p5-mpc-mpc_130-100ms.md) (67.7–72.2) | +147 | 11.55 → 13.22 | [65.8 %](../../Plans/benchmark-results/match_v2x-p5-mpc-mpc_130-500ms.md) (63.6–68.1) | +114 | 13.62 → 15.92 |
| 1.6 | [64.9 %](../../Plans/benchmark-results/match_v2x-p5-mpc-mpc_160-100ms.md) (62.7–67.1) | +107 | 11.54 → 12.62 | | | |
| 2.0 | [60.2 %](../../Plans/benchmark-results/match_v2x-p5-mpc-mpc_200-100ms.md) (57.7–62.7) | +72 | 11.57 → 12.00 | | | |

- **Everything from t = 0.8 to 1.3 is within noise at 100 ms** (+126 to +147). From 1.6 the gain falls off: too few cuts.
- **At 500 ms the lower t wins.** t = 1.0 against t = 1.3 directly: 52.2 % (50.1–54.3), +15 Elo, depth 16.55 against 15.83 ([result](../../Plans/benchmark-results/match_v2x-p5-mpc-100-vs-130-500ms.md)). With more time the extra depth is worth more than the cuts it gets wrong.
- **t = 1.0 is the default** (`SearchEngine.DefaultProbCutPercent` = 100).

### What it gained

- **Kept: about +120 Elo at 500 ms**, the largest gain of phase 5 (commit 6c7c938).
- **Depth:** at 500 ms the midgame depth rose from 13.6 to 16.6 plies; at 100 ms from 11.6 to 13.8.
- **Speed:** at a fixed depth of 10, `mid-bench` needs 3.8 M nodes instead of 17.5 M, 4.6 times fewer.
- **The solver starts earlier:** the first solve came about 1 empty square earlier, 21.3 → 22.5 at 500 ms, because the midgame iterations reach the hand-over sooner.
- **Still correct:** FFO #40–44 all solve correctly, in 4.0 s. Multi-ProbCut stops where the solver takes over.
- **Removed:** the old selective search, its options `SelectiveMargin` and `SelectivePercent`, and the match keys `sel-margin=` and `sel-percent=`.
- **Kept as switches:** `SearchOptions.ProbCutPercent` (`mpc=`) to retune t when the search changes, and `SearchOptions.Selective = false` for the `mpc` command.

Ideas that were noted but not tried: a fit per depth pair instead of the linear model, and t by game phase. Multi-ProbCut in the endgame is item 5.

## Item 4: evaluation ordering and midgame ETC

**Before.** The midgame ordered its moves by the hash move, then the response killers, then the square values ([chapter 05](05-move-ordering.md)). The plan proposed ordering by a shallow search, as Egaroucid does, and ETC in the midgame.

Five additions were tried behind switches, against Stello 2.0 with Multi-ProbCut:

- **Evaluation ordering from `look` L:** Egaroucid's depth-0 key, from the opponent's weighted mobility, its potential mobility and the child's evaluation ([chapter 05](05-move-ordering.md#evaluation-ordering-in-deeper-nodes) has the formula). It was tried with the killers before the evaluated moves, and without them.
- **ETC from `look` L:** a child whose stored upper bound already reaches β ends the node ([chapter 07](07-transposition-table.md#use-in-the-midgame-search)).
- **IID** (internal iterative deepening): a node without a hash move first searches itself to half the depth, to find one.
- **Shallow-search keys:** the evaluation in the key replaced by a search of each child to a quarter of the depth.

`mid-bench` got `--side <keys>`, which takes any Stello side's switches, to count nodes at a fixed depth. At depth 12, 100 positions, 1 pass:

| Variant | Nodes | Time |
|---|---:|---:|
| Current (Multi-ProbCut, old ordering) | 11,015,727 | 3.26 s |
| Evaluation ordering from `look` 2 / 4 / 8 | 11.87 M / 10.93 M / 11.04 M | 3.58 / 3.28 / 3.32 s |
| … from `look` 2 / 4, without killers | 12.10 M / 10.68 M | 3.73 / 3.24 s |
| ETC from `look` 4 | 10.92 M | 3.31 s |
| IID from `look` 3 / 5 / 7 | 10.88 M / 10.97 M / 11.01 M | about 3.27 s |
| Evaluation ordering from `look` 4 without killers, children searched to `look` / 4 from `look` 4 / 6 / 8 | 28.6 M / 19.0 M / 15.6 M | 8.17 / 5.47 / 4.55 s |
| **Evaluation ordering from `look` 4 without killers, plus ETC from `look` 4** | **10,582,393** (−3.9 %) | 3.24 s |

Matches, 1,180 games each:

| Variant | 100 ms | 500 ms |
|---|---|---|
| Evaluation ordering from `look` 2 / 4 / 6 | [49.0 %](../../Plans/benchmark-results/match_v2x-p5-order-order_2-100ms.md) (−7) / [49.6 %](../../Plans/benchmark-results/match_v2x-p5-order-order_4-100ms.md) (−3) / [49.4 %](../../Plans/benchmark-results/match_v2x-p5-order-order_6-100ms.md) (−4) | |
| … from `look` 4, without killers | [51.7 %](../../Plans/benchmark-results/match_v2x-p5-order-order_4-killers_off-100ms.md) (49.5–53.8), +11 | |
| … from `look` 3, without killers | [51.4 %](../../Plans/benchmark-results/match_v2x-p5-order-order_3-killers_off-100ms.md) (49.3–53.5), +10 | |
| ETC from `look` 4 / 8 | [49.3 %](../../Plans/benchmark-results/match_v2x-p5-order-etc_4-100ms.md) (−5) / [50.1 %](../../Plans/benchmark-results/match_v2x-p5-order-etc_8-100ms.md) (+1) | |
| **Evaluation ordering from `look` 4 without killers, plus ETC from `look` 4** | [**53.3 %**](../../Plans/benchmark-results/match_v2x-p5-order-order_4-killers_off-etc_4-100ms.md) (51.1–55.5), **+23** | [**51.9 %**](../../Plans/benchmark-results/match_v2x-p5-order-order_4-killers_off-etc_4-500ms.md) (49.9–53.9), **+13**, depth 16.49 → 16.78 |

- **Kept: evaluation ordering from `look` 4 without killers, plus ETC from `look` 4** (commit 12e3833). It is now the default, with no switch (`EvaluationOrderMinLook` and `MidgameEtcMinLook` in [SearchEngine.cs](../../Stello.Net/Stello.Engine/SearchEngine.cs)).
  - It passed at 100 ms. At 500 ms the lower bound is 49.9 %, just short of the gate.
  - It is kept under the speed rule: 3.9 % fewer nodes at a fixed depth, 0.3 plies more depth at 500 ms, and no loss in either match.
- **Each part alone was even.** Only together did they pass. The gains are small because the old ordering was already good.
- **The hash move is almost always there.** Iterative deepening and Multi-ProbCut's shallow searches store a move in nearly every deep node. So IID finds nothing to add, and the evaluation only orders the moves after the hash move.
- **The killers hurt the evaluation ordering.** Placed before the evaluated moves, they push better-evaluated moves back. Without them the same ordering went from −3 to +11 Elo.
- **A real shallow search per child costs far more than it saves** at the depths Stello reaches (at most about 17 plies): about 1.4–2.6 times the nodes of the old ordering. Egaroucid does it only from depth 8 on PV nodes and 16 on null-window nodes, and its searches are deeper.
- **Removed:** IID, the shallow-search keys and all the switches of this item (`order=`, `killers=`, `etc=`, `iid=`, `order-shallow=`).

## Item 5: a selective endgame and the solve start

**The question.** Egaroucid solves selectively: Multi-ProbCut in the endgame at 74 %, then 88 … 100 %, from 24–28 empty squares ([chapter 18](18-the-egaroucid-port.md#the-endgame-in-stages)). Stello's faster solver of phase 3 hardly changed the games, because it only starts when the midgame would reach 7 empty squares from the end ([chapter 08](08-endgame-solver.md#when-the-solver-runs)). Would a selective pass, or an earlier start, help?

### The selective pass

It ran between the midgame and the exact solve: the solver with Multi-ProbCut at every node from 14 empty squares.

- **The shallow test** was a midgame search (Multi-ProbCut off) to Egaroucid's endgame depth, about a third of the empty squares with the same parity.
- **The pre-check** used the static evaluation within 4 discs of the margin, as in the midgame.
- **Its results** were stored in the endgame table with a negative depth, so the exact solve used only their moves, never their values.
- **Its root score** became the move's heuristic score; the win/loss/draw and exact solves followed as before.
- **The switches:** `end-mpc=` (t × 100) and `end-dist=`, which lets the midgame hand over earlier.

### The error model: `mpc-end`

The trainer's new `mpc-end` command ([MpcCommand.cs](../../Stello.Net/tools/Stello.Trainer/MpcCommand.cs), `RunEnd`):

- **The data:** 1,950 test positions, 150 per disc count from 38 to 50 (14–26 empty squares), each solved exactly with a limit of 60 s. 111 took longer and were left out, mostly at 24–26 empty squares, so 1,839 positions remained and the sample leans slightly towards easier positions.
- **The searches:** each position without selectivity to every depth up to min(10, empties − 8). That took 15 minutes on 24 threads.
- **σ of exact − shallow, in discs:** 6–8 for the static evaluation; 3.7–5.6 at Egaroucid's shallow depth (4.9 at 14 empty squares, 4.2 at 20, 4.4 at 24). That is larger than in the midgame, because the deep side is now the exact result.
- **The fit:** σ = 17.0938 − 25.4434·x + 14.9928·x² − 0.2715·s, off by 8.5 % on average over 120 cells. The mean of exact − shallow is −0.49 discs.
- **The single-move bug** showed up here first: in the first fit, σ was 8–11 discs at 16–19 and 23 empty squares, flat over the depths. The fix is the one described under item 3.

### Matches

At 100 ms, 1,180 games each, against Stello 2.0 after item 4:

| Variant | Score | Elo | First solve (empty squares, default → variant) | Result |
|---|---:|---:|---:|---|
| Selective pass, t = 1.0, at the usual hand-over (`end-mpc=100`) | 49.7 % (47.7–51.8) | −2 | 20.1 → 19.3 | [result](../../Plans/benchmark-results/match_v2x-p5-end-end-mpc_100-100ms.md) |
| … handing over 2 squares earlier (`end-dist=9`) | 49.9 % (47.9–52.0) | −1 | 20.1 → 19.4 | [result](../../Plans/benchmark-results/match_v2x-p5-end-end-mpc_100-end-dist_9-100ms.md) |
| … 4 squares earlier (`end-dist=11`) | 47.9 % (45.6–50.2) | −14 | 20.2 → 19.5 | [result](../../Plans/benchmark-results/match_v2x-p5-end-end-mpc_100-end-dist_11-100ms.md) |
| … t = 0.7, `end-dist=9` | 49.7 % (47.7–51.7) | −2 | 20.1 → 19.7 | [result](../../Plans/benchmark-results/match_v2x-p5-end-end-mpc_70-end-dist_9-100ms.md) |
| … t = 1.5, `end-dist=9` | 48.9 % (46.8–51.0) | −7 | 20.2 → 19.1 | [result](../../Plans/benchmark-results/match_v2x-p5-end-end-mpc_150-end-dist_9-100ms.md) |
| No selective pass, exact solve when the midgame would reach 5 empty squares (`end-dist=5`) | 50.0 % (47.9–52.1) | 0 | 20.2 → 19.8 | [result](../../Plans/benchmark-results/match_v2x-p5-end-end-dist_5-100ms.md) |
| … 6 (`end-dist=6`) | 51.1 % (48.9–53.2) | +7 | 20.1 → 20.0 | [result](../../Plans/benchmark-results/match_v2x-p5-end-end-dist_6-100ms.md) |
| … 8 (`end-dist=8`) | 49.0 % (46.9–51.1) | −7 | 20.2 → 20.3 | [result](../../Plans/benchmark-results/match_v2x-p5-end-end-dist_8-100ms.md) |
| … 9 (`end-dist=9`) | 48.6 % (46.6–50.6) | −10 | 20.1 → 20.4 | [result](../../Plans/benchmark-results/match_v2x-p5-end-end-dist_9-100ms.md) |

- **Rejected; the engine code was removed** (6960a31). No variant is better.
  - **The selective pass costs time** and pushes the exact solve about one square later.
  - **Its result is no better than the midgame's.** When the solve starts, the last midgame iteration has already searched, selectively, to 8 empty squares from the end. A selective endgame search gives about the same answer, later.
- **When to start solving is flat** around today's rule: the midgame stops when its next iteration would leave 7 or fewer empty squares (`EndgameDistance`). The rule stays.
- **The clog search was not tried.** It finds forced early game ends, mostly wipeouts. Egaroucid needs it because its selective endgame can miss them. With no selective endgame there is nothing for it to fix, and wipeouts are rare in these games.
- **Kept:** the trainer's `mpc-end` command, with the single-move fix in both commands, for a later try: for example with Egaroucid's staged levels (74 % … 100 %), or at longer time controls, where a selective endgame has more time to pay off.

## The evaluation cap and engine version 4

Two small changes prepared the second book recalculation (commit b8e7109):

- **The evaluation is capped at ±64 discs.** In the first 2.0 recalculation one book leaf got −65 discs: the pattern evaluation is a sum of weights and can exceed what a game can end with. The midgame evaluation is now clamped to ±64 × 100 (`EvaluateLeaf` in [SearchEngine.cs](../../Stello.Net/Stello.Engine/SearchEngine.cs)).
- **`OpeningBook.EngineVersion` is 4.** The book records the engine version with every searched value ([chapter 10](10-opening-book.md)). Version 3 was the first 2.0 release. Raising it marks every value of the old search as out of date, so `recalc` searches every leaf that is not exact again ([chapter 12](12-book-tool.md#recalculating-the-leaves-recalc)).

## The book recalculated again

### The fix in `recalc`

After the first 2.0 release, a flaw was found in its book ([chapter 22](22-phase-4-the-release.md)). 2,718 leaves carried the C++ book's `INEXACT` flag. They were imported as `WinLossDraw`, with evaluation-sized values such as −18. `recalc` kept every `WinLossDraw` value that its new search could not solve, so these leaves kept their C++-era values through the first 2.0 recalculation too.

The fix (d21e42f): only a value beyond ±32,600, a proven win or loss, is kept when the new search does not solve the position. Anything else gets the new value. A proven draw has the value 0, which the rule cannot tell from an unproven value, so the 5 proven draws of the book were searched again too and became heuristic ([chapter 12](12-book-tool.md#comparing-two-books-compare)).

### The run

- **Where:** a second PC (Ryzen 7 7800X3D), so that it did not disturb the timed matches on the laptop.
- **How:** engine version 4 (the phase 5 search: items 1–4, with the ±64 cap), 60 s per position, 8 workers, about 14 hours. It was committed as efc6038 on its own branch and merged as abb5160.
- **Depth:** 22–29 plies, mostly 24–26. The first 2.0 search reached 17–19 plies in the same time.
- **What was searched:** every leaf without a proof, including the 2,718 stale `WinLossDraw` leaves.

### What changed

- **15,776 values** changed.
- **606 heuristic leaves became exact**, and **595 became proven wins or losses**.
- **The first move changed in 1,694 positions.**

The full report is in [chapter 12](12-book-tool.md#comparing-two-books-compare).

### Checked against Egaroucid

Are the new values right? The Egaroucid port searched sampled leaves at level 21. The table gives each book's average distance from Egaroucid's score, with the correlation:

| Leaves | First 2.0 book | Final 2.0 book |
|---|---:|---:|
| 200 random leaves, heuristic in both books | 2.19 discs (0.93) | 2.02 discs (0.94) |
| 100 random former stale `WinLossDraw` leaves | 3.80 discs (0.80) | 2.02 discs (0.93) |

- **The deeper search is a little closer** on ordinary leaves (2.19 → 2.02 discs).
- **The stale leaves are now as good as the rest.** Their C++ values were 3.8 discs off; the new ones are 2.0 discs off.
- **The average size of a heuristic leaf value grew** from 3.2 to 4.9 discs. This comes from the stale leaves: their old values averaged 0.77 discs, against 4.39 from Egaroucid. It is not a bias of the deeper search: on the leaves that were heuristic in both books, the averages are 5.08 and 5.10 discs.
- **Both books are a little optimistic:** they rate the move that leads to the leaf about 0.7–0.8 discs higher than Egaroucid does. Book moves are the ones Stello liked best, so some optimism is expected.

### The opening

- **The replies to d3** are now c5 −0.87, c3 −2.62 and e3 −7.75 discs. Egaroucid's level 21 gives 0, 0 and −6. In the first 2.0 book they were c5 −0.25, c3 −0.62 and e3 −1.50.
- **The main line leaves the tiger at move 3:** after f5 d6, c5 is rated +0.87 for Black, against +0.75 for c3, the tiger.
- **The main line is now 19 moves** (it was 15), and it follows the named line "Rotating Flat (Kling Continuation)" for 18 of them ([chapter 10](10-opening-book.md#a-game-from-the-book)).

### In play

With books on, 500 ms per move, 1,180 games from `starts.txt`, the final book against the first 2.0 release's book scored **51.7 %** (49.7–53.6), +11 Elo ([result](../../Plans/benchmark-results/match_v2x-book2x-vs-book20-500ms.md)). That is not significant, but not worse, so the new book is the book of the final 2.0 (ff87861). As in phase 4, the start positions at plies 10–12 leave the books little room: they only differ in the lines they hold beyond that.

## Item 6: checking the moves outside the book

**The idea.** A book move costs no time. So in a book position, `ComputerPlayer` could spend that move's normal time searching the moves the book does not have: the same search as for any other move, restricted to those moves. If the best of them beat the book move's value by a margin, 1 disc in these matches, it was played instead.

- **Two modes** (`SearchOptions.BookCheck`, match key `book-check=`; the margin `book-margin=`):
  - `exit`: only when the book move leads out of the book;
  - `all`: in every book position.
- **Matches:** against the default, both with the final book, 100 ms per move, 1,180 games from `starts.txt`.

| Variant | Score (95 %) | Elo | Result |
|---|---|---:|---|
| `exit`, margin 1 disc | 49.6 % (47.5–51.6) | −3 | [result](../../Plans/benchmark-results/match_v2x-p5-item6-exit-m100-100ms.md) |
| `all`, margin 1 disc | 50.4 % (48.2–52.6) | +3 | [result](../../Plans/benchmark-results/match_v2x-p5-item6-all-m100-100ms.md) |

**Why nothing shows.** The games start at plies 10–12, so each side plays only about 1.2 book moves per game. A count over 100 start positions (200 games):

- `exit` searched 101 times, found a better move than the book's 26 times, and switched (more than 1 disc better) 14 times;
- `all` searched 245 times and switched 24 times.

A change in 7–12 % of the games, each worth a fraction of a disc, is far below what 1,180 games can measure.

- **Rejected.** Neither mode passes the 100 ms gate. The code is in history as 124cf2d and was removed in 6e08ec5.
- **In the apps** games start from the first move and stay in the book much longer, so the check would run more often. But a 0.1–1 s search that overrules a 60 s book value is more likely noise than a find.
- **A better place for the idea is the book itself:** the book tool could search the missing moves of each book position for 60 s and add the good ones, as the C++ program's calclib did ("the best move not yet in the book"). `BookSearch` already has the `onlyMoves` search it would need.

## Against Egaroucid after item 4

With items 1–4 done, Stello 2.0 played the Egaroucid port again, no books, 1,180 games each. The table also shows Stello 1 ([chapter 19](19-stello-1-against-egaroucid.md#matches)) and Stello 2.0 after phase 2, with the pattern evaluation but the old search and solver:

| Stello | 100 ms | 500 ms | 2000 ms |
|---|---:|---:|---:|
| Stello 1 | 12.9 % (Egaroucid 87.1 %), −331 | 5.4 % (Egaroucid 94.6 %), −497 | |
| After phase 2 | 59.5 %, +67 ([result](../../Plans/benchmark-results/match_v2-p2-eval-vs-egaroucid-100ms.md)) | 38.2 %, −84 ([result](../../Plans/benchmark-results/match_v2-p2-eval-vs-egaroucid-500ms.md)) | |
| After phase 5, item 4 | **76.4 %** (74.2–78.6), **+204** ([result](../../Plans/benchmark-results/match_v2x-p5-item4-vs-egaroucid-100ms.md)) | **57.7 %** (55.4–59.9), **+54** ([result](../../Plans/benchmark-results/match_v2x-p5-item4-vs-egaroucid-500ms.md)) | **40.8 %** (38.7–43.0), **−64** ([result](../../Plans/benchmark-results/match_v2x-p5-item4-vs-egaroucid-2000ms.md)) |

The details of the phase 5 matches, Stello against Egaroucid:

| | 100 ms | 500 ms | 2000 ms |
|---|---:|---:|---:|
| Midgame depth | 13.9 / 8.7 | 16.8 / 13.4 | 19.2 / 21.5 |
| First solve (empty squares) | 20.7 / 18.0 | 22.4 / 20.7 | 24.0 / 22.9 |
| Nodes per second | 4.12 M / 3.77 M | 4.33 M / 4.18 M | 4.65 M / 4.93 M |
| Thinking time per game | 1.4 / 1.8 s | 6.5 / 7.2 s | 24.4 / 26.2 s |
| Draws | 81 | 153 | 194 |

- **The engines are even somewhere between 500 ms and 2 s.** At short times Stello's cheaper search wins; with more time Egaroucid's better evaluation (5.05 discs error against Stello's 5.47, [chapter 04](04-evaluation.md)) pays off with depth.
- **Depth is not comparable across the engines.** Egaroucid's is nominal, at its own Multi-ProbCut selectivity. What compares is the trend: Stello's lead in depth, about 5 plies at 100 ms, turns into 2 plies less at 2 s.
- **At 100 ms Egaroucid thinks longer** (1.8 against 1.4 s per game); its longest move took 590 ms, because its clog search has no time limit ([chapter 19](19-stello-1-against-egaroucid.md#measurement-caveats)).
- **At 2 s Stello's table may be too small:** 2^19 slots against Egaroucid's 2^21. This was not measured.

## What twice the time is worth

Stello 2.0 after phase 5 against itself with twice the time, no books, 1,180 games each:

| Match | Score | Elo | Midgame depth | First solve | Result |
|---|---:|---:|---:|---:|---|
| 200 against 100 ms | 61.8 % (59.5–64.1) | +84 | 13.77 → 15.14 | 20.1 → 21.2 | [result](../../Plans/benchmark-results/match_v2x-timeodds-100-vs-200ms.md) |
| 1000 against 500 ms | 55.4 % (53.3–57.6) | +38 | 16.65 → 17.98 | 22.1 → 23.2 | [result](../../Plans/benchmark-results/match_v2x-timeodds-500-vs-1000ms.md) |

- **Twice the time gives 1.3–1.4 plies** and the solver one empty square earlier. Stello 1 gained 0.9 and 0.75 plies.
- **But it is worth less at 500 ms.** For Stello 1 twice the time was worth +91 and +64 Elo ([chapter 17](17-measuring-the-engine.md#strength-what-more-time-is-worth)); for the final 2.0 it is +84 and +38. The value of more time falls as the search gets deeper.
- **This is what a 2× faster engine is worth,** whether the speed comes from a faster language, more threads or a better algorithm: about +84 Elo at 100 ms and +38 at 500 ms.

## C++ against C#

Would the same engine be faster in C++? Egaroucid makes a fair test, because the port in `Egaroucid.Engine` is a close translation of the C++ source: both search the same trees (the node counts agree within 2 %), so the times compare the languages and their compilers, not the algorithms. The full write-up is [cpp-vs-csharp-speed.md](../../Plans/cpp-vs-csharp-speed.md) (dff7129). On 1 thread, FFO #40–49 solved exactly and 100 midgame positions at level 15:

| Build | FFO #40–49 | Midgame, level 15 |
|---|---:|---:|
| C++ native, AVX2 (`g++ -O2 -march=native`) | **11.6 s** | **12.5 s** |
| C++ native, no AVX2 | 17.1 s | 18.8 s |
| C++ WebAssembly + SIMD128 (Emscripten, run in Node 22) | 17.7 s | 18.8 s |
| C# native (.NET 10) | 26.8 s | 29.0 s |
| C# WebAssembly (.NET 10 AOT, as the web app is built) | 34.2 s | 50.5 s |

| C++ faster than C# by | Endgame | Midgame |
|---|---:|---:|
| Native, both without AVX2 | 1.57× | 1.54× |
| Native, C++ with AVX2 | 2.3× | 2.3× |
| WebAssembly | 1.9× | 2.7× |

- **In the browser** C++ in WebAssembly is as fast as native C++ without AVX2, and faster than native C#. .NET loses more in WebAssembly, most of all in the evaluation-heavy midgame.
- **On the desktop** C++ gains 1.5× from the compiler and 2.3× with AVX2. C# could use AVX2 through `System.Runtime.Intrinsics`, which might close part of that gap; the port does not, so it was not measured.
- **Threads were not measured.** WebAssembly threads are mature in Emscripten, but experimental in .NET's WebAssembly runtime.

Put together with the time-odds matches: a 2–2.7× faster engine in the browser would be worth roughly what twice the time is worth, +84 Elo at 100 ms and +38 at 500 ms, or a little more. More threads could give the same on the desktop. Which way Stello goes next, item 7 (Lazy SMP or YBWC) or a core in another language, is the subject of [chapter 24](24-results-and-what-comes-next.md).

## Summary

| Item | Change | Result | Decision | Commit |
|---|---|---|---|---|
| 1 | Soft limit 80–100 %; predicting the next iteration | +4 to +14 Elo, not significant; −25 to −37 | Rejected: the 2/3 rule stays, `soft-pct=` stays as a switch | 299bcb7 |
| 2a | PVS in every midgame node | +28 (100 ms), +30 (500 ms) | **Kept** | 53644bd |
| 2b | Midgame table with both bounds and two moves | −37 to +4 | Rejected, removed | 53644bd, 65ccc87 |
| 3 | Multi-ProbCut with a fitted error model (`mpc`), t = 1.0 | +126 (100 ms), +120 (500 ms); depth 13.6 → 16.6 at 500 ms; 4.6× fewer nodes at depth 10 | **Kept**; the old selective search removed, `mpc=` stays | 6c7c938 |
| 4 | Evaluation ordering from `look` 4 without killers, ETC from `look` 4 | −3.9 % nodes at depth 12; +23 (100 ms), +13 (500 ms) | **Kept** as a speed change; IID and shallow-search keys dropped | 12e3833 |
| 5 | Selective endgame (`mpc-end`); solve start 5–9 squares | −14 to −1; −10 to +7 | Rejected; `mpc-end` stays, clog search not tried | 6960a31 |
| — | Evaluation capped at ±64 discs, `OpeningBook.EngineVersion` 4 | — | Kept | b8e7109 |
| — | `recalc` replaces win/loss/draw values that are no proof | 2,718 stale leaves searched again | Kept | d21e42f |
| — | The book recalculated with the phase 5 search | 51.7 % (+11) against the first 2.0 book | Kept | efc6038, abb5160, ff87861 |
| 6 | Checking the moves outside the book (`exit`, `all`) | −3, +3 | Rejected, removed | 124cf2d, 6e08ec5 |

A side experiment of the same days, a larger evaluation for the desktop (9b1ee56), belongs to the evaluation work and is told in [chapter 20](20-phases-0-2-the-evaluation.md).

## Reproducing the results

```text
dotnet run -c Release --project Stello.Net/tools/Stello.Benchmark -- <command> [options]
```

The switches of the rejected items and of the old selective search are gone, so only some of the matches can be played again with today's code. The others need the commit in the table above.

| What | Command |
|---|---|
| Time use | `match --a engine=stello,time-ms=100 --b engine=stello,time-ms=100,soft-pct=100,name=soft-pct_100` |
| t of Multi-ProbCut | `match --a engine=stello,time-ms=500,mpc=130,name=mpc130 --b engine=stello,time-ms=500,mpc=100,name=mpc100` |
| Nodes at a fixed depth with a switch | `mid-bench --depth 12 --passes 1 --side mpc=130` |
| Against Egaroucid | `match --a engine=egaroucid,time-ms=2000 --b engine=stello,time-ms=2000` |
| Twice the time | `match --a engine=stello,time-ms=100 --b engine=stello,time-ms=200` |
| The book match | `match` with `book=on` on both sides and `book-file=<old book>` on side A |
| The book | `recalc`, `compare` and `match` of the book tool ([chapter 12](12-book-tool.md#recalculating-the-master-book)) |

The error models come from the trainer, built in Release and run from the `Stello.Net` folder:

```text
cd Stello.Net
dotnet tools/Stello.Trainer/bin/Release/net10.0/Stello.Trainer.dll mpc --per-discs 300 --from 14 --to 52 --max-depth 13
dotnet tools/Stello.Trainer/bin/Release/net10.0/Stello.Trainer.dll mpc --fit '../Traning data/mpc-values.csv'
dotnet tools/Stello.Trainer/bin/Release/net10.0/Stello.Trainer.dll mpc-end --per-discs 150 --from 38 --to 50 --solve-s 60
```

- The first line searches the 11,700 positions of item 3, writes `Traning data/mpc-values.csv` and prints the σ table and the fit.
- The second fits the midgame model again from that file. This is how the refit without single-move rows was made (item 5).
- The third solves and searches the endgame positions of item 5, writing `Traning data/mpc-end-values.csv`.

The trainer's commands need the converted training data, which is not in the repository. Egaroucid's resources must be in `Egaroucid-main/bin/resources` or `$EGAROUCID_RESOURCES` ([chapter 18](18-the-egaroucid-port.md#the-resources)).

## Where in the code

| File | Main members |
|---|---|
| [SearchEngine.cs](../../Stello.Net/Stello.Engine/SearchEngine.cs) | `Search` (PVS in every node, ETC), `TryProbCut`, `ProbCutShallowDepth`, `ProbCutErrors`, `DefaultProbCutPercent`, `OrderByEvaluation`, `EvaluateLeaf` (the ±64 cap), `IterationDone`, `WithSoftPercent` |
| [Search/SearchOptions.cs](../../Stello.Net/Stello.Engine/Search/SearchOptions.cs) | `SoftTimePercent`, `Selective`, `ProbCutPercent` |
| [MpcCommand.cs](../../Stello.Net/tools/Stello.Trainer/MpcCommand.cs) | `Run` (`mpc`), `RunEnd` (`mpc-end`), the fits |
| [MatchCommands.cs](../../Stello.Net/tools/Stello.Benchmark/MatchCommands.cs) | `ParseSide`: the keys `soft-pct=`, `mpc=`, `book-file=` |
| [BenchCommands.cs](../../Stello.Net/tools/Stello.Benchmark/BenchCommands.cs) | `MidBench` with `--side` |
| [BookRecalculator.cs](../../Stello.Net/Stello.Engine/BookRecalculator.cs) | the rule for win/loss/draw values |
| [OpeningBook.cs](../../Stello.Net/Stello.Engine/OpeningBook.cs) | `EngineVersion` |

---

Previous: [22 Phase 4: the release](22-phase-4-the-release.md) · Next: [24 Results and what comes next](24-results-and-what-comes-next.md)
