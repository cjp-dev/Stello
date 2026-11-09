# 21 – Phase 3: the endgame solver

[Back to the index](README.md)

## In short

Phase 3 replaced Stello 1's endgame solver with one adapted from Egaroucid (commit 6c335cb, 31 October 2026).

- **The last 13 empty squares** are Egaroucid's code, taken from the C# port and adapted to Stello's bitboards. Reversing the bits turns the board half a turn, under which Othello and all the solver's tables are the same, so nothing is converted.
- **Above 13 empty squares** a new null-window search was written on Stello's endgame transposition table, shaped like Egaroucid's midgame search and ordered by the new pattern evaluation of phase 2.
- **What made it fast** was, above all, the order of the root moves: in solve mode a 4-ply midgame search now orders them first. That alone halved the nodes on FFO #40–44.
- **The gate:** FFO #40–49 all correct in **47.1 s**, against 369 s for Stello 1 and 25.6 s for the port. Move Now answers within 100 ms during a solve. In games the new solver is as strong as the old one (51.1 % at 500 ms), because the search starts solving at almost the same point. In the browser, positions with 22 empty squares now solve in 0.6–1.3 s instead of 1.1–4.5 s.

How the solver works is described in [chapter 08](08-endgame-solver.md). This chapter tells how it was adapted and developed, and what was measured.

## Where it started

Stello 1's solver ([Stello 1's chapter 08](stello-1/08-endgame-solver.md)) was the C++ solver, ported:

- one routine, `Solve`, from 7 empty squares up: a move list, the endgame hash table, principal variation search, enhanced transposition cutoff from 10 empty squares, ordering by the evaluation from 18 empty squares and "fastest-first" (fewest replies for the opponent) below;
- `SolveShallow`, `SolveLast2` and `SolveLast` for the last 6 squares, with parity;
- a win/loss/draw pass, then the exact score.

On FFO #40–49, one thread ([Stello 1](../Plans/benchmark-results/baseline-solve-bench.json), [the port](../Plans/benchmark-results/solve-bench_egaroucid_ffo40-49.json)):

| Engine | Time | Nodes |
|---|---:|---:|
| Stello 1 | 369 s | 7.41 G |
| Egaroucid port | 25.6 s | 0.52 G |

