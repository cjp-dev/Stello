# 18 – Measuring the engine

[Back to the index](README.md)

## In short

Before the engine is changed, there must be a way to tell whether a change helps. The console tool [Stello.Benchmark](../../Stello.Net/tools/Stello.Benchmark) measures the engine in three ways:

- **`match`** plays two engine settings against each other from 590 balanced start positions, two games per position with the colours swapped. This measures **strength**.
- **`solve-bench`** solves FFO endgame positions with known scores. This measures the **endgame solver**: correct scores, nodes and time to solve.
- **`mid-bench`** searches 100 midgame positions to a fixed depth. This measures **midgame speed** and records the nodes, move and score of every search, so a later run can check that a change kept the search **unchanged**.

The numbers at the end of this chapter are the baseline: the engine as it was ported from C++.

## Running the tool

```text
dotnet run -c Release --project Stello.Net/tools/Stello.Benchmark -- <command> [options]
```

| Command | What it does |
|---|---|
| `match --a <side> --b <side> [--starts <file>] [--max-starts N] [--workers N] [--name <name>]` | Plays side A against side B. Writes a Markdown and a JSON report to `Plans/benchmark-results/match_<name>`. |
| `solve-bench [--from 40] [--to 44] [--passes 1] [--hash-bits 19] [--out <file.json>] [--compare <file.json>]` | Solves FFO positions #40 to #44 (or another range of #40–#59) exactly. |
| `mid-bench [--depth 10] [--passes 3] [--hash-bits 19] [--out <file.json>] [--compare <file.json>]` | Searches the midgame positions to a fixed depth. |
| `make-starts [--plies 10-12] [--max-value 40]` | Writes the start positions for `match` from the opening book. |
| `make-positions [--games 20] [--depth 6] [--empties 48,42,36,30,26]` | Writes the midgame positions for `mid-bench`. |

The data files are in [tools/Stello.Benchmark/Data](../../Stello.Net/tools/Stello.Benchmark/Data) and are copied next to the tool when it is built. They are in git, so every run uses the same positions; `make-starts` and `make-positions` are only needed to make new ones.

## The engine match: `match`

### Sides

A side is a short list of settings:

