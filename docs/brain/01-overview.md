# 01 – Overview

[Back to the index](README.md)

This chapter shows the big picture: the projects, the main types, and what happens when the computer makes one move. The later chapters go into each part.

## The projects

The solution [Stello.Net.slnx](../../Stello.Net/Stello.Net.slnx) has twelve projects. The arrows show which project uses which; the apps and the test projects also use the engine's types directly. The projects for comparing engines (the frozen Stello 1, the Egaroucid port and the port's tests) and the evaluation trainer are used only by the tools and tests, never by the apps ([chapter 18](18-the-egaroucid-port.md) and [chapter 20](20-phases-0-2-the-evaluation.md)).

```mermaid
flowchart LR
    Wpf["Stello.Net<br/>WPF app<br/>net10.0-windows"] --> Shared["Stello.App<br/>view models and services<br/>net10.0"]
    Web["Stello.Web<br/>Blazor WebAssembly app<br/>net10.0"] --> Shared
    Shared --> Engine["Stello.Engine<br/>the brain<br/>net10.0"]
    AppTests["Stello.Net.Tests<br/>view-model tests"] --> Wpf
    AppTests --> Shared
    EngineTests["Stello.Engine.Tests<br/>engine tests"] --> Engine
    Tool["Stello.BookTool<br/>book tool<br/>net10.0"] --> Engine
    Bench["Stello.Benchmark<br/>matches and benchmarks<br/>net10.0"] --> Engine
    V1["Stello.Engine.V1<br/>Stello 1, frozen<br/>net10.0"] --> Engine
    Bench --> V1
    Bench --> Egaroucid["Egaroucid.Engine<br/>C# port of Egaroucid<br/>net10.0"]
    EngineTests --> V1
    Trainer["Stello.Trainer<br/>evaluation trainer<br/>net10.0"] --> Engine
    Trainer --> V1
    Trainer --> Egaroucid
    EgaroucidTests["Egaroucid.Engine.Tests<br/>port tests"] --> Egaroucid
    EgaroucidTests --> Bench
    EgaroucidTests --> Engine
    Reference["tools/Egaroucid.Reference<br/>C++ reference, outside the solution"] -. "fixtures" .-> EgaroucidTests
    Text[("Stello.Net/Book/opening-book.txt<br/>master opening book")] -. "build" .-> Tool
    Tool -. "build" .-> Book[("Stello.Net/Book/opening-book.bin")]
    Book -. "copied as Data/OPENING" .-> Wpf
    Book -. "copied as wwwroot/data/OPENING.bin" .-> Web
    Trainer -. "train" .-> Eval[("Stello.Net/Book/stello-eval.bin<br/>trained evaluation")]
    Eval -. "embedded resource" .-> Engine
```

