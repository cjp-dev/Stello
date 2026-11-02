# 22 – Phase 4: the release

[Back to the index](README.md)

## In short

Phase 4 made Stello 2 the engine of the WPF and web apps: the first 2.0 release, on 1 November 2026.

- **Cleaning up:** `Stello.Engine` always uses the pattern evaluation and the new solver. Stello 1's evaluator, edge tables, corner extension and solver were removed from it; `Stello.Engine.V1` keeps them.
- **The weights** are built into the engine assembly as an embedded resource, instead of a separate file as planned. Every app and tool has them without loading code.
- **The GUI** shows heuristic scores in discs with one decimal, and the About box describes the new evaluation and solver and credits Egaroucid.
- **The opening book was recalculated by Stello 2** (engine version 3): every leaf that was not exact was searched again for 60 s, with 8 workers on a second computer, in about 16 hours. 11,322 values changed and 1,307 positions got another first move.
- **The gates, with books on at 500 ms:** Stello 2 with the new book scored **88.9 %** (+362 Elo) against Stello 1 with its book, and **49.2 %** against Stello 2 with the old book, so the new book is not worse. The live site plays, stops and solves.
- **A flaw found afterwards:** 2,718 book leaves marked win/loss/draw by the C++ book kept their old values, because `recalc` kept every win/loss/draw value it could not solve. It was fixed in d21e42f, and the second recalculation of [chapter 23](23-phase-5-the-search.md) replaced them.

## Cleaning up