| Key | Meaning |
|---|---|
| `time-ms=100` | 100 ms per move (`TimePerMove`) |
| `game-s=60` | 60 s for the whole game (`TimePerGame`); the side's clock runs down during the game, as in the app |
| `depth=8` | 8 plies per move (`FixedDepth`) |
| `hash-bits=19` | the size of each hash table: 2<sup>19</sup> slots (the app's default) |
| `name=new` | the name in the report |

For example `--a time-ms=100,name=old --b time-ms=200,name=new`. Neither side uses the opening book unless it has `book=on`, so the games test the search and the evaluation. When later steps add settings to the engine (another evaluation, threads), they become new keys.

### Start positions

[starts.txt](../../Stello.Net/tools/Stello.Benchmark/Data/starts.txt) holds 590 positions from the master opening book: every book position 10 to 12 plies from the start whose best book move has a value of at most 40 evaluation units either way. The book's values come from deep searches, so these are openings that real games reach and that neither side has clearly won. They are in a fixed shuffled order, so the first N positions (`--max-starts N`) mix the plies and the openings.

From each start position a **pair** of games is played: first side A plays the side to move, then side B. A start position that favours one colour is played once with each colour, so its effect cancels out. Every game starts with empty hash tables and fresh book trackers.

### The result

The report gives, from side B's point of view:

- the **points** (win 1, draw ½) and the **score**, B's share of the points, with a 95 % interval over the pairs: $\bar{s} \pm 1.96\, \sigma / \sqrt{n}$, where $s$ is a pair's share of its two points and $n$ the number of pairs;
- the **Elo difference** that belongs to the score, $\Delta = -400 \log_{10}(1/s - 1)$, for the score and both ends of the interval;
- the **likelihood of superiority**: the chance that B is really stronger, $\Phi\big((\bar{s} - 0.5) / (\sigma/\sqrt{n})\big)$;
- the **disc difference** per game with its 95 % interval. A change that helps usually moves the disc difference before it shows in the score, so it is a more sensitive sign, but the score is what counts;
- for each side: the average midgame depth, the number of empty squares at which it first solved a game, the nodes per second and the thinking time per game.

B is stronger if the whole interval of the score is above 50 %.

Othello games are seldom drawn, so the score varies more from game to game than in draughts or chess. With 1 180 games (all 590 pairs) the 95 % interval is about ±2.5 % of score, roughly ±19 Elo (see the baseline below).

### Timing

With `time-ms` or `game-s` the result depends on the machine: `match` plays as many games at a time as there are physical cores (half the logical processors, `--workers` changes it), and both sides of a game run on the same thread, so they share any slowdown. Heavy work on the machine during a match still makes the result less exact. `depth` gives the same games on every machine.

## The endgame benchmark: `solve-bench`

[ffo-40-59.txt](../../Stello.Net/tools/Stello.Benchmark/Data/ffo-40-59.txt) holds the FFO endgame positions #40 to #59 (20 to 34 empty squares) with the exact score of the best moves, from [the FFO test suite page](http://radagast.se/othello/ffotest.html) and Edax's `problem/fforum-40-59.obf`. `solve-bench` solves each position with `SearchLimits.Solve` and empty hash tables, and checks the score and the move. With `--passes N` each position is solved N times, one round over all positions after the other, and the fastest time counts.

By default it solves #40–#44, the five positions of [chapter 08](08-endgame-solver.md#worked-example-the-ffo-positions), which takes about 20 s. `--to 49` adds five harder positions (about 6 minutes).

## The midgame benchmark: `mid-bench`

[midgame.txt](../../Stello.Net/tools/Stello.Benchmark/Data/midgame.txt) holds 100 midgame positions: the engine played itself from the first 20 start positions at depth 6, and a position was taken when 48, 42, 36, 30 and 26 squares were empty. `mid-bench` searches each to a fixed depth (10 by default, about 8 s for all 100) with empty hash tables, three interleaved passes, and reports the nodes, the fastest time and the nodes per second, by number of empty squares.

### Checking that a change kept the search unchanged

The search is deterministic: the same position, depth and hash size always visit the same nodes. A change that should only make the engine faster (a faster move generator, a lock-free hash table, a search that can run on several threads but runs on one) must therefore give exactly the same nodes, move and score in every search:

```text
Stello.Benchmark mid-bench --out before.json          (with the old code)
Stello.Benchmark mid-bench --compare before.json      (with the new code)
  identical: all 100 searches visited the same nodes and found the same move and score.
  time 7.762 s against 7.143 s (-8.0 % speed).
```

`solve-bench` has the same `--out` and `--compare`. A change that is meant to play differently (a new evaluation) will differ, and must instead win a `match`.

## Comparing engines

Since the Egaroucid port ([chapter 19](19-the-egaroucid-port.md)), the tool can measure three engines. A match side chooses one with the key `engine=`, and `solve-bench` and `mid-bench` with `--engine`:

| Value | Engine |
|---|---|
| `stello` (the default) | the engine in `Stello.Engine`, which becomes Stello 2.0 |
| `stello-v1` | Stello 1: the engine before Stello 2.0, frozen in `Stello.Engine.V1` as the baseline |
| `egaroucid` | the C# port of Egaroucid in `Egaroucid.Engine`; it needs Egaroucid's resources |

For example `--a engine=stello-v1,time-ms=100 --b engine=egaroucid,time-ms=100`. An Egaroucid side takes `level=` instead of `depth=`, and also `book-acc=` and `seed=`. `book=on` turns on the opening book of either engine.

Three commands compare the engines part by part:

| Command | What it does |
|---|---|
| `eval-compare [--data <folder>] [--per-empties 5000]` | Scores both evaluations against the final scores of Egaroucid's training games (not in the repository). |
| `make-exact [--games 100] [--empties 28,26,24]` | Writes midgame positions with every move solved exactly by the port, to `Data/midgame-exact.txt`. |
| `mid-quality [--times 50,200,1000] [--engines stello,stello-v1,egaroucid]` | Measures the discs each engine's move loses against perfect play, at a fixed time per move. |

[Chapter 20](20-engine-comparison.md) gives the results and the exact commands.

## Baseline

Measured on 29 October 2026 with the engine as ported from C++, Release build, on a 12th Gen Intel Core i7-12850HX (24 logical processors; Linux VM).

### Strength: what more time is worth

| Match | Games | Score of B | Elo of B | Discs per game | Midgame depth A / B | First solve A / B |
|---|---:|---:|---:|---:|---:|---:|
| A 100 ms/move, B 100 ms/move (check of the match itself) | 600 | 49.7 % (46.4–53.1 %) | −2 (−25 to +21) | 0.0 ± 1.0 | 10.47 / 10.47 | 18.7 / 18.7 |
| A 100 ms/move, B **200 ms/move** | 1 180 | **62.8 %** (60.2–65.3 %) | **+91** (+72 to +110) | +4.3 ± 0.7 | 10.45 / 11.35 | 18.6 / 19.5 |
| A 500 ms/move, B **1 000 ms/move** | 1 180 | **59.2 %** (56.7–61.6 %) | **+64** (+47 to +82) | +2.5 ± 0.6 | 12.39 / 13.14 | 20.4 / 21.1 |

- **The match is fair:** the same settings on both sides score 50 % within the interval.
- **Twice the time is worth about +90 Elo at 100 ms per move and about +65 Elo at 500 ms per move**: almost one ply deeper in the midgame (0.9 and 0.75 plies) and the solver taking over about one empty square earlier. So any change that makes the engine twice as fast (a faster evaluation, a faster solver, more threads) is worth roughly this much. The value falls with longer thinking times, as in other games, but slowly: five times more time still leaves two thirds of it.
- About 6 % of the games are drawn, and the 95 % interval of 1 180 games is about ±2.5 % of score (±19 Elo).
- The reports are in `Plans/benchmark-results/match_100ms_vs_100ms`, `match_100ms_vs_200ms` and `match_500ms_vs_1000ms` (35 minutes for 1 180 games).

### Endgame: `solve-bench --to 49`

| FFO # | Empty squares | Score | Move | Nodes | Time | Nodes/s |
|---|---:|---:|---|---:|---:|---:|
| 40 | 20 | +38 | a2 | 18,003,998 | 1.1 s | 16.4M |
| 41 | 22 | 0 | h4 | 43,973,573 | 2.3 s | 18.9M |
| 42 | 22 | +6 | g2 | 55,502,230 | 2.9 s | 18.9M |
| 43 | 23 | −12 | g3 | 168,848,898 | 10.2 s | 16.5M |
| 44 | 23 | −14 | d2 | 43,206,934 | 2.6 s | 16.5M |
| 45 | 24 | +6 | b2 | 921,423,413 | 50.4 s | 18.3M |
| 46 | 24 | −8 | b3 | 745,781,507 | 37.7 s | 19.8M |
| 47 | 25 | +4 | g2 | 152,401,503 | 8.4 s | 18.2M |
| 48 | 25 | +28 | f6 | 3,538,801,785 | 165.9 s | 21.3M |
| 49 | 26 | +16 | e1 | 1,720,545,278 | 87.6 s | 19.6M |
| **Total** | | | | **7,408,489,119** | **369.3 s** | **20.1M** |

All scores and moves are correct. #40–#44 take 19.2 s, with about the node counts of [chapter 08](08-endgame-solver.md#worked-example-the-ffo-positions). #45–#49 take 5 minutes, mostly #48 (3.5 billion nodes). Saved as `Plans/benchmark-results/baseline-solve-bench.json`.

### Midgame: `mid-bench` (depth 10)

| Empty squares | Positions | Nodes | Time | Nodes/s |
|---:|---:|---:|---:|---:|
| 48 | 20 | 4,483,921 | 1.29 s | 3.47M |
| 42 | 20 | 5,715,802 | 1.73 s | 3.30M |
| 36 | 20 | 5,925,517 | 1.81 s | 3.27M |
| 30 | 20 | 4,821,057 | 1.50 s | 3.22M |
| 26 | 20 | 2,557,162 | 0.81 s | 3.16M |
| **Total** | 100 | **23,503,459** | **7.14 s** | **3.29M** |

Saved as `Plans/benchmark-results/baseline-mid-bench.json`, for `--compare`.

The midgame search visits about **3.3 million nodes per second**, the solver about **20 million**. Most midgame nodes are leaves, which are evaluated, and the evaluation (the edge tables, the two-ply edge search, corner stability and mobility, [chapter 04](04-evaluation.md)) costs much more than the solver's work per node.

**Timing noise.** Times on this machine vary by up to about 8 % between runs (a single pass against the best of three in the example above), so a speed change of a few per cent needs several passes, and the same number of passes on both sides.

## Where in the code

| File | Main members |
|---|---|
| [Program.cs](../../Stello.Net/tools/Stello.Benchmark/Program.cs) | `Main`, `DataFile`, `RepositoryFolder`, `Cancellable` |
| [MatchCommands.cs](../../Stello.Net/tools/Stello.Benchmark/MatchCommands.cs) | `Match`, `MakeStarts`, `ParseSide` |
| [MatchReport.cs](../../Stello.Net/tools/Stello.Benchmark/MatchReport.cs) | `MatchReport.Of`, `Markdown`, `EloOf`, `SideSummary` |
| [BenchCommands.cs](../../Stello.Net/tools/Stello.Benchmark/BenchCommands.cs) | `SolveBench`, `MidBench`, `MakePositions`, `Run` (the interleaved passes), `Compare` |
| [BookMatch.cs](../../Stello.Net/Stello.Engine/BookMatch.cs) | `EngineMatch.Play` (the games), `MatchSide`, `SideStats`, `MatchSummary.Of` |
| [BenchmarkPosition.cs](../../Stello.Net/Stello.Engine/Benchmark/BenchmarkPosition.cs) | `Read`, `Parse`, `Format` (the position files) |

## Tests

[BenchmarkTests.cs](../../Stello.Net/Stello.Engine.Tests/BenchmarkTests.cs) checks the position format, that the three data files are valid (FFO #40–#59 with their scores and legal best moves, 100 midgame positions with at least two moves, distinct legal start lines), that an engine match plays its pairs and records each side's depth and nodes, that a side with a game clock stays within its time, and which moves count as searched.
