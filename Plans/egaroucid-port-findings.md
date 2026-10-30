# Findings from porting Egaroucid

What porting the Egaroucid engine to C# has taught us, written down as we go: how Egaroucid differs from Stello, the quirks of the C++ code the port must reproduce, where the port deliberately differs, the measurements, and ideas for the improved Stello engine.

- **The plan:** [egaroucid-port-plan.md](egaroucid-port-plan.md). This file is updated after every phase.
- **Later:** in phase 7 it becomes the docs chapters 19 (the port) and 20 (the comparison).
- **The sources:**
  - Egaroucid 7.9: `Egaroucid-main/src/engine`, © Takuto Yamana, GPL-3.0-or-later.
  - The port: `Stello.Net/Egaroucid.Engine`.
  - The reference: `Stello.Net/tools/Egaroucid.Reference`.

## Status

| Phase | State | What it showed |
|---|---|---|
| 0 Licence | done | — |
| 1 C++ reference | done | Egaroucid builds with plain g++ in 35 s; its results are deterministic once times are left out |
| 2 Foundations | done | Board, flips, hash, stability and evaluation are exactly equal to C++ |
| 3 Single-threaded search | done | All reference searches are exactly equal, node for node |
| 4 Time management and book | done | Book values exactly equal to C++; the time mode works, but at short times the unlimited clog search and the book exit can take longer than the limit |
| 5 Comparison with Stello | next | |
| 6 Multi-threading (YBWC) | | |
| 7 Docs | | |

## Egaroucid compared with Stello

This section compares how the two engines are built. Phase 5 measures what each part is worth.

### Board and moves

| | Egaroucid | Stello |
|---|---|---|
| Board | `player`, `opponent`: the side to move first | `Black`, `White` |
| Square numbering | a1 = bit 63, h8 = bit 0 | a1 = bit 0, h8 = bit 63 |
| Playing a move | in place, undone afterwards (`move` / `undo`), with the evaluation features updated along | a new immutable `Board` for every move |
| Flips | table lookups (`bb_seed`, `bb_flipped`) | walking the rays |
| Legal moves | shift-and-mask in 8 directions (`calc_legal`) | shift-and-mask (`LegalMoves`, Dumb7Fill) |

### Evaluation

| | Egaroucid | Stello |
|---|---|---|
| Kind | 16 trained patterns × 4 places = 64 features, plus a table indexed by the side to move's disc count | Hand-tuned from 1992: edge tables with a two-ply edge look-ahead, stable discs from the corners, mobility, potential mobility |
| Phases | 60, one per number of discs, each with its own weights | none |
| Score | whole discs (−64..64) for the side to move; tables in 1/32 discs | evaluation units (hundreds; a win is 32,600 + discs) |
| Size | 36.7 million int16 weights (73.5 MB), file `eval.egev2` 38 MB | 8 tables of 6,561 values |
| Cost | 64 table lookups; the features are updated move by move | a 10 × 10 mailbox per call, edge indices, a legal-move count, two potential-mobility counts |
| Extra | a separate move-ordering evaluation for the endgame (4 patterns, one phase) | — |

### Midgame search

| | Egaroucid | Stello |
|---|---|---|
| Algorithm | principal variation search everywhere, with separate null-window search functions | fail-hard alpha-beta; null windows only at the root after the first move |
| Transposition table | one table, 2^25 slots. Each slot holds a lower and an upper bound, depth and selectivity, two best moves and an importance flag. A position is looked for in 3 consecutive slots, the first slot of lower level is replaced, and all importances are cleared once 40 % of the slots have been newly taken | two tables (midgame and endgame) of 2^19 slots with two entries each (deepest and newest) |
| Selective search | Multi-ProbCut from depth 3: a search at about half the depth (a third in the endgame) predicts the result, with an error fitted as a cubic of discs and both depths, at 7 confidence levels (74 % … 100 %) | at depths 7 and 5, a depth-3 null-window search at beta + \|beta\|/2 + 50 |
| Enhanced transposition cutoff | from depth 16 | from 10 empty squares, endgame only |
| Move ordering | the transposition table's two moves first; then mobility (corners count double), potential mobility, and the value of a shallow search: the evaluation, a depth-1 search, or a 74 % search at depth/8 | hash move, a response-killer table, dynamic square values |
| Shallowest levels | special functions for depth 1 and 2, trying the squares in a fixed order (corners, centre, A/B, X/C) | the general search |
| Iterative deepening | +3 plies up to depth 15, then +1. The shown value is smoothed: (0.9 × previous + 1.1 × new) / 2 | +1 ply |
| Clog search | before a selective search, a full 5-ply search finds the root moves that end the game soon, with exact values | — |
| Levels | 0–60, each a midgame depth and selectivity, and from how many empty squares it solves the endgame at which selectivity (`level.hpp`) | depth or time |

### Endgame

