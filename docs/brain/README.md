# The Stello brain

Stello is an Othello program. Its "brain" is the engine library `Stello.Engine`, which decides the computer's move. The brain consists of:

- **the board and rules**: bitboards and fast move generation;
- **the evaluation**: edge tables, corners, mobility;
- **the search**: alpha-beta with a hash table, move ordering and selective search;
- **the endgame solver**: perfect play near the end of the game;
- **the opening book**: a tree of known openings, which can learn from games.

These documents explain how the parts work and how they fit together. They describe the current C# code in [Stello.Net](../../Stello.Net). How the C++ original was ported is described in [Stello porting documentation.md](../../Stello%20porting%20documentation.md).

## The brain on one page

This is how the computer finds a move, from the position to the move that is played. Each box is explained in a chapter.

```mermaid
flowchart TD
    Start["Position, side to move and time limits"] --> Single{"Only one legal move?"}
    Single -- "yes" --> Play["Play it at once"]
    Single -- "no" --> Tracker{"Book still in use?<br/>(BookTracker)"}
    Tracker -- "yes" --> Book{"Position in the book?<br/>(OpeningBook)"}
    Book -- "yes" --> BookMove["Play the book move"]
    Book -- "no" --> Next
    Tracker -- "no" --> Next["Iterative deepening:<br/>next depth (1, 2, 3, ...)"]
    Next --> Near{"Would this depth reach<br/>within 7 plies of the end?"}
    Near -- "no" --> Midgame["Midgame search<br/>alpha-beta, move ordering,<br/>hash table, evaluation"]
    Midgame --> Limit{"Time or depth<br/>limit reached?"}
    Limit -- "no" --> Next
    Limit -- "yes" --> Best["Play the best move found"]
    Near -- "yes" --> WLD["Endgame solver:<br/>win, loss or draw?"]
    WLD --> Exact["Endgame solver:<br/>exact disc difference"]
    Exact --> Best
```

The time limit can stop the search at any point; the best move found so far is then played. After a draw the exact pass is not needed. The app handles passes before it asks the brain for a move.

## Chapters

| # | Chapter | Content |
|---|---|---|
| 01 | [Overview](01-overview.md) | Projects, main types, the life of one computer move, design principles |
| 02 | [Board and squares](02-board-and-squares.md) | Bitboards, square numbering, the start position |
| 03 | [Rules and move generation](03-rules-and-move-generation.md) | Legal moves with shift-and-mask, flips, pass, game over, perft |
| 04 | [Game record](04-game-record.md) | Move history, undo/redo, the text file format |
| 05 | [Evaluation](05-evaluation.md) | Edge tables, corners, stability, mobility |
| 06 | [Move ordering](06-move-ordering.md) | Square values, the response table, the hash move |
| 07 | [Midgame search](07-midgame-search.md) | Negamax alpha-beta, iterative deepening, PVS, selective search |
| 08 | [Transposition table](08-transposition-table.md) | Two-entry slots, bounds, replacement |
| 09 | [Endgame solver](09-endgame-solver.md) | Win/loss/draw and exact passes, fastest-first, parity, enhanced transposition cutoff |
| 10 | [Time control](10-time-control.md) | Time modes, time per move, "Move Now" |
| 11 | [Opening book](11-opening-book.md) | The file format, symmetries, the position index, `BookTracker` |
| 12 | [Book learning](12-book-learning.md) | Adding games, dropout expansion, minimax, self-play |
| 13 | [App integration](13-app-integration.md) | The game loop, threading, settings, book files |
| 14 | [Glossary](14-glossary.md) | The terms used in these documents |
| 15 | [References](15-references.md) | Articles, papers and source code on the internet |

## Pictures

| Picture | Chapter | Shows |
|---|---|---|
| [board-square-index.svg](images/board-square-index.svg) | 02 | The C# square index 0–63 |
| [bitboard-bit-order.svg](images/bitboard-bit-order.svg) | 02 | The bit order of a bitboard and the eight direction shifts |
| [board-legacy-numbers.svg](images/board-legacy-numbers.svg) | 02 | The legacy 10 × 10 numbers with the border ring |
| [start-position.svg](images/start-position.svg) | 02 | The start position and black's legal moves |
| [shift-and-mask.svg](images/shift-and-mask.svg) | 03 | Legal-move generation in one direction, step by step |
| [flips-example.svg](images/flips-example.svg) | 03 | A move that flips in three directions |
| [edges-and-corners.svg](images/edges-and-corners.svg) | 05 | The four edges and their square order, corners, X- and C-squares |
| [edge-index-example.svg](images/edge-index-example.svg) | 05 | Computing the base-3 index of an edge |
| [corner-diagonal.svg](images/corner-diagonal.svg) | 05 | A corner reached along the diagonal, and a possible corner |
| [corner-stability.svg](images/corner-stability.svg) | 05 | Stable discs counted from a corner |
| [square-values.svg](images/square-values.svg) | 06 | Static square values, and the values next to the corners |
| [alpha-beta-tree.svg](images/alpha-beta-tree.svg) | 07 | Alpha-beta cutoffs in a real two-ply search |
| [tt-layout.svg](images/tt-layout.svg) | 08 | Hash table slots, the entry fields and the slot hash |
| [parity-quadrants.svg](images/parity-quadrants.svg) | 09 | Quadrants and parity with six empty squares |
| [book-file-layout.svg](images/book-file-layout.svg) | 11 | The book file layout and the first bytes of the master book |
| [book-symmetries.svg](images/book-symmetries.svg) | 11 | The four first moves, their symmetries to d3 and the book's replies |
| [book-minimax-example.svg](images/book-minimax-example.svg) | 12 | A small book after adding a game, evaluating and minimax |

## Reading paths

- **Everything:** read the chapters in order.
- **Just the search:** 01, 02, 05, 06, 07, 08, 09.
- **Just the opening book:** 01, 02 (square numbers), 11, 12.
- **Tuning the engine:** 05, 06, 07, 08, 09, 10, and phase 8 in [the specification](../../Migrate%20Othello%20game%20from%20C++%20to%20C%23.md) for what has already been tried.
- **Changing the app:** 01, 10, 13.

## How to view these documents

- **On GitHub:** diagrams and formulas are shown directly.
- **In VS Code:** open the Markdown preview (Ctrl+Shift+V). Formulas are shown by the built-in preview. The diagrams need the extension *Markdown Preview Mermaid Support* (`bierner.markdown-mermaid`).

The diagrams are written in [Mermaid](https://mermaid.js.org/), the formulas in LaTeX math, and the pictures are SVG files in the [images](images) folder.
