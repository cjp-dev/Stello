# Migrate Stello Othello game from C++ to C#

## Purpose

Translate the old C++ project "Stello" to C#. Stello is an Othello (Reversi) program where a human plays against a computer engine. The new project must keep the functionality of the old game engine and have a WPF user interface where the user can enter moves and play a game of Othello against the computer.

## Scope

- **Source:** `Stello C++/` – MFC (Visual C++ 6) MDI application. Build project file: `Stello.dsp`.
- **Target:** `Stello.Net/` – currently an empty WPF solution (`net10.0-windows`, `UseWPF`, nullable and implicit usings enabled).
- Only the files listed in `Stello.dsp` are part of the build. `BRAIN/Interfa.c`, `BRAIN/Hash.cpp`, `BRAIN/Mergelib.cpp` and `BORDERS/BORDERS.C` are not compiled into the application:
  - `Interfa.c` is an older non-Windows user interface. Use it only as a reference for intended behaviour.
  - `Hash.cpp` is not in the build. The hash functions that are used live in `Minmax.cpp`.
  - `BORDERS.C` is an offline tool that generated the edge table in `Kanter.cpp`. Port only the generated table, not the tool.
  - `Mergelib.cpp` (merging black/white opening libraries) is out of scope unless stated otherwise.

### In scope

1. The game engine (`BRAIN/`): board, move generation, evaluation, alpha-beta search, hash table, time control, opening book, and book learning.
2. A WPF user interface that covers the features in the "Functional requirements" section.
3. Reading and writing the existing binary opening book file `OPENING`.

### Out of scope

- Printing and print preview (`Print...`, `Print Preview`, `Print Setup...`).
- The MDI window layout (`Vindue` menu: Cascade/Tile/Arrange Icons). Use a single main window instead.
- The WinHelp help file (`HLP/`). Replace it with a simple About dialog, and a rules/help dialog if needed.
- Debug output files (`moves`, `expanding`) unless needed for testing.

## Source overview

| C++ file(s) | Responsibility |
|---|---|
| `Stello.cpp` | App startup: reads `rev.cfg`, allocates tree nodes, loads the opening book (`Get_book`), and initialises the hash table and board |
| `MainFrm.cpp` | Menu command handlers: new game, switch side, forward/back, merge game into book, minmax book, self-play, time setting |
| `StelloView.cpp` | Draws the board and handles mouse clicks (human move → computer reply) |
| `StelloDoc.cpp` | New game. `Serialize()` is empty, so games cannot actually be saved or loaded |
| `Analyse.cpp` | Analysis window that shows the engine's search (moves, evaluation, depth, time) |
| `Spiltid.cpp` | Form for setting the time per game/move |
| `BRAIN/Reversi.h` | Core types: `board` (10×10 with border, squares 11–88), `movelist`, `gamerec`, `tree`, and colours `LIGHT/DARK/EMPTY/BORDER` |
| `BRAIN/Treak.cpp` | Move generation and making moves |
| `BRAIN/Minmax.cpp` | Alpha-beta search with killer moves, selective extensions, and hash table (2^19 entries) |
| `BRAIN/Eval.cpp`, `Kanter.cpp` | Evaluation: edge stability table `sikker[6561]`, mobility, and corner/X-square weights |
| `BRAIN/Sort.cpp` | Move ordering (`scores[100]`, `humres`/`comres` tables) |
| `BRAIN/Kontrol.cpp` | Time control (`tider[]`/`rtider[]` budgets) |
| `BRAIN/Book.cpp`, `Tree.cpp` | Opening book tree (first-child/next-sibling), load/save, learning, self-play, and book minimax |

## Functional requirements

Each C++ command should map to a WPF command. Proposed mapping:

| C++ menu (Danish) | Meaning | Required in C# |
|---|---|---|
| Fil → Nyt Spil (Ctrl+N) | New game | Yes |
| Fil → Open / Save / Save As | Load/save game | Yes, as a new feature (see "Game file format" below) |
| Spil → Skift Side | Swap colours; the computer moves at once | Yes |
| Spil → Tid for et spil | Set thinking time | Yes (dialog) |
| Spil → Flet spil | Add the current game to the opening book with the result (asks "Did black win?") | Yes |
| Spil → Minmaxlib | Re-evaluate and minimax the opening book | Yes (long-running, see "Non-functional requirements") |
| Spil → Lær spil | Self-play to extend the book; appends to `SELFPLAY` log | Yes (long-running, must be cancellable) |
| Træk → Frem (Ctrl+F) / Tilbage (Ctrl+T) | Redo/undo a move through the game history | Yes |
| Træk → Træk nu | Force the computer to move now. **Has no handler in C++** | Yes, implement it: stop the search and play the best move found so far |
| Vis → Analyse (Ctrl+A) | Show the analysis panel | Yes (side panel or separate window) |
| Vis → Toolbar / Status Bar | Toggle toolbar/status bar | Optional |
| Hjælp → Stello info | About dialog | Yes |

