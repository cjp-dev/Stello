# Stello brain documentation – specification

This document specifies a set of documents that explain the "brain" of Stello.Net: the algorithms, the data structures, the data (edge tables, opening book) and how they work together. The documentation uses **Markdown, Mermaid diagrams, math formulas and hand-made SVG pictures**. It links to the source code in this repository and to articles, wikis and source code on the internet where that helps the reader.

## 1. Purpose

- Explain how the C# engine plays Othello, so that a developer can understand, change and tune it without first reading all of the source.
- Use diagrams, pictures and worked examples wherever they make a point easier to understand than text.
- Be a companion to the source code, not a copy of it. The documentation explains *why* and *how*; the code shows the details.

## 2. Audience

- **Primary:** a C# developer who knows the rules of Othello, but not game-tree search or Othello engines.
- **Secondary:** the author, as a reference when working on phase 8 (performance) or on the evaluation.
- The reader is not expected to know the C++ version. C++ is only mentioned where it explains a design choice or a kept quirk; the details are in [Stello porting documentation.md](Stello%20porting%20documentation.md).

## 3. Scope

**In scope:**

- `Stello.Engine`: the board, rules, game record, evaluation, move ordering, midgame search, transposition table, endgame solver, time control, opening book, book learning and `ComputerPlayer`.
- The data: the edge tables (`EdgeTables.cs`) and the opening book file (`Data/OPENING`).
- How the app drives the engine: `MainViewModel` states, threading, cancellation, "Move Now", settings to `SearchLimits`, and where the book is loaded from and saved to.

**Out of scope:**

- WPF layout, styling and XAML details.
- A line-by-line description of the code.
- A comparison with the C++ version (see the porting documentation).
- Any change to the code. Writing the documentation must not change source files, tests or project files.

## 4. Relation to the other documents