| | Egaroucid | Stello |
|---|---|---|
| Staging | the endgame is searched with rising selectivity: 74 % → 88 % → … → 100 %, each pass filling the transposition table for the next | a win/loss/draw pass with window (−1, 1), then exact |
| More than 13 empty squares | the midgame code with the main table, ETC and Multi-ProbCut | the endgame solver |
| 11–13 empty squares | null-window search with a small local table (1,024 slots per number of empty squares). Ordering: mobility (corners double) × 40 + the move-ordering evaluation × 12. A child with at most one reply is searched at once | |
| 7–10 empty squares | local table; ordering by quadrant parity and mobility | evaluation ordering from 18 empty squares, fastest-first from 7 |
| 5–6 empty squares | squares that are alone in their region first (`empty1_bb`), no ordering beyond that | quadrant parity from 6 |
| Last 4 squares | special code (`last4` … `last1`), checking neighbours before computing flips | `SolveShallow`, `SolveLast2`, `SolveLast` |
| Stability cutoff | in every endgame node, with thresholds per number of discs | — |

### Opening book and time

| | Egaroucid | Stello |
|---|---|---|
| Book | 1,302,322 positions (`book.egbk3`, 32 MB), each stored in its canonical orientation (the smallest of the 8 symmetries) with the value in discs and the level it was searched at | about 11,200 positions with values in evaluation units; learns from games |
| Choosing a book move | among the children in the book: the best, drawn at random if several are equal (accuracy 0), or weighted towards the best at higher accuracies. A child is skipped if its value is more than 8 below the position's own | the best book move, with variation |
| Leaving the book | with accuracy 0, the moves that are not in the book get a null-window search against the best book value, and a full search if one of them beats it | — |
| Time | per game: `calc_time_limit_ply` keeps a margin, plans the midgame moves, and gives up to 90 % of the clock when the endgame looks solvable (its speed constants assume a many-threaded build). In time mode, selectivity is 100 % up to depth 13, then 74 % | per move or per game |

## C++ quirks the port reproduces

Some behaviour of the C++ code is surprising. Exact parity needs it reproduced, and the port comments each case.

- **`bit_around` is not the plain neighbourhood of a square.** It leaves out the neighbours in directions too short to flip anything, for example the corner next to a C-square. A computed 3 × 3 neighbourhood would give different node counts. Lesson: copy every C++ table; don't recompute it.
- **`last3` reads the empty squares of the wrong board.** Its parity test uses `search->board`, which is the position with 4 empty squares, not the 3-empty board it is given.
- **ETC code is compiled into the depth ≤ 4 search.** The preprocessor reads the `constexpr` depths in `#if MID_ETC_DEPTH_NWS <= MID_SIMPLE_ORDERING_DEPTH` as 0. The ETC itself never runs there, but the "no flips: stop" test in the move loop does.
- **The endgame update of the features is partial.** It updates only the 16 move-ordering features; the other 48 hold stale values. This is harmless because nothing reads them there.
- **The feature stack has 60 slots.** The search never updates the features for the 60th move. The port has one more slot, so tests can play whole games.
- **The console's single-threaded search still runs the parallel paths.** It always passes `use_multi_thread = true`, even with `-t 1`. The YBWC code then searches serially but sorts moves with `std::sort`, whose order of equal moves depends on the C++ library. The reference driver calls the engine with `false` instead.
- **The SIMD and Generic builds count endgame nodes differently** (`last1_nws`). The port follows Generic.
- **Hash numbers exist only for hash levels 25–27** (`resources/hash`). Any other level gets random numbers seeded by the clock, so searches can't be reproduced.
- **The endgame's local table is never cleared** (`thread_local` in C++). The reference driver clears it before each position.
- **The value smoothing uses C's `round`**, which rounds halves away from zero. .NET must use `MidpointRounding.AwayFromZero`.
- **The Multi-ProbCut table is computed with doubles.** A compiler may fuse multiply and add, so the reference is built with `-ffp-contract=off`; a checksum of the whole table matches.
- **`ai()` gives no move when the side to move must pass.** It computes the legal moves before it handles the pass, so it searches nothing: no move, value 126, depth −1. This is confirmed with the C++ reference. Egaroucid's own callers pass first, and so does the port's match adapter.
- **The clog search has no time limit.** In time mode it always searches 5 moves deep. On 1 thread that takes 25–190 ms, sometimes more than a whole 100 ms move. The iterative deepening then gets only what is left, at least one iteration. C++ hides this cost by searching the clog moves in parallel.
- **Leaving the book can take twice the time.** With accuracy 0, both the null-window search of the moves outside the book and the full search that may follow it get the whole time limit. In the smoke match, the move after the book took about 200 ms of a 100 ms limit.
- **An outer stop does not end the current iteration.** In time mode, C++ gives each iteration its own flag, which only the iteration's own timer clears.

## Where the port deliberately differs

None of these changes a result; the parity tests check that.

