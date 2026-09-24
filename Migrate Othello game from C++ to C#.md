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
- By default the human plays black (DARK) and moves first. **Confirm this against the C++ code.**

## Engine requirements

- Port the engine faithfully. For the same position, search depth and settings, the C# engine should pick the same move and return the same evaluation as the C++ engine. Deterministic behaviour is required for the tests (see "Testing and acceptance criteria").
- Keep the board representation (10×10 with border squares, square index = row × 10 + column) during the first port so the tables (`scores[100]`, `sikker[6561]`, hash keys) can be reused unchanged. Optimisations such as bitboards may come later but are out of scope for now.
- Replace global variables (`mainboard`, `game`, `playnm`, `human`, `computer`, `calc`, `libon`, …) with instance state in engine classes, for example `Board`, `Game`, `SearchEngine`, `OpeningBook`, `TimeControl`.
- Integer widths: C++ `short int` is 16 bits, `int`/`long` are 32 bits (MSVC). Use `short`/`int` in C# where overflow or packing matters, such as book file values and scores like `±32665`.
- Replace the fixed node pool (`init_nodes(ANTAL_KNUDER)`) with normal C# objects, but keep any limit on book size.
- The random numbers used for hash keys only need to be consistent within one run, unless the book or other files depend on them.

## Data files

| File | Format | Handling in C# |
|---|---|---|
| `OPENING` (in the working directory; copies in `BRAIN/`, `BOOKTEST/`, `OldBook/`) | Binary; written by `Put_book()` in `Book.cpp` (count header followed by the recursively written tree) | Must read and write the existing format byte for byte. Document the exact layout (field sizes, endianness, structure padding) in code comments. Which copy is the master file? **To be decided** |
| `rev.cfg` | Binary dump of the `revdef` struct (settings) | Replace with a JSON settings file (e.g. in `%AppData%\Stello`). Importing the old `rev.cfg` is optional |
| `SELFPLAY` | Text log ("played game N") | Keep as a text log |
| `OldBook/UOpening` | Old book | Out of scope |

- Do not use the current working directory for file locations. Use the application folder or `%AppData%\Stello`, and copy the default `OPENING` file into the output during the build.

### Game file format

The C++ version cannot save games (`Serialize()` is empty). Proposal: save the move list as a text file, e.g. `.stello` or plain `f5 d6 c3 …` notation, so games can be loaded, replayed with Frem/Tilbage and merged into the book.

## Non-functional requirements

- **Responsive UI:** The C++ version searches on the UI thread (`BeginWaitCursor`). In C#, the search, Minmaxlib and self-play must run on a background thread (`Task`), use `CancellationToken` for "Træk nu", new game and undo, and report progress to the analysis panel.
- **Architecture:** Two projects in `Stello.Net.slnx`:
  - `Stello.Engine` – a class library with no WPF dependency.
  - `Stello.Net` – the WPF app, using MVVM (view models and commands; no game logic in code-behind).
- **Tests:** An `Stello.Engine.Tests` project (xUnit or MSTest) – see "Testing and acceptance criteria".
- **Language:** UI text: **to be decided** – keep Danish, change to English, or use resource files for both. Code identifiers are in English; keep the original Danish name in a comment where it helps to trace the code back to the C++ source (e.g. `// C++: sikker`).
- **Code quality:** No compiler warnings with nullable reference types enabled.

## Testing and acceptance criteria

1. Unit tests for move generation: start position, passes, full-board and wipe-out positions, and flips in all 8 directions.
2. Perft-style node counts from the start position for depth 1–8 match known values (1: 4, 2: 12, 3: 56, 4: 244, 5: 1396, 6: 8200, 7: 55092, 8: 390216).
3. For a set of test positions, evaluation and best move at a fixed depth match the C++ engine. The reference values have to be produced by building and running the C++ version or by an instrumented port.
4. Loading `OPENING` and saving it again without changes gives a byte-identical file.
5. A full game can be played human vs. computer through the UI, including pass, undo/redo, switch side and game over.
6. The UI stays responsive while the computer is thinking, and "Træk nu" makes the computer move within ~100 ms.

## Suggested phases

1. Engine core: board, move generation, making moves, and tests.
2. Evaluation, search, hash table and time control, with comparison tests.
3. Opening book: load/save, lookup during play, Flet spil, Minmaxlib, and self-play.
4. WPF UI: board, menus/commands, time dialog, and analysis panel.
5. Settings, saving/loading games, and polish.

## Open questions

- Should the UI be in Danish, English or both?
- Which `OPENING` file is the master copy, and may the book file format be changed later?
- Is saving/loading games (a new feature) wanted, and in which format?
- Should human vs. human and computer vs. computer modes be added? They are not in the C++ version.
- Which difficulty or time settings from `rev.cfg`/`Kontrol.cpp` should be shown in the UI?
- Can the C++ project still be built (VC6), so reference values can be produced for the comparison tests?