| Document | Describes |
|---|---|
| [Migrate Othello game from C++ to C#.md](Migrate%20Othello%20game%20from%20C++%20to%20C%23.md) | Requirements for the port, phase by phase. |
| [Stello porting documentation.md](Stello%20porting%20documentation.md) | How each part of the C++ code was ported: 1:1, changed, or a kept quirk. |
| `docs/brain/` (this specification) | How the C# brain works today, explained for a new reader. |

The brain documentation links to the porting documentation for C++ history instead of repeating it.

## 5. Format and tools

### 5.1 Markdown

- GitHub Flavored Markdown (tables, fenced code blocks, task lists not needed).
- It must render correctly in two places:
  - **GitHub** (the repository is hosted on GitHub).
  - **VS Code's Markdown preview.** Mermaid needs the extension *Markdown Preview Mermaid Support* (`bierner.markdown-mermaid`). Math is supported by the built-in preview.
- Headings: one `#` title per file, then `##` sections and `###` subsections. No deeper than `####`.
- Text in English, short sentences, the same terms as the code (see section 8).

### 5.2 Mermaid diagrams

- Written in fenced code blocks with the language `mermaid`.
- Allowed diagram types (supported by GitHub and the VS Code extension): `flowchart`, `sequenceDiagram`, `stateDiagram-v2`, `classDiagram`. Types marked *beta* or *experimental* in the [Mermaid documentation](https://mermaid.js.org/) are not used, because GitHub may run an older Mermaid version.
- No custom themes or colours (`%%{init}%%`), so that the diagrams follow GitHub's light and dark themes.
- Node labels with special characters (`(`, `)`, `<`, `>`, `≥`, `:`) are quoted: `A["empties ≥ 18"]`.
- One idea per diagram, and at most about 25 nodes. Split larger diagrams.
- Every diagram has a sentence before it that says what it shows.

### 5.3 Math

- Inline math with `$…$`, block math with `$$…$$` on lines of their own.
- GitHub renders math with MathJax, and VS Code with KaTeX. Only the common subset is used: fractions, sums, subscripts, `\cdot`, `\max`, `\min`, `\lfloor … \rfloor`, `\begin{aligned}`, `\text{}`. No `\newcommand`, no `\begin{align}` (use `aligned` inside `$$`), no packages.
- Every formula is followed by a sentence that names its variables and the code constant it comes from, for example "400 is `CurrentMobilityWeight`".

### 5.4 SVG pictures

- Hand-written SVG files in `docs/brain/images/`, included with `![alt text](images/name.svg)`.
- Rules:
  - `viewBox` set, no fixed pixel size above 800 wide, so the picture scales.
  - A solid white background rectangle, so the picture is readable in GitHub's dark theme.
  - No scripts, no external references, no embedded bitmaps, no web fonts. Fonts: `font-family="sans-serif"` or `monospace`.
  - Text is real `<text>` (searchable, sharp at any size), at least 12 units high.
  - `<title>` and `<desc>` elements describing the picture (accessibility).
  - Valid XML, and at most about 50 KB per file.
  - The alt text in the Markdown says what the picture shows, not just its name.
- Common style, so all board pictures look alike:

  | Element | Style |
  |---|---|
  | Board | green `#1B7F3B`, grid lines `#0B4D22`, a1 in the **top-left** corner (as in the app and in [Reversi on Wikipedia](https://en.wikipedia.org/wiki/Reversi)) |
  | Coordinates | a–h above, 1–8 to the left |
  | Black disc | fill `#111111` |
  | White disc | fill `#F5F5F5`, stroke `#333333` |
  | Legal move | small dot `#FFC107` |
  | Flipped disc / highlighted square | red outline `#D32F2F` |
  | Square numbers | `monospace`, dark grey on light squares |

### 5.5 Other elements

- Tables for constants, file layouts and comparisons.
- Code snippets (`csharp`) at most about 25 lines, copied exactly from the code, with a link to the file. For long algorithms, pseudocode in a `text` block is preferred.
- Hex dumps (`text` blocks) for the book file.
- GitHub alerts (`> [!NOTE]`, `> [!WARNING]`) are allowed for kept quirks and pitfalls; they show as plain quotes in VS Code, which is acceptable.

## 6. File structure

```text
docs/brain/
  README.md                   index, reading order, "the brain on one page"
  01-overview.md
  02-board-and-squares.md
  03-rules-and-move-generation.md
  04-game-record.md
  05-evaluation.md
  06-move-ordering.md
  07-midgame-search.md
  08-transposition-table.md
  09-endgame-solver.md
  10-time-control.md
  11-opening-book.md
  12-book-learning.md
  13-app-integration.md
  14-glossary.md
  15-references.md
  images/
    *.svg
```

- File names in lower case with hyphens, numbered in reading order.
- Every chapter ends with "Previous / Next" links and a link back to `README.md`.

## 7. Chapter template

Each chapter (02–13) uses these sections, in this order. A section may be left out if it has no content.

1. **In short** – three to five sentences: what the part does and why it matters.
2. **Data structures** – types, fields, sizes, with a class diagram or a picture.
3. **Algorithm** – step by step, with a flowchart, pseudocode and formulas.
4. **Worked example** – a concrete position or file, with the real values (see section 12).
5. **Design notes** – why it is done this way, trade-offs, kept C++ quirks (with a link to the porting documentation), and ideas tried and rejected (for phase 8 items, a link to the specification).
6. **Where in the code** – a table of files and main members.
7. **Tests** – which tests cover the part, with links.
8. **Further reading** – external links (see section 9).

## 8. Writing conventions

- **Terms:** the same names as the code (`Board`, `Square`, `SearchEngine.Solve`, `BookTracker`, …) in backticks. A term is explained when it is first used in a chapter, and it is in the glossary.
- **Squares:** always written as text ("f5"). The C# index (0–63) is added where bit operations are shown. The legacy number (a1 = 11, h8 = 88) is only used in the evaluation and book chapters, and always together with the text form.
- **Colours:** "Black" and "White" for the players; X = black, O = white in board strings, as in `Board.ToString`.
- **Scores:** say the unit every time: evaluation units (midgame), disc difference (endgame), or book value. The point of view is always the side to move (negamax), unless stated otherwise.
- **Depth:** say whether it is plies or the C++ `look` value (remaining depth after the current move).
- **Constants:** give the name and the value, for example `EndgameDistance` = 7. The values are taken from the code when the chapter is written.
- **C++ references:** only where they explain something; as a relative link to the file in `Stello C++/` and the function name.

## 9. Linking rules

### 9.1 Links into the repository

- Relative links from `docs/brain/` to source files, for example `[SearchEngine.cs](../../Stello.Net/Stello.Engine/SearchEngine.cs)`. They work both on GitHub and in VS Code.
- Spaces in paths are written as `%20`, for example `../../Stello%20C++/BRAIN/Eval.cpp`.
- **No line-number anchors** (`#L120`), because they break when the code changes. Name the member instead (`SearchEngine.Solve`).
- Links between chapters use relative file links and heading anchors (`07-midgame-search.md#selective-search`).

### 9.2 External links

- External links are used where a technique has a good general description, so the chapter can focus on how Stello uses it. Examples: alpha-beta, principal variation search, ProbCut, transposition tables, enhanced transposition cutoff, bitboards, the FFO test suite.
- Preferred sources, in this order:
  1. [Chess Programming Wiki](https://www.chessprogramming.org/) (covers Othello techniques too);
  2. papers by the authors of a technique (for example Michael Buro for ProbCut);
  3. pages by authors of strong Othello programs (Gunnar Andersson's pages on radagast.se);
  4. Wikipedia;
  5. source code of open-source Othello engines on GitHub (for example [Edax](https://github.com/abulmo/edax-reversi)).
- Links to external source code point to the repository or to a file on the default branch, never to a line number.
- Every external link is opened and checked when it is added. The link text says what the reader will find ("ProbCut on the Chess Programming Wiki"), not "here".
- All external links are also collected in `15-references.md`, grouped by chapter.

### 9.3 Checked starting set of external links

These links were opened and checked while writing this specification. More may be added.

| Topic | Link |
|---|---|
| Othello programming overview | [Othello – Chess Programming Wiki](https://www.chessprogramming.org/Othello) |
| Writing an Othello program (search, evaluation, book, endgame, fastest-first) | [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html) |
| Rules, notation, start position | [Reversi – Wikipedia](https://en.wikipedia.org/wiki/Reversi) |
| Principal variation search | [Principal Variation Search – CPW](https://www.chessprogramming.org/Principal_Variation_Search) |
| Selective search | [ProbCut – CPW](https://www.chessprogramming.org/ProbCut) |
| ProbCut paper | [Buro (1995), ProbCut: An Effective Selective Extension of the Alpha-Beta Algorithm (pdf)](https://skatgame.net/mburo/ps/probcut.pdf) |
| Transposition tables, replacement schemes (two-tier) | [Transposition Table – CPW](https://www.chessprogramming.org/Transposition_Table) |
| Enhanced transposition cutoff | [Enhanced Transposition Cutoff – CPW](https://www.chessprogramming.org/Enhanced_Transposition_Cutoff) |
| Endgame test positions | [The FFO endgame test suite – radagast.se](http://radagast.se/othello/ffotest.html) |
| A strong open-source bitboard engine | [Edax on GitHub](https://github.com/abulmo/edax-reversi) |
| Mermaid on GitHub | [Creating diagrams – GitHub Docs](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/creating-diagrams) |
| Math on GitHub | [Writing mathematical expressions – GitHub Docs](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/writing-mathematical-expressions) |

Candidates to check before use: CPW pages *Alpha-Beta*, *Negamax*, *Iterative Deepening*, *Bitboards*, *Move Ordering*, *Killer Heuristic*, *Fail-Soft*, *Aspiration Windows*, *MTD(f)*; Wikipedia *Negamax*, *Alpha–beta pruning*, *Computer Othello*; [Perft for Reversi by Aart Bik](http://www.aartbik.com/MISC/reversi.html); the [CommunityToolkit.Mvvm documentation](https://learn.microsoft.com/dotnet/communitytoolkit/mvvm/).

## 10. Content per chapter

The lists below are what each chapter **must** cover. "Mermaid", "SVG" and "Math" say which kind of figure is expected.

### README.md – index

- What the brain is, in one paragraph.
- **The brain on one page:** a Mermaid flowchart from "position + time" to "move": book → (midgame search with evaluation and hash table | endgame solver) → move, with links to the chapters.
- Reading order, and short paths for three readers: "just the search", "just the book", "tuning the engine".
- How to view the documents (GitHub, or VS Code with the Mermaid extension).

### 01 – Overview

- The four projects and what depends on what (Mermaid flowchart).
- The main engine types and their relations (Mermaid class diagram): `Board`, `Square`, `Player`, `Move`, `Game`, `SearchEngine`, `SearchLimits`, `SearchResult`, `SearchInfo`, `OpeningBook`, `BookTracker`, `ComputerPlayer`, `BookLearner`, and the internal `Bitboards`, `Evaluator`, `EdgeTables`, `MoveOrdering`, `TranspositionTable`, `TimeControl`, `BookNode`.
- **The life of one computer move** (Mermaid sequence diagram): `MainViewModel` → `Task.Run` → `ComputerPlayer.ChooseMove` → `BookTracker`/`OpeningBook.TryGetMove` → `SearchEngine.Search` → `IProgress<SearchInfo>` → `SearchResult` → `Game.Play` → board refresh.
- Design principles: immutable 16-byte boards, no global state, engine without UI dependency, synchronous engine run on a background task.

### 02 – Board and squares

- `Board` as two 64-bit bitboards; why this is small and fast; immutability (no undo code).
- **SVG `board-square-index.svg`:** the board with the C# index 0–63 in each square, a1 top-left.
- **SVG `bitboard-bit-order.svg`:** how bit 0…63 of a `ulong` maps to a1…h8, and how the eight directions become shifts (+1, −1, +8, −8, +7, +9, −7, −9).
- **SVG `board-legacy-numbers.svg`:** the legacy 10×10 numbering with the border ring, used by the book and the edge tables.
- `Square` conversions (text, index, legacy), `Player`, `Board.Parse`/`ToString` (64 characters, `X`/`O`/`-`).
- **SVG `start-position.svg`:** the start position with black's four legal moves marked.

### 03 – Rules and move generation

- Legal-move generation with shift-and-mask (`Bitboards.LegalMoves`): the masks that stop wrap-around at columns a and h (`NotColumnA`, `NotColumnH`, `InnerColumns`, `InnerRows`), and why 6 steps per direction are enough.
- **SVG `shift-and-mask.svg`:** one direction worked step by step: own discs, opponent discs, shifted, masked, result.
- Flips (`Bitboards.Flips`): walking one ray per direction and keeping it only when it ends in an own disc.
- **SVG `flips-example.svg`:** a move that flips in several directions, flipped discs outlined.
- Pass, game over, disc count, winner; the rule that empty squares go to the winner.
- Mermaid flowchart of `Board.Play` (legal? → flips → new board).
- Perft table (depth 1–8) from `PerftTests`, and what perft proves.
- Further reading: bitboards and fill algorithms (CPW), Aart Bik's Reversi perft page.

### 04 – Game record

- `Game`: lists of moves and positions, `Ply`, `Undo`/`Redo`, `Play` after undo cuts off the redo list, `Pass` only when the player must pass, `SwitchSides`, `NewGame`.
- **Mermaid state diagram:** positions and the current ply with undo/redo/play/new game.
- `GameRecordFormat`: the text file ("f5 d6 c3 pass …"), loading by replaying, error messages. A short example file.

### 05 – Evaluation

- The idea: edges and corners decide most Othello games; mobility and potential mobility for the rest.
- **Edge tables:** eight tables of $3^8 = 6561$ values. The index formula:

  $$i = \sum_{k=0}^{7} d_k \cdot 3^{7-k}, \qquad d_k \in \{0 = \text{white}, 1 = \text{black}, 2 = \text{empty}\}$$

  and a table of what each of the eight tables holds (`sikker`, `white_v`/`black_v`, `white_h`/`black_h`, `white_m`/`black_m`, `hjo_trek` flags).
- **SVG `edges-and-corners.svg`:** the four edges with their square order (row 1 a1→h1, column h h1→h8, row 8 a8→h8, column a a1→a8), the corners, X-squares and C-squares.
- **SVG `edge-index-example.svg`:** one real edge, its digits and its computed index, and the looked-up `sikker` value.
- **Edge look-ahead:** a Mermaid flowchart of the two-ply search on the edge tables (base score, opponent replies lower it, own moves raise it), corners reached along the diagonal, and the "potential corner" blend:

  $$\text{score} = \frac{p \cdot s_{\text{with}} + (1000 - p) \cdot s_{\text{without}}}{1000}$$

  with the formula for $p$ from the code.
- **SVG `corner-diagonal.svg`:** a corner reached along the diagonal through the X-square.
- Corner stability (60 points per stable disc) with a picture of the counted discs.
- Lazy cut-off (why 1000 is the bound), mobility and potential mobility formulas with the weights `CurrentMobilityWeight` (400) and `PotentialMobilityWeight` (600).
- `Evaluator.IsDangerous` and why a dangerous corner move is searched instead of evaluated.
- The kept C++ quirk (`score = min(tscore, escore)`) as a `[!NOTE]`, with a link to the porting documentation.
- Worked example: `Evaluate` for the start position and for one midgame position, with each term shown.
- Further reading: CPW Othello (evaluation section), Gunnar Andersson's page (mobility-based and pattern-based evaluation).

### 06 – Move ordering

- Why ordering matters for alpha-beta (the best move first gives the most cutoffs).
- **SVG `square-values.svg`:** a heat map of the static square values, and a second board showing how the values next to a corner change when the corner is owned, taken by the opponent, or empty.
- The response table ("killer response"): for each opponent move, a score per reply; +4 on a cutoff, +1 otherwise; cleared before each search.
- The order: hash move, then response scores, then square values; the stable insertion sort (`MoveOrdering.SortDescending`).
- Mermaid flowchart of `MoveOrdering.Order`.
- The endgame ordering is described in chapter 09 and only linked here.

### 07 – Midgame search

- Negamax alpha-beta, fail-soft, explained with a small example tree.
- **SVG `alpha-beta-tree.svg`:** a three-ply tree with values, the alpha/beta window at each node, and the pruned branches crossed out.
- The `look` convention and leaf evaluation in the parent's move loop.
- Iterative deepening (Mermaid flowchart): depth 1, 2, …; root move list reordered after each iteration; stop on time; switch to the endgame solver when within `EndgameDistance` of the end.
- Principal variation search at the root (null window, then re-search), with a Mermaid sequence or flowchart.
- Extensions: a single legal reply at the horizon, a dangerous corner move.
- **Selective search** (ProbCut-like): at `look` = `SelectiveLook1` and `SelectiveLook2`, a shallow null-window search of depth `SelectiveShallowLook` against

  $$\beta' = \beta + \left\lfloor \frac{|\beta|}{2} \right\rfloor + \text{SelectiveMargin}$$

  (and the mirrored bound for alpha), and `SelectiveScoreLimit`. How this differs from Buro's statistical ProbCut.
- Passes and game over (±(`WinScore` + disc difference)).
- Response-table updates after each child.
- Node counting and the deadline check every 1024 nodes.
- Mermaid flowchart of one search node, from hash probe to hash store.
- Further reading: PVS, ProbCut (CPW and Buro's paper), Gunnar Andersson's page (search, selective search).

### 08 – Transposition table

- Why transpositions happen in Othello (the same position from different move orders), with a small example.
- **SVG `tt-layout.svg`:** the table as 2^bits slots of two entries; the fields of an entry (both bitboards, tag, depth, bound, value, best move); the slot hash.
- Full-position keys: no false hits, compared with 32-bit check keys in C++ (link to the porting documentation).
- Bounds (`Exact`, `Lower`, `Upper`) and how a probe can give a cutoff, narrow the window, or only give a move (math for the three cases).
- The replacement rule (first entry: deepest; second entry: always replaced; a new entry at least as deep moves the old first entry down), as a Mermaid flowchart. Link to the "two-tier system" on CPW.
- Two tables (midgame, endgame) and why they must not be mixed; where each is probed and stored (the midgame heights and `EndgameHashMinEmpties`).
- Memory use for the default size.
- Tests: `TranspositionTableTests`.

### 09 – Endgame solver

- When the solver takes over (`EndgameDistance`), and `SearchLimits.Solve`.
- The two passes: win/loss/draw with the window (−1, 1), then the exact score; the windows used after each result. Mermaid flowchart.
- **Mermaid flowchart "which solver at how many empties":** `Solve` (≥ `EndgameHashMinEmpties`), with enhanced transposition cutoff (≥ `TranspositionCutoffMinEmpties`), evaluation ordering (≥ `EvaluationOrderMinEmpties`), fastest-first (≥ `FastestFirstMinEmpties`); `SolveShallow` (≤ `ShallowEmpties`); `SolveLast` (1 empty).
- Fastest-first ordering (fewest opponent replies, corners count double) and why it finds refutations quickly. Link to Gunnar Andersson's endgame section.
- Parity in `SolveShallow`. **SVG `parity-quadrants.svg`:** the four quadrants with an example of odd and even empty counts.
- Enhanced transposition cutoff: a Mermaid flowchart, and a link to CPW.
- The final score (disc difference with the empty squares to the winner), as a formula.
- The FFO positions #40–#44 used in the tests: a table of positions, side to move, score, best move(s), time and nodes from the latest measured run, with a link to the FFO test suite page. (#43 and #44 have two best moves each.)
- A pointer to phase 8 in the specification for the tried and rejected optimisations.

### 10 – Time control

- The modes (`TimeControlMode`): fixed depth, time per move, time per game, solve.
- The formulas in `TimeControl` for time per game and for the endgame, written as math with each constant named (`FastEndgameEmpties`).
- Soft and hard limits (start a new iteration only below 2/3 of the time).
- Stopping: the deadline, `moveNowToken` (returns the best move so far) and `cancellationToken` (throws). Mermaid sequence diagram of "Move Now" from the menu to the played move.
- `SearchInfo` progress and `SearchResult`/`ScoreKind`, with how the analysis panel shows each kind.

### 11 – Opening book

- What the book is: a tree of moves and values, normalised so that black's first move is d3.
- **The file format:**
  - **SVG `book-file-layout.svg`:** the int32 header, then nested chains (int16 count; per node int16 move, value, flag; then the node's child chain).
  - A hex dump of the first bytes of `Data/OPENING` with each field explained.
  - The header is an allocation counter, not the node count (23 530 against 23 389 nodes in the master file); the 32600 rule for the first node of a chain.
- **Normalisation:** **SVG `book-symmetries.svg`:** the four symmetries that keep the start position (identity, main diagonal, anti-diagonal, half turn), and which first move each maps to d3.
- The position index: every position in the tree (black, white, side to move) mapped to its replies; why this finds any transposition. Mermaid flowchart of `TryGetMove` (transform board, look up, first legal reply, map back).
- Worked example: black plays one of the four first moves, and the book gives the same (mirrored) reply for all four (value −39), as in `OpeningBookTests`.
- `BookTracker` as a Mermaid state diagram (ask while fewer than `MaxMisses` misses in a row; reset on new game and Back).
- `ComputerPlayer`: book first, then search, as a Mermaid flowchart.
- Load validation (chain length, depth, truncation) and byte-identical save.

### 12 – Book learning

- Node values and flags (`Calculated`, `Exact`, `Inexact`); the value of a list for the side to move.
- `AddGame`: walking the game through the position index, adding missing moves, values ±`WinValue` (32665) or 0, passes as move 0, `MaxGameDepth`. Mermaid flowchart.
- `EvaluatePositions`: searching leaves, taking values from transpositions, and **dropout expansion** (the best move not in the book is searched and added). Mermaid flowchart. Link to Gunnar Andersson's opening-knowledge section, which describes the same idea.
- `Minimax` (`MinimaxRounds` rounds, why more than one) and the sort that decides what the book plays.
- **SVG `book-minimax-example.svg`:** a small book tree before and after minimax, with the values backed up and the lists sorted.
- `PlayGame` and `SelfPlay`: the loop as a Mermaid flowchart, checkpoints every `CheckpointInterval` positions, cancellation.
- Where the learned book is saved (`%AppData%\Stello\OPENING`) and why the shipped book is never overwritten.

### 13 – App integration

- **Mermaid state diagram of `MainViewModel`:** human to move, computer thinking, automatic pass, game over, learning; the commands that move between states.
- A table of which commands are enabled in which state.
- Threading: `Task.Run`, `Progress<T>` back on the UI thread, the search id that ignores late reports, `StopAsync` before every command that changes the game.
- Settings to engine limits (`GameSettings.ToLimits`) and the clock restore on Back/Forward.
- Book loading order (`BookLoader`: user book, shipped book, empty book) as a Mermaid flowchart, and the file locations (`AppPaths`).

### 14 – Glossary

- Every term used in more than one chapter, with a one-line explanation and a link to the chapter that explains it. At least: ply, `look`, negamax, alpha, beta, window, null window, fail-soft, fail high/low, cutoff, bound, exact, principal variation, iterative deepening, selective search, extension, transposition, hash move, response table, fastest-first, parity, enhanced transposition cutoff, win/loss/draw pass, empties, X-square, C-square, edge index, stable disc, mobility, potential mobility, legacy square number, normalisation, dropout expansion, perft, FFO.

### 15 – References

- All external links from all chapters, grouped by chapter, each with one line on what it contains.

## 11. Image catalogue

| File | Chapter | Shows |
|---|---|---|
| `board-square-index.svg` | 02 | C# square index 0–63 |
| `bitboard-bit-order.svg` | 02 | Bit order and the eight direction shifts |
| `board-legacy-numbers.svg` | 02 | Legacy 10×10 numbers with the border |
| `start-position.svg` | 02 | Start position and black's legal moves |
| `shift-and-mask.svg` | 03 | One direction of legal-move generation, step by step |
| `flips-example.svg` | 03 | A move with flips in several directions |
| `edges-and-corners.svg` | 05 | Edges, square order, corners, X- and C-squares |
| `edge-index-example.svg` | 05 | An edge, its base-3 digits and index |
| `corner-diagonal.svg` | 05 | A corner reached along the diagonal |
| `square-values.svg` | 06 | Static square values, and the values next to a corner |
| `alpha-beta-tree.svg` | 07 | A small alpha-beta tree with cutoffs |
| `tt-layout.svg` | 08 | Transposition-table slots and entry fields |
| `parity-quadrants.svg` | 09 | Quadrants and parity |
| `book-file-layout.svg` | 11 | Byte layout of the book file |
| `book-symmetries.svg` | 11 | The four symmetries and the first moves |
| `book-minimax-example.svg` | 12 | Book values before and after minimax |

More pictures may be added where they help; the table in `README.md` is kept up to date.

## 12. Worked examples and correctness

- Every number in the documentation (constants, table values, scores, node counts, times, book values, byte values) comes from the current code, the current data files or a real run. Nothing is estimated or made up.
- Constants are read from the source. Sizes and counts are read from the data (for example the book header and node count).
- Values that need a run (evaluation terms, a search result, a hex dump) are produced in one of these ways:
  - from existing tests and their expected values, linked from the chapter;
  - with a temporary console program or test **outside the repository** (for example in `%TEMP%`), which is deleted afterwards. Nothing from this is committed.
- Times and node counts say the build (Release), the date and that they depend on the machine.
- When the code changes later, the chapter that describes that code is updated in the same commit.

## 13. Acceptance criteria

1. All files in section 6 exist, and each chapter follows the template in section 7.
2. Each chapter 02–13 has at least one diagram or picture; all the pictures in section 11 exist.
3. All Mermaid diagrams render without errors on GitHub and in VS Code with the Mermaid extension.
4. All formulas render on GitHub (MathJax) and in VS Code (KaTeX).
5. All SVG files follow section 5.4, are valid XML, and are readable in GitHub's light and dark themes.
6. All relative links resolve (to files and heading anchors). All external links open and match their link text.
7. Every public engine type and every internal type listed in chapter 01 is described in at least one chapter.
8. All numbers are checked as described in section 12.
9. No source, test or project file is changed, and no temporary programs or files are left in the repository.
10. The glossary covers the terms in section 10, chapter 14.

## 14. Work plan

The documentation is written in phases; the user reviews and commits after each phase.

| Phase | Content |
|---|---|
| D1 | `docs/brain/` structure, `README.md` (with "the brain on one page"), 01 Overview, glossary and references skeletons. |
| D2 | 02 Board and squares, 03 Rules and move generation, 04 Game record, with their pictures. |
| D3 | 05 Evaluation, 06 Move ordering. |
| D4 | 07 Midgame search, 08 Transposition table, 09 Endgame solver, 10 Time control. |
| D5 | 11 Opening book, 12 Book learning. |
| D6 | 13 App integration; complete glossary and references; link check, render check on GitHub and in VS Code, final review against section 13. |

## 15. Open points

- The repository must be pushed to GitHub before the render check on GitHub (acceptance criteria 3–5) can be done. Until then the check is done in VS Code only.
- If a future Mermaid version on GitHub supports more diagram types (for example block or packet diagrams for the file layout), an SVG may later be replaced by a diagram. This is not required.
