# 20 – Comparing Stello with Egaroucid

[Back to the index](README.md)

## In short

The C# port of Egaroucid ([chapter 19](19-the-egaroucid-port.md)) was measured against Stello one component at a time, on 1 thread each. The result:

- **Egaroucid is far stronger.** It scores **87 % at 100 ms per move** (+331 Elo) and **95 % at 500 ms** (+497 Elo), winning by about 14 discs a game. The gap grows with time.
- **The evaluation is the main cause.** Egaroucid's pattern evaluation predicts the final score about 2.8 times better, and its moves get better with more time. Stello's hardly do.
- **The endgame solver is 14 times faster,** but in games it decides less than that suggests.
- **The opening books change nothing** at 500 ms.

"Stello" in this chapter is the engine as it was before Stello 2.0 began. It is now frozen as **Stello 1** in the project [Stello.Engine.V1](../../Stello.Net/Stello.Engine.V1), and the benchmark tool plays it as `engine=stello-v1` or `--engine stello-v1`. So the results can be measured again after `Stello.Engine` changes.

The full report is [egaroucid-comparison-report.md](../../Plans/egaroucid-comparison-report.md). The raw results are in [Plans/benchmark-results](../../Plans/benchmark-results).

## How it was measured

- **Machine:** Intel i7-12850HX laptop VM, 24 logical processors. Timings vary by up to about 8 %.
- **Date:** 30 October 2026.
- **Threads:** both engines search on 1 thread.
  - Matches and `mid-quality` run 12 searches at a time, with nothing else heavy on the machine.
  - The speed benchmarks run one search at a time.