- **Bit order:** the port keeps Egaroucid's (a1 = bit 63), so the C# can be read next to the C++. The conversion to Stello's board happens in the match adapter (phase 5).
- **Flips:** computed along rays with bit scans instead of the C++ lookup tables. The flipped discs are given by the rules.
- **Flips of the last move** (`count_last_flip`): computed with the flip code instead of the tables. These are the same numbers.
- **Weights:** one table (73.5 MB) and a second, colour-swapped view of the features, instead of a second, colour-swapped copy of every table (147 MB). The Generic build does not clamp weights to ±4092, and neither does the port.
- **Globals:** the transposition table, the local table and the stop flag live in a `SearchContext` object, one per engine.
- **Transposition table size:** exact parity needs the C++ size, 2^25 + 2 slots, about 800 MB per context in C#. A smaller table uses the lower bits of the hash code. It searches differently, but is fine for matches, where every game needs its own table.
- **No locks yet:** the single-threaded port has none. The parallel paths throw `NotSupportedException` until phase 6.
- **The book reads value and level only**, since only they are used in play. The leaf data, line counts, editing, expansion and the older file formats (`.egbk2`, `.egbk`, Edax) are left out.
- **The book draws from a seeded `Random`.** C++ seeds its generator from the clock. With a seed, games and matches can be repeated.
- **Time mode, iterations:** C++ runs each iteration in its own thread and stops waiting when its time is up. The port runs it on the calling thread, with a flag that checks its deadline every 256 reads. A timer thread was tried first: under load, its callback came up to 260 ms late.
- **Time mode, stopping:** a stop from outside (Move Now) also ends the current iteration, because the iteration's flag has the outer flag as its parent.
- **Time mode, always a move:** if a stop comes before the first iteration has finished (for example during the clog search), a level-1 search gives the move. C++ can return no move then.
- **Time per game, extra pondering left out:** with 38 or more empty squares and at least 2 s for the move, C++ first runs `ai_get_values` (600 ms), then `ai_align_move_levels` and possibly `request_more_time`, to decide whether a move deserves extra time. These are left out (about 400 C++ lines of ponder and self-play machinery). Phase 5 uses mostly time per move; if time-per-game matches matter, this can be ported then.

## Measurements

All on 1 thread, on an Intel i7-12850HX laptop VM; timings vary by up to about 8 %.

| What | C# port | C++ Generic | Stello | Note |
|---|---:|---:|---:|---|
| perft 11 from the start (212,258,800 leaves) | 0.61 s | 1.35 s | — | port: rays and bit scans |
| Evaluation, incremental (move + evaluate + undo) | 2.2 M/s | — | — | |
| Evaluation from scratch | 0.4 M/s | — | — | used only at the root |
| FFO #40–44, exact | 4.3 s | — | 19.2 s | the same nodes as C++ |
| FFO #40–49, exact (520,296,918 nodes) | 28.2 s | about 19.4 s | 369 s | port 1.45× slower than C++, 13× faster than Stello |
| 100 `midgame.txt` positions, level 21 (1,449,749,430 nodes) | 180 s | about 161.5 s | — | 1.12×; about 8 M nodes/s |
| Loading the evaluation and hash numbers | 0.11 s | about 1.4 s (start-up incl. other tables) | — | |
| Loading the book (1,302,322 positions) | 0.22 s | — | — | about 50 MB in a `Dictionary` |
| 200 ms per move, `midgame.txt` | depth 10 at 100 %, clog search 23–90 ms | — | — | |
| Smoke match: port vs port, 100 ms per move, book on, 20 games | no illegal moves, 54 book moves, 1,146 searched | — | — | 1 m 21 s; slowest moves about 200 ms (book exit, clog search) |

- **What counts as a node differs:** the two engines count nodes differently, so their node rates can't be compared. Stello's mid-bench gives 3.3 M nodes/s; time to a given strength is what phase 5 measures.
- **The 13× endgame gap:** Stello solves exactly, while Egaroucid at level 60 steps up through the selectivities. Part of the gap may be that staging.

## Ideas for the improved engine

These are hypotheses until phase 5 has measured them.

- **The endgame solver is the clearest gap:** 13× on FFO #40–49. The candidate causes, to be tested one by one:
  - the selectivity staging that fills the transposition table;
  - ordering by a small trained evaluation from 11 to 13 empty squares;
  - the local table;
  - stability cutoffs;
  - neighbour checks in the last 4 squares.
- **Pattern evaluation with incremental features:** a stronger evaluation, and 64 lookups per leaf instead of Stello's mailbox and mobility counts.
- **Multi-ProbCut with a fitted error model,** instead of Stello's fixed margin.
- **Scores in discs everywhere:** midgame and endgame values then share one scale, and the book can mix them.
- **The transposition table:** bounds plus two moves per slot, and clearing importance instead of having two separate tables.
- **The clog search:** a cheap guard against missing a quick wipe-out in a selective search. On one thread at short time controls it is not cheap, though, so it needs a depth or time limit there.
- **Leaving the book:** checking the moves outside the book against the best book value, as Egaroucid does, guards against weak book lines. It needs its own time budget.