| Project | Contents |
|---|---|
| [Stello.Engine](../../Stello.Net/Stello.Engine) | Stello 2.0: rules, game record, the pattern evaluation (with the trained weights built in), the midgame search, the endgame solver, the opening book, book learning and the book tool's searches. No UI code and no dependencies outside .NET. |
| [Stello.App](../../Stello.Net/Stello.App) | The view models ([CommunityToolkit.Mvvm](https://learn.microsoft.com/dotnet/communitytoolkit/mvvm/)), the settings, and the interfaces for the engine host, dialogs, files and storage, shared by both apps (chapters 14 and 15). |
| [Stello.Net](../../Stello.Net/Stello.Net) | The WPF desktop app: windows, dialogs, and files in `%AppData%\Stello`. It runs the engine on a background task. |
| [Stello.Web](../../Stello.Net/Stello.Web) | The web app (Blazor WebAssembly): the same game in the browser. It runs the engine in a Web Worker. Its menus, panels, dialogs and documentation viewer come from the shared [GameShell](https://github.com/cjp-dev/GameShell) library, a git submodule in `external/GameShell` (clone with `--recurse-submodules`). |
| [Stello.Engine.Tests](../../Stello.Net/Stello.Engine.Tests) | xUnit tests of the engine: perft, patterns and folded weights, search, endgame solver (FFO positions) and its bit tricks, book, book learning, book recalculation, the benchmark positions, and the evaluation and recorded searches of the frozen Stello 1. |
| [Stello.Net.Tests](../../Stello.Net/Stello.Net.Tests) | xUnit tests of the view models, with fake dialogs, settings and book storage. |
| [Stello.BookTool](../../Stello.Net/tools/Stello.BookTool) | Console tool for the master opening book: `import`, `format`, `build`, `verify`, `stats`, `recalc` (search the leaves again), `compare` and `match` (chapter 12). |
| [Stello.Benchmark](../../Stello.Net/tools/Stello.Benchmark) | Console tool that measures the engine: `match` between engine settings or engines, the FFO endgame benchmark `solve-bench`, the midgame benchmark `mid-bench`, which also checks that a change kept the search unchanged, `eval-compare` and `mid-quality` for the evaluation and the move quality, and the commands that write their position files ([chapter 17](17-measuring-the-engine.md)). Its sides can be Stello 2.0, the frozen Stello 1 or the Egaroucid port ([chapter 19](19-stello-1-against-egaroucid.md)). |
| [Stello.Engine.V1](../../Stello.Net/Stello.Engine.V1) | The frozen baseline: the engine before Stello 2.0 (its search, evaluation, move ordering, time control and `ComputerPlayer`, on `Stello.Engine`'s board and rules), unchanged so that matches and benchmarks always compare against the same engine (`engine=stello-v1`). Tools and tests only; see the [Stello 1 documentation](stello-1/01-overview.md). |
| [Stello.Trainer](../../Stello.Net/tools/Stello.Trainer) | Console tool that trains the pattern evaluation from Egaroucid's training data (not in the repository): `convert`, `train`, `report` and `size` (chapter 04), `mpc`, which fits Multi-ProbCut's error model for the midgame search (chapter 06), and `mpc-end`, which fits one for a selective endgame search that the engine does not use ([chapter 23](23-phase-5-the-search.md)). |
| [Egaroucid.Engine](../../Stello.Net/Egaroucid.Engine) | A C# port of the Egaroucid engine (© Takuto Yamana, GPL-3.0-or-later), exact node for node on 1 thread, for comparing engines ([chapter 18](18-the-egaroucid-port.md)). Tools and tests only. |
| [Egaroucid.Engine.Tests](../../Stello.Net/Egaroucid.Engine.Tests) | xUnit tests that compare the port with the C++ original, and the match adapter. Skipped when Egaroucid's resources are missing ([chapter 18](18-the-egaroucid-port.md)). |
| [tools/Egaroucid.Reference](../../Stello.Net/tools/Egaroucid.Reference) | Not a .NET project: the C++ reference harness that builds the original Egaroucid and writes the port's test fixtures ([chapter 18](18-the-egaroucid-port.md)). |

The trained evaluation is [Stello.Net/Book/stello-eval.bin](../../Stello.Net/Book/stello-eval.bin) (0.99 MB). `Stello.Engine` embeds it as a resource (`Stello.Engine.stello-eval.bin`, read on first use by `EvalWeights.Default`), so the apps need no extra file (chapter 04). The master opening book is the text file [Stello.Net/Book/opening-book.txt](../../Stello.Net/Book/opening-book.txt). The book tool builds [opening-book.bin](../../Stello.Net/Book/opening-book.bin) from it, which the WPF app links as `Data/OPENING` and the web app copies to `wwwroot/data/OPENING.bin` when it is built. The engine tests use both files, and the C++ file [Stello C++/OPENING](../../Stello%20C++/OPENING) to test the import (chapter 10). The benchmark tool copies the evaluation and the book next to itself, together with [opening-book-stello1.txt](../../Stello.Net/Book/opening-book-stello1.txt), the book a Stello 1 side plays from.

## The engine at a glance

The engine types fall into six groups. "Internal" types are only visible inside the engine and the projects it names in `InternalsVisibleTo` (the tests, the tools and `Stello.Engine.V1`).

| Group | Type | Visibility | Role |
|---|---|---|---|
| Board and rules | [`Board`](../../Stello.Net/Stello.Engine/Board.cs) | public | An immutable position: two 64-bit bitboards, one per colour. |
| | [`Square`](../../Stello.Net/Stello.Engine/Square.cs) | public | A square 0–63 (a1 = 0, h8 = 63), with text ("f5") and legacy conversions. |
| | [`Player`](../../Stello.Net/Stello.Engine/Player.cs) | public | `Black` or `White`. |
| | [`Bitboards`](../../Stello.Net/Stello.Engine/Bitboards.cs) | internal | Legal moves, flips and potential mobility on raw 64-bit masks. |
| Game record | [`Move`](../../Stello.Net/Stello.Engine/Move.cs) | public | A square or a pass. |
| | [`Game`](../../Stello.Net/Stello.Engine/Game.cs) | public | The moves and positions of a game, with undo and redo. |
| | [`GameRecordFormat`](../../Stello.Net/Stello.Engine/GameRecordFormat.cs) | public | Reads and writes a game as a text move list. |
| Evaluation | [`PatternShape`, `PatternSet`](../../Stello.Net/Stello.Engine/Evaluation/Patterns.cs) | internal | The patterns of squares, their places on the board and their base-3 indices. |
| | [`Symmetry`](../../Stello.Net/Stello.Engine/Evaluation/Symmetry.cs) | internal | The 8 symmetries of the board on bitboards. |
| | [`EvalWeights`](../../Stello.Net/Stello.Engine/Evaluation/EvalWeights.cs) | internal | The trained weights per stage, the file format, and the evaluation of a position; `Default` is the built-in file. |
| Search | [`SearchEngine`](../../Stello.Net/Stello.Engine/SearchEngine.cs) | public | Iterative deepening principal variation search with Multi-ProbCut and, in the partial file [Endgame/SearchEngine.Endgame.cs](../../Stello.Net/Stello.Engine/Endgame/SearchEngine.Endgame.cs), the endgame solver. |
| | [`MoveOrdering`](../../Stello.Net/Stello.Engine/Search/MoveOrdering.cs) | internal | Sorts moves near the leaves by hash move, response table and square values. |
| | [`TranspositionTable`](../../Stello.Net/Stello.Engine/Search/TranspositionTable.cs) | internal | Remembers results for positions already searched. |
| | [`TimeControl`, `TimeBudget`](../../Stello.Net/Stello.Engine/Search/TimeControl.cs) | internal | Turns the time settings into a soft and a hard time limit. |
| | [`SearchOptions`](../../Stello.Net/Stello.Engine/Search/SearchOptions.cs) | internal | Switches for experiments (another evaluation, the soft time share, Multi-ProbCut off or another t); the apps use `Default`. |
| | [`SearchLimits`](../../Stello.Net/Stello.Engine/SearchLimits.cs) | public | Fixed depth, time per move, time per game, or solve; opening book toggle and transposition table capacity. |
| | [`SearchResult`, `SearchInfo`, `ScoreKind`](../../Stello.Net/Stello.Engine/SearchResult.cs) | public | The result of a search, progress reports, and what kind of score it is. |
| Endgame solver | [`EndgameBits`](../../Stello.Net/Stello.Engine/Endgame/EndgameBits.cs) | internal | The solver's own legal moves and flips, parity, mobility counts and stability thresholds. |
| | [`StableDiscs`](../../Stello.Net/Stello.Engine/Endgame/StableDiscs.cs) | internal | Discs that can never be flipped, for the stability cutoff. |
| | [`LocalTable`](../../Stello.Net/Stello.Engine/Endgame/LocalTable.cs) | internal | A small table of bounds for the null-window solver at 7 to 13 empty squares. |
| Opening book | [`OpeningBook`](../../Stello.Net/Stello.Engine/OpeningBook.cs) | public | Loads, saves and looks up the book. |
| | [`BookEntry`, `BookOrigin`, `BookEffort`](../../Stello.Net/Stello.Engine/BookEntry.cs) | internal | One book move, with its value, where the value comes from, and how hard it was searched for. |
| | [`BookTextFormat`, `BookBinaryFormat`, `LegacyBookFormat`](../../Stello.Net/Stello.Engine/BookTextFormat.cs) | internal | The text book, the binary book, and the import of the C++ book. |
| | [`BookTracker`](../../Stello.Net/Stello.Engine/BookTracker.cs) | public | Decides when to stop asking the book. |
| | [`ComputerPlayer`](../../Stello.Net/Stello.Engine/ComputerPlayer.cs) | public | Chooses the computer's move: the book first, then the search. |
| | [`BookLearner`](../../Stello.Net/Stello.Engine/BookLearner.cs) | public | Adds games to the book, evaluates it, and plays self-play games. |
| | [`BookRecalculator`](../../Stello.Net/Stello.Engine/BookRecalculator.cs), [`BookMinimax`](../../Stello.Net/Stello.Engine/BookMinimax.cs), [`BookComparison`](../../Stello.Net/Stello.Engine/BookComparison.cs), [`BookMatch`](../../Stello.Net/Stello.Engine/BookMatch.cs) | internal | The book tool's work: searching the leaves again, backing the values up, comparing two books and playing them against each other (chapter 12). |

### Board and game types

A `Board` is only 16 bytes, so a game simply keeps a copy of the board after every move. `Board` uses `Bitboards` for the rules.

```mermaid
classDiagram
    class Board {
        <<record struct>>
        +ulong Black
        +ulong White
        +LegalMoves(player) ulong
        +Flips(player, square) ulong
        +Play(player, square) Board
        +IsGameOver bool
    }
    class Square {
        <<record struct>>
        +int Index
        +ToLegacy() int
    }
    class Move {
        <<record struct>>
        +Square? Square
        +IsPass bool
    }
    class Game {
        +int Ply
        +Board Board
        +Player ToMove
        +Play(move)
        +Pass()
        +Undo()
        +Redo()
    }
    class GameRecordFormat {
        <<static>>
        +Format(game) string
        +Parse(text) Game
    }
    class Bitboards {
        <<internal static>>
        +LegalMoves(own, opponent) ulong
        +Flips(own, opponent, square) ulong
    }
    Game o-- Board : one per ply
    Game o-- Move : one per ply
    Move --> Square
    Board ..> Bitboards : uses
    GameRecordFormat ..> Game : reads and writes
```

### The brain types

`ComputerPlayer` is the entry point for choosing a move. It asks the book first and searches if the book has no move. `BookLearner` uses the same parts to improve the book.

```mermaid
classDiagram
    class ComputerPlayer {
        +BookTracker BookTracker
        +ChooseMove(board, player, limits, ...) SearchResult
    }
    class BookTracker {
        +ShouldConsult bool
        +Record(found)
        +Reset()
    }
    class OpeningBook {
        +NodeCount int
        +PositionCount int
        +TryGetMove(board, player, random, move) bool
        +Save(path)
    }
    class BookEntry {
        <<internal>>
        +Move Move
        +short Value
        +BookOrigin Origin
        +BookEffort Effort
    }
    class SearchEngine {
        +Search(board, player, limits, ...) SearchResult
        +ClearHash()
    }
    class TranspositionTable {
        <<internal>>
        +TryGet(own, opponent, tag, entry) bool
        +Store(own, opponent, tag, depth, bound, value, move)
    }
    class MoveOrdering {
        <<internal>>
        +Order(moves, hashMove, previousMove, player, board)
    }
    class EvalWeights {
        <<internal>>
        +Default EvalWeights
        +Evaluate(own, opponent) int
    }
    class PatternSet {
        <<internal>>
        +Indices(own, opponent, indices)
    }
    class TimeControl {
        <<internal static>>
    }
    class BookLearner {
        +AddGame(moves, result)
        +EvaluatePositions()
        +Minimax()
        +SelfPlay()
    }
    ComputerPlayer *-- BookTracker
    ComputerPlayer --> OpeningBook : 1. asks
    ComputerPlayer --> SearchEngine : 2. searches
    OpeningBook *-- BookEntry : moves of each position
    SearchEngine *-- TranspositionTable : midgame and endgame
    SearchEngine *-- MoveOrdering
    SearchEngine ..> EvalWeights
    SearchEngine ..> TimeControl
    EvalWeights ..> PatternSet
    BookLearner --> OpeningBook : changes
    BookLearner --> SearchEngine : evaluates positions
    BookLearner ..> ComputerPlayer : self-play games
```

## The life of one computer move

This sequence shows what happens from the moment it is the computer's turn until its move is on the board. The view model runs on the UI thread; `ComputerPlayer` and `SearchEngine` run on a background task, so the window stays responsive while the computer thinks. This is the desktop app, where `LocalEngineHost` starts the task; in the web app the same steps run in a Web Worker (chapter 14).

```mermaid
sequenceDiagram
    participant VM as MainViewModel (UI thread)
    participant CP as ComputerPlayer (background task)
    participant BT as BookTracker
    participant OB as OpeningBook
    participant SE as SearchEngine
    Note over VM: RunAsync checks: game over? must pass? human to move?
    VM->>CP: IEngineHost.ChooseMoveAsync: Task.Run(ChooseMove(board, player, limits, progress, tokens))
    CP->>BT: UseOpeningBook & ShouldConsult?
    opt the book is enabled and still in use
        CP->>OB: TryGetMove(board, player, random)
        OB-->>CP: found or not found
        CP->>BT: Record(found)
    end
    alt a book move was found
        CP-->>VM: SearchResult (ScoreKind.Book)
    else no book move
        CP->>SE: Search(board, player, limits, progress, tokens)
        Note over SE: iterative deepening (chapter 06),<br/>then the endgame solver (chapter 08)
        loop after each root move, and every 100 ms
            SE-->>VM: SearchInfo through Progress (posted to the UI thread)
        end
        SE-->>CP: SearchResult (Heuristic, WinLossDraw or Exact)
        CP-->>VM: SearchResult
    end
    Note over VM: Game.Play(move), then update the clock and the analysis panel
    Note over VM: RunAsync goes on with the next turn
```

The code for this is `MainViewModel.RunAsync` and `MainViewModel.ComputerMoveAsync` in [MainViewModel.cs](../../Stello.Net/Stello.App/ViewModels/MainViewModel.cs), `LocalEngineHost.ChooseMoveAsync` in [LocalEngineHost.cs](../../Stello.Net/Stello.App/Services/LocalEngineHost.cs), [`ComputerPlayer.ChooseMove`](../../Stello.Net/Stello.Engine/ComputerPlayer.cs) and [`SearchEngine.Search`](../../Stello.Net/Stello.Engine/SearchEngine.cs). If the table size in the settings differs from the engine's (`SearchLimits.HashBits`), the host first creates a new engine of that size (chapter 07).

Inside `SearchEngine.Search`, a move is found in these steps:

1. **No search when there is no choice.** Without a legal move the result is a pass; with exactly one legal move it is played at once (`ScoreKind.None`).
2. **Preparation.** The response table is cleared, the root moves are ordered by their square values (chapter 05), and `TimeControl` gives the soft and the hard time limit (chapter 09).
3. **Midgame iterations.** Depth 1, 2, 3, … plies, each a principal variation search with the hash table (chapter 07), Multi-ProbCut, enhanced transposition cutoffs and move ordering; the leaves are scored by the pattern evaluation in 1/100 disc (chapters 04 and 06). After each iteration the search stops if the depth or the soft time limit is reached.
4. **The endgame solver.** When the next iteration would end with 7 or fewer empty squares on the board, the solver takes over: first win/loss/draw, then, if there is time, the exact disc difference (chapter 08).
5. **The result.** If the hard limit or Move Now stops the search, the best move found so far is returned; cancel throws instead.

Two tokens control a running search:

- **cancel**: used by New Game, Open, Back and the other commands that change the game. The search stops and nothing is played.
- **move now**: used by the "Move Now" command. The search stops and the best move found so far is played.

## Design principles

- **Immutable boards.** `Board` is a 16-byte value. Making a move returns a new board, so the search needs no "undo move" code and the game record can store every position.
- **Bitboards.** Each colour is one 64-bit number with one bit per square. Legal moves and flips are computed for all squares at once with shifts and masks.
- **No global state.** All state is in objects: the search engine owns its hash tables and move-ordering tables, the book owns its positions. Several engines could run side by side.
- **The engine does not know the UI.** It reports progress through `IProgress<SearchInfo>` and is stopped with cancellation tokens. This is why it can be tested without a window.
- **A synchronous engine.** `SearchEngine.Search` runs to the end on the calling thread. The app decides where it runs (a background task); the tests call it directly.
- **Two ancestors.** The frame of the midgame search (iterative deepening, the hash table heights, the response table, the root reordering), the time control and the book come from the C++ original, with modern data structures ([Stello porting documentation.md](../../Plans/Stello%20porting%20documentation.md)). The pattern evaluation, the endgame solver, Multi-ProbCut and the evaluation ordering follow Egaroucid, with the weights and error models trained and fitted for Stello itself.
- **Measured, not assumed.** A change is kept only when engine matches or benchmarks show a gain ([chapter 16](16-the-goal-and-the-plan.md)). The frozen baseline in `Stello.Engine.V1` is checked by recorded searches, so there is always the same engine to measure against.

## Some numbers

| Item | Value |
|---|---|
| Size of a `Board` | 16 bytes (two `ulong`) |
| Pattern evaluation | 15 patterns in 62 places, 10 stages, 233,676 weights per stage; file 0.99 MB |
| Scores | 1/100 disc for the side to move; a finished game ±(32 600 + disc difference); the solver in whole discs |
| Hash tables | 2 tables (midgame, endgame), configurable from 65K to 16.7M entries per table ($3\text{ MB}$ to $768\text{ MB}$ total), default $2^{19}$ slots of 2 entries of 24 bytes ($48\text{ MB}$ total) |
| Master opening book | 11 200 positions, 22 878 book moves, all leaves searched for 60 s by the Stello 2.0 search (`OpeningBook.EngineVersion` 4); binary file 193 KB |
| Engine tests / app tests | 255 / 83 (and 34 for the Egaroucid port) |

---

Previous: [Index](README.md) · Next: [02 Board and squares](02-board-and-squares.md)