The port is 14 times faster with 14 times fewer nodes: most of the gap is in what the solver searches, not in how fast it searches it. [Chapter 19](19-stello-1-against-egaroucid.md#endgame-solve-bench) found the gap mattered less in games than these numbers suggest, but much for analysis and solve mode.

## The plan

The plan ([improve-stello-strength-plan.md](../Plans/improve-stello-strength-plan.md), phase 3) answered "can Egaroucid's solver just replace Stello's?" with "mostly yes" ([chapter 16](16-the-goal-and-the-plan.md#can-egaroucids-solver-just-replace-stellos)):

- **New files in `Stello.Engine/Endgame/`, adapted from `Egaroucid.Engine`:** `Last1`–`Last4`, the fast search up to 6 squares, the search with a local table at 7–10 squares and the one at 11–13 squares; the stability cutoff, the local table, the parity tables and the flips. They use Stello's bitboards directly. At 11–13 squares the new evaluation orders the moves instead of Egaroucid's ordering table, unless a small trained table measurably helps.
- **Above 13 squares,** Stello's `Solve` is rebuilt as Egaroucid's null-window/PVS structure: transposition entries with bounds and best moves, ETC, ordering by mobility (corners counted double), potential mobility and a shallow search with the new evaluation. No selectivity yet.
- **The win/loss/draw pass first, or exact at once** with a window: `solve-bench` decides.
- **Cancellation:** the new code must keep checking the time and the tokens and sending progress reports. The web app's Stop and Move Now work only through those reports ([chapter 08](08-endgame-solver.md#stopping-progress-and-move-now)): every 1,024 nodes the solver calls `CheckAbort`, which sends a report if at least 100 ms have passed since the last one, and the Web Worker polls its stop signal on every report.

The gate: FFO #40–49 all correct in at most about 2 × the port's 25.6 s, with the port's scores; the endgame tests pass, including the minimax comparison and the endgame suite; Move Now during a solve answers within 100 ms; a 500 ms match against the old solver, both on the new evaluation, is not worse; the browser's solve time for 20–22 empty squares is noted.

## What was built

| Part | Empty squares | Where it comes from |
|---|---|---|
| `SolveRoot`, the win/loss/draw pass and the exact pass | the root | Stello's, kept |
| `EndWindow`: PVS with a window at principal-variation nodes | 7 and more | New, shaped like Egaroucid's midgame search |
| `EndDeep`: null-window search with the endgame table and ETC | 14 and more | New, shaped like Egaroucid's midgame search |
| Move ordering by mobility, potential mobility and the pattern evaluation after a small search | 7 and more (`EndWindow`), 14 and more (`EndDeep`) | Egaroucid's weights (`move_ordering.hpp`), Stello's evaluation |
| `EndLocal`, `EndSimple`, with the local table | 7 to 13 | Adapted from the port's `EndSearch.cs` |
| `EndFast`, `Last4` … `Last1` | 1 to 6 | Adapted from the port's `EndSearch.cs` |
| The stable-disc cutoff and the stable edges | all | Adapted from the port's `Stability.cs` |
| `SolveShallow`, `SolveLast2`, `SolveLast` for principal-variation nodes | 6 and fewer | Stello's, kept |
| The 4-ply ordering search in solve mode | the root | New |

The half-turn trick made the adaptation direct. Egaroucid's bitboards have a1 at bit 63, Stello's at bit 0; reversing the bits maps square $i$ to $63 - i$, a half turn of the board. Othello is the same after a half turn, and so is every table in the solver: the flip directions, the neighbour masks, the quadrants, the corners and the stable edges. So the adapted code runs on Stello's bitboards as they are, and the moves it returns are Stello's. [EndgameBits.cs](../Stello.Net/Stello.Engine.V2/Endgame/EndgameBits.cs) holds the adapted flips, legal moves and small tables, and a test checks them against Stello's own `Bitboards`.

The whole solver is described layer by layer in [chapter 08](08-endgame-solver.md#the-layers).

## What made it fast

`solve-bench`, FFO #40–44, all correct, on the way to the final version:

| Version | Nodes | Time |
|---|---:|---:|
| First working version | 151 M | 10.5 s |
| No pattern evaluation in the ordering at 11–13 empty squares | 179 M | 7.9 s |
| … and ETC from 12 instead of 16 empty squares | 173 M | 7.6 s |
| … and in solve mode a 4-ply midgame search first, to order the root moves | **94 M** | **4.1 s** |
| Egaroucid port, for comparison | 84 M | 4.0 s |

- **The root order matters most.** Without the short midgame search, FFO #43 took 94 M nodes instead of 33 M. With the best move first, the null-window searches of the other root moves only have to show that they are not better, which is much cheaper than finding a better move late. In games the midgame iterations run before the solver anyway, so this only changes solve mode (`SearchLimits.Solve`): midgame iterations 1 to 4 (`SolveOrderingLooks`) run first.
- **The evaluation is too slow for the cheap nodes.** Dropping it from the ordering at 11–13 empty squares costs 18 % more nodes and saves 25 % of the time.
- **ETC from 12 instead of 16 empty squares** saved a little more: 3 % of the nodes and 4 % of the time.
- After these three changes the solver needed 12 % more nodes than the port on FFO #40–44 and about the same time.

## What was tried and dropped

- **The pattern evaluation in the ordering at 11–13 empty squares.** It saves about 20 % of the nodes, but each node becomes about 40 % slower. Egaroucid uses a small 4-pattern table there, updated move by move; Stello orders by the opponent's mobility alone ([chapter 08](08-endgame-solver.md#7-to-13-empty-squares)).
- **The midgame table's best moves as hash moves in the solver,** where the endgame table has none. FFO #40–44 then took 6.9–10.6 s instead of 4.1 s. The endgame ordering is better than the choice of a shallow midgame search.
- **Other depths for the ordering search:** a 1-ply search from 14, 16, 18 or 20 empty squares changed the time by under 15 %. The final rule searches 1 ply from 16 empty squares in `EndDeep`, and 0–2 plies in `EndWindow` ([chapter 08](08-endgame-solver.md#move-ordering-above-13-empty-squares)).
- **Exact at once** with a full window, instead of the win/loss/draw pass first: 50.0 s against 45.6 s on FFO #40–49. The win/loss/draw pass also gives a playable result early if the time runs out. It stays.

## The switch

During phase 3 both solvers were in `Stello.Engine`, behind `SearchOptions.NewEndgame`:

- `endgame=new|classic` on a match side, `--endgame new|classic` in `solve-bench` and `mid-bench`;
- [EndgameTests.cs](../Stello.Net/Stello.Engine.Tests/EndgameTests.cs) ran on both solvers.

Phase 4 removed the old solver and the switch from `Stello.Engine` ([chapter 22](22-phase-4-the-release.md#cleaning-up)). Stello 1 keeps it: to play the old solver now, play `engine=stello-v1`.

## The gate

### FFO #40–49

`solve-bench --to 49`, exact, one thread, an empty table for each position ([Stello 2](../Plans/benchmark-results/v2-p3-solve-bench-40-49.json)):

| Position | Empty squares | Score | Stello 1 | Stello 2 | Egaroucid port |
|---|---:|---:|---:|---:|---:|
| #40 | 20 | +38 | 1.1 s | 0.7 s | 0.5 s |
| #41 | 22 | 0 | 2.3 s | 0.5 s | 0.5 s |
| #42 | 22 | +6 | 2.9 s | 0.8 s | 0.8 s |
| #43 | 23 | −12 | 10.2 s | 1.5 s | 1.9 s |
| #44 | 23 | −14 | 2.6 s | 0.6 s | 0.4 s |
| #45 | 24 | +6 | 50.4 s | 11.1 s | 6.3 s |
| #46 | 24 | −8 | 37.7 s | 2.2 s | 1.5 s |
| #47 | 25 | +4 | 8.4 s | 1.3 s | 0.4 s |
| #48 | 25 | +28 | 165.9 s | 12.0 s | 5.7 s |
| #49 | 26 | +16 | 87.6 s | 16.3 s | 7.6 s |
| **Total** | | | **369 s, 7.41 G nodes** | **47.1 s, 1.10 G nodes** | **25.6 s, 0.52 G nodes** |

- **All scores and moves are correct** for all three engines. (#43 has two best moves, c7 and g3.)
- **Stello 2 is 7.8 times faster than Stello 1** and 1.8 times slower than the port: inside the gate of about 2 times. It searches about 23 M nodes per second.
- **The slow ones are #45, #48 and #49.** There Stello 2 needs 1.9 to 3.0 times the port's nodes; on #40–44 it needs 10 % more. Where the difference comes from was not measured further; the ideas the findings left are [below](#ideas-left-for-later).
- **#48 shows the gain best:** Stello 1 needed 3.5 G nodes, Stello 2 0.24 G.

### The tests

[EndgameTests.cs](../Stello.Net/Stello.Engine.Tests/EndgameTests.cs) ran on both solvers:

- FFO #40–44;
- minimax on small endgames and on the last 1–4 squares;
- the 112 positions of the endgame.c suite.

New tests check that:

- the new solver agrees with Stello 1's on 30 positions with 14–18 empty squares, to test `EndDeep`, the endgame table and ETC;
- Move Now during a solve of FFO #49 (26 empty squares) returns a legal move within 100 ms;
- the adapted flips, legal moves and final score equal Stello's own, and stable discs are never flipped in random games ([EndgameBitsTests.cs](../Stello.Net/Stello.Engine.Tests/EndgameBitsTests.cs)).

Stello.Engine.Tests had 264 tests and Egaroucid.Engine.Tests 34, all green.

### In games

The new solver against Stello 1's, both with `stello-eval.bin` and the same midgame search, 1,180 games, no books:

| Time per move | Score | Elo | First solve (empty squares, old → new) | Report |
|---:|---:|---:|---:|---|
| 100 ms | 49.7 % (47.5–51.9 %) | −2 | 18.8 → 19.4 | [result](../Plans/benchmark-results/match_v2-p3-endgame-new-vs-classic-100ms.md) |
| 500 ms | 51.1 % (49.2–53.0 %) | +8 | 20.7 → 21.2 | [result](../Plans/benchmark-results/match_v2-p3-endgame-new-vs-classic-500ms.md) |

The gate passes: the 500 ms match is not worse. But **the solver is much faster and the games hardly change.** The search only starts solving when its next midgame iteration would end with 7 or fewer empty squares (`EndgameDistance`). That point depends on the midgame search, not on the solver, so the faster solver starts only about half a square earlier. By then the game is mostly decided. Egaroucid instead solves selectively from 24–28 empty squares, with Multi-ProbCut at 74 %, then 88 … 100 %. That became phase 5, item 5, where a selective pass and earlier or later solve starts were measured and gave nothing ([chapter 23](23-phase-5-the-search.md)).

The new solver matters most where the time is long: analysis, solve mode and longer time controls.

### In the browser

The gate asked for the browser's solve time. The apps got the new solver in phase 4, so it was measured there (commit 5414efb): eight positions from Stello 2's games at depth 6, with 20 and 22 empty squares, were opened in the web app (Blazor WebAssembly, AOT Release build, headless Chromium, 60 s per move, no book), and the computer played the move. The time to the move:

| | 20 empty squares | 22 empty squares |
|---|---:|---:|
| Stello 2, local AOT build | 0.1–0.3 s | 0.6–1.3 s (6.5–23 M nodes) |
| Stello 1, the live site | 0.3–0.7 s | 1.1–4.5 s (10–60 M nodes) |

- All scores are the same.
- In the browser Stello 2 runs at 8–22 M nodes per second, about native speed: the solver needs no BMI2 or AVX2 instructions. Most of the gain comes from needing fewer nodes.
- After the deploy, the live site solved a position with 22 empty squares in 0.59 s ([chapter 22](22-phase-4-the-release.md#the-deploy-and-the-live-check)).

## Ideas left for later

From phase 3, in the findings:

- **A small move-ordering table for 11–13 empty squares,** like Egaroucid's 4-pattern table updated move by move. The full pattern evaluation saves 20 % of the nodes there but is too slow.
- **When to start solving:** from an estimate of the solve time, instead of the fixed "the midgame iteration would end within 7 empty squares". Phase 5 measured earlier and later fixed starts, with no gain ([chapter 23](23-phase-5-the-search.md)).
- **A selective endgame,** as in Egaroucid: tried in phase 5 and dropped.

## Design notes

- **Adapted, not ported.** The last 13 squares are Egaroucid's code on Stello's bitboards. Above 13 squares the search is written on Stello's table and evaluation, because Egaroucid's version there is its whole midgame search with its 38 MB evaluation.
- **Stello's parts were kept where they work:** the win/loss/draw pass, the PVS at the root, the progress reports and the shallow solver for principal-variation nodes.
- **The ordering was tuned by nodes and time together.** The pattern evaluation at 11–13 empty squares and the midgame table's moves both save nodes or seem better, and both cost time. `solve-bench` measures the time.
- **Root order first.** The largest single gain came from outside the solver: a 4-ply midgame search before it.
- **Licence.** The adapted files carry `SPDX-License-Identifier: GPL-3.0-only` and name their Egaroucid originals (© Takuto Yamana, GPL-3.0-or-later).

## Where in the code

The C# search files of this chapter are kept, frozen, in `Stello.Engine.V2`. Since Stello 3.0 the same search runs in Rust ([chapter 25](25-phases-0-4-the-engine-in-rust.md)); Part 1 points to that code.

| File | Main members |
|---|---|
| [SearchEngine.Endgame.cs](../Stello.Net/Stello.Engine.V2/Endgame/SearchEngine.Endgame.cs) | `SolveChild`, `EndNull`, `EndWindow`, `EndDeep`, `OrderingValue`, `EndLocal`, `EndSimple`, `EndFast`, `Last4` … `Last1`, `StabilityCut` |
| [EndgameBits.cs](../Stello.Net/Stello.Engine.V2/Endgame/EndgameBits.cs) | `Flips`, `LegalMoves`, `LoneEmpties`, `Parity`, `WeightedMoves`, `PotentialMobility`, the stability thresholds |
| [StableDiscs.cs](../Stello.Net/Stello.Engine.V2/Endgame/StableDiscs.cs), [LocalTable.cs](../Stello.Net/Stello.Engine.V2/Endgame/LocalTable.cs) | The stable discs; the local table of 7–13 empty squares |
| [SearchEngine.cs](../Stello.Net/Stello.Engine.V2/SearchEngine.cs) | `Search` (the solve-mode ordering, the two passes), `SolveRoot`, `SolveShallow`, `CheckAbort` |
| Source: [Egaroucid.Engine/EndSearch.cs](../Stello.Net/Egaroucid.Engine/EndSearch.cs), [Stability.cs](../Stello.Net/Egaroucid.Engine/Stability.cs) | The port's endgame code that the last 13 squares were adapted from |
| [Stello.Engine.V1/SearchEngine.cs](../Stello.Net/Stello.Engine.V1/SearchEngine.cs) | Stello 1's solver, the one that was replaced |

## Further reading

- [Chapter 08](08-endgame-solver.md): how the solver works now.
- [Egaroucid on GitHub](https://github.com/Nyanyan/Egaroucid): `src/engine/endsearch_nws.hpp`, `endsearch_nws_last_generic.hpp`, `stability_cutoff.hpp`, `move_ordering.hpp`.
- [The FFO endgame test suite – radagast.se](http://radagast.se/othello/ffotest.html): the positions of the gate.
- [Enhanced Transposition Cutoff – Chess Programming Wiki](https://www.chessprogramming.org/Enhanced_Transposition_Cutoff) and [Null Window – Chess Programming Wiki](https://www.chessprogramming.org/Null_Window).
- [References](30-references.md).

---

Previous: [20 Phases 0–2: the evaluation](20-phases-0-2-the-evaluation.md) · Next: [22 Phase 4: the release](22-phase-4-the-release.md)
