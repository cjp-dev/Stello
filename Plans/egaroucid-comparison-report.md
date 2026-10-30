# Stello compared with the Egaroucid port

The first comparison of Stello's engine with the C# port of Egaroucid 7.9, from phase 5 of [egaroucid-port-plan.md](egaroucid-port-plan.md). Both engines run on 1 thread. The raw results are in [benchmark-results](benchmark-results/); how the port was built is in [egaroucid-port-findings.md](egaroucid-port-findings.md).

- **Machine:** Intel i7-12850HX laptop VM, 24 logical processors.
  - Matches and `mid-quality` run 12 searches at a time; nothing else heavy ran meanwhile.
  - The speed benchmarks run one search at a time.
- **Date:** 30 October 2026.
- **Tool:** `Stello.Net/tools/Stello.Benchmark`. The commands are given with each section, so every number can be measured again.

## Summary

Egaroucid is far stronger. Against Stello it scores **87 % at 100 ms per move** (+331 Elo) and **95 % at 500 ms** (+497 Elo), winning by about 14 discs a game. The gap grows with time.

Where the difference comes from, in order of weight:

1. **The evaluation.** Egaroucid's pattern evaluation predicts the final score about 2.8 times better (mean error 4.0 against 11.1 discs). Stello's search reaches about the same depth in the same time, but it searches towards a much worse estimate.
2. **The search turns time into quality; Stello's hardly does.** In positions solved exactly, Egaroucid's moves lose 1.23 discs at 50 ms and 0.51 at 1000 ms. Stello's lose 1.61 and 1.38: 20 times more thinking gains Stello almost nothing.
3. **The endgame solver is 14 times faster,** but it decides fewer games than the numbers suggest. In time mode, both engines first solve at about 20 empty squares at 500 ms, and the games are mostly decided before that.

The opening books change nothing at 500 ms: 94.4 % with books against 94.6 % without.

## Matches