Phases 2 and 3 had kept both ways side by side behind switches ([chapter 16](16-the-goal-and-the-plan.md#one-switch-per-change)). Once both had passed their gates, commit 720f224 removed the old ways from `Stello.Engine`:

| Removed from `Stello.Engine` | Still in `Stello.Engine.V1` |
|---|---|
| Stello 1's evaluator (`Evaluator.cs`) and its edge tables (`EdgeTables.cs`) | Yes |
| The dangerous-corner extension (`IsDangerous`), worth nothing with the new evaluation ([chapter 20](20-phases-0-2-the-evaluation.md#the-search-on-the-new-scale)) | Yes |
| Stello 1's endgame solver (`Solve`) | Yes |
| `SearchOptions.DangerousCorners`, `SearchOptions.NewEndgame` | — |
| The match keys `eval=classic`, `danger=` and `endgame=`, and `--endgame` | — |

- **`SearchEngine` always uses the pattern evaluation** (`EvalWeights.Default`) and the new solver. `SearchOptions.Pattern` can still give it another evaluation file for a match.
- **The old selective search stayed** for the first 2.0 release. It was not tied to the old evaluation, and phase 2 found no better margin. Phase 5 replaced it with Multi-ProbCut ([chapter 23](23-phase-5-the-search.md)).
- **`eval-compare`** in the benchmark tool and the trainer's `report` use Stello 1's evaluator from `Stello.Engine.V1`.
- **The tests:** the plan said `EvaluatorTests` would be rewritten for the new evaluation. Instead they kept testing Stello 1's evaluator, now in `Stello.Engine.V1`; the new evaluation has its own [PatternTests.cs](../Stello.Net/Stello.Engine.Tests/PatternTests.cs). The endgame tests compare the new solver with Stello 1's, and the book-learning worked example got Stello 2's values ([below](#what-the-recalculation-changed)).
- **`OpeningBook.EngineVersion` became 3,** so the next book recalculation would search every value found by an older engine again ([below](#the-book-recalculated-by-stello-2)).

## Shipping the weights

**The plan:** ship `Book/stello-eval.bin` as a file, linked into the WPF app like the opening book. In the web app, copy it to `wwwroot/data` with the same build step as the book, fetch it with `StartupBook` beside the book, and pass it to the worker's `Init`.

**What was done:** `Stello.Engine.csproj` embeds the file as the resource `Stello.Engine.stello-eval.bin`, and `EvalWeights.Default` reads it on first use ([chapter 04](04-evaluation.md#the-file), [chapter 14](14-app-integration.md#what-the-builds-put-in-the-apps)).

- **Every app and tool gets the same bytes** with no loading code: the WPF app, the web app, the book tool, the benchmark tool, the trainer and the tests.
- **The browser downloads it inside the engine assembly,** which is now 1.15 MB, 1.03 MB compressed: about 1 MB more than before. That is the budget that was set for the evaluation.
- **No new failure mode.** A file that the app must find and fetch can be missing or stale; an embedded resource cannot.
- **The licence.** The training data is not shipped; the trained weights are, and are credited in [THIRD-PARTY-NOTICES.md](../THIRD-PARTY-NOTICES.md).

## The GUI in discs and About 2.0

Commit 5414efb:

- **Heuristic scores in discs with one decimal.** The search scores are in 1/100 disc since phase 2, so `AnalysisViewModel.FormatScore` shows a score of 123 as `+1.2 discs` ([chapter 15](15-gui-and-settings.md#telemetry-fields-explained)). Stello 1's scores had been in its own evaluation units.
- **Exact scores** are whole discs (`+12 discs`); `Win`, `Loss`, `Draw` and `Book` are unchanged.
- **The About box** describes the pattern evaluation, trained on 500 million positions from Egaroucid's games, and the endgame solver adapted from Egaroucid's, and credits Egaroucid and its author ([chapter 15](15-gui-and-settings.md)). Its version text, "Stello Version 2.0" (`AboutInfo.Version`), had come with the C# port and stayed as it was: it now names the right engine.

## The book recalculated by Stello 2

### Why

The book's values are search results. A book searched by Stello 1 holds Stello 1's opinions, in Stello 1's evaluation units. The decision was that the book is recalculated with the new engine, so its values are the new engine's scores in discs ([chapter 16](16-the-goal-and-the-plan.md#the-decisions)).

`recalc` in the book tool ([chapter 12](12-book-tool.md#recalculating-the-leaves-recalc)) searches every leaf whose value is not exact, unless it was already searched at least as hard by the current engine version (`OpeningBook.EngineVersion`). Raising the version to 3 therefore made it search every heuristic and win/loss/draw leaf again. Exact values were kept: they do not depend on the engine.

Two things were prepared first:

- **Stello 1 keeps its book.** The old book was saved as [Book/opening-book-stello1.txt](../Stello.Net/Book/opening-book-stello1.txt), and a Stello 1 match side with `book=on` plays from it. Stello 2's sides use `opening-book.txt`.
- **`book-file=<text book>`** gives a Stello match side another book (b8bc973), for the match of the new book against the old one.

### The versions of the book

The book had been recalculated twice before, with Stello 1. Those runs are described in the archive, in [Stello 1's chapter 12](stello-1/12-book-tool.md#recalculating-the-master-book) and [chapter 10](stello-1/10-opening-book.md):

| Engine version | When | Engine | The run | Against the book before it |
|---|---|---|---|---|
| none | — | Stello C++ | The C++ book, imported: 1,091 exact, 3,384 win/loss/draw and 7,132 heuristic values (and 15 unknown) | — |
| 1 | October 2026 | Stello 1 | Every leaf for 60 s, 8 workers on the i7 laptop, about a day | 52.8 % (51.6–54.0 %), stronger |
| 2 | October 2026 | Stello 1 after the fix of its leaf evaluation (`EdgeContext.Reply`) | 8,623 non-exact leaves for 60 s, 7 workers | 51.2 % (50.0–52.4 %), stronger |
| **3** | **1 November 2026** | **Stello 2, the first 2.0 release** | **Every non-exact leaf for 60 s, 8 workers on the Ryzen 7 7800X3D, about 16 hours** | **49.2 % (47.3–51.2 %), not worse** |
| 4 | 2 November 2026 | Stello 2 with the phase 5 search, the final 2.0 | Every leaf without a proof for 60 s, 8 workers on the same computer, about 14 hours | 51.7 % (49.7–53.6 %), not worse ([chapter 23](23-phase-5-the-search.md)) |

The book matches of versions 1 and 2 were played by the book tool's `match` at depth 10: version 1 from the 2,163 positions whose first move had changed, version 2 from all 265 book positions at ply 8 (530 games). Those of versions 3 and 4 were played by the benchmark tool, 1,180 games at 500 ms per move from `starts.txt`, with Stello 2 on both sides.

### The run

- **Time per position.** A first try with 30 s per position already reached depth 17–18, as deep as Stello 1's last recalculation in 60 s. The run used 60 s, like Stello 1's, so the book's values are as good as the engine can make them in a minute.
- **A second computer.** The run took about 16 hours with 8 workers on a Ryzen 7 7800X3D, overnight from 31 October to 1 November. So it did not disturb the timed matches on the laptop, where phase 5 started meanwhile on its own branch.
- **The heuristic values are in 1/100 disc.** The solved ones stay ±(32 600 + discs).
- `verify` passes, and `opening-book.bin` was rebuilt from the text book (153baa2).

The report that `recalc` wrote:

```text
# Recalculated Stello.Net/Book/opening-book.txt with 60 s per position (2026-11-01 08:24)
# Positions: 11,200 (before 11,200); book moves: 22,878 (before 22,878)
# Values changed: 11,322
# Origin before -> after: book moves (values changed)
#   Heuristic   -> Heuristic     5,426 (5,418)
#   Heuristic   -> WinLossDraw      17 (17)
#   Heuristic   -> Exact           103 (103)
#   WinLossDraw -> WinLossDraw   2,744 (0)
#   WinLossDraw -> Exact           116 (96)
#   Exact       -> Exact         3,216 (0)
#   BackedUp    -> BackedUp     11,256 (5,688)
# Positions whose first book move changed: 1,307
```

The line `WinLossDraw -> WinLossDraw 2,744 (0)` hides the flaw described [below](#the-flaw-found-afterwards): not one of those values changed.

### What the recalculation changed

| | Version 2 (Stello 1) | Version 3 (Stello 2) |
|---|---|---|
| Exact / win-loss-draw / heuristic values | 3,216 / 2,860 / 5,546 | 3,435 / 2,761 / 5,426 |
| Newly solved exactly | — | +219 (+103 from heuristic, +116 from win/loss/draw) |
| Values changed | — | 11,322 (5,634 leaves, 5,688 backed up) |
| Positions with another first move | — | 1,307 |
| White's replies to d3 | c5 −10, c3 −29, e3 −150 (Stello 1's units) | c5 −25, c3 −62, e3 −150 (1/100 disc: −0.25, −0.62, −1.50 discs) |
| Depth reached in 60 s | 17–18 | about 17 on average, 18–19 near the start |
| Main line from the start | 17 moves | 15 moves (±0.25 discs), the same up to g5 |

- **One leaf had −65 discs.** The midgame evaluation was not capped, and a sum of 62 weights can go beyond what a game can end with. The final 2.0 caps the evaluation at ±64 discs (b8e7109, [chapter 04](04-evaluation.md#from-weights-to-search-scores)).
- **The book-learning worked example** ([chapter 11](11-book-learning.md#worked-example-learning-on-an-empty-book), `BookLearnerTests`) learns a book from one game with a fixed-depth search. With Stello 1's evaluation, the new move e3 came out better than the game move c5 and moved to the front. With Stello 2, c5 (−62) stays ahead of e3 (−275).

## The gates

The plan's gate for phase 4 had two matches with books on, 500 ms per move, 1,180 games from `starts.txt`:

| Match | Score | Elo | Discs per game | Midgame depth | Report |
|---|---:|---:|---:|---:|---|
| Stello 2 + new book against Stello 1 + its book | **88.9 %** (87.2–90.7 %) | **+362** | +12.6 | 13.05 against 12.25 | [result](../Plans/benchmark-results/match_v2-p4-book-vs-v1-500ms.md) |
| Stello 2 + new book against Stello 2 + the version 2 book (`book-file=`) | **49.2 %** (47.3–51.2 %) | −5 | −0.1 | 13.35 against 13.34 | [result](../Plans/benchmark-results/match_v2-p4-newbook-vs-oldbook-500ms.md) |

- **Both pass.** Stello 2 with its book is far stronger than Stello 1 with its book: it won 1,026 games, drew 47 and lost 107. The new book is not worse than the old one.
- **The books matter little here.** The games start at plies 10–12, so the books only differ in the lines they hold beyond that, and an even result is what to expect. The first match is about as strong as phase 2's match without books (+393), so the book costs Stello 2 nothing.

The other checks of the gate were that the tests of `Stello.Engine` and the Egaroucid port pass, that the WPF app builds with `-p:EnableWindowsTargeting=true`, and the live check below.

## The deploy and the live check

After the deploy (run 41290149765), the live site at https://polite-sky-0e9899910.3.azurestaticapps.net was checked with Playwright in headless Chromium:

| Check | Result |
|---|---|
| About | Shows the Stello 2.0 text: the pattern evaluation, Egaroucid's solver |
| Plays | With the book on and 1 s per move, the computer played 15 book moves (Score: "Book"), then searched to depth 14–15 at about 1.8 M nodes per second, with scores such as "−0.4 discs" |
| Stops | During a 60 s search, Move Now played the best move so far within 0.11 s, including the browser driver's own delay |
| Solves | A position with 22 empty squares was solved in 0.59 s (6.5 M nodes, +12 discs), as in the local build |
| Errors | No page errors |

How the browser's solve times compare with Stello 1's is in [chapter 21](21-phase-3-the-endgame-solver.md#in-the-browser).

## The flaw found afterwards

The C++ book marked some nodes `INEXACT`. The import of the C++ book ([chapter 10](10-opening-book.md#the-c-file-import-only)) took them as win/loss/draw values. But most of them hold evaluation-sized values, for example −18 or −56: they are no proof of a win or a loss.

`recalc` kept every win/loss/draw value that its search could not solve, so as not to replace a proof with a guess. So **2,718 leaves with the `INEXACT` flag kept their C++ values** through the recalculations of versions 1, 2 and 3. In version 3 it shows in the report's `WinLossDraw -> WinLossDraw 2,744 (0)`.

- **The fix (d21e42f, 2026-11-01):** `recalc` keeps a win/loss/draw value only when it is beyond ±32,600, a proven win or loss. Any other win/loss/draw value gets the new search's value. `BookRecalculatorTests` checks both cases ([BookRecalculator.cs](../Stello.Net/Stello.Engine/BookRecalculator.cs)).
- **The fix came too late for the first 2.0 release.** Its book is version 3, with the stale leaves.
- **The second recalculation** (version 4, for the final 2.0) searched them too. Their old values were 0.77 discs on average (in absolute size), against 4.39 discs from the Egaroucid port's searches at level 21 ([chapter 23](23-phase-5-the-search.md)).

## The documentation

Phase 4 also wrote a documentation chapter on the new evaluation and solver, with the phase 2 and 3 measurements, and updated the other chapters for Stello 2 (689bfbf). For the final 2.0 that content was reorganised: how 2.0 works is in chapters [04](04-evaluation.md) and [08](08-endgame-solver.md), how it got there in chapters 16–24, and Stello 1's documents went to the [archive](stello-1/README.md).

## Design notes

- **Built in, not loaded.** An embedded resource gives every app and tool the same bytes with no loading code. The plan's separate file would have needed a copy step, a fetch and a parameter in both apps.
- **One book per engine.** A book's values are its engine's opinions. Stello 1 keeps the book it made, so a match of Stello 1 with books on still measures Stello 1.
- **The long work on another machine.** A 16-hour recalculation on the laptop would have stopped the timed matches. On the second computer it ran overnight while phase 5 went on.
- **"Not worse" is the right gate for a book.** From start positions at plies 10–12 a book cannot show much. The book was recalculated because its values must be the engine's, in its unit, not to win the match.
- **Keep a proof, not a flag.** The flaw came from trusting the origin `WinLossDraw` without its value. The fix trusts only what the value proves.

## Where in the code

| File | Main members |
|---|---|
| [EvalWeights.cs](../Stello.Net/Stello.Engine.V2/Evaluation/EvalWeights.cs), [Stello.Engine.V2.csproj](../Stello.Net/Stello.Engine.V2/Stello.Engine.V2.csproj) | `Default`; the `EmbeddedResource` for `stello-eval.bin` (in the frozen 2.0 engine, `Stello.Engine.V2`; since 3.0 the Rust engine embeds the file, [chapter 25](25-phases-0-4-the-engine-in-rust.md)) |
| [AnalysisViewModel.cs](../Stello.Net/Stello.App/ViewModels/AnalysisViewModel.cs) | `FormatScore`: scores in discs |
| [AboutInfo.cs](../Stello.Net/Stello.App/Models/AboutInfo.cs) | The About text and `Version` |
| [OpeningBook.cs](../Stello.Net/Stello.Engine/OpeningBook.cs) | `EngineVersion` (now 4) |
| [BookRecalculator.cs](../Stello.Net/Stello.Engine/BookRecalculator.cs) | `Run`: which values are kept, `NeedsSearch` |
| [Stello.BookTool](../Stello.Net/tools/Stello.BookTool) | `recalc`, `compare`, `build`, `verify` ([chapter 12](12-book-tool.md)) |
| [MatchCommands.cs](../Stello.Net/tools/Stello.Benchmark/MatchCommands.cs) | `book=` and `book-file=` |
| [Book/opening-book-stello1.txt](../Stello.Net/Book/opening-book-stello1.txt) | Stello 1's book |

## Tests

In [Stello.Engine.Tests](../Stello.Net/Stello.Engine.Tests):

- [BookRecalculatorTests.cs](../Stello.Net/Stello.Engine.Tests/BookRecalculatorTests.cs): a proven win/loss/draw value is kept, and one that is no proof is replaced (`Run_ReplacesAWinLossDrawValueThatIsNoProof`).
- [BookLearnerTests.cs](../Stello.Net/Stello.Engine.Tests/BookLearnerTests.cs): the worked example of chapter 11, with Stello 2's values.
- [EvaluatorTests.cs](../Stello.Net/Stello.Engine.Tests/EvaluatorTests.cs): Stello 1's evaluator, now in `Stello.Engine.V1`.

In [Stello.Net.Tests](../Stello.Net/Stello.Net.Tests): `ViewModelTests.FormatScore`, the scores in discs.

## Reproducing the book gates

```text
dotnet run -c Release --project Stello.Net/tools/Stello.Benchmark -- match --a engine=stello-v1,time-ms=500,book=on,name=stello1-book --b time-ms=500,book=on,name=stello2-newbook
dotnet run -c Release --project Stello.Net/tools/Stello.Benchmark -- match --a time-ms=500,book-file=<old book.txt>,name=stello2-oldbook --b time-ms=500,book=on,name=stello2-newbook
```

The recalculation itself is `Stello.BookTool recalc Stello.Net/Book/opening-book.txt`, with the steps after it in [chapter 12](12-book-tool.md#recalculating-the-master-book).

---

Previous: [21 Phase 3: the endgame solver](21-phase-3-the-endgame-solver.md) · Next: [23 Phase 5: the search](23-phase-5-the-search.md)
