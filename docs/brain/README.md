# The Stello brain

Stello is an Othello program. Its "brain" is the engine library `Stello.Engine`, which decides the computer's move. The brain consists of:

- **the board and rules**: bitboards and fast move generation;
- **the evaluation**: a pattern evaluation, 15 patterns of squares with weights trained on 500 million positions, in 10 stages of the game;
- **the search**: alpha-beta with a hash table, move ordering and selective search;
- **the endgame solver**: perfect play near the end of the game, with null-window searches adapted from Egaroucid;
- **the opening book**: the known opening positions, which can learn from games.

These documents explain how the parts work and how they fit together. They describe the current C# code in [Stello.Net](../../Stello.Net). This is Stello 2.0: its evaluation and endgame solver are described in [chapter 21](21-stello-2-evaluation-and-solver.md). Chapters 04 and 08 describe Stello 1's evaluation and solver, which the frozen baseline `Stello.Engine.V1` keeps. How the C++ original was ported is described in [Stello porting documentation.md](../../Plans/Stello%20porting%20documentation.md).

## The brain on one page

This is how the computer finds a move, from the position to the move that is played. Each box is explained in a chapter.

```mermaid
flowchart TD
    Start["Position, side to move and time limits"] --> Tracker{"Book enabled and still in use?<br/>(UseOpeningBook, BookTracker)"}
    Tracker -- "yes" --> Book{"Position in the book?<br/>(OpeningBook)"}
    Book -- "yes" --> BookMove["Play the book move"]
    Book -- "no" --> Single
    Tracker -- "no" --> Single{"Only one legal move?"}
    Single -- "yes" --> Play["Play it at once"]
    Single -- "no" --> Next["Iterative deepening:<br/>next depth (1, 2, 3, ...)"]
    Next --> Near{"Would this depth reach<br/>within 7 plies of the end?"}
    Near -- "no" --> Midgame["Midgame search<br/>alpha-beta, move ordering,<br/>hash table, pattern evaluation"]
    Midgame --> Limit{"Time or depth<br/>limit reached?"}
    Limit -- "no" --> Next
    Limit -- "yes" --> Best["Play the best move found"]
    Near -- "yes" --> WLD["Endgame solver (null-window searches):<br/>win, loss or draw?"]
    WLD --> Exact["Endgame solver:<br/>exact disc difference"]
    Exact --> Best
```

The time limit can stop the search at any point; the best move found so far is then played. In solve mode (`SearchLimits.Solve`) a 4-ply midgame search first orders the moves, then the solver runs. After a draw the exact pass is not needed. The app handles passes before it asks the brain for a move.

## Chapters

| # | Chapter | Content |
|---|---|---|
| 01 | [Overview](01-overview.md) | Projects, main types, the life of one computer move, design principles |
| 02 | [Board and squares](02-board-and-squares.md) | Bitboards, square numbering, the start position |
| 03 | [Rules and move generation](03-rules-and-move-generation.md) | Legal moves with shift-and-mask, flips, pass, game over, perft |
| 04 | [Evaluation](04-evaluation.md) | Stello 1's evaluation: edge tables, corners, stability, mobility |
| 05 | [Move ordering](05-move-ordering.md) | Square values, the response table, the hash move |
| 06 | [Midgame search](06-midgame-search.md) | Negamax alpha-beta, iterative deepening, PVS, selective search |
| 07 | [Transposition table](07-transposition-table.md) | Two-entry slots, bounds, replacement |
| 08 | [Endgame solver](08-endgame-solver.md) | Stello 1's solver: win/loss/draw and exact passes, fastest-first, parity, enhanced transposition cutoff |
| 09 | [Time control](09-time-control.md) | Time modes, time per move, "Move Now" |
| 10 | [Opening book](10-opening-book.md) | Positions in canonical form, the text and binary files, the import of the C++ book, `BookTracker` |
| 11 | [Book learning](11-book-learning.md) | Adding games, dropout expansion, minimax, self-play |
| 12 | [Book tool](12-book-tool.md) | The console tool for the master book: import, build, verify, statistics, recalculating the leaves, comparing and matching books |
| 13 | [Game record](13-game-record.md) | Move history, undo/redo, the text file format |
| 14 | [App integration](14-app-integration.md) | The game loop, threading, engine hosts, WebAssembly worker, settings, book files |
| 15 | [GUI and settings](15-gui-and-settings.md) | Interface layout, menu commands, settings dialog, opening book and TT memory, real-time analysis |
| 16 | [Glossary](16-glossary.md) | The terms used in these documents |
| 17 | [References](17-references.md) | Articles, papers and source code on the internet |
| 18 | [Measuring the engine](18-measuring-the-engine.md) | Engine matches, the FFO endgame benchmark, the midgame benchmark, and the baseline numbers |
| 19 | [The Egaroucid port](19-the-egaroucid-port.md) | The C# port of Egaroucid: licence, pipeline, C++ to C# file map, node-for-node parity with the C++ original, deliberate differences, how to run it |
| 20 | [Comparing Stello with Egaroucid](20-engine-comparison.md) | Matches, evaluation error, midgame quality, midgame and endgame speed; what explains the gap, and what it means for Stello 2.0 |
| 21 | [Stello 2: the pattern evaluation and the new endgame solver](21-stello-2-evaluation-and-solver.md) | Patterns, symmetries and stages; the trainer and its experiments; the file format; the solver's layers by empty squares; the measurements against Stello 1 and Egaroucid; Stello 1 as the baseline; what comes next |

## Reading paths

- **Everything:** read the chapters in order.
- **Just the search:** 01, 02, 03, 21, 05, 06, 07, 09 (04 and 08 for Stello 1).
- **Just the opening book:** 01, 02 (square numbers), 10, 11, 12.
- **Tuning the engine:** 18 (how to measure a change), 21, 05, 06, 07, 09, and phase 8 in [Stello porting documentation.md](../../Plans/Stello%20porting%20documentation.md#phase-8--performance-tuning-rounds-1-and-2-paused) for what has already been tried, with measurements, and the remaining ideas.
- **UI & application architecture:** 01, 13, 14, 15.
- **Comparing engines / Stello 2.0 work:** 18, 19, 20, 21, then [improve-stello-strength-plan.md](../../Plans/improve-stello-strength-plan.md) and [improve-stello-strength-findings.md](../../Plans/improve-stello-strength-findings.md).

## How to view these documents

- **On GitHub:** diagrams and formulas are shown directly.
- **In VS Code:** open the Markdown preview (Ctrl+Shift+V). Formulas are shown by the built-in preview. The diagrams need the extension *Markdown Preview Mermaid Support* (`bierner.markdown-mermaid`).

The diagrams are written in [Mermaid](https://mermaid.js.org/), the formulas in LaTeX math, and the pictures are SVG files in the `images` folder.