- **Tool:** [Stello.Benchmark](../../Stello.Net/tools/Stello.Benchmark) ([chapter 18](18-measuring-the-engine.md)). Every number can be measured again with the commands [below](#reproducing-the-results).

## The results per component

### Matches

The games start from the 590 positions in `starts.txt`, each played twice with the colours swapped: 1,180 games per match. The intervals are 95 % over the pairs of games.

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

- **At 100 ms, Egaroucid thinks 20 % longer per game.** Its clog search has no time limit, and its slowest move took 488 ms. That extra time is worth a few tens of Elo: twice the time gave Stello +91 Elo ([chapter 18](18-measuring-the-engine.md#strength-what-more-time-is-worth)). It doesn't explain +331.
- **At 500 ms, the two use the same time** within 3 %. Egaroucid's single 1,003 ms move is the move after leaving the book, whose two searches each get the full limit.
- **The books:** Egaroucid's holds 1.3 million positions, Stello's about 11,000. The start positions come from Stello's own book, so Stello knows them best. Even so, the result is the same as without books.

Reports: `match_stello-100ms_vs_egaroucid-100ms`, `match_stello-500ms_vs_egaroucid-500ms` and `match_stello-500ms-book_vs_egaroucid-500ms-book` (`.md` and `.json`).

### Evaluation: `eval-compare`

Both evaluations score 267,174 positions from Egaroucid's training data: a seeded random 5,000 for each number of empty squares. The target is the game's final score for the side to move. Stello's evaluation is in its own units, so it is divided by a fitted scale (units per disc).

| Empty squares | Draw guess | Egaroucid error | Egaroucid r | Stello error (scale per group) | Stello r | Stello units per disc |
|---|---:|---:|---:|---:|---:|---:|
| 51–55 | 10.50 | 2.44 | 0.963 | 5.05 | 0.858 | 20.3 |
| 41–45 | 15.09 | 4.10 | 0.958 | 6.60 | 0.899 | 19.5 |
| 31–35 | 16.89 | 4.14 | 0.966 | 8.78 | 0.853 | 30.7 |
| 21–25 | 18.55 | 4.28 | 0.970 | 9.58 | 0.853 | 49.0 |
| 11–15 | 19.63 | 4.66 | 0.968 | 9.23 | 0.877 | 75.7 |
| 1–5 | 19.70 | 2.55 | 0.986 | 7.96 | 0.907 | 112.8 |
| **All** | | **3.96** | **0.969** | **11.07** (one scale, 75.2) | **0.764** | 75.2 |

The errors are mean absolute errors in discs. "Draw guess" is the error of always predicting 0. r is the correlation, which does not depend on the scale. The full table, every 5 empty squares, is in `eval-compare.md`.

- **Egaroucid's error is flat:** about 4 discs from 50 to 6 empty squares.
- **Stello's best scale changes 5-fold over the game,** from about 20 units per disc in the opening to 113 near the end. Its evaluation has no phases: mobility dominates early, discs and edges late, and nothing rescales them. With one scale for the whole game the error is 11.1 discs; even with the best scale for each group it stays at 5–10 discs.
- **Speed from scratch:** Egaroucid 0.33 M evaluations per second, Stello 2.14 M. But Egaroucid updates its features move by move in the search (2.2 M move + evaluate + undo per second), so per leaf the two cost about the same.

**This test favours Egaroucid.** Its weights were probably trained on this very data. For positions with 49 or more empty squares, the target is Egaroucid's own search value, not a game result. So the factor 2.8 is an upper bound. `mid-quality` below has no such bias.

### Midgame quality: `mid-quality`

How many discs does a move lose against perfect play?
- **The positions:** 300, from 100 games Stello played against itself at depth 6. One position was taken at each of 28, 26 and 24 empty squares.
- **The answers:** `make-exact` solved every legal move of every position exactly with the port, in 1 h 23 m on 12 threads. Stello's solver gave the same score and best move on the 4 positions it was given. The file is [midgame-exact.txt](../../Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt).
- **The test:** each engine chooses a move with a fresh hash table at a fixed time per move. The loss is the best score minus the score of the chosen move.

| ms/move | Stello loss | Egaroucid loss | Difference (paired) | Stello best move | Egaroucid best move |
|---:|---:|---:|---:|---:|---:|
| 50 | 1.61 ± 0.34 | 1.23 ± 0.32 | +0.37 ± 0.38 | 66 % | 73 % |
| 200 | 1.65 ± 0.33 | 0.95 ± 0.24 | +0.69 ± 0.36 | 65 % | 76 % |
| 1000 | 1.38 ± 0.30 | 0.51 ± 0.18 | +0.87 ± 0.31 | 69 % | 87 % |

The losses are in discs per move, with 95 % intervals.

- **Egaroucid's loss halves from 50 to 1000 ms; Stello's barely moves.** Neither engine solves these positions in that time (Egaroucid in 1.3 % of the searches at 1000 ms), so this is the midgame search with its evaluation. Stello's deeper searches keep choosing the moves its evaluation prefers.
- **At 50 ms the difference is within the noise** (+0.37 ± 0.38). There Egaroucid also took about 40 % more time (69 against 47 ms) because of the clog search.
- **Small losses add up.** One or two discs a move, over about 25 midgame moves per side, gives the 13–15 discs per game of the matches.

Report: `mid-quality.md` and `.json`.

### Midgame speed: `mid-bench`

The 100 `midgame.txt` positions at depth 10, best of 3 passes, an empty table for each search.

| Engine | Nodes | Time | Nodes per second |
|---|---:|---:|---:|
| Stello, depth 10 | 23,503,459 | 6.81 s | 3.45 M |
| Egaroucid, depth 10, no selectivity (`--mpc 100`) | 33,132,409 | 4.92 s | 6.74 M |
| Egaroucid, depth 10, 74 % (`--mpc 74`) | 23,371,480 | 4.08 s | 5.72 M |

- **Even without selectivity, Egaroucid searches depth 10 faster** (4.9 against 6.8 s). It visits more nodes, but each node is cheaper: incremental features, and special code for depth 1 and 2.
- **Multi-ProbCut at 74 % saves 17 % of the time at depth 10,** and much more at the deeper levels a game reaches.
- **Search speed is not the main difference:** at depth 10 the two are within 1.4×.

Results: `mid-bench_stello_depth10.json`, `mid-bench_egaroucid_depth10_mpc100.json` and `mid-bench_egaroucid_depth10_mpc74.json`.

### Endgame: `solve-bench`

FFO #40–49 solved exactly on 1 thread. All scores and moves are correct for both engines.

| Engine | Table | Nodes | Time |
|---|---|---:|---:|
| Egaroucid port | 2^21 slots (48 MB, Stello's memory) | 518,943,586 | 25.6 s |
| Egaroucid port | 2^25 slots (800 MB, C++ default) | 520,296,918 | 25.9 s |
| Egaroucid C++ Generic | 2^25 slots | 520,296,918 | about 19.4 s |
| Stello | 2 × 2^19 slots × 2 | — | 369 s |

- **The table size doesn't matter here:** 2^21 slots need 0.3 % fewer nodes than 2^25 and take the same time.
- **The port solves these 14 times faster than Stello.** Part of that may be the staging: the selective passes at 74 % … 99.9 % fill the table before the exact pass.
- **In games the gap matters less.** At 500 ms both engines first solve at about 20 empty squares, because Egaroucid's time mode solves at 74 % and spends its time on the midgame. At 100 ms Stello even solves slightly earlier (18.4 against 17.8 empty squares).

Results: `solve-bench_egaroucid_ffo40-49.json` and `solve-bench_egaroucid_ffo40-49_hash25.json`; Stello's is `baseline-solve-bench.json` ([chapter 18](18-measuring-the-engine.md#endgame-solve-bench---to-49)).

## What explains the difference

In order of weight:

1. **The evaluation.** Its error is 4.0 discs against Stello's 11.1, and flat over the game, while Stello's scale drifts 5-fold. Stello's search reaches about the same depth in the same time, but it searches towards a much worse estimate. Searching deeper with a biased evaluation does not remove the bias.
2. **The search turns time into quality.** Egaroucid's loss per move halves with 20 times more time; Stello's hardly changes. Multi-ProbCut with a fitted error model turns the faster evaluation into depth.
3. **The endgame solver** is 14 times faster. It matters most for analysis and long time controls, less in games at 500 ms.
4. **Time use** is a small effect. Stello leaves time unused (313 ms of 500 ms on average): it starts no new iteration after 2/3 of the limit. Egaroucid's clog search overruns short limits.

**Not measured:** hybrids, such as Stello's search with Egaroucid's evaluation, and multi-threading.

## Measurement caveats

- **Node counts can't be compared across engines.** The two count nodes differently, so their node rates are only a rough guide. Time to a given strength is what counts, measured with matches and `mid-quality`.
- **Depth is not comparable either.** Egaroucid's midgame depth counts nominal plies at its Multi-ProbCut selectivity; Stello's counts full-width plies. At 100 ms Stello searches deeper (10.4 against 8.8 plies) and still scores 13 %.
- **Egaroucid's midgame depth includes selective endgame searches.** The adapter counts them as midgame because they aren't exact. This raises its average at 500 ms.
- **The clog search ignores the time limit.** On 1 thread it takes 25–190 ms, sometimes more than a whole 100 ms move. So Egaroucid gets about 20 % more time at 100 ms, and about 40 % more in `mid-quality` at 50 ms.
- **`eval-compare` favours Egaroucid,** as explained above.
- **Timings vary by up to about 8 %** on this machine; the `--compare` check of [chapter 18](18-measuring-the-engine.md) applies within one engine only.

## Reproducing the results

```text
dotnet run -c Release --project Stello.Net/tools/Stello.Benchmark -- <command> [options]
```

Egaroucid's resources must be in `Egaroucid-main/bin/resources` or in `$EGAROUCID_RESOURCES` ([chapter 19](19-the-egaroucid-port.md#running-it)). `engine=stello` is now Stello 2.0 ([chapter 21](21-stello-2-evaluation-and-solver.md)), so use **`stello-v1`** to measure the engine of this chapter.

| What | Command |
|---|---|
| Match, 100 ms | `match --a engine=stello-v1,time-ms=100,name=stello-100ms --b engine=egaroucid,time-ms=100,name=egaroucid-100ms` |
| Match, 500 ms | `match --a engine=stello-v1,time-ms=500,name=stello-500ms --b engine=egaroucid,time-ms=500,name=egaroucid-500ms` |
| Match, 500 ms with books | add `book=on` to both sides |
| Evaluation | `eval-compare` (reads `Traning data/Egaroucid_Train_Data/0001_egaroucid_7_5_1_lv17` unless `--data` says otherwise; `--per-empties 5000`) |
| Exact positions | `make-exact` (300 positions at 28, 26 and 24 empty squares; continues an existing `Data/midgame-exact.txt`) |
| Midgame quality | `mid-quality --engines stello-v1,egaroucid` (times `--times 50,200,1000`) |
| Midgame speed | `mid-bench --engine stello-v1`, `mid-bench --engine egaroucid`, `mid-bench --engine egaroucid --mpc 74` |
| Endgame | `solve-bench --engine stello-v1 --to 49`, `solve-bench --engine egaroucid --to 49` (add `--hash-bits 25` for the C++ table size) |

- **The side keys:** `engine=stello|stello-v1|egaroucid`, plus `time-ms`, `game-s`, `depth` (Stello) or `level` (Egaroucid), `hash-bits`, `book=on|off`, and for Egaroucid `book-acc` and `seed`. Chapter 19 [explains Egaroucid's keys](19-the-egaroucid-port.md#in-the-benchmark-tool).
- **`--engine`** chooses the engine for `solve-bench` and `mid-bench`; `mid-quality` takes a list in `--engines`.
- **`eval-compare`** scores Stello 1's evaluation, which Stello 1 keeps in `Stello.Engine.V1`. Stello 2's trained evaluation is measured with the trainer's `report` ([chapter 21](21-stello-2-evaluation-and-solver.md#training)).
- **The training data is not in the repository** (its terms forbid redistribution), so `eval-compare` needs a local copy. `make-exact`'s output is committed, so `mid-quality` runs without it.
- `match`, `eval-compare` and `mid-quality` write their reports to `Plans/benchmark-results`. A long run should have the machine to itself.

## What this means for Stello 2.0

The comparison set the order of the work in [improve-stello-strength-plan.md](../../Plans/improve-stello-strength-plan.md):

1. **A new evaluation first,** trained on Egaroucid's training data, in discs and with game phases, small enough for the web (about 1 MB). It is the step that makes extra thinking time worth something. Trained weights may be shipped; the data itself may not.
2. **Then Egaroucid's endgame solver,** adapted as a replacement for Stello's. The last 13 empty squares need no midgame evaluation; above that, the solver is rebuilt on Stello's search, ordered by the new evaluation.
3. **Then the search:** time use, PVS and a better transposition table, Multi-ProbCut with an error model fitted to the new evaluation, move ordering by a shallow search, a selective endgame, and checking the moves outside the book.

Each step is measured against Stello 1 (`engine=stello-v1`) and Egaroucid (`engine=egaroucid`) with the tools in this chapter. Steps 1 and 2 are done: [chapter 21](21-stello-2-evaluation-and-solver.md) describes them and their results.

## Where in the code

| File | Main members |
|---|---|
| [MatchCommands.cs](../../Stello.Net/tools/Stello.Benchmark/MatchCommands.cs) | `ParseSide` (the `engine=` key and the other side keys), `LoadEgaroucid` |
| [EgaroucidPlayer.cs](../../Stello.Net/tools/Stello.Benchmark/EgaroucidPlayer.cs) | the port as a match player (`IMatchPlayer`) |
| [V1MatchPlayer.cs](../../Stello.Net/Stello.Engine.V1/V1MatchPlayer.cs) | Stello 1 with its book as a match player |
| [BenchSearcher.cs](../../Stello.Net/tools/Stello.Benchmark/BenchSearcher.cs) | `Create`: the engine of `solve-bench` and `mid-bench` |
| [EvalCommands.cs](../../Stello.Net/tools/Stello.Benchmark/EvalCommands.cs) | `EvalCompare` |
| [QualityCommands.cs](../../Stello.Net/tools/Stello.Benchmark/QualityCommands.cs) | `MakeExact`, `MidQuality` |

---

Previous: [19 The Egaroucid port](19-the-egaroucid-port.md) · Next: [21 Stello 2: the pattern evaluation and the new endgame solver](21-stello-2-evaluation-and-solver.md)
