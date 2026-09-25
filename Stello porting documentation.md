# Stello porting documentation

This document describes, phase by phase, how the C++ program Stello (`Stello C++/`, MFC, Visual C++ 6) was ported to C#/.NET 10/WPF (`Stello.Net/`). For each phase it describes:

- the algorithms, data structures and notable features of the C++ code;
- how they are implemented in C#;
- whether each part is a **1:1 port**, **changed** (improved algorithm or data structure, or a bug fixed), or a **C++ quirk kept** on purpose, and why.

The requirements are in [Migrate Othello game from C++ to C#.md](Migrate%20Othello%20game%20from%20C++%20to%20C%23.md).

## Overview

| C++ | C# | Phase |
|---|---|---|
| `BRAIN/Reversi.h` `board`, `movelist` | `Board`, `Square`, `Player` | 1 |
| `BRAIN/Treak.cpp` (`makelist`, `trymove`, `makemove`, `init_game`) | `Board`, `Bitboards` | 1 |
| `gamerec`, `playnm`, `OnFrem`/`OnTilbage` | `Game`, `Move`, `GameRecordFormat` | 2 |
| `BRAIN/Eval.cpp`, `BRAIN/Kanter.cpp` | `Evaluation/Evaluator`, `Evaluation/EdgeTables` | 3 |
| `BRAIN/Sort.cpp` | `Search/MoveOrdering` | 3 |
| `BRAIN/Minmax.cpp` (search, hash table, endgame) | `SearchEngine`, `Search/TranspositionTable` | 3 |
| `BRAIN/Kontrol.cpp` (`getcomputer`, time control) | `SearchEngine.Search`, `Search/TimeControl`, `SearchLimits` | 3 |
| `BRAIN/Book.cpp`, `Book.h` (`booktree`, `getlib`) | `OpeningBook`, `BookNode`, `BookTracker` | 4 |
| `getcomputer` book part, `libon`/`tryagain` | `ComputerPlayer`, `BookTracker` | 4, 5 |
| MFC views and frames (`MainFrm`, `StelloView`, `Analyse`, `Spiltid`) | WPF `MainWindow`, `Views/*`, `ViewModels/*` | 5 |

Terms used below:

- **Legacy square number:** the C++ board is a 10×10 array; square = 10 × row + column, both 1-based (a1 = 11, h8 = 88). Row = tens digit. C# keeps this number only for the book file and the ported tables (`Square.ToLegacy`/`FromLegacy`).
- **Ply:** one move by one side. C++ `look`/`varlook` is the remaining depth *after* the current move, so a search with `varlook = n` looks n + 1 plies ahead.

---

## Phase 0 – Setup

**C++:** One MFC MDI project (`Stello.dsp`). The engine files in `BRAIN/` are compiled into the application and communicate with the UI through global variables. Not in the build: `Interfa.c` (older non-Windows UI), `Hash.cpp` (the hash code actually used is in `Minmax.cpp`), `Mergelib.cpp`, and `BORDERS/BORDERS.C` (an offline generator for `Kanter.cpp`).

**C#:**

| Project | Contents |
|---|---|
| `Stello.Engine` (`net10.0`) | Rules, game record, evaluation, search, opening book. No UI dependency. |
| `Stello.Engine.Tests` (xUnit) | Engine tests. |
| `Stello.Net` (`net10.0-windows`, WPF) | UI with MVVM (`CommunityToolkit.Mvvm`). |
| `Stello.Net.Tests` (xUnit) | View-model tests (phase 5). |

- `Stello C++/OPENING` is linked into the output of the app and the engine tests as `Data/OPENING`.
- `InternalsVisibleTo` lets the tests reach internal types (edge tables, book nodes, the view model's `Idle` task).

**Assessment:** Changed. The global variables and the MFC document/view structure are replaced by a separate engine library, so the engine can be tested without a UI.

---

## Phase 1 – Board and rules

### C++

- **Board (`board` in `Reversi.h`):** `char sq[100]`, a 10×10 array where the outer ring holds `BORDER` so that walking in a direction stops by itself. Square values are the enum `contents`: `LIGHT = 0` (white), `DARK = 1` (black), `EMPTY = 2`, `BORDER = 3`. `ndiscs[2]` holds the disc counts, updated incrementally.
- **Frontier list (`board.possible`):** the empty squares next to at least one disc, the only squares where a move can be legal. `trymove` removes the played square and adds new empty neighbours, so move generation only looks at these squares.
- **Direction tables (`dirs[89][16]`, `dirs1[89][16]`):** for each square, a 0-terminated list of the direction offsets (−11, −10, −9, −1, 1, 9, 10, 11) that can hold a flippable line (`dirs`), or that also lead to a new empty square (`dirs1`). Squares on the edge have fewer directions.
- **Move generation (`makelist`):** for each frontier square and each direction: skip opponent discs; if the line ends with an own disc, the move is legal and the other directions are skipped.
- **Making a move:** `trymove` (search), `ftrymove` (endgame; does not update the frontier list), and `makemove` (main board; also stores the move and the board in `game`). They walk each direction, flip the line, and update `ndiscs`.
- **Start position (`init_game`):** d4 and e5 white (44/55), e4 and d5 black (45/54). Black moves first; the human is black.
- There are many hand-optimised variants of the same routine (`legalmoves`, `f_legalmove`, `trylist`, `try1list`, `legaleval`, …).

### C#

- **`Player`:** `Black`/`White`, with `Opponent()`.
- **`Square`:** index 0–63 (row × 8 + column, a1 = 0). Conversion to and from text ("f5") and to and from the legacy square number.
- **`Board`:** an immutable `readonly record struct` with two 64-bit bitboards (`Black`, `White`). It provides legal moves, flips, `Play`, `IsGameOver`, disc counts, and `Parse`/`ToString` (64 characters a1..h8, `X`/`O`/`-`, the same layout as the FFO test positions).
- **`Bitboards` (internal):**
  - `LegalMoves` uses shift-and-mask scans in 8 directions (6 steps per direction). The opponent mask excludes the edge columns/rows that would wrap around, so no border squares are needed.
  - `Flips` walks one ray per direction and keeps the line only if it ends in an own disc.
  - `PotentialMobility` is used by the evaluation (phase 3).
- **Tests:** perft from the start position to depth 8 (4, 12, 56, 244, 1396, 8200, 55092, 390216), flips in all 8 directions, the longest possible line, no wrap-around at the edges, pass, full board, wipe-out.

### Assessment

| Part | Port | Notes |
|---|---|---|
| Board representation | Changed | Bitboards instead of the mailbox, frontier list and direction tables. Faster, and a board is a 16-byte value, so no copy/undo code is needed. |
| Move generation, flips | Changed | Same rules, bitboard algorithm. One implementation instead of the many C++ variants. |
| Start position, who starts | 1:1 | |
| Legacy square numbers | Kept | Only for the book file and ported tables. |

Orientation check: the legacy square number is 10 × row + column (tens = row). The start position and the four first moves look the same under both readings. The C++ `text_move` in `Analyse.cpp` confirms it: the letter comes from `move % 10` and the digit from `move / 10`.

---

## Phase 2 – Game record

### C++

- **`gamerec`:** `moves[80]` (legacy square numbers, 0 = pass), `boards[80]` (a copy of the whole board after every ply), `sidste` (length of the game). `playnm` (global) is the current ply. `game.moves[0]` is black's first move.
- **Undo/redo (`OnTilbage`/`OnFrem` in `MainFrm.cpp`):** move `playnm` and copy `game.boards[playnm]` back to `mainboard`. They also restore the computer's clock (`timesleft[]`), clear the reply tables, re-enable the book, and set `human` to the side to move (the human continues with whichever colour is to move).
- **Passes:** when the computer cannot move (`get_com`), a 0 is stored as its move. When the human cannot move, any click on the board passes.
- **New move after undo:** `makemove` writes at `playnm` and sets `sidste = playnm`, so the undone moves are lost.
- **Saving:** `CStelloDoc::Serialize` is empty; games cannot be saved. (`savegame`/`save1game` exist but are not used.)

### C#

- **`Move`:** a square or a pass (`Move.Pass`), written as "f5" or "pass".
- **`Game`:** a list of moves plus a list of positions (board and side to move); `Ply` is the current position.
  - `Undo`/`Redo` move `Ply`. `Play` after an undo removes the undone moves.
  - `Pass()` is only allowed when the player has no legal move (`MustPass`).
  - It also provides `IsGameOver`, `Winner`, `Human`/`Computer`, `SwitchSides` (C++ `OnSkiftSide`) and `NewGame`.
- **`GameRecordFormat`:** a text file with the moves separated by whitespace ("f5 d6 c3 pass …"), written up to the current position.
  - Loading replays every move and stops with a message such as "Move 9: a8 is not a legal move for Black."

### Assessment

| Part | Port | Notes |
|---|---|---|
| History with undo/redo | Changed | Same behaviour. A list of immutable boards (16 bytes each) instead of fixed arrays of 80 mailbox boards. No 80-ply limit. |
| Pass stored as a move | 1:1 | 0 in C++, `Move.Pass` in C#. |
| Human takes the side to move after Back/Forward | Changed in phase 5 | See phase 5. |
| Save/load | New | The C++ version could not save games. |

---

## Phase 3 – Evaluation and search

### 3.1 Edge tables (`Kanter.cpp`, generated by `BORDERS.C`)

**C++:** Eight tables of 6561 = 3⁸ `short` values, indexed by one edge of the board read as a base-3 number. The first square is the most significant digit; white = 0, black = 1, empty = 2. The tables are from white's point of view.

| Table | Meaning |
|---|---|
| `sikker` | Value of the edge (stability, corners, C-squares). |
| `white_v`, `black_v` | Edge index after white/black takes the first corner of the edge. |
| `white_h`, `black_h` | Edge index after white/black takes the last corner. |
| `white_m`, `black_m` | Edge index after white/black makes its best non-corner edge move. |
| `hjo_trek` | Bit flags: corner can be taken (per colour, per corner), taken stably, "dangerous" corner, and a good middle move exists. |

The four edges are row 1 (a1→h1), column h (h1→h8), row 8 (a8→h8) and column a (a1→a8). The four corners are a1, h1, h8, a8; each corner is the first or last square of two edges.

**C#:** `Evaluation/EdgeTables.cs` is generated from `Kanter.cpp` with `sed`, so the numbers are copied unchanged. The C++ names are kept in comments. Tests check the table sizes and the corner moves on an empty edge.

**Assessment:** 1:1 (data).

### 3.2 Evaluation (`eval` in `Eval.cpp`)

**C++:** `eval(alfa, beta, plmov, player, bd)` scores the position for `player`, the side to move at the leaf. `plmov` is the number of moves the other side had in the parent position. The parts are:

1. **Game over:** a player without discs gives ±(32600 + discs). If `alfa > 32600`, it returns −32600 at once.
2. **Edge look-ahead (the main term):** a two-ply search on the edge tables.
   - Base score = Σ `sikker[edge]` × sign (sign −1 for black, because the tables are from white's view).
   - The opponent's replies lower the score (min): a middle edge move (`smidt`), or a corner it can take (`sj`).
   - The player's own moves raise it (max): middle edge moves (`midt`) and corners (`hj`), each followed by the opponent's best edge reply.
   - A corner counts only if the table does not mark it as dangerous (`FARLIG_*`). A corner can also be reached along the diagonal: an opponent disc on the X-square, a line of opponent discs, and an own disc at the end (`hj`/`sj`).
   - If the diagonal is only *potentially* open (`phj`/`psj`), the score is blended: `propc` = the chance (per mille) that the corner is reached later, `(64 − discs) × 1000 / 64 / 2 + 500`. The blend is `(propc × with + (1000 − propc) × without) / 1000`.
3. **Corner stability:** from each occupied corner, stable discs are counted diagonally inwards while at least 1 and at least 2 stable discs lie horizontally/vertically. Each counts 60 points for (or against) the player.
4. **Lazy cut-off:** if the score so far is more than 1000 (the maximum mobility score) outside the alpha-beta window, it returns without computing mobility.
5. **Mobility:** `400 × (own − opp) / (own + opp + 2)`. "Own" is the side to move's move count, from `countmov`; "opp" is `plmov`.
6. **Potential mobility:** the same formula with weight 600, counting (empty square, direction) pairs next to opponent discs (`countmov` `potential[]`).

Notable features and quirks:

- The edge indices are **globals (`sindex1..4`) computed by `dangerous()`**, which the search always calls right before `eval`.
- In the player's middle-move and corner blocks, the potential-corner branch (`psj && !hj`) assigns `score = min(tscore, escore)`, which **overwrites the best score so far** instead of lowering the reply score (`tscore`).
- The corner-stability loop counts the next diagonal square without checking its colour.
- `pbonus` is computed but never used.
- The four corner blocks are written out four times (about 1000 lines of copy-paste).

**C#:** `Evaluation/Evaluator.cs`

- `Evaluate(board, player, alpha, beta, opponentMobility)` builds the C++ 10×10 array for each call (`FillMailbox`), so that the diagonal walk and the stability loop are line-by-line ports with legacy square numbers.
- The edge indices are computed inside `Evaluate` (`EdgeIndices`), not taken from globals.
- The corner/edge logic is written once and driven by a corner table: each corner lists its two edges, whether it is their first or last square, and its diagonal. An `EdgeContext` struct holds the flags and does the blend.
- The per-colour flag sets are named records (`ColourFlags`) instead of `#define` bit names.
- Mobility and potential mobility use bitboards.
- Tests: the empty-edge indices, the square order inside an edge, wipe-out, the same score for a board mirrored in the a1–h8 diagonal (200 random positions), and a balanced start position.

**Assessment:**

| Part | Port | Notes |
|---|---|---|
| Edge look-ahead, blend, corner diagonals | 1:1 logic | Restructured into loops; the results are the same. |
| `score = min(tscore, escore)` overwrite | C++ quirk kept | Marked with a comment. Changing it would change the playing style. |
| Corner stability (including the unchecked diagonal square) | 1:1 | |
| Lazy cut-off, mobility weights and formulas | 1:1 | |
| Globals `sindex1..4` | Changed | Computed locally, so there is no hidden dependency on `dangerous()`. |
| `pbonus` | Removed | Dead code. |
| Mailbox built per evaluation | Performance cost | Kept for a faithful port; a candidate for phase 8. |

### 3.3 `dangerous()` (selective extension)

**C++:** Computes the edge indices (see above). Then, if the move just played is a corner and `hjo_trek` flags that corner as dangerous for the mover (for example the pattern `-*****-*`), it returns true. The search then does not evaluate the position but searches one more ply.

**C#:** `Evaluator.IsDangerous(after, mover, move)`. **Assessment:** 1:1.

### 3.4 Move ordering (`Sort.cpp`)

**C++:**

- **`scores[100]`:** a static value per square (corner 127, X-square −64, C-square −32, …). Before every sort, the squares next to each corner are changed: 34/34/24 if the player owns the corner; ±32 depending on the next edge square and −16 on the X-square if the opponent owns it; −16/−64 if the corner is empty. It is a global array that is overwritten each time.
- **Response killer (`humres`/`comres[78][78]`):** for each previous opponent move, a score for each reply. After a child search, the reply the child found gets +4 if it refuted the move (a cutoff below) and +1 otherwise. The tables are cleared before every search (`delres`). There is one table per side (computer/human).
- **`sortlist`:** the hash move first, then the moves with a response score (sorted by score), then the rest by square value. It uses a stable insertion sort with a sentinel.
- **`simsort` (endgame):** up to two killer moves packed into a `short`, then square values.
- **Bug:** after sorting by response score, `rscores[*lpoi]` indexes the table without the −11 offset, so it reads the score of another square.

**C#:** `Search/MoveOrdering.cs`

- `Order`: hash move, response scores, then square values. The dynamic square values are computed into a local copy (`SquareScores`). The response table is indexed by the replying *colour* instead of computer/human.
- `SortDescending`: a stable insertion sort, the same algorithm as C++.

**Assessment:** 1:1 ordering rules. **Bug fixed:** the response-score index. **Changed:** no global mutable array. The tables are indexed by colour; this is equivalent while one side is the computer, and also correct when the engine searches for both sides.

### 3.5 Hash table (`Minmax.cpp`, `hash.h`)

**C++:**

- 2¹⁹ entries (`HASHSIZE 19`). An entry holds a flag, the depth, the best move (`yx`), the value, and a 32-bit check key `a1`.
- The key is Zobrist-style: two 32-bit `rand()` values per square and colour, seeded with `srand(0x1234567)`. The slot is chosen from key `a0`, and the entry is checked only against `a1`, so false hits are possible.
- Flags: `OK_HASH` (exact), `LO_HASH` (upper bound), `HI_HASH` (lower bound), `XX_HASH` (depth too small, only the move is usable).
- Replacement: always, except when the same position is already stored with a greater depth.
- The table is only used at plies ≤ `varlook − 1` (get) and ≤ `varlook` (put) in the midgame. In the endgame it is used at ply ≤ `allway − 5`.
- **Midgame and endgame share one table** although their values mean different things (evaluation units vs. disc difference).

**C#:** `Search/TranspositionTable.cs`

- The entry stores the **whole position** (both bitboards) plus a tag (side to move), so there are no false hits. The slot is chosen with a multiply-rotate hash, so there is no Zobrist key table.
- Same flags (`Bound.Exact`/`Lower`/`Upper`) and the same get/put heights in the midgame. Since phase 8, each slot has two entries (see phase 8); the C++ rule "keep the deeper entry for the same position" is kept for the first entry.
- **Two tables**, one for the midgame and one for the endgame.
- In the endgame, bounds from the table also narrow alpha/beta (new).

**Assessment:** Changed (correctness: no false hits, no mixing of midgame and endgame values; phase 8: two entries per slot).

### 3.6 Midgame search (`findmax`, `findmax1`, `findmax2`, `zero_findmax` in `Minmax.cpp`)

**C++:**

- **Negamax alpha-beta, fail-soft.** Leaves are evaluated in the parent's move loop: when `look == 0` and the move is not dangerous, it calls `eval`.
- **Three variants, depending on a stored game tree** (`tree` nodes: first child + next sibling, pool of 300,000 nodes, `SAVE_DEPTH 8`):
  - `findmax`: the node is in the tree; moves are taken in the tree's order.
  - `findmax1`: the node is new but there is room; the sorted move list is added to the tree.
  - `findmax2`: no tree.
  - A new best move is moved to the front of its sibling list (`put_in_front`), so the next iteration searches it first.
- **Selective search (`SELEXT`):** at `look` 7 and 5, a depth-3 null-window search against `beta + |beta|/2 + 50` (or `alpha − |alpha|/2 − 50`). If it fails high (low), the node returns `beta` (`alpha`) at once. It is a ProbCut-like pruning. `#define SELEXT 0` is combined with `#ifdef SELEXT`, so it is **on**.
- **Extensions:** a single legal reply at the horizon searches one ply more; a dangerous corner move (3.3) is searched instead of evaluated.
- **Pass:** searched with the same `look`. Game over gives ±(32600 + disc difference).
- **Root (`zero_findmax`):** the first move with a full window, the rest with a null window. After the second improvement it moves the move to position 2 and restarts the loop (`goto tryagain`).
- **Tree reuse between moves (`copytree`):** if the opponent played the expected move, the next search starts at the old depth − 1.

**C#:** `SearchEngine.Search` (the private recursive overload) and `SearchRoot`

- The same negamax with fail-soft and leaf evaluation in the parent loop, the same `look` semantics, the same extensions, the same selective search (constants `SelectiveLook1/2`, `SelectiveShallowLook`, `SelectiveMargin`), and the same response-killer updates.
- **The stored game tree is replaced by the hash table:** the hash move gives the ordering, and the root move list is kept between iterations with each new best move moved to the front (as `put_in_front`).
- **Root:** standard PVS (null window, then a full-window re-search when a move beats alpha) for `look > 2`, as `zero_findmax` is used for `varlook > 2`.
- Game-over values give the empty squares to the winner (standard rule).

**Assessment:**

| Part | Port | Notes |
|---|---|---|
| Alpha-beta, leaf evaluation, extensions, selective search, response killers | 1:1 | |
| Stored game tree (`findmax`/`findmax1`) | Changed | The hash table and root-move reordering give the same ordering with far less code; no node pool. |
| `zero_findmax` restart trick | Changed | Standard PVS re-search; same result. |
| Tree reuse between moves | Dropped | The hash table persists between moves, so most of the benefit remains. |
| Game-over value | Changed | Empty squares go to the winner (standard rule). |

### 3.7 Endgame solver (`slutmax`, `slutmax1`, `slutmax2`, `slutmax3`, `zero_slutmax`)

**C++:**

- **When:** the iterative deepening loop switches to the solver when `allway − varlook ≤ 7` (`allway` = empty squares − 1), that is, when the midgame search is within 7 plies of the end.
- **Two passes:**
  1. Win/loss/draw with the window (−1, 1) (`low_min`, result flag `b1calc`).
  2. If there is time, the exact score:
     - win: window (value − 1 (or value), 64);
     - loss: (−64, value);
     - draw: already exact.
  - Optimisation: after a win, the root moves before the winning move are removed, since they are known not to win.
- **Scores:** `char` disc difference; empty squares at the end are not counted.
- **`slutmax1`:** hash table, `simsort` with two killer moves when more than 3 moves remain, no board copy for the last move. `slutmax3`/`slutmax2` handle the last 2 empties with undo lists instead of copies.
- **Next move:** after a full solve (`bcalc`), the computer's next reply is taken directly from the stored tree (`bcopytree`) without searching.

**C#:** `SearchEngine.SolveRoot`, `Solve`, `SolveShallow`, `SolveLast`

- The same switch rule (`EndgameDistance = 7`), the same two passes, and the same result handling. When losing, the midgame move is kept, as in C++.
- The windows after the win/loss/draw pass are (value − 1, 65) and (−65, value + 1). These are safe with fail-soft bounds.
- `Solve` (≥ 7 empties): the endgame hash table with bound narrowing, the hash move first, then:
  - an evaluation-based order at ≥ 18 empties;
  - otherwise fastest-first (fewest opponent replies, corners counting double), with the square value as tie-breaker.
- `SolveShallow` (≤ 6 empties): no move list and no hash table. It tries the empty squares directly, those in quadrants with an odd number of empties first (parity).
- `SolveLast`: the last empty square, without move generation.
- Scores give the empty squares to the winner (standard rule, needed for the FFO test values).
- New limit `SearchLimits.Solve`: go straight to the solver without a midgame search (used by the FFO tests).
- Tests: FFO #40–#44 exact score and best move; 40 random small endgames compared with a plain full-depth search.

**Assessment:**

| Part | Port | Notes |
|---|---|---|
| Switch rule, win/loss/draw then exact | 1:1 | |
| Move ordering in the solver | Changed (improved) | Fastest-first, parity and evaluation ordering replace `simsort` (killer + square value). This was needed to solve the FFO positions in reasonable time. |
| Final score | Changed | Empty squares go to the winner (standard rule). |
| Tree reuse after a full solve | Dropped | The endgame hash table usually answers the next move quickly. |
| Speed | Open point | FFO #40–#44 took about 30 s in Release after phase 3; 19 s after phase 8 round 1. |

### 3.8 Iterative deepening and time control (`getcomputer`, `Kontrol.cpp`)

**C++:**

- **Time modes (`tid_kontrol`):**
  - `sogedybde`: fixed depth `lookahead`.
  - `tid_per_trek`: level 0–14 with `tider[]`/`rtider[]` in ms. `tider` ≈ 2/3 of `rtider`: an iteration is only started while the elapsed time is below `tider`; the search stops at `rtider`.
  - `spil_tid`: minutes per game.
- **Game time (`calc_time`):** subtract `lookahead − 2` (6 at the default level) from the empty squares, halve to get the number of the computer's moves left, then time per move = remaining time / moves × (1/4 + 3 × discs / 256). An iteration is only started below 2/3 of that. In the endgame (`calc_end_time`), half of the remaining time may be used.
- **Stopping:** a Windows multimedia timer (`timeSetEvent`) sets `tid_udlobet`, and the search jumps out with `longjmp(env, 1)`. The best move so far is used, including the best move of an unfinished iteration. `timeout()` also allows 30 % extra at the root.
- **Other:** a single legal move is played at once. `backthink` (thinking on the opponent's time) is present but not used.
- **UI updates:** the search calls the UI directly (`make_try`, `make_res`). The search runs on the UI thread with a wait cursor.

**C#:** `SearchEngine.Search`, `Search/TimeControl.cs`, `SearchLimits`

- **Modes:** `SearchLimits.FixedDepth(plies)`, `TimePerMove(time)`, `TimePerGame(remaining)` and `Solve`. Depth is given in plies (C++ `lookahead + 1`), and time per move in seconds instead of a level number (the soft limit is 2/3 of the time, as `tider`/`rtider`).
- **Game time:** the same `calc_time` and `calc_end_time` formulas, with a constant of 6 fast endgame squares.
- **Stopping:** a `Stopwatch` deadline, checked every 1024 nodes; the search unwinds with a private exception instead of `longjmp`.
- **Tokens:** `cancellationToken` throws `OperationCanceledException`; `moveNowToken` returns the best move so far (C++ "Træk nu" had no handler).
- **Progress:** `IProgress<SearchInfo>` after every root move. The result is a `SearchResult`: move, score, `ScoreKind` (none, heuristic, win/loss/draw, exact, book), depth, nodes, evaluations, time.
- Tests: fixed depth reached, the same result every time, progress for every depth, time per move and per game kept, "move now" and cancel within 100 ms, depth 4 beats a random and a greedy player in at least 48 of 50 games.

**Assessment:**

| Part | Port | Notes |
|---|---|---|
| Iterative deepening loop, endgame switch, single-move shortcut | 1:1 | |
| Time formulas | 1:1 | Levels replaced by seconds/plies. |
| Timer + `longjmp` | Changed | Deadline checks and exception unwinding; safe in .NET. |
| UI callbacks | Changed | `IProgress<SearchInfo>`; the engine does not know about the UI. |
| Thinking on the UI thread | Changed | The engine is synchronous; the app runs it on a background task (phase 5). |
| `backthink` (pondering) | Not ported | Not used in C++. |

---

## Phase 4 – Opening book

### C++ (`Book.cpp`, `Book.h`)

- **Node (`booktree`):** first child (`barn`), next sibling (`sosk`), `move` (legacy square number, 0 = pass), `value`, `flag` (`CALCULATED`/`EXACT`/`INEXACT`, used by book learning). Nodes come from a pool (`init_book_nodes`) sized as the file header + 10,000.
- **Normalisation:** the book only stores games that start with d3 (34). The root list holds white's replies to d3. `convop` maps any move to the book's frame, depending on black's first move:
  - d3: identity;
  - c4: mirror in the main diagonal;
  - f5: mirror in the anti-diagonal;
  - e6: half turn.
  - A special case (normalised moves 2–4 = c3, c4, c5) mirrors the rest of the game once more in the main diagonal, because that position is symmetric.
- **File (`Get_book`/`Put_book`, `readbook`/`savebook`):**
  - Layout (little-endian; the byte-swapping calls are commented out): `int32` header, then the root chain. A chain is `int16` count, then for each node `int16` move, value, flag, followed by that node's reply chain.
  - The header is the node **allocation counter** `abook`, not the number of nodes in the tree. For the master file it is 23,530 against 23,389 real nodes.
  - `savebook` writes 0 instead of 32600 for the first node of each chain.
  - `BRAIN/OPENING` has a big-endian header (an older Unix version); the master is `Stello C++/OPENING`.
- **Lookup (`getlib`, `getopn`):**
  - Black's first move is chosen at random (`rand()`) among the four first moves.
  - Later, the game is walked from the root, move by move, through the normalised tree. On a mismatch, three transposition patterns are tried (swapping move x with x−2, x−1 with x+1, or x with x+2). A pattern is used only if replaying both move orders gives the same board (`play_game`, `equalbd`); then the tree is walked again (`transgetopn`).
  - The **first legal reply** in the chain is used. The code that picks the best value is overwritten right after, and random selection is disabled with `#if 0`.
- **When to ask (`libon`/`tryagain`):**
  - On a miss `libon` becomes false and `tryagain` is increased; the book is still asked while `tryagain < 3`, so the game can transpose back.
  - A hit resets `tryagain`.
  - New game and Back re-enable the book.

### C#

- **`BookNode` (internal):** move, value, flag, and a `List<BookNode>` of replies in book order (instead of child/sibling pointers).
- **`OpeningBook.Load`/`Save`:**
  - Reads and writes exactly the layout above with `BinaryReader`/`BinaryWriter` (little-endian).
  - The header value is kept, and `Save` writes max(header, node count), so load + save gives a byte-identical file. The 32600 rule is kept.
  - Loading validates the data (chain length ≤ 64, depth ≤ 64, no truncation) and throws `InvalidDataException` instead of overrunning buffers.
- **Position index:** at load time the tree is replayed from the position after d3. Every reachable position (black and white bitboards + side to move) is mapped to its list of replies. Lines with illegal moves are skipped.
- **`TryGetMove(board, player, random, out BookMove)`:**
  - At the start position, black's first move is chosen at random (injectable `Random`).
  - Otherwise the current board is transformed by the four symmetries that keep the start position (identity, main diagonal, anti-diagonal, half turn; each is its own inverse) and looked up in the index. The first legal reply is mapped back.
- **`BookTracker`:** the `libon`/`tryagain` rule as "ask while there are fewer than 3 misses in a row"; `Reset()` for a new game and Back.
- **`ComputerPlayer` (engine, used from phase 5):** asks the book first (through the tracker) and searches when there is no book move. A book move has `ScoreKind.Book` and the book value as its score.
- Tests:
  - node count and byte-identical round trip;
  - all four first moves over 100 seeds;
  - the same reply for the four symmetric first moves (c5, e3, d6, f4, value −39);
  - a legal move in every book position (more than 1000 positions);
  - a position not in the book;
  - the 32600 rule, three kinds of invalid file, and the tracker rules;
  - `ComputerPlayer`: book first, search outside the book, stop after three misses, pass without asking the book.

### Assessment

| Part | Port | Notes |
|---|---|---|
| File format, header, 32600 rule | 1:1 | Kept for byte-identical files. |
| Tree structure | Changed | Child lists instead of pointer chains and a node pool. |
| Normalisation by symmetry | 1:1 idea, changed implementation | The board is transformed instead of the move list; this also covers the c3-c4-c5 special case. |
| Transpositions | Changed (improved) | The position index finds every transposition; C++ only tried three swap patterns. The C# version can therefore stay in the book where the C++ version left it. |
| Reply choice | 1:1 | First legal reply in book order. |
| `libon`/`tryagain` | 1:1 | `BookTracker`. |
| Validation of the file | New | Protects against damaged files. |
| Big-endian book file | Not supported | Only the little-endian master file is used. |

---

## Phase 5 – WPF user interface

### C++ (MFC, `Stello.cpp`, `MainFrm.cpp`, `StelloView.cpp`, `StelloDoc.cpp`, `Analyse.cpp`, `Spiltid.cpp`, `Stello.rc`)

- **Structure:** an MDI application. `CStelloDoc`/`CStelloView` show the board, `CAnalyse` (a form view) shows the analysis, and `CSpiltid` (a form view) sets the time. All game state is in engine globals (`mainboard`, `game`, `playnm`, `human`, `computer`, `gameover`, `frem`, `tilbage`, …). The menus are in Danish.
- **Startup (`CStelloApp::InitInstance`):** reads `rev.cfg` (a binary dump of the settings struct; defaults: 5 minutes per game, level 8), allocates the tree nodes, loads the book (`Get_book`), and initialises the hash table and the board.
- **Board view (`OnDraw`):** 64 white rectangles with black or white ellipses, drawn in `MM_LOENGLISH`. There are no coordinates, no legal-move markers, no last-move marker and no disc count.
- **Human move (`OnLButtonDown`):**
  - It hit-tests the 64 rectangles and checks the move with `makelist`/`inlist`, beeping (`MessageBeep`) on an illegal move.
  - When the human has no legal move, any click passes.
  - After the human's move, the computer's reply (`get_com`: `getcomputer` + `makemove`) runs **on the UI thread** with a wait cursor. A beep sounds when the computer has moved. A computer pass is stored as move 0.
- **Commands (`CMainFrame`):**
  - Nyt Spil: new game.
  - Skift Side: swap colours; the computer moves at once.
  - Frem/Tilbage: forward/back one ply; the human takes the side to move; the computer's clock is restored from `timesleft[]`.
  - Tid for et spil (`CSpiltid`): minutes per game, 1–60; sets `tid_kontrol = spil_tid` and both clocks.
  - Analyse: opens the analysis view.
  - Flet spil, Minmaxlib, Lær spil: book learning (phase 7).
  - Træk nu: in the menu, but **there is no handler**.
  - Open/Save exist but do nothing (`Serialize` is empty).
  - Print, the toolbar, the status bar and WinHelp are the MFC defaults.
- **Analysis view (`IDD_ANALYSE`, `make_try`, `make_result`):**
  - Knuder (nodes), Værdi (value; `p` = exact endgame, `e` = win/loss/draw), Tid (time), Evalueringer (evaluations), and Træk (current move, depth, and move number/total).
  - It is updated by direct calls from the search. Since the search runs on the UI thread, it is repainted with `UpdateWindow`.

### C# (`Stello.Net`)

- **MVVM with `CommunityToolkit.Mvvm`:** generated observable properties and `[RelayCommand]` commands. The code-behind only has `InitializeComponent`, the Exit menu item, and stopping the search when the window closes.
- **`App.OnStartup`:** loads `Data/OPENING` from the application folder. If this fails, the app plays without a book and shows a notice. It creates `ComputerPlayer` (engine + book + `Random`) and `MainViewModel`.
- **`ViewModels/MainViewModel`:**
  - State: the `Game`, 64 `SquareViewModel`s (disc, legal-move marker, last-move marker), disc counts, status text, sides, computer clock, and window title.
  - **Computer move (`ComputerMoveAsync`):** `ComputerPlayer.ChooseMove` runs with `Task.Run` and is awaited, so the result comes back on the UI thread. A `CancellationTokenSource` for cancel and one for "move now" are created per move. `Progress<SearchInfo>` updates the analysis panel. Late progress reports are ignored with a search id.
  - **Game loop (`RunAsync`):** refreshes the board, then:
    - shows the result when the game is over;
    - **passes automatically** when the side to move has no legal move, with a notice in the status bar;
    - stops when the human is to move;
    - otherwise lets the computer move, and repeats.
  - Every command that changes the game (New, Open, Switch Sides, Back, Forward) first stops the search and waits for it (`StopAsync`). The `Idle` task lets the tests wait for the computer.
  - **Human move (`Play`):** ignored with a beep while the computer thinks, after the game is over, or on an illegal square.
  - **Back/Forward:** go back or forward to the previous or next position where the human is to move. The computer's moves and passes in between are skipped, and the human keeps their colour. The computer's clock is restored as with C++ `timesleft[]`, and the book is re-enabled after Back.
  - **Open/Save/Save As:** the text format from phase 2 (`*.stello`). After Open the human takes the side to move (as C++ Frem/Tilbage). Errors are shown in a message box.
  - **Settings:** fixed depth (1–20 plies), time per move (1–60 s), or time per game (1–60 min). The computer's clock is reset when the mode or the game time changes. The defaults are as C++: 5 minutes per game, depth 8.
  - **Move Now:** cancels the "move now" token, and the engine plays its best move so far.
- **`ViewModels/AnalysisViewModel`:** move, depth (plies, or "to the end (n empty)"), value ("+123", "Win"/"Loss"/"Draw", "+12 discs", "Book"), best move, nodes, evaluations, time.
- **`ViewModels/SettingsViewModel`, `Views/SettingsWindow`:** radio buttons for the mode and sliders for the values; values are clamped to their ranges.
- **`Views/BoardView`:** a scalable (`Viewbox`) green 8×8 board of buttons in a `UniformGrid`, with a1 in the top-left corner and a–h / 1–8 labels. Data triggers draw the discs, the legal-move dots and the last-move dot. Each square has a tooltip and automation name ("f5") for accessibility and keyboard focus.
- **`MainWindow`:**
  - The menus are in English. Shortcuts: Ctrl+N, Ctrl+O, Ctrl+S, Ctrl+M (Move Now), Ctrl+Z/Ctrl+T (Back), Ctrl+Y/Ctrl+F (Forward), Ctrl+A (Analysis). The C++ shortcuts Ctrl+T/Ctrl+F still work.
  - A status bar shows the status, disc counts, the human's colour and the computer's clock. The analysis panel is on the right and can be hidden.
- **`Services/IDialogService`, `DialogService`:** file dialogs, settings dialog, message boxes, About box and beep. They are behind an interface so the view models can be tested.
- **Engine addition:** `ComputerPlayer` (book first, then search; see phase 4) and `ScoreKind.Book`.
- **Tests (`Stello.Net.Tests`, 36 tests):**
  - the start state and a startup notice;
  - the computer replies, and illegal moves or moves during thinking beep;
  - Back/Forward between the human's turns;
  - Switch Sides makes the computer move;
  - Move Now, and New Game stopping the search;
  - the save/open round trip, the side to move after Open, an invalid file, an automatic pass, a finished game;
  - the settings and the clock, the analysis toggle, About;
  - score formatting, the settings dialog model, and `GameSettings.ToLimits`.
- **Smoke test:** the app starts, loads the book, and runs (checked by starting the executable). A full game played by hand through the UI is part of the manual acceptance test.

### Assessment

| Part | Port | Notes |
|---|---|---|
| MDI document/view, form views | Changed | One main window with a side panel (MDI out of scope). |
| Computer thinking on the UI thread | Changed (improved) | Background task; the UI stays responsive. |
| Direct UI calls from the engine | Changed | `IProgress<SearchInfo>` and the view model. |
| "Træk nu" | New | It had no handler in C++; it now plays the best move so far. |
| Human pass by clicking | Changed | Automatic pass with a notice in the status bar. |
| Back/Forward | Changed | They step between the human's turns and keep the human's colour. In C++ they moved one ply and gave the human the side to move, so the human could end up playing the other colour. The C++ rule is kept after Open. |
| Clock restore on Back/Forward | 1:1 | As `timesleft[]`. |
| Book re-enabled after New/Back | 1:1 | `BookTracker.Reset`. |
| Board drawing | Changed (improved) | Coordinates, disc counts, legal-move and last-move markers, scalable, keyboard and screen reader support. |
| Time settings | Changed | The three engine modes with plies/seconds/minutes instead of the C++ level numbers; C++ only exposed minutes per game in the UI. |
| Open/Save | New | Text move lists. |
| Danish UI text | Changed | English. |
| Print, toolbar, WinHelp, MDI window menu | Not ported | Out of scope. |
| Settings persistence (`rev.cfg`) | Done in phase 6 | JSON in `%AppData%`. |
| Book learning menu items | Done in phase 7 | Book menu. |

---

## Phase 6 – Settings and file locations

### C++ (`Stello.cpp`, `Stello.h`, `BRAIN/Interfa.c`, `BRAIN/Book.cpp`)

- **`rev.cfg`:** a binary dump (`fread(&revdef, sizeof(revdef), 1, fd)`) of the `config` struct, read at startup from the **current directory**. The struct is `short` fields:
  - `timedef` (time mode), `leveldyb` (depth level), `leveltid` (time level);
  - `borderdef`, `hvmudef`, `notadef` (board orientation), `anadef`/`gamdef` (whether the analysis and game windows are open);
  - plus the saved window size and state.
- **The Windows version ignores it:** in `CStelloApp::opset` all the code that uses `revdef` is commented out. The defaults are always used: `tid_kontrol = spil_tid`, `GameTid = 5` minutes, `lookahead = 8`, no pondering, no analysis window. The older UI in `Interfa.c` still applied the file.
- **No saving:** the Windows version never writes `rev.cfg`. A time set with "Tid for et spil" is lost when the program closes. The window layout is the MFC default.
- **Other files are also read and written in the current directory:** `opening` (book), `selfplay`, `expanding`, `moves` (debug).

### C# (`Stello.Net`)

- **`Models/AppSettings`:** everything saved between sessions: `GameSettings` (mode, depth, seconds per move, minutes per game), `ShowAnalysis`, and `WindowPlacement` (position, restored size, maximised).
  - `Normalize()` clamps every value to its range and falls back to the default mode for an unknown mode (for example `Solve`).
  - An empty window size means "use the default position".
- **`Services/ISettingsStore`, `JsonSettingsStore`:** JSON in `%AppData%\Stello\settings.json` (`System.Text.Json`, enums as text, indented).
  - A missing, damaged or unreadable file gives the defaults.
  - Saving writes a temporary file and then renames it, so a crash cannot leave half a file. It returns false instead of throwing.
- **`MainViewModel`:**
  - Loads the settings at startup (depth/time mode, whether the analysis panel is shown, window position).
  - Saves them when the settings dialog is confirmed, when the analysis panel is toggled, and when the window closes.
  - A failed save is shown in the status bar.
- **`MainWindow`:** restores the saved position and size, and maximises if it was maximised. It is only restored if the window would still be on a screen (a monitor may have been removed). When closing, it saves the restored size, also when maximised.
- **`Services/AppPaths`:** all file locations are fixed and independent of the current directory:
  - the settings file;
  - the user's book `%AppData%\Stello\OPENING` (phase 7 writes to it);
  - the shipped book `<app folder>\Data\OPENING`.
- **`Services/BookLoader`:** tries the user's book first, then the shipped book; missing files are skipped. If a file cannot be read, a notice is shown and the next file is tried. If no book is found, the app plays without one.
- **Tests (`Stello.Net.Tests`, 51 tests in total):**
  - the store: defaults without a file, round trip, readable JSON without computed properties, damaged file, values out of range, missing sections, a write failure;
  - the book loader: missing user book, damaged user book, no book;
  - the view model: the saved settings are used at startup, and they are saved after the settings dialog, the analysis toggle and the window placement; a save failure is shown.

### Assessment

| Part | Port | Notes |
|---|---|---|
| Settings file | Changed | JSON instead of a binary struct dump; readable, versionable, and robust against damaged files. |
| Settings actually used and saved | New | The Windows C++ version read `rev.cfg` but ignored it, and never saved. |
| Defaults (5 minutes per game, depth 8) | 1:1 | As `opset`. |
| Window position and analysis panel | 1:1 idea | `anadef` and the window fields in `config`, now actually saved. |
| Import of an old `rev.cfg` | Not ported | Optional in the specification; the Windows version never wrote one, and it ignored the values anyway. |
| Board orientation, "border", multiple moves (`notadef`, `borderdef`, `hvmudef`) | Not ported | Only used by the old `Interfa.c` UI. |
| File locations | Changed | `%AppData%\Stello` and the application folder instead of the current directory. |
| User book before shipped book | New | Prepares book learning (phase 7) without changing the shipped file. |

---

## Phase 7 – Book learning

### C++ (`BRAIN/Book.cpp`, `MainFrm.cpp`)

- **Node values:** a node's value is the value of its move for the player who makes it. A list of replies is "worth" the maximum of its values for the player to move. Flags (`Book.h`):
  - `CALCULATED`: the leaf's value comes from a search;
  - `EXACT`/`INEXACT`: that search solved the position exactly, or for win/loss/draw.
  - They are stored in the file, so a long learning run can be continued later.
- **Flet spil (`OnSpilFletspil` → `convert_game` + `mmgame`):** asks "Spil i database ?" and "Var det sort der vandt ?".
  - `convert_game` normalises the game to d3 (`convop`, with the c3-c4-c5 special case) and replays it.
  - `mmgame(1, ±32665)` walks the tree along the game and appends missing moves as new nodes. The value is +32665 for the winner's moves and −32665 for the loser's; it alternates by depth, so a pass is stored as move 0 and keeps the alternation.
  - At every depth, `getlibpos` first checks whether the position is already in the book (also through a transposition) and continues there. Existing nodes keep their values. It stops at depth 56.
- **Minmaxlib (`OnMinmaxlib`):** `calc_lib` (= `minmaxlib`), then 10 × `minmax_lib`, then `sort_lib`, then `Put_book`. The menu sets level 3, but `calc_lib` overrides it with level 11 (`LIBLEVEL`), that is 2 minutes per searched position.
  - **`minmaxlib`:** walks the whole tree. It removes nodes with illegal moves. For each leaf that is not `CALCULATED`:
    - if the book has a reply for the resulting position (a transposition, via `getlib`), it takes that reply's value;
    - otherwise it searches the position (`getvalue` = `getcomputer` with the book off and the hash table cleared) and sets `CALCULATED`/`EXACT`/`INEXACT`.
  - **"Dropout expansion":** in a list of replies where no node is `CALCULATED` yet, it searches the best move *not* in the book (`remove_oldmov` + `getvalue`) and appends it as a new node. This is how the book grows.
  - The book is saved every 10 searched positions.
  - `getvalue` turns solved results into ±(32600 + discs), and a solved draw into −32600.
  - Interior nodes get `flag &= !CALCULATED`, which clears all flags (`!` instead of `~`).
  - **`mmlib` (`minmax_lib`):** backs the values up the tree (negamax). Leaves that transpose into the book take the book's value. Called 10 times, so values also travel through transpositions.
  - **`sort_lib`:** sorts every list of replies by value, best first (stable insertion sort, `sort_nodes`). Lookup takes the first legal reply, so this decides what the computer plays from the book.
- **Lær spil (`OnSelfplay` → `selfplay`):**
  - First `calc_lib` twice, minimax, sort and save.
  - Then repeatedly:
    - `splay` plays a game engine against engine, with the book on, until the endgame is solved (`bcalc`) or the game ends;
    - `convert_game` + `mmgame` add it; the result comes from the global `value`, which is the last *midgame* value even when the endgame was solved;
    - "played game N" is appended to `selfplay` in the current directory;
    - then `calc_lib` twice, minimax, sort and save again.
  - The loop ends only at `book_timeout()`, which always returns false. So it runs forever on the UI thread, and the program has to be killed.
- **`extend_lib`:** a deeper expansion driver (`extendlib`), not reachable from the Windows menus.

### C# (`Stello.Engine`)

- **`BookFlags` (`[Flags]` enum):** `Calculated`, `Exact`, `Inexact`, stored as the same 16-bit values.
- **`OpeningBook`:**
  - `CreateEmpty()` for learning from nothing.
  - `Rebuild()` recomputes the node count and the position index after changes.
  - `TryFindReplies(board, player)` returns a position's replies and the symmetry that maps the board to the book's frame.
  - `FirstMoveSymmetry` and `Transform` are now internal helpers.
- **`SearchEngine.Search(…, onlyMoves)`:** searches only the given moves, even a single one (C++ `remove_oldmov` + `calclib`).
- **`BookLearner`:**
  - **`AddGame(moves, result)`:** walks the game from the start position.
    - At every ply it first looks the resulting position up in the position index (with symmetries). If found, it continues from that position's replies; otherwise it finds or adds the move in the current list, mapped into the book's frame with the current symmetry.
    - Values are ±32665 for the winner's/loser's moves, or 0 for a draw. Passes are move 0. It stops at depth 56, and the moves are checked for legality.
  - **`EvaluatePositions`:** a port of `minmaxlib`: illegal nodes are removed, leaves are searched or taken from transpositions, and the dropout expansion adds the best move not in the book. Solved results are stored as ±(32600 + discs). A checkpoint callback saves the book every 10 searched positions. Progress is reported, and cancellation stops within one search.
  - **`Minimax`:** a port of `mmlib` (10 rounds) plus `sort_lib` (stable, best first).
  - **`PlayGame`:** engine against engine through `ComputerPlayer` (book on), until the endgame is solved exactly or the game is over. The result comes from the exact score of the mover, or from the final board.
  - **`SelfPlay`:** the `selfplay` loop until cancelled. It calls back after each game (the app writes the log).
- **Tests (`BookLearnerTests`, 16 tests):**
  - a line stored normalised to d3, and the book suggesting it for all symmetric move orders;
  - known, repeated and symmetric games add no nodes (also into the master book);
  - draw values, pass nodes, and an illegal game;
  - `EvaluatePositions`: the searched leaf plus two dropout moves; a second run searches nothing new; illegal moves are removed;
  - `Minimax` values and order;
  - `PlayGame` gives a legal game, and `SelfPlay` runs until cancelled and saves checkpoints;
  - flags survive save/load;
  - `onlyMoves`.

### C# (`Stello.Net`)

- **Book menu:** Add Game to Book…, Evaluate Book…, Self-play…, Stop Learning.
- **`MainViewModel`:**
  - **`AddGameToBook`:** needs at least two moves and a confirmation. A finished game uses its result; otherwise it asks "Did Black win the game?" (Yes/No/Cancel). The computer's search is stopped first, and the book is saved.
  - **`EvaluateBook`** (evaluate + minimax) and **`SelfPlay`:** after a confirmation, `LearnAsync` stops the game search and sets `IsLearning`, then runs the learner with `Task.Run`.
    - While learning, moves on the board beep, and New, Open, Switch Sides, Back, Forward, Settings and the Book commands are disabled.
    - Progress is shown in the status bar. Stop Learning cancels.
    - At the end the book is always saved, the book tracker is reset, a summary is shown (positions searched, games played, book size), and the game continues.
  - **Search time per learned position:** fixed depth if that mode is selected, otherwise the "time per move" setting (1–60 s).
  - Closing the window also cancels learning.
- **`Services/IBookStore`, `FileBookStore`:** saves the book to `%AppData%\Stello\OPENING` (temporary file + rename, never the shipped book) and appends "played game N: moves" to `%AppData%\Stello\SELFPLAY`.
- **`App`:** if no book is found, it starts with an empty book, so learning also works without the shipped file.
- **Tests (7 new app tests, 58 in total):**
  - adding a finished game, or an unfinished game with the answer from the dialog;
  - cancelling at either question, and a game that is too short;
  - Evaluate Book saves and finishes;
  - self-play disables the game commands, beeps on moves, and stops on Stop Learning.

### Assessment

| Part | Port | Notes |
|---|---|---|
| Node values, flags, file layout | 1:1 | |
| `mmgame` (add a game) | 1:1 idea | The walk uses the position index with symmetries instead of `convop`/`getlibpos`, so any transposition is found, also the c3-c4-c5 case. |
| `convert_game` rewriting the shown game | Not ported | Only needed because C++ stored the normalised game globally. |
| `minmaxlib` (search leaves, dropout expansion) | 1:1 | Including illegal-node removal and saving every 10 positions. |
| `flag &= !CALCULATED` | Kept in effect | Written as `Flag = None`, which is what the C++ code does. |
| Solved draw stored as −32600 | Bug fixed | Stored as 0. |
| Hash table cleared before every learning search | Changed | Not needed; the C# hash table cannot give false hits. |
| `mmlib` × 10, `sort_lib` | 1:1 | |
| Self-play result from the global `value` | Bug fixed | Uses the exact endgame score, or the final board. |
| Endless self-play on the UI thread | Changed | Background task with progress and Stop Learning. |
| 2 minutes per learned position (level 11) | Changed | The user's setting (fixed depth or time per move), so learning is usable interactively. |
| Files in the current directory (`opening`, `selfplay`) | Changed | `%AppData%\Stello`; the shipped book is never overwritten. |
| `extend_lib`/`extendlib` | Not ported | Not reachable from the Windows UI. |
| `Mergelib.cpp` (merge black/white libraries) | Not ported | Not in the build; out of scope. |

---

## Phase 8 – Performance tuning (round 1; paused)

This phase has no C++ counterpart: it improves the C# endgame solver (section 3.7) and hash table (section 3.5). The full list of what was tried, with measurements, and the remaining candidates is in the specification (Phase 8), so the rejected ideas do not have to be tested again.

### What changed in C#

- **Hash table (`Search/TranspositionTable.cs`), two entries per slot:**
  - The first entry keeps the deepest result for the slot (C++ rule: a deeper entry for the same position is kept). The second entry is always replaced.
  - A new entry that is at least as deep as the first moves the old first entry to the second place.
  - `TryGet` checks both entries. `bits` now means 2^bits slots (twice as many entries as before), so the default uses about 24 MB per table.
  - C++ had one entry per slot. Deep endgame results were overwritten by shallow ones, which cost many nodes in long searches (FFO #43).
- **Enhanced transposition cutoff (`SearchEngine.Solve`):** from 10 empty squares, the children are looked up in the endgame hash table before they are sorted or searched. If one of them has an exact value or an upper bound that proves a cutoff for us, it is returned at once. New; not in C++.
- **Flips computed once (`SearchEngine.Solve`):** the flips of each move are computed once, used for ordering, sorted together with the moves (`SortDescending` with three spans), and reused when the move is played. Before, they were computed twice.

### Results (FFO #40–#44, Release)

| | Total time | Nodes | #43 |
|---|---|---|---|
| After phase 3 | 31.6 s | 522 M | 18.8 s, 292 M |
| After phase 8 round 1 | 19.1 s | 328 M | 10.1 s, 168 M |

- All scores and best moves are unchanged.
- `TranspositionTableTests` (5 tests) cover the two-entry slots: store/find, other position or tag, the deeper result is kept, a deep entry survives many colliding shallow ones, clear.
- All 158 engine tests and 58 app tests pass (Debug and Release). The engine test run takes about 30 s in Release and about 55–75 s in Debug, against about 87 s before.
- The target of under 10 s is not reached yet.

### Tested and rejected

Reverted, with the measurements in the specification: MTD(f) for the exact pass, one wide exact window without the win/loss/draw pass, ordering by a shallow midgame search, potential mobility in the ordering key, a stability cutoff, other solver thresholds, and larger hash tables.

### Assessment

| Part | Port | Notes |
|---|---|---|
| Two entries per hash slot | Changed (improved) | The C++ replacement rule is kept for the first entry. |
| Enhanced transposition cutoff | New | Endgame solver only. |
| Flips computed once | Changed (optimisation) | Same results. |