### Board and game play

- Show an 8×8 board with coordinates (a–h, 1–8) and the disc count for each side.
- Mark the legal moves for the human player. Illegal clicks are ignored.
- Handle a pass automatically when a player has no legal move, and show a message when it happens.
- Detect game over and show the result (final disc count and winner).
- Show whose turn it is, and show the computer's last move.
- By default the human plays black (DARK) and moves first (confirmed: `init_game()` in `Treak.cpp`, `curcl = DARK`).
- Only human vs. computer. Human vs. human and computer vs. computer are out of scope.

## Engine requirements

- Port the engine's algorithms (evaluation, alpha-beta search, endgame solver, hash table, time control, opening book). It does not have to give exactly the same moves and values as the C++ engine, and the code may be modernised. For a given position, depth and settings the C# engine must always return the same result.
- The board may use bitboards. The old square index (row × 10 + column, a1 = 11, h8 = 88) is still used for the book file and the ported tables (`scores[100]`, `sikker[6561]`).
- Replace global variables (`mainboard`, `game`, `playnm`, `human`, `computer`, `calc`, `libon`, …) with instance state in engine classes, for example `Board`, `Game`, `SearchEngine`, `OpeningBook`, `TimeControl`.
- Integer widths: C++ `short int` is 16 bits, `int`/`long` are 32 bits (MSVC). Use `short`/`int` in C# where overflow or packing matters, such as book file values and scores like `±32665`.
- Replace the fixed node pool (`init_nodes(ANTAL_KNUDER)`) with normal C# objects, but keep any limit on book size.
- The random numbers used for hash keys only need to be consistent within one run, unless the book or other files depend on them.

## Data files

| File | Format | Handling in C# |
|---|---|---|
| `OPENING` (in the working directory; copies in `BRAIN/`, `BOOKTEST/`, `OldBook/`) | Binary; written by `Put_book()` in `Book.cpp` (count header followed by the recursively written tree) | Must read and write the existing format byte for byte. Document the exact layout (field sizes, endianness, structure padding) in code comments. Master copy: `Stello C++/OPENING`, shipped as `Data/OPENING` |
| `rev.cfg` | Binary dump of the `revdef` struct (settings) | Replace with a JSON settings file (e.g. in `%AppData%\Stello`). Importing the old `rev.cfg` is optional |
| `SELFPLAY` | Text log ("played game N") | Keep as a text log |
| `OldBook/UOpening` | Old book | Out of scope |

- Do not use the current working directory for file locations. Use the application folder or `%AppData%\Stello`, and copy the default `OPENING` file into the output during the build.

### Game file format

The C++ version cannot save games (`Serialize()` is empty). Games are saved as a text move list (`f5 d6 c3 … pass …`) so they can be loaded, replayed with Back/Forward and merged into the book.

## Non-functional requirements

- **Responsive UI:** The C++ version searches on the UI thread (`BeginWaitCursor`). In C#, the search, Minmaxlib and self-play must run on a background thread (`Task`), use `CancellationToken` for "Træk nu", new game and undo, and report progress to the analysis panel.
- **Architecture:** Projects in `Stello.Net.slnx`:
  - `Stello.Engine` – a class library (`net10.0`) with no WPF dependency.
  - `Stello.Net` – the WPF app, using MVVM with `CommunityToolkit.Mvvm` (view models and commands; no game logic in code-behind).
  - `Stello.Engine.Tests` – xUnit tests, see "Testing and acceptance criteria".
  - `Stello.Net.Tests` – xUnit tests for the WPF view models.
