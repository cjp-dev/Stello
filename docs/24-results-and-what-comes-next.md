# 24 – Results and what comes next

[Back to the index](README.md)

## In short

The final measurements of Stello 2.0 were made on 2 November 2026, after the last change (all 1,180-game matches on one thread per engine, from `starts.txt`):

- **Against Stello 1,** each with its own opening book, Stello 2.0 scores **96.4 %** at 100 ms per move (+571 Elo) and **94.7 %** at 500 ms (+501 Elo). Stello 1 wins 33 and 43 of the 1,180 games.
- **Against the Egaroucid port,** Stello 2.0 scores:
  - **75.6 %** (+196 Elo) without books at 100 ms;
  - **58.8 %** (+62 Elo) without books at 500 ms;
  - **54.9 %** (+34 Elo) with both books at 500 ms.

  At 2 s per move Egaroucid is stronger (−64 Elo, measured with the same search after phase 5 item 4). The engines are even at about 1 s per move.
- **The endgame solver** solves FFO #40–49 in **46.8 s**, against Stello 1's 369 s and the Egaroucid port's 25.6 s.

Stello 1 lost to the Egaroucid port by 331 Elo at 100 ms and 497 Elo at 500 ms ([chapter 19](19-stello-1-against-egaroucid.md)). Stello 2.0 wins against it at short time limits and loses only at long ones. What remains of the gap is mainly the evaluation: Egaroucid's is more accurate, and that pays off when both engines search deeply.

The next steps are open. The most promising is more threads (Lazy SMP), then perhaps a faster language for the engine's core.

## The final measurements

| Match | Stello 2.0 scores | Elo (95 %) | Result |
|---|---|---|---|
| Against Stello 1, both with their books, 100 ms | 96.4 % (95.4–97.4) | +571 (+527 to +628) | [result](../Plans/benchmark-results/match_v2-final-vs-v1-books-100ms.md) |
| Against Stello 1, both with their books, 500 ms | 94.7 % (93.5–95.9) | +501 (+464 to +546) | [result](../Plans/benchmark-results/match_v2-final-vs-v1-books-500ms.md) |
| Against Egaroucid, no books, 100 ms | 75.6 % (73.4–77.7) | +196 (+177 to +217) | [result](../Plans/benchmark-results/match_v2-final-vs-egaroucid-100ms.md) |
| Against Egaroucid, no books, 500 ms | 58.8 % (56.5–61.0) | +62 (+46 to +78) | [result](../Plans/benchmark-results/match_v2-final-vs-egaroucid-500ms.md) |
| Against Egaroucid, both with their books, 500 ms | 54.9 % (52.6–57.1) | +34 (+18 to +50) | [result](../Plans/benchmark-results/match_v2-final-vs-egaroucid-books-500ms.md) |
| Against Egaroucid, no books, 2 s (after phase 5 item 4) | 40.8 % (38.7–43.0) | −64 (−80 to −49) | [result](../Plans/benchmark-results/match_v2x-p5-item4-vs-egaroucid-2000ms.md) |

The 2 s match was not repeated. Items 5 and 6 of phase 5 were rejected and removed, so the search is the same as in that match. The only later changes are the evaluation's cap at ±64 discs and the new opening book, which no-book matches do not use.