`match --a time-ms=T,name=stello-Tms --b engine=egaroucid,time-ms=T,name=egaroucid-Tms`, plus `book=on` on both sides for the book match. The games start from the 590 positions in `starts.txt` (ply 10–12 of Stello's book, balanced). Each position is played twice with the colours swapped, giving 1,180 games. The intervals are 95 % over the pairs of games.

| | 100 ms, no books | 500 ms, no books | 500 ms, both books |
|---|---:|---:|---:|
| Egaroucid's score | **87.1 %** (85.3–88.8) | **94.6 %** (93.4–95.7) | **94.4 %** (93.2–95.6) |
| Elo for Egaroucid | **+331** (+306 to +360) | **+497** (+461 to +540) | **+490** (+454 to +533) |
| Egaroucid wins / draws / losses | 995 / 65 / 120 | 1,099 / 34 / 47 | 1,096 / 35 / 49 |
| Discs per game for Egaroucid | +13.3 ± 0.7 | +14.7 ± 0.6 | +14.0 ± 0.5 |
| Midgame depth, Stello / Egaroucid | 10.4 / 8.8 | 12.3 / 13.5 | 12.4 / 13.8 |
| First solve, empty squares, Stello / Egaroucid | 18.4 / 17.8 | 20.1 / 20.3 | 20.2 / 20.3 |
| Thinking time per game, Stello / Egaroucid | 1.5 / 1.8 s | 7.1 / 7.3 s | 6.6 / 7.3 s |
| Time per searched move, Stello / Egaroucid: average | 66 / 71 ms | 313 / 294 ms | 305 / 295 ms |
| Time per searched move, Stello / Egaroucid: longest | 118 / 488 ms | 518 / 531 ms | 520 / 1,003 ms |
| Wall time | 5 m 25 s | 23 m 57 s | 23 m 08 s |

**Is the time fair?**
- **At 100 ms, Egaroucid thinks 20 % longer per game.** Some of its moves overrun the limit: the clog search has no time limit, and its slowest move took 488 ms. Stello uses less than its limit: it starts no new iteration after 2/3 of the limit, a rule from the original C++ program.
- **At 500 ms, the two use the same time** within 3 %. Egaroucid's single 1,003 ms move is the move after leaving the book, whose two searches each get the full limit.
- The extra 20 % at 100 ms is worth a few tens of Elo; the step-1 match of 100 ms against 200 ms gave +91 Elo for twice the time. That doesn't explain +331.

**Depth is not the explanation.** At 100 ms Stello searches deeper (10.4 against 8.8) and still scores 13 %.
- Egaroucid's midgame depth counts nominal plies at its Multi-ProbCut selectivity. Stello's counts full-width plies, so the depths are not equal work.
- Egaroucid's average also includes selective endgame searches, which the adapter counts as midgame because they aren't exact. This raises its average at 500 ms.

**The books:**
- Egaroucid's book holds 1.3 million positions; Stello's about 11,000.
- The start positions come from Stello's own book, so Stello knows them best. Even so, the result is the same as without books.
- At 500 ms, both engines' play is strong enough that a book's first moves don't matter much; at 10–12 plies into a game, the books are near their ends anyway.

## Evaluation

`eval-compare`: 267,174 positions, a seeded random 5,000 for each number of empty squares, from the 25.5 million positions of `Egaroucid_Train_Data/0001_egaroucid_7_5_1_lv17`. The target is the final score of the game for the side to move. That data is not in the repository.

| Empty squares | Draw guess | Egaroucid MAE | Egaroucid r | Stello MAE (scale fitted per group) | Stello r | Stello units per disc |
|---|---:|---:|---:|---:|---:|---:|
| 51–55 | 10.50 | 2.44 | 0.963 | 5.05 | 0.858 | 20.3 |
| 46–50 | 13.91 | 3.67 | 0.958 | 7.38 | 0.743 | 26.9 |
| 41–45 | 15.09 | 4.10 | 0.958 | 6.60 | 0.899 | 19.5 |
| 36–40 | 16.18 | 4.15 | 0.963 | 7.96 | 0.874 | 23.8 |
| 31–35 | 16.89 | 4.14 | 0.966 | 8.78 | 0.853 | 30.7 |
| 26–30 | 17.60 | 4.13 | 0.969 | 9.33 | 0.849 | 39.2 |
| 21–25 | 18.55 | 4.28 | 0.970 | 9.58 | 0.853 | 49.0 |
| 16–20 | 19.22 | 4.54 | 0.969 | 9.62 | 0.863 | 61.6 |
| 11–15 | 19.63 | 4.66 | 0.968 | 9.23 | 0.877 | 75.7 |
| 6–10 | 20.00 | 4.48 | 0.972 | 8.80 | 0.894 | 92.5 |
| 1–5 | 19.70 | 2.55 | 0.986 | 7.96 | 0.907 | 112.8 |
| **All** | | **3.96** | **0.969** | **11.07** (one scale, 75.2) | **0.764** | 75.2 |

- MAE is the mean absolute error in discs. "Draw guess" is the error of always predicting 0. r is the correlation, which does not depend on the scale.
- **Stello's best possible scale changes 5-fold over the game:** about 20 units per disc in the opening, 113 near the end.
  - Its evaluation has no phases: mobility dominates early, discs and edges late, and nothing rescales them.
  - With one scale for the whole game the error is 11.1 discs; even with the best scale for each group it stays at 5–10 discs.
- **Egaroucid's error is flat:** about 4 discs from 50 to 6 empty squares.

**Speed, from scratch:** Egaroucid 0.33 M evaluations per second, Stello 2.14 M.
- In the search, Egaroucid updates its features move by move: 2.2 M move + evaluate + undo per second (findings, phase 2).
- Stello evaluates from scratch at every leaf. Per leaf, the two cost about the same.

**This test favours Egaroucid:**
- Egaroucid's weights were probably trained on this very data.
- For positions with 49 or more empty squares, the target is Egaroucid's own search value, not a game result.
- So the 2.8× is an upper bound for that reason. `mid-quality` below does not have this bias.

## Midgame quality

`make-exact`, then `mid-quality`.
- **The positions:** 300 positions from 100 games Stello played against itself at depth 6, from start positions 21–120. One position was taken at each of 28, 26 and 24 empty squares.
- **The solving:** every legal move of each position was solved exactly by the port: 1 h 23 m on 12 threads.
- **The check:** Stello's solver gave the same score and best move on the 4 positions it was given.
- **The positions file:** `Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt`.

Each engine chooses a move with a fresh hash table at a fixed time per move. The loss is the best score minus the score of the chosen move.

| ms/move | Stello loss | Egaroucid loss | Difference (paired) | Stello best move | Egaroucid best move |
|---:|---:|---:|---:|---:|---:|
| 50 | 1.61 ± 0.34 | 1.23 ± 0.32 | +0.37 ± 0.38 | 66 % | 73 % |
| 200 | 1.65 ± 0.33 | 0.95 ± 0.24 | +0.69 ± 0.36 | 65 % | 76 % |
| 1000 | 1.38 ± 0.30 | 0.51 ± 0.18 | +0.87 ± 0.31 | 69 % | 87 % |

The losses are in discs per move, with 95 % intervals.

- **Egaroucid's loss halves from 50 to 1000 ms; Stello's barely moves.**
  - At 28–24 empty squares neither engine solves the game within these times: Stello never, Egaroucid in 1.3 % of the searches at 1000 ms. So this is the midgame search with its evaluation.
  - Stello's deeper searches keep choosing the moves its evaluation prefers. Searching deeper with a biased evaluation does not remove the bias.
- **At 50 ms the difference is within the noise** (+0.37 ± 0.38). There, Egaroucid also took about 40 % more time (69 against 47 ms) because of the clog search.
- **Per move, the losses look small** (one or two discs), but a game has about 25 midgame moves per side. Small losses add up to the 13–15 discs per game the matches show.

## Midgame search speed

`mid-bench` on the 100 `midgame.txt` positions at depth 10, best of 3 passes, an empty table for each search.

| Engine | Nodes | Time | Nodes per second |
|---|---:|---:|---:|
| Stello, depth 10 | 23,503,459 | 6.81 s | 3.45 M |
| Egaroucid, depth 10, no selectivity (`--mpc 100`) | 33,132,409 | 4.92 s | 6.74 M |
| Egaroucid, depth 10, 74 % (`--mpc 74`) | 23,371,480 | 4.08 s | 5.72 M |

- **Even without selectivity, Egaroucid searches depth 10 faster** (4.9 against 6.8 s). It visits more nodes, but each node is cheaper: incremental features, and special code for depth 1 and 2. The two engines count nodes differently, so the node rates are only a rough guide.
- **Multi-ProbCut at 74 % saves 17 % of the time at depth 10,** and much more at the deeper levels a game reaches. In the 500 ms matches, Egaroucid's nominal depth is 13.5.
- **Search speed is not the main difference:** at depth 10 the two are within 1.4×. The evaluation and the selectivity decide the matches.

## Endgame

`solve-bench --engine egaroucid --to 49` gives FFO #40–49 exactly, 1 thread. All scores and moves are correct for both engines.

| Engine | Table | Nodes | Time |
|---|---|---:|---:|
| Egaroucid port | 2^21 slots (48 MB, Stello's memory) | 518,943,586 | 25.6 s |
| Egaroucid port | 2^25 slots (800 MB, C++ default) | 520,296,918 | 25.9 s |
| Egaroucid C++ Generic (phase 3) | 2^25 slots | 520,296,918 | about 19.4 s |
| Stello (step 1) | 2 × 2^19 slots × 2 | — | 369 s |

- **The table size doesn't matter here:** 2^21 slots need 0.3 % fewer nodes than 2^25 and give the same time.
- **The port solves these 14 times faster than Stello.** Part of that is the staging: the selective passes at 74 % … 99.9 % fill the table before the exact pass.
- **In games the gap matters less than it looks:**
  - In time mode, both engines first solve at about 20 empty squares at 500 ms. Egaroucid's time-per-move mode uses 74 % selectivity in the endgame, and spends its time on the midgame.
  - At 100 ms, Stello even solves slightly earlier (18.4 against 17.8 empty squares).

## What this means for the improved engine

In order of expected gain:

1. **A trained pattern evaluation, in discs, with phases.** Both the evaluation test and `mid-quality` point to it as the main cause.
   - It is the step that makes extra time worth something.
   - It needs a trainer, postponed to the improved-engine step, and training data. Egaroucid's data may be used for training but not redistributed.
2. **Multi-ProbCut with a fitted error model.** It turns the faster evaluation into depth. Its value has to be measured with the new evaluation, because Multi-ProbCut's error depends on how good the evaluation is.
3. **The endgame solver** (14×): staging, the local table, stability cutoffs, and ordering by a small evaluation near the end. This matters most for analysis and at long time controls.
4. **Time management:**
   - Stello leaves time unused: 313 ms of 500 ms on average, because it starts no new iteration after 2/3 of the limit.
   - Egaroucid's clog search needs a limit at short time controls.

**What was not measured:**
- Hybrids, such as Stello's search with Egaroucid's evaluation. They would separate the evaluation from the search exactly, and belong to the improved-engine step.
- Multi-threading, which comes in phase 6.