- **Language:** The UI text is in English. Code identifiers are in English; keep the original Danish name in a comment where it helps to trace the code back to the C++ source (e.g. `// C++: sikker`).
- **Settings:** A JSON file in `%AppData%\Stello\settings.json`: time mode (fixed depth, time per move, or time per game) and its value.
- **Code quality:** No compiler warnings with nullable reference types enabled.
- **Porting documentation:** Every phase updates [Stello porting documentation.md](Stello%20porting%20documentation.md) before it is done. For each phase it describes:
  - the algorithms, data structures and notable features of the C++ code (with file and function names);
  - how they are implemented in C#/.NET/WPF (types, files);
  - for each part, whether it is a 1:1 port or was changed (improved algorithm or data structure, bug fixed, C++ quirk kept on purpose), and why;
  - known differences in behaviour and open points.

## Testing and acceptance criteria

1. Unit tests for move generation: start position, passes, full-board and wipe-out positions, and flips in all 8 directions.
2. Perft-style node counts from the start position for depth 1–8 match known values (1: 4, 2: 12, 3: 56, 4: 244, 5: 1396, 6: 8200, 7: 55092, 8: 390216).
3. The endgame solver finds the known exact scores for the FFO test positions #40–#44.
4. At a fixed depth the search always gives the same result. At depth 4 the engine beats a greedy player and a random player in at least 95% of 50 games each. There are no comparison tests against the C++ engine.
5. Loading `OPENING` and saving it again without changes gives a byte-identical file.
6. A full game can be played human vs. computer through the UI, including pass, undo/redo, switch side and game over.
7. The UI stays responsive while the computer is thinking, and "Træk nu" makes the computer move within ~100 ms.

## Phases

Each phase ends with its section in the porting documentation (see "Non-functional requirements").

0. Setup: solution structure, projects, packages, and the book file in the output.
1. Board and rules, with tests.
2. Game record: history, undo/redo, and text save/load.
3. Evaluation, search, hash table, endgame solver, and time control.
4. Opening book: read/write and lookup during play (read-only).
5. WPF UI: board, menus/commands, settings dialog, and analysis panel.
6. Settings persistence and polish.
7. Book learning (later): Flet spil (Add Game to Book), Minmaxlib (Minimax Book), and Lær spil (Self-play).
8. Performance tuning (final step), see below.

### Phase 8 – Performance tuning

**Status: paused (rounds 1 and 2 done 2026-09-25).** After phase 3, FFO #40–#44 were solved correctly but took about 31.6 s in a Release build (522 M nodes) and about 80 s in a Debug build. Zebra takes about 3 s per position. The engine searches about 15–20 million nodes per second but visits 2–10 times more nodes than Zebra; #43 is the slowest.

Measurement method: a throwaway console benchmark (outside the repository) that solves FFO #40–#44 with `SearchLimits.Solve` in a Release build. For each position it prints the score, nodes, time, nodes per second, and when the win/loss/draw pass finished. Timings vary by about ±1 s between runs.

#### Round 1 results – kept (in the code)

| Change | Result |
|---|---|
| Hash table with two entries per slot (one keeps the deepest result, one is always replaced) instead of one entry | The biggest gain. Total about 17–19 s; #43 dropped from 292 M to about 117–168 M nodes. A single-entry table of 2²³ entries gave a similar result, so the problem was entries being overwritten. |
| Enhanced transposition cutoff (look up the children in the hash table before searching, from 10 empties) | About 10 % fewer nodes (32.5 s → 31.8 s with the old table). |
| Flips computed once per move and sorted with the moves (not computed again when the move is played) | Neutral for speed; simpler code. |

With these, FFO #40–#44 take **19.1 s (328 M nodes)** in Release. The engine test run takes about 30 s in Release and about 55–75 s in Debug. The target (under 10 s) is **not reached yet**.

#### Round 1 results – tested and rejected (reverted; do not repeat as they were)

