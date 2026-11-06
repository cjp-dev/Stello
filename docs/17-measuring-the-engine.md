# 17 – Measuring the engine

[Back to the index](README.md)

## In short

Before the engine is changed, there must be a way to tell whether a change helps. The console tool [Stello.Benchmark](../Stello.Net/tools/Stello.Benchmark) measures the engine in three ways:

- **`match`** plays two engine settings against each other from 590 balanced start positions, two games per position with the colours swapped. This measures **strength**.
- **`solve-bench`** solves FFO endgame positions with known scores. This measures the **endgame solver**: correct scores, nodes and time to solve.
- **`mid-bench`** searches 100 midgame positions to a fixed depth. This measures **midgame speed** and records the nodes, move and score of every search, so a later run can check that a change kept the search **unchanged**.

The tool plays three engines: Stello 2.0, the frozen Stello 1 and the Egaroucid port ([Comparing engines](#comparing-engines)). Every step from Stello 1 to Stello 2.0 was measured with it, and every result file of that work is in [Plans/benchmark-results](../Plans/benchmark-results) ([The result files](#the-result-files)). The numbers at the end of this chapter are [the baseline before 2.0](#the-baseline-before-20): the engine as it was ported from C++, now frozen as Stello 1. What Stello 2.0 measures is told phase by phase in chapters [20](20-phases-0-2-the-evaluation.md) to [24](24-results-and-what-comes-next.md).

## Running the tool

```text
dotnet run -c Release --project Stello.Net/tools/Stello.Benchmark -- <command> [options]
```

| Command | What it does |
|---|---|
| `match --a <side> --b <side> [--starts <file>] [--max-starts N] [--workers N] [--name <name>]` | Plays side A against side B ([below](#the-engine-match-match)). |
| `solve-bench [--engine stello\|stello-v1\|egaroucid] [--eval <file>] [--positions <file>] [--from 40] [--to 44] [--passes 1] [--hash-bits N] [--out <file.json>] [--compare <file.json>]` | Solves FFO positions #40 to #44 (or another range of #40–#59) exactly ([below](#the-endgame-benchmark-solve-bench)). |
| `mid-bench [--engine stello\|stello-v1\|egaroucid] [--eval <file>] [--side <keys>] [--positions <file>] [--depth 10] [--mpc 100 \| --level L] [--passes 3] [--hash-bits N] [--out <file.json>] [--compare <file.json>]` | Searches the midgame positions to a fixed depth ([below](#the-midgame-benchmark-mid-bench)). |
| `eval-compare [--data <folder>] [--files N] [--per-empties 5000] [--name <name>]` | Scores Stello 1's and Egaroucid's evaluations against the final scores of Egaroucid's training games ([Comparing engines](#comparing-engines)). |
| `make-exact [--starts <file>] [--from 20] [--games 100] [--depth 6] [--empties 28,26,24] [--workers N] [--out <file>]` | Writes midgame positions with every move solved exactly by the Egaroucid port. |
| `mid-quality [--positions <file>] [--times 50,200,1000] [--engines stello,stello-v1,egaroucid \| --sides "<side>;<side>"] [--workers N] [--max-positions N] [--name <name>]` | The discs each engine's move loses against perfect play, at a fixed time per move. |
| `make-starts [--book <book.txt>] [--plies 10-12] [--max-value 40] [--out <file>]` | Writes the start positions for `match` from the opening book. |
| `make-positions [--starts <file>] [--games 20] [--depth 6] [--empties 48,42,36,30,26] [--out <file>]` | Writes the midgame positions for `mid-bench`. |

Without options a command uses the data files in [tools/Stello.Benchmark/Data](../Stello.Net/tools/Stello.Benchmark/Data), which are copied next to the tool when it is built, together with both opening books and Stello 2.0's evaluation file from `Stello.Net/Book`. The data files are in git, so every run uses the same positions; `make-starts`, `make-positions` and `make-exact` are only needed to make new ones. Ctrl+C stops a `match` cleanly and reports the games played so far.

## The engine match: `match`

### Sides

A side is a short list of `key=value` settings, separated by commas. It needs exactly one of `time-ms`, `game-s` and `depth` (or `level`):

| Key | Meaning | Engines |
|---|---|---|
| `engine=stello-v1` | the engine: `stello` (Stello 2.0, the default), `stello-v1` (Stello 1) or `egaroucid` ([below](#comparing-engines)) | all |
| `time-ms=100` | 100 ms per move (`TimePerMove`) | all |
| `game-s=60` | 60 s for the whole game (`TimePerGame`); the side's clock runs down during the game, as in the app | all |
| `depth=8` | 8 plies per move (`FixedDepth`) | Stello 2.0, Stello 1 |
| `level=21` | an Egaroucid level from 1 to 60 instead of a depth ([chapter 18](18-the-egaroucid-port.md#levels)) | Egaroucid |
| `hash-bits=19` | the size of each hash table: 2<sup>19</sup> slots (the app's default, 10–26). Egaroucid's table has 2<sup>21</sup> slots by default, the same memory, and at most 2<sup>25</sup> | all |
| `book=on` | use the opening book (off by default): Stello 2.0's [opening-book.txt](../Stello.Net/Book/opening-book.txt) ([chapter 10](10-opening-book.md)), Stello 1's [opening-book-stello1.txt](../Stello.Net/Book/opening-book-stello1.txt) or Egaroucid's `book.egbk3` | all |
| `book-file=<file>` | another text book; turns the book on | Stello 2.0, Stello 1 |
| `book-acc=0` | Egaroucid's book accuracy; 0 plays one of the best book moves | Egaroucid |
| `seed=1` | seeds Egaroucid's choice among equal book moves, anew every game | Egaroucid |
| `eval=<file>` | another pattern evaluation file; `default` is the built-in one ([chapter 04](04-evaluation.md#the-file)) | Stello 2.0 |
| `soft-pct=80` | the soft time limit in per cent of the hard limit: no new iteration after it (default two thirds, [chapter 09](09-time-control.md#how-the-search-uses-the-limits)) | Stello 2.0 |
| `mpc=100` | Multi-ProbCut's t × 100: a cut needs the shallow search to be t standard deviations outside the window (default 100, [chapter 06](06-midgame-search.md#selective-search)) | Stello 2.0 |
| `name=new` | the name in the report and in the file name | all |

Since Stello 3.0, `engine=stello` is the Rust engine and `engine=stello-v2` the frozen Stello 2.0 ([chapter 25](25-phases-0-4-the-engine-in-rust.md)). The Rust engine also accepts `threads=N` (1–256, default 1) and `ybwc=on|off` (default on); with threads, keep `--workers` × threads within the cores ([chapter 26](26-phases-5-9-threads.md#threads-through-every-layer)). `mid-bench` and `solve-bench` take `--threads N` too.

For example `--a time-ms=100,name=old --b time-ms=200,name=new`. Neither side uses the opening book unless it has `book=on`, so the games test the search and the evaluation. A key that does not belong to the side's engine is an error.

The keys `eval`, `soft-pct` and `mpc` are what is left of the switches of the 2.0 work. Each change was tried behind a switch and a key, and the key went once the change was kept or dropped: `eval=classic`, `danger=` and `endgame=` of phases 2 and 3 (the old evaluation and solver are only in Stello 1 now, so play `engine=stello-v1`), and `sel-margin=`, `sel-percent=`, `order=`, `killers=`, `etc=`, `end-dist=` and the other switches of phase 5 ([chapter 23](23-phase-5-the-search.md)). Their result files keep the keys in their names.

### Start positions

[starts.txt](../Stello.Net/tools/Stello.Benchmark/Data/starts.txt) holds 590 positions from the master opening book: every book position 10 to 12 plies from the start whose best book move has a value of at most 40 evaluation units either way (Stello 1's units: the file was made from the book before 2.0). The book's values come from deep searches, so these are openings that real games reach and that neither side has clearly won. They are in a fixed shuffled order, so the first N positions (`--max-starts N`) mix the plies and the openings.

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

[ffo-40-59.txt](../Stello.Net/tools/Stello.Benchmark/Data/ffo-40-59.txt) holds the FFO endgame positions #40 to #59 (20 to 34 empty squares) with the exact score of the best moves, from [the FFO test suite page](http://radagast.se/othello/ffotest.html) and Edax's `problem/fforum-40-59.obf`. `solve-bench` solves each position with `SearchLimits.Solve` and empty hash tables, and checks the score and the move. With `--passes N` each position is solved N times, one round over all positions after the other, and the fastest time counts.

By default it solves #40–#44. `--to 49` adds five harder positions. Stello 2.0 solves #40–#49 in about 47 s ([chapter 08](08-endgame-solver.md#the-ffo-positions)); Stello 1 needed about 6 minutes ([the baseline](#endgame-solve-bench---to-49)). `--positions` reads another file in the same format.

## The midgame benchmark: `mid-bench`

[midgame.txt](../Stello.Net/tools/Stello.Benchmark/Data/midgame.txt) holds 100 midgame positions: the engine played itself from the first 20 start positions at depth 6, and a position was taken when 48, 42, 36, 30 and 26 squares were empty. `mid-bench` searches each to a fixed depth (10 by default; Stello 1 needs about 7 s for all 100) with empty hash tables, three interleaved passes, and reports the nodes, the fastest time and the nodes per second, by number of empty squares.

- **`--side <keys>`** gives the Rust Stello search settings of a match side, for example `--side mpc=130`, to count the nodes of a setting at a fixed depth.
- **`--eval <file>`** searches with another Stello 2.0 evaluation file.
- **`--mpc` and `--level`** are for Egaroucid only: `--mpc 74` … `--mpc 100` sets the selectivity of its fixed-depth search (100, no selectivity, is the default), and `--level L` searches at an Egaroucid level instead of a depth.

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

Since the Egaroucid port ([chapter 18](18-the-egaroucid-port.md)), the tool can measure three engines. A match side chooses one with the key `engine=`, `solve-bench` and `mid-bench` with `--engine`, and `mid-quality` takes a list in `--engines`:

| Value | Engine |
|---|---|
| `stello` (the default) | Stello 2.0, the engine in `Stello.Engine` that the apps use ([chapter 01](01-overview.md)) |
| `stello-v1` | Stello 1: the engine before 2.0, frozen in `Stello.Engine.V1` as the baseline ([Stello 1 documentation](stello-1/README.md)) |
| `egaroucid` | the C# port of Egaroucid in `Egaroucid.Engine`; it needs Egaroucid's resources ([chapter 18](18-the-egaroucid-port.md#running-it)) |

Three commands compare the engines part by part:

| Command | What it does |
|---|---|
| `eval-compare` | Scores Stello 1's and Egaroucid's evaluations against the final scores of Egaroucid's training games. The data is not in the repository (its terms forbid redistribution); by default it is read from `Traning data/Egaroucid_Train_Data/0001_egaroucid_7_5_1_lv17`. Stello 2.0's evaluation is measured with the trainer's `report` ([below](#the-trainers-measurements)). |
| `make-exact` | Writes midgame positions from Stello's games with every move solved exactly by the port, to `Data/midgame-exact.txt`. An existing file is continued. |
| `mid-quality` | Measures the discs each engine's move loses against perfect play on those positions, at a fixed time per move, with paired differences for every pair of engines. `--sides "<side>;<side>"` takes match sides instead of engine names, for example with `eval=` or `mpc=`. |

The commands used most in the 2.0 work:

| What | Command |
|---|---|
| Stello 2.0 against Stello 1, 100 ms | `match --a engine=stello-v1,time-ms=100,name=stello1 --b time-ms=100,name=stello2` |
| Stello 2.0 against Egaroucid, 500 ms | `match --a engine=egaroucid,time-ms=500,name=egaroucid --b time-ms=500,name=stello2` |
| The same with each engine's own book | add `book=on` to both sides |
| A new evaluation file against the built-in one | `match --a time-ms=100,name=default --b time-ms=100,eval=<file>,name=new` |
| A search setting against the default | `match --a time-ms=100,name=default --b time-ms=100,mpc=130,name=mpc130` |
| The endgame | `solve-bench --to 49`, `solve-bench --engine stello-v1 --to 49`, `solve-bench --engine egaroucid --to 49` |
| Midgame speed | `mid-bench`, `mid-bench --engine stello-v1`, `mid-bench --engine egaroucid --mpc 74`; add `--eval <file>` or `--side <keys>` for another Stello 2.0 setting |
| Midgame quality | `mid-quality --engines stello,stello-v1,egaroucid`, or `--sides "<side>;<side>"` |

[Chapter 19](19-stello-1-against-egaroucid.md) gives the results for Stello 1 against Egaroucid and its exact commands. Each step of the 2.0 work had to pass such a match before it was kept ([chapter 16](16-the-goal-and-the-plan.md)); the results are in chapters [20](20-phases-0-2-the-evaluation.md) to [24](24-results-and-what-comes-next.md) and in [improve-stello-strength-findings.md](../Plans/improve-stello-strength-findings.md).

## The trainer's measurements

The evaluation trainer [Stello.Trainer](../Stello.Net/tools/Stello.Trainer) ([chapter 20](20-phases-0-2-the-evaluation.md)) has three commands that measure rather than train:

```text
dotnet run -c Release --project Stello.Net/tools/Stello.Trainer -- <command> [options]
```

| Command | What it does |
|---|---|
| `report --model <file> [--data <folder>] [--max-per-discs N] [--name <name>]` | The error of an evaluation file on the test split of the training data, per number of empty squares, next to Stello 1's evaluation (with a fitted scale) and the Egaroucid port. For example `report --model Stello.Net/Book/stello-eval.bin`. |
| `mpc [--from 14] [--to 52] [--per-discs 100] [--max-depth 12] [--workers N] [--out <file.csv>]` | Searches test positions without selectivity at every depth and fits Multi-ProbCut's error model from the spread of deep against shallow results ([chapter 06](06-midgame-search.md#selective-search)). `mpc --fit <file.csv>` only fits a saved file. |
| `mpc-end [--from 38] [--to 50] [--per-discs 100] [--max-depth 10] [--solve-s 60] [--workers N] [--out <file.csv>]` | The same against exact scores, for a selective endgame. It was tried in phase 5 and dropped ([chapter 23](23-phase-5-the-search.md)). |

All three need the converted training data in `Traning data/stello-bin`, which is not in the repository.

## The result files

`match`, `eval-compare`, `mid-quality` and the trainer's `report` write a Markdown report and a JSON file with the same name to [Plans/benchmark-results](../Plans/benchmark-results):

| Command | File name |
|---|---|
| `match` | `match_<name>`, where `<name>` is `--name` or both side names joined by `_vs_` |
| `eval-compare` | `eval-compare`, or `--name` |
| `mid-quality` | `mid-quality`, or `--name` |
| `report` (trainer) | `eval-<model file name>`, or `--name` |

`solve-bench` and `mid-bench` print their tables and write a JSON file only with `--out`; `--compare` reads one. The trainer's `mpc` and `mpc-end` write their raw values to `Traning data/mpc-values.csv` and `mpc-end-values.csv`. `make-starts`, `make-positions` and `make-exact` write the data files.

The results of the 2.0 work are named by phase: `v2-p<phase>-*` and `match_v2-p<phase>-*` for phases 0 to 4; `match_v2x-*` for phase 5 and the second book recalculation, and `grid-*` for the trainer reports of the larger evaluation that was not pursued (these names are kept as they were made, because other documents link to them); `match_v2-final-*` and `v2-final-*` for the final measurements of [chapter 24](24-results-and-what-comes-next.md). The files without a prefix (`baseline-*`, `match_100ms_vs_200ms`, `match_stello-*`, `eval-compare`, `mid-quality`, `mid-bench_*`, `solve-bench_*`) are the measurements of Stello 1 below and in [chapter 19](19-stello-1-against-egaroucid.md).

## The baseline before 2.0

These are the numbers the 2.0 work started from. They were measured on 29 October 2026 with the engine as ported from C++ (now Stello 1, `engine=stello-v1`), Release build, on a 12th Gen Intel Core i7-12850HX (24 logical processors; Linux VM). `Stello.Engine.V1` is frozen, so Stello 1 still visits the same nodes and plays the same moves today; only the times depend on the machine.

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
- Stello 2.0 was measured the same way at the end of phase 5: twice the time is worth +84 Elo at 100 ms and +38 Elo at 500 ms ([100 against 200 ms](../Plans/benchmark-results/match_v2x-timeodds-100-vs-200ms.md), [500 against 1,000 ms](../Plans/benchmark-results/match_v2x-timeodds-500-vs-1000ms.md), [chapter 23](23-phase-5-the-search.md)).

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

All scores and moves are correct. #40–#44 take 19.2 s, with about the node counts of [Stello 1's solver chapter](stello-1/08-endgame-solver.md#worked-example-the-ffo-positions). #45–#49 take 5 minutes, mostly #48 (3.5 billion nodes). Saved as `Plans/benchmark-results/baseline-solve-bench.json`.

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

The midgame search visits about **3.3 million nodes per second**, the solver about **20 million**. Most midgame nodes are leaves, which are evaluated, and Stello 1's evaluation (the edge tables, the two-ply edge search, corner stability and mobility, [Stello 1 documentation](stello-1/04-evaluation.md)) costs much more than the solver's work per node.

**Timing noise.** Times on this machine vary by up to about 8 % between runs (a single pass against the best of three in the example above), so a speed change of a few per cent needs several passes, and the same number of passes on both sides.

## Where in the code

| File | Main members |
|---|---|
| [Program.cs](../Stello.Net/tools/Stello.Benchmark/Program.cs) | `Main`, `DataFile`, `RepositoryFolder`, `Cancellable` |
| [MatchCommands.cs](../Stello.Net/tools/Stello.Benchmark/MatchCommands.cs) | `Match`, `MakeStarts`, `ParseSide` |
| [MatchReport.cs](../Stello.Net/tools/Stello.Benchmark/MatchReport.cs) | `MatchReport.Of`, `Markdown`, `EloOf`, `SideSummary` |
| [BenchCommands.cs](../Stello.Net/tools/Stello.Benchmark/BenchCommands.cs) | `SolveBench`, `MidBench`, `MakePositions`, `Run` (the interleaved passes), `Compare` |
| [BenchSearcher.cs](../Stello.Net/tools/Stello.Benchmark/BenchSearcher.cs) | `Create`: the engine of `solve-bench` and `mid-bench` (`--engine`, `--eval`, `--side`, `--mpc`, `--level`) |
| [EvalCommands.cs](../Stello.Net/tools/Stello.Benchmark/EvalCommands.cs) | `EvalCompare` |
| [QualityCommands.cs](../Stello.Net/tools/Stello.Benchmark/QualityCommands.cs) | `MakeExact`, `MidQuality` |
| [EgaroucidPlayer.cs](../Stello.Net/tools/Stello.Benchmark/EgaroucidPlayer.cs) | the Egaroucid port as a match player |
| [V1MatchPlayer.cs](../Stello.Net/Stello.Engine.V1/V1MatchPlayer.cs) | Stello 1 with its book as a match player |
| [BookMatch.cs](../Stello.Net/Stello.Engine/BookMatch.cs) | `EngineMatch.Play` (the games), `MatchSide`, `SideStats`, `MatchSummary.Of` |
| [BenchmarkPosition.cs](../Stello.Net/Stello.Engine/Benchmark/BenchmarkPosition.cs) | `Read`, `Parse`, `Format` (the position files) |
| [ReportCommand.cs](../Stello.Net/tools/Stello.Trainer/ReportCommand.cs), [MpcCommand.cs](../Stello.Net/tools/Stello.Trainer/MpcCommand.cs) | the trainer's `report`, `mpc` and `mpc-end` |

## Tests

[BenchmarkTests.cs](../Stello.Net/Stello.Engine.Tests/BenchmarkTests.cs) checks the position format, that the three data files are valid (FFO #40–#59 with their scores and legal best moves, 100 midgame positions with at least two moves, distinct legal start lines), that an engine match plays its pairs and records each side's depth and nodes, that a side with a game clock stays within its time, and which moves count as searched.

---

Previous: [16 The goal and the plan](16-the-goal-and-the-plan.md) · Next: [18 The Egaroucid port](18-the-egaroucid-port.md)
