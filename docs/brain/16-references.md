# 16 – References

[Back to the index](README.md)

Articles, papers and source code on the internet that explain the techniques used by Stello in more depth. All links were opened and checked when they were added.

<!-- Keep grouped by chapter; add each external link used in a chapter here too. -->

## General

| Link | What you find there |
|---|---|
| [Reversi – Wikipedia](https://en.wikipedia.org/wiki/Reversi) | The rules, the start position, the square notation (a1 top-left) and scoring. |
| [Computer Othello – Wikipedia](https://en.wikipedia.org/wiki/Computer_Othello) | An overview of search, evaluation and opening books in Othello programs, and their history. |
| [Othello – Chess Programming Wiki](https://www.chessprogramming.org/Othello) | Othello programming: search, evaluation, bitboards, programs and papers. |
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | A short guide by the author of Zebra: search, selective search, evaluation, opening books, and fastest-first in the endgame. |
| [Edax on GitHub](https://github.com/abulmo/edax-reversi) | The source of a very strong open-source bitboard Othello engine, for comparison. |

## 01 Overview

| Link | What you find there |
|---|---|
| [Introduction to the MVVM Toolkit – Microsoft Learn](https://learn.microsoft.com/dotnet/communitytoolkit/mvvm/) | `CommunityToolkit.Mvvm`, used by the app's view models. |

## 02 Board and squares, 03 Rules and move generation

| Link | What you find there |
|---|---|
| [Bitboards – Chess Programming Wiki](https://www.chessprogramming.org/Bitboards) | What bitboards are and the basic operations on them. |
| [Perft for Reversi – Aart Bik](http://www.aartbik.com/MISC/reversi.html) | Perft numbers for Othello from the start position; Stello's perft test uses the same numbers up to depth 8. |

## 05 Evaluation

| Link | What you find there |
|---|---|
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | "Position evaluation": disk-square tables, mobility-based evaluation (like Stello's) and pattern-based evaluation. |
| [Computer Othello – Wikipedia](https://en.wikipedia.org/wiki/Computer_Othello) | Evaluation techniques, including mobility and potential mobility. |
| [Othello – Chess Programming Wiki](https://www.chessprogramming.org/Othello) | The "Evaluation" section, with references to IAGO (edge stability, mobility, potential mobility). |

## 06 Move ordering

| Link | What you find there |
|---|---|
| [Alpha-Beta – Chess Programming Wiki](https://www.chessprogramming.org/Alpha-Beta) | "Savings": why searching the best move first gives the most cutoffs. |
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | "Move ordering": killer responses in Othello. |

## 07 Midgame search

| Link | What you find there |
|---|---|
| [Alpha-Beta – Chess Programming Wiki](https://www.chessprogramming.org/Alpha-Beta) | Alpha-beta, fail-soft and fail-hard, in negamax form. |
| [Principal Variation Search – Chess Programming Wiki](https://www.chessprogramming.org/Principal_Variation_Search) | PVS: null-window searches after the first move, and re-searches. |
| [ProbCut – Chess Programming Wiki](https://www.chessprogramming.org/ProbCut) | ProbCut and Multi-ProbCut, the statistical forms of the selective search idea. |
| [Buro (1995): ProbCut: An Effective Selective Extension of the Alpha-Beta Algorithm (pdf)](https://skatgame.net/mburo/ps/probcut.pdf) | The original ProbCut paper. |
| [Negamax – Wikipedia](https://en.wikipedia.org/wiki/Negamax) | Negamax with alpha-beta and with a transposition table, in pseudocode. |
| [Fail-Soft – Chess Programming Wiki](https://www.chessprogramming.org/Fail-Soft) | Scores outside the window, and why they help. |
| [Null Window – Chess Programming Wiki](https://www.chessprogramming.org/Null_Window) | Null-window (zero-window) searches as a yes/no test. |
| [Iterative Deepening – Chess Programming Wiki](https://www.chessprogramming.org/Iterative_Deepening) | Iterative deepening for time control and move ordering. |
| [Odd-Even Effect – Chess Programming Wiki](https://www.chessprogramming.org/Odd-Even_Effect) | Why node counts and scores differ between odd and even depths. |
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | "Searching": alpha-beta, move ordering and selective search in Othello. |

## 08 Transposition table

| Link | What you find there |
|---|---|
| [Transposition Table – Chess Programming Wiki](https://www.chessprogramming.org/Transposition_Table) | What is stored, bounds, collisions, and replacement schemes, including the two-tier system Stello uses. |
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | "Transposition tables", with the tiger-opening example. |

## 09 Endgame solver

| Link | What you find there |
|---|---|
| [Enhanced Transposition Cutoff – Chess Programming Wiki](https://www.chessprogramming.org/Enhanced_Transposition_Cutoff) | Looking up the children in the hash table before searching them. |
| [endgame.c – radagast.se](http://radagast.se/othello/endgame.c) | A small endgame solver (Smith, Weill, Andersson) with region parity, a fixed square order, special code for the last squares and 112 test positions. |
| [The FFO endgame test suite – radagast.se](http://radagast.se/othello/ffotest.html) | The test positions #40–#59, their correct scores and moves, and Zebra's times. |
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | "Endgame": why ordering decides everything, and fastest-first. |
| [Edax on GitHub](https://github.com/abulmo/edax-reversi) | A much faster open-source bitboard endgame solver. |

## 10 Time control

| Link | What you find there |
|---|---|
| [Iterative Deepening – Chess Programming Wiki](https://www.chessprogramming.org/Iterative_Deepening) | Iterative deepening as the basis of time management. |

## 11 Opening book

| Link | What you find there |
|---|---|
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | "Opening knowledge": why an opening book pays off, and how it can be built. |
| [Computer Othello – Wikipedia](https://en.wikipedia.org/wiki/Computer_Othello) | "Opening book": how Othello programs use opening books. |

## 12 Book learning

| Link | What you find there |
|---|---|
| [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) | "Opening knowledge": adding the best move that no game has played to every book position, then minimaxing the book. The same idea as Stello's dropout expansion. |

## 14 App integration

| Link | What you find there |
|---|---|
| [Introduction to the MVVM Toolkit – Microsoft Learn](https://learn.microsoft.com/dotnet/communitytoolkit/mvvm/) | The library behind `ObservableObject`, `[ObservableProperty]` and the commands. |
| [RelayCommand attribute – Microsoft Learn](https://learn.microsoft.com/dotnet/communitytoolkit/mvvm/generators/relaycommand) | Generated commands, `CanExecute`, `NotifyCanExecuteChangedFor`, and why an async command is disabled while it runs. |
| [Progress&lt;T&gt; class – Microsoft Learn](https://learn.microsoft.com/dotnet/api/system.progress-1) | Callbacks run on the `SynchronizationContext` captured when the object is created. |
| [Cancellation in managed threads – Microsoft Learn](https://learn.microsoft.com/dotnet/standard/threading/cancellation-in-managed-threads) | Cooperative cancellation with `CancellationTokenSource` and `OperationCanceledException`. |

## Writing these documents

| Link | What you find there |
|---|---|
| [Mermaid](https://mermaid.js.org/) | The diagram language used in these documents. |
| [Creating diagrams – GitHub Docs](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/creating-diagrams) | How GitHub shows Mermaid diagrams. |
| [Writing mathematical expressions – GitHub Docs](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/writing-mathematical-expressions) | How GitHub shows math formulas. |

---

Previous: [15 Glossary](15-glossary.md) · Next: [Index](README.md)