The FFO benchmark is in [v2-final-solve-bench-40-49.json](../Plans/benchmark-results/v2-final-solve-bench-40-49.json): 1.10 G nodes, 46.8 s, all scores and moves correct. The table by position is in [chapter 08](08-endgame-solver.md#the-ffo-positions).

### What the engines did in the games

| Match | Midgame depth (Stello 2.0 / other) | First solve, empty squares | Nodes per second | Thinking time per game |
|---|---|---|---|---|
| Stello 1, 100 ms | 13.91 / 10.37 | 21.2 / 18.5 | 4.27 M / 2.75 M | 1.3 s / 1.4 s |
| Stello 1, 500 ms | 16.81 / 12.30 | 23.3 / 20.2 | 4.48 M / 2.74 M | 6.0 s / 6.5 s |
| Egaroucid, 100 ms | 13.81 / 8.46 | 20.6 / 17.9 | 3.92 M / 3.53 M | 1.4 s / 1.8 s |
| Egaroucid, 500 ms | 16.74 / 13.31 | 22.3 / 20.6 | 4.20 M / 4.03 M | 6.6 s / 7.2 s |
| Egaroucid, books, 500 ms | 16.75 / 13.54 | 22.2 / 20.5 | 4.47 M / 4.06 M | 6.1 s / 7.2 s |
| Egaroucid, 2 s | 19.24 / 21.45 | 24.0 / 22.9 | | |

Egaroucid's depth is its nominal search depth. Its selectivity is part of its levels, so the depths of the two engines are not directly comparable. Stello 2.0 reaches 3–4 plies deeper than Stello 1 in the same time. Against Egaroucid it is deeper at short time limits and starts solving 1.7–2.7 empty squares earlier. At 2 s Egaroucid's selective search runs deeper.

### From Stello 1 to Stello 2.0

| | Stello 1 | After phase 2 (evaluation) | Stello 2.0 |
|---|---|---|---|
| Evaluation error on the test split | 12.14 discs | 5.47 discs | 5.47 discs |
| FFO #40–49 | 369 s | 369 s | 46.8 s |
| Midgame depth at 500 ms (against Stello 1) | 12.3 | 13.6 | 16.8 |
| Against Egaroucid, 100 ms | 12.9 % (−331) | 59.5 % (+67) | 75.6 % (+196) |
| Against Egaroucid, 500 ms | 5.4 % (−497) | 38.2 % (−84) | 58.8 % (+62) |
| Against Stello 1 with books, 500 ms | — | 88.9 % (+362, first 2.0 release) | 94.7 % (+501) |

What each step brought, in Elo at 500 ms against Egaroucid: the evaluation about +410, the search of phase 5 about +150. The new solver plays about as well as the old one in games, so it hardly shows there; it matters for solving and for the opening book. The details are in chapters [20](20-phases-0-2-the-evaluation.md), [21](21-phase-3-the-endgame-solver.md), [22](22-phase-4-the-release.md) and [23](23-phase-5-the-search.md). The first 2.0 release (1 November 2026) had the new evaluation, solver and book. The final 2.0 (2 November 2026) added the phase 5 search and the book recalculated by it.

## What explains the remaining gap

- **The evaluation.** Stello's evaluation has a test error of 5.47 discs, Egaroucid's 5.05. A larger evaluation with Egaroucid's own pattern shapes and 60 stages reached 5.23 in Stello's trainer, but played no better, because it made the search 17 % slower ([chapter 20](20-phases-0-2-the-evaluation.md)). The rest of the gap must come from Egaroucid's other inputs and its training.
- **Time.** A better evaluation pays off with depth. Stello 2.0 wins at 100 and 500 ms, where its faster search reaches further, and loses at 2 s. Twice the time is worth +84 Elo at 100 ms and +38 at 500 ms ([chapter 23](23-phase-5-the-search.md)).
- **The solver.** Egaroucid's solver is still 1.8 times faster on FFO #40–49 (25.6 s against 46.8 s). Above 13 empty squares it orders the moves with its larger evaluation.
- **The language.** Native C++ is 1.5 times faster than C#, and 2.3 times faster with AVX2. In WebAssembly it is 1.9–2.7 times faster ([cpp-vs-csharp-speed.md](../Plans/cpp-vs-csharp-speed.md)). The Egaroucid port runs in C# too, so this does not explain the gap between the two engines. It does limit both against their C++ originals.

## What comes next

None of these is decided. They are listed roughly by value for the work.

**Since then:** the plan for Stello 3.0 took up items 2 and 1 in the other order. The core was rewritten in Rust first, node for node like 2.0, and is 2.8–3.1 times faster in the midgame and 1.75 times on FFO #40–49; it beats 2.0 with 59.6 % at 500 ms. Threads, Lazy SMP and YBWC come next. This is [Part 3, chapter 25](25-phases-0-4-the-engine-in-rust.md). The list below is kept as it was written for 2.0.

1. **More threads: Lazy SMP or YBWC** (phase 5 item 7, not started).
   - The desktop app runs on one core of a machine with many. With 4 threads at 1.5–3 times the speed, the time-odds matches suggest roughly +50 to +150 Elo.
   - Lazy SMP is the simpler method: the threads share the hash table and search the same position with small differences. It worked for the Checkers program in the same repository, including WebAssembly threads in the browser.
   - For the web app, .NET's WebAssembly threads are still experimental. The browser version could stay single-threaded at first.
   - The Egaroucid port's own parallel search (YBWC, its phase 6) was postponed and could be resumed for comparison.
2. **A faster engine core.** A rewrite of the core in Rust or C++ would make each thread 1.5–2.7 times faster, in the browser too. It is a large job: the whole engine would have to be validated again, node for node, against the C# engine. More threads give more for less work, so this comes second.
3. **The evaluation.**
   - Better training rather than a larger table. A larger evaluation did not help (chapter 20); a more accurate one of the same size might.
   - A faster evaluation, for example by updating the indices incrementally, would let a larger model pay for itself.
4. **The endgame.**
   - A small move-ordering table for 11–13 empty squares, like Egaroucid's 4-pattern table updated move by move. The full pattern evaluation saves 20 % of the nodes there but is too slow.
   - The solve start from an estimate of the solve time, then a selective solve as Egaroucid does. Both were tried in phase 5 with the fixed rule and gave nothing at 100–500 ms. They may help at longer time controls.
5. **The opening book.**
   - Search the moves the book does not have, in every book position, for 60 s each, and add the good ones. Checking them during the game, with the game's time, gave nothing ([chapter 23](23-phase-5-the-search.md)).
   - The five proven draws that version 4 turned into heuristic values are searched again by the next recalculation (a new `EngineVersion`), and should be solved again, as version 3 solved them. The rule that lost them is fixed: a recalculation now keeps a draw that Stello solved.
6. **The apps.** The computer-against-computer game mode (`GameMode.ComputerVsComputer`) exists in the code but cannot be chosen from any menu.

## How to repeat the final measurements

From `Stello.Net`, with `EGAROUCID_RESOURCES` pointing to Egaroucid's resources ([chapter 17](17-measuring-the-engine.md)):

```text
B="dotnet tools/Stello.Benchmark/bin/Release/net10.0/Stello.Benchmark.dll"
$B match --a engine=stello-v1,time-ms=100,book=on --b time-ms=100,book=on --name v2-final-vs-v1-books-100ms
$B match --a engine=stello-v1,time-ms=500,book=on --b time-ms=500,book=on --name v2-final-vs-v1-books-500ms
$B match --a engine=egaroucid,time-ms=100 --b time-ms=100 --name v2-final-vs-egaroucid-100ms
$B match --a engine=egaroucid,time-ms=500 --b time-ms=500 --name v2-final-vs-egaroucid-500ms
$B match --a engine=egaroucid,time-ms=500,book=on --b time-ms=500,book=on --name v2-final-vs-egaroucid-books-500ms
$B solve-bench --from 40 --to 49 --out ../Plans/benchmark-results/v2-final-solve-bench-40-49.json
```

Each match takes 5 minutes at 100 ms and 21–23 minutes at 500 ms on the 24-thread laptop. No other heavy work should run at the same time. Since Stello 3.0, the default side (`engine=stello`) is the Rust engine; to measure Stello 2.0 again, give the second side `engine=stello-v2` (and `solve-bench --engine stello-v2`).

---

Previous: [23 Phase 5: the search](23-phase-5-the-search.md) · Next: [25 Phases 0–4: the engine in Rust](25-phases-0-4-the-engine-in-rust.md)
