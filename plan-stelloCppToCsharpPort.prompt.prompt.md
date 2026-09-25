## Plan: Port Stello Othello from C++ (MFC) to C# (WPF)

The game engine becomes its own library, `Stello.Engine`. It keeps the same algorithms as the C++ code but may be modernised, for example with bitboards. On top of it goes an English WPF app where a human plays the computer, built with the MVVM pattern (view models and commands, no game logic in the window code). The existing binary `OPENING` book file is still read and written; the book-learning features come in a later phase. There are no tests against the C++ version. Correctness is checked with move counts from the start position, known endgame positions and strength tests.

Every phase ends by writing its section in [Stello porting documentation.md](Stello%20porting%20documentation.md): the C++ algorithms, data structures and notable features, the C# implementation, and whether each part is a 1:1 port or was changed, and why.

**Phase 0 – Setup**
1. Update [Migrate Othello game from C++ to C#.md](Migrate%20Othello%20game%20from%20C++%20to%20C%23.md) with your decisions: English UI, modernising allowed, no C++ comparison, book learning later, xUnit, master book `Stello C++/OPENING`. Remove the questions that are now answered.
2. Create `Stello.Net/Stello.Engine` (class library, `net10.0`, no WPF) and `Stello.Net/Stello.Engine.Tests` (xUnit). Add both to `Stello.Net.slnx`. The WPF project references the engine.
3. Add `CommunityToolkit.Mvvm` to the WPF project. Link `Stello C++/OPENING` into the output as `Data/OPENING`.

**Phase 1 – Board and rules** (*depends on 0*)
4. `Square`: internal index 0–63, conversion to and from the old C++ index (`10*row+col`, a1=11, h8=88) and to text like "f5". The book file and the ported tables use the old index.
5. `Board`: two 64-bit bitboards (black, white). Methods: legal moves, make move (returns flipped discs), pass, disc count. Start position as in `init_game()` in `BRAIN/Treak.cpp`: white on d4/e5 (44/55), black on e4/d5 (45/54). Black moves first.
6. Tests: flips in all 8 directions, passes, full board, wipe-out, and move counts from the start position for depth 1–8.

**Phase 2 – Game record** (*depends on 1; parallel with 3 and 4*)
7. `Game`: move history with positions (like `gamerec`/`playnm`/`sidste`), passes stored as moves, undo/redo, game over, winner. Human = black by default, and a "switch side" operation as in `OnSkiftSide`.
8. `GameRecordFormat`: save and load a text move list ("f5 d6 c3 … pass …"). Check that every move is legal when loading and give clear error messages.

**Phase 3 – Search engine** (*depends on 1*)
9. `Evaluator`: port `eval()` from `BRAIN/Eval.cpp`. This includes edge stability `sikker[6561]` (from `Kanter.cpp`), `black_v/white_v/black_h/white_h[6561]`, corner logic, and the mobility weights `CURMOB`/`POTMOB`/`BONUS`. Edge patterns are computed from the bitboards. The large tables go in `static readonly` data files.
10. `MoveOrdering`: `scores[100]` from `Sort.cpp` and the reply tables `humres`/`comres` (killer moves).
11. `TranspositionTable`: 2^19 entries with lower/upper/exact flags, depth and score. Hash keys come from a fixed-seed random generator.
12. `SearchEngine`: alpha-beta search (`findmax*` in `Minmax.cpp`) with iterative deepening and an exact endgame search (`slutmax*`, `zero_findmax`) when few empty squares remain. Win/loss scores use the same ±32600 convention. Progress reporting replaces the C++ callbacks `make_try`/`make_result`: current move, depth, nodes, evaluations, score, time, and whether the result is exact. Uses a `CancellationToken`, plus a separate "move now" signal that returns the best move found so far.
13. `TimeControl`: three modes (fixed depth, time per move, time per game). Port the `tider[]` budgets, `calc_time` and the 30% overshoot allowed by `timeout()` from `Kontrol.cpp`.
14. Tests:
    - exact scores for FFO endgame positions #40–#44;
    - the same result every time at a fixed depth;
    - at depth 4 the engine beats a greedy and a random player in at least 95% of 50 games;
    - cancel and "move now" respond within 100 ms.

**Phase 4 – Opening book, read-only** (*depends on 1; parallel with 3*)
15. `OpeningBook` reader/writer for the little-endian format in `Get_book`/`readbook`/`Put_book`:
    - a 32-bit node count;
    - then, recursively, a 16-bit child count, followed by 16-bit move, value and flag for each node, then its children.
16. Book lookup (`getlib`/`getopn` with `convop` symmetry). Black's first move is chosen at random from d3/c4/f5/e6 using a `Random` that tests can replace. The book is used until it runs out, like `libon`/`tryagain`.
17. Tests: reading and then writing `Stello C++/OPENING` gives a byte-identical file, and every move the book suggests is legal.

**Phase 5 – WPF interface** (*depends on 2, 3, 4*)
18. `MainViewModel`: current game, board, whose turn, disc count, status, and whether the computer is thinking. The computer moves via `Task.Run`, and the result is sent back to the UI thread. New game, undo and switch side cancel a running search.
19. `BoardView`: green 8×8 board with a–h/1–8 labels, markers for legal moves and the last move, click to move. Illegal clicks give a beep or visual hint. Pass and game over are shown in a message or the status bar.
20. Menus and commands:
    - File: New, Open, Save, Save As, Exit.
    - Game: Switch Side, Move Now, Settings….
    - Moves: Back (Ctrl+T or Ctrl+Z), Forward.
    - View: Analysis panel (Ctrl+A).
    - Help: About.
    - Book: added in phase 7.
21. Analysis panel: Nodes, Value, Time, Evaluations, current move and depth (as `IDD_ANALYSE`).
22. Settings dialog: choose a mode and value – depth (1–20), seconds per move (1–60), or minutes per game (1–60, as `Spiltid`).
23. About dialog: "Stello Version 2.0", with the original "Copyright (C) 1998 Futuresoft" credit.

**Phase 6 – Settings and polish** (*depends on 5*)
24. Settings as JSON in `%AppData%\Stello\settings.json`: time mode and value, whether the analysis panel is shown, window position. The book is read from the application folder and written to `%AppData%\Stello\OPENING` the first time it is changed.

**Phase 7 – Book learning** (*later; depends on 4 and 5*)
25. `BookLearner`:
    - `convert_game` (symmetry normalisation);
    - `mmgame` (adds a game to the book with its result);
    - `calc_lib`/`minmax_lib`/`sort_lib`;
    - `selfplay` (with the `SELFPLAY` text log).
26. Book menu: Add Game to Book (asks for the result), Minimax Book, Self-play. These run in the background with progress and Cancel, and save the book when done.

**Phase 8 – Performance tuning** (*final step; depends on 3*)
27. Make the endgame solver faster. FFO #40–#44 now take about 30 s in total in Release (about 80 s in Debug); #43 is the slowest. Candidates are listed in the specification ("Phase 8 – Performance tuning"): better ordering far from the end, enhanced transposition and stability cutoffs, a faster exact pass, and incremental hashing.
28. Keep the normal test run short: build the engine optimised, or put the FFO tests in a separate test category.
29. Done when all tests still pass and FFO #40–#44 take under 10 s in total in Release.

**Relevant files**
- `Stello C++/BRAIN/Reversi.h` – types and constants (`board`, `gamerec`, `booktree`, `tid_type`, `MAXMOVES`)
- `Stello C++/BRAIN/Treak.cpp` – `init_game`, `makelist`, `makemove`, `inlist`
- `Stello C++/BRAIN/Minmax.cpp` – `findmax*`, `slutmax*`, `zero_findmax`, `f_hash_init`, hash get/put
- `Stello C++/BRAIN/Eval.cpp`, `Kanter.cpp`, `Sort.cpp` – evaluation and tables
- `Stello C++/BRAIN/Kontrol.cpp` – `getcomputer`, `timeout`, `calc_time`, `tider[]`
- `Stello C++/BRAIN/Book.cpp`, `Tree.cpp` – book file I/O, lookup, learning
- `Stello C++/MainFrm.cpp`, `StelloView.cpp`, `Analyse.cpp`, `Spiltid.cpp`, `Stello.rc` – how the old UI behaves
- `Stello.Net/Stello.Net.slnx`, `Stello.Net/Stello.Net/Stello.Net.csproj`, `MainWindow.xaml` – to modify
- New: `Stello.Net/Stello.Engine/*`, `Stello.Net/Stello.Engine.Tests/*`

**Verification**
1. `dotnet build Stello.Net/Stello.Net.slnx` finishes with no warnings (nullable enabled).
2. `dotnet test` passes: move counts, rules, game record, FFO endgame positions, book round-trip, strength and cancellation tests.
3. Manual checks:
   - play a full game against the computer;
   - a pass happens on both sides;
   - undo and redo back to the start;
   - switch side in the middle of a game;
   - Move Now while the computer is thinking;
   - save, reopen and continue a game;
   - the analysis panel updates live;
   - the window stays responsive during the search.
4. Phase 7: add a game to the book, restart, and check that the book suggests the new line.

**Decisions**
- In scope: engine, WPF human vs computer, three time modes, text save/load, `OPENING` read/write, book learning (phase 7).
- Out of scope: printing, multiple windows, the WinHelp file, Danish UI, human vs human, computer vs computer, `Interfa.c`, `Hash.cpp`, `Mergelib.cpp`, `BORDERS.C` (only the table it generated is used), `OldBook/`.
- Chosen by me, easy to change: `CommunityToolkit.Mvvm`, JSON settings in `%AppData%`, bitboards, and "same result at a fixed depth" as the main determinism requirement.

**Further considerations**
1. Bitboards vs. the original 10×10 board: bitboards are faster, but porting `Eval.cpp` means rebuilding the edge indexes from bits. I recommend bitboards plus unit tests on the edge-index function. The alternative is to keep 10×10 for a quicker, lower-risk port.
2. Strength target: without C++ reference values we can't prove the port matches the original. Would you accept "beats greedy/random, solves FFO positions correctly" as the definition of done for the engine?