| Tried | Result |
|---|---|
| MTD(f) for the exact pass (null-window steps from the win/loss/draw bound) | Worse: 31.6 s → 38.0 s total; #43 went from 292 M to 428 M nodes (five steps from −2 to −12). Measured with the old single-entry hash table; the re-test with the two-entry table was not finished. Only worth trying again with a good first guess, not the win/loss/draw bound. |
| Exact search with one wide window (−65, 65) and no win/loss/draw pass | No gain: 32.8 s against 31.6 s. |
| Ordering by a shallow midgame search (0 or 1 ply with the Stello evaluation) from 12, 14 or 16 empties | Much worse: 51–101 s. The Stello evaluation is a poor move orderer for the endgame; fastest-first is better. (The existing evaluation-based ordering from 18 empties was kept; with it at 14 empties #43 got worse.) |
| Potential mobility added to the fastest-first key | Within the noise: about 5 % fewer nodes, but no measurable time gain (16.9 s against 17.1 s). |
| Stability cutoff (stable discs from full lines, edges and stable neighbours) | No gain: node count almost unchanged, time slightly worse (17.7 s against 17.1 s). |
| Other thresholds: shallow solver from 5, 6 or 7 empties; endgame hash table from 6, 7 or 8 empties | All within the noise (17.1–18.1 s). The current 6 and 7 were kept. |
| Larger hash table (2²¹–2²³ slots) with the two-entry slots | No further gain (17.4–17.9 s against 17.0 s at 2¹⁹). |

#### Round 2 – ideas from endgame.c (2026-09-25)

Source: the endgame solver by Warren D. Smith and Jean-Christophe Weill, improved by Gunnar Andersson ([endgame.c](http://radagast.se/othello/endgame.c)). Stello already had its fastest-first ordering and a quadrant form of its parity ordering. Four ideas for the last few empty squares were new; each was added alone, measured and kept only if faster.

Measurement method:

- A throwaway console benchmark outside the repository, Release build.
- **Suite:** the 112 endgame.c test positions, 100 of them with 12 empty squares, solved 30 times. It uses `SearchLimits.Solve` and a 2¹⁶-slot hash table, cleared between positions. Timings vary by about ±2 %.
- **FFO #40–#44** with the default engine. Timings vary by about ±0.5 s.
- Where the difference was small, the baseline and the variant builds were run alternately.
- All variants gave the same scores (checksum of all suite scores) and the same FFO scores and moves.

Baseline: suite 2.38 s (39.1 M nodes), FFO 19.5 s (328 M nodes).

| Tried | Result |
|---|---|
| **Kept:** special code for the last two empty squares (`SolveLast2`): both squares tried directly, without the parity loop | Same nodes; suite 2.38 → 2.32 s (−2.5 %), FFO 19.5 → 18.9 s (−3.5 %), better in 5 of 6 and 6 of 6 alternating runs. |
| Fixed square order of preference in `SolveShallow` (Weill's order: corners, c1, c3, d1, d3, d2, c2, C-squares, X-squares), as 9 groups | 3 % fewer nodes, but slower: 2.64 s. |
| The same order as 3 groups (corners, others, C- and X-squares) or 2 groups (C- and X-squares last) | 1–2 % fewer nodes; 2.54 s and 2.43 s, no gain. |
| The empty squares in a list prepared once when `SolveShallow` takes over (as the linked list in endgame.c), in the old order | Same nodes, 25 % slower (2.98 s). |
| Parity by the connected empty regions (computed once per shallow search) instead of the four quadrants | 0.3 % fewer nodes, slower (2.53 s). |
| No parity below 5, 4 or 3 empty squares (endgame.c found 4 best) | 7 %, 4 % and 0 % more nodes; 2.46, 2.49 and 2.42 s, no gain. |

Conclusion: with bitboards the shallow solver is limited by the cost per node, not by the ordering. The ordering ideas save 0–3 % nodes but cost more per node than they save. The endgame.c test positions are now test data: `Stello.Engine.Tests/Data/endgame-c-positions.txt` with `EndgameTests.Solve_MatchesTheEndgameCSuite`.

#### Remaining candidates for the next round

- Faster move generation and flips (for example lookup-table based flips), since nodes per second are only about 15–20 M.
- Special code for the last 3–4 empty squares (the last 2 are done, see round 2).
- Better ordering in the middle of the endgame (10–18 empties), for example a shallow *endgame* search or a proper weighted-mobility formula, to get the node counts closer to Zebra.
- MTD(f) or an aspiration window only with a good first guess (see above).
- Midgame: iterative-deepening move ordering and hash-table use at the root.
- Build the engine with optimisations in Debug, or move the slow FFO tests to a separate test category, so the normal test run stays short (now about 55–75 s in Debug).

Acceptance: the same test results as before, and FFO #40–#44 in less than 10 s in total in a Release build.

## Decisions

- UI in English only.
- The engine uses the same algorithms but may be modernised; bitboards are allowed.
- Book learning comes in a later phase, after the game is playable.
- Games are saved as a text move list.
- Only human vs. computer.
- Time settings: fixed depth, time per move, and time per game.
- No comparison tests against the C++ engine.
- Tests use xUnit.
- The master opening book is `Stello C++/OPENING`.