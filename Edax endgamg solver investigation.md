# Edax endgame solver investigation

## Background

- **What was read:** the source code of [Edax](https://github.com/abulmo/edax-reversi) by Richard Delorme and Toshihiko Okuhara, version 4.6. It is a shallow clone of the `master` branch, commit `14f048c` of 10 March 2025.
- **Files studied:**
  - [endgame.c](https://github.com/abulmo/edax-reversi/blob/master/src/endgame.c), [search.c](https://github.com/abulmo/edax-reversi/blob/master/src/search.c) and [root.c](https://github.com/abulmo/edax-reversi/blob/master/src/root.c);
  - [midgame.c](https://github.com/abulmo/edax-reversi/blob/master/src/midgame.c), [move.c](https://github.com/abulmo/edax-reversi/blob/master/src/move.c) and [hash.c](https://github.com/abulmo/edax-reversi/blob/master/src/hash.c) with `hash.h`;
  - [board.c](https://github.com/abulmo/edax-reversi/blob/master/src/board.c), `bit.c` and [settings.h](https://github.com/abulmo/edax-reversi/blob/master/src/settings.h);
  - the flip generators `flip_bmi2.c` and `flip_avx_ppfill.c`, and the last-flip counter `count_last_flip_bmi2.c`.
- **Licence:** Edax is under the **GNU GPL version 3**. Its ideas can be used freely, but its code (including its generated tables) cannot be copied into Stello unless Stello is also released under the GPL. Everything below is described as an idea to re-implement.

**Stello today** (after phase 8 round 2, Release build, 2026-09-25):

| Measure | Value |
|---|---|
| FFO #40–#44 | 18.7 s, 328 M nodes |
| endgame.c suite (112 positions × 30) | 2.31 s, 39.1 M nodes |
| Speed | about 16–17 M nodes/s |

Edax's own comments in `settings.h` give about 60–70 M nodes/s for its flip generators on the author's machine. The two programs count nodes differently, so only times are directly comparable.

---

## 1. The endgame solver

### 1.1 How the Edax solver is built

Edax uses a different routine for each range of empty squares. Every routine except the root is a **null-window search** (NWS): it receives only α, and β = α + 1.

| Empty squares | Routine | What it uses |
|---|---|---|
| ≥ 15 (when solving) | `NWS_midgame`, `PVS_midgame` | Ordering by a shallow midgame search with the pattern evaluation, hash table, ETC, stability cutoff, ProbCut at selectivity < 100 % |
| 8–14 | `NWS_endgame` | Hash table (probed only with 2+ moves), stability cutoff, fast ordering (`movelist_evaluate_fast`), lazy selection of the next best move, hash store with the search cost |
| 5–7 | `search_shallow` | No hash table and no move list; a linked list of empties walked odd quadrants first; stability cutoff; incremental parity |
| 4 | `search_solve_4` | Fully unrolled; stability cutoff; parity cases sorted by swaps |
| 3 | `solve_3` | Unrolled; parity plus square value sorted by swaps |
| 2 | `solve_2` | Unrolled; neighbour pre-check |
| 1 | `solve_1` | Counts the flips only (table lookup), with a lazy cut-off |

At the root, `iterative_deepening` first runs a midgame search to depth *n* − 10 + 2 (`ITERATIVE_MIN_EMPTIES` = 10). It then solves with an **aspiration window** around that score, and with **increasing selectivity** (ProbCut at 73 %, 87 %, … 100 %) for 21 or more empty squares.

### 1.2 Comparison with Stello

| Topic | Edax | Stello |
|---|---|---|
| Flips | Table lookup per line with BMI2 `pext`/`pdep`, or AVX2 in parallel for 4 directions | A loop per direction (`Bitboards.Flips`, `RayLeft`/`RayRight`) |
| Last square | Counts flips only, via a table; the opponent's count only if α requires it | Computes the flips bitboard, then two popcounts; no α |
| Legality pre-check | `NEIGHBOUR[x] & opponent` before computing flips | None; `Flips` is called for every empty square |
| 2 / 3 / 4 empty squares | `solve_2`, `solve_3`, `search_solve_4`, fully unrolled | `SolveLast2` (round 2); 3–6 in the generic `SolveShallow` |
| Parity | 4-bit quadrant mask updated with XOR per move | 4 popcounts per node over the quadrants |
| Window inside the tree | Null window everywhere; PVS and aspiration only at the root | PVS in `Solve` with full windows in pass 2 |
| Root strategy | Midgame search first, then an aspiration solve, with iterative selectivity | Win/loss/draw pass, then the exact pass |
| Hash table | 4-way buckets, lower and upper bound, 2 best moves, replacement by date, then search cost, then depth, prefetch | 2 entries per slot, one value and a bound type, 1 move, replacement by depth |
| Hash use | From 8 empty squares | From 7 empty squares |
| ETC | Only in the midgame NWS (depth > 5), **not** in `NWS_endgame` | From 10 empty squares |
| Stability cutoff | Tried only when α is above a threshold per number of empties | Tried and rejected in round 1 (without thresholds) |
| Ordering key (8–14) | Hash moves 1 and 2; wipeout; weighted mobility (corners ×2); mover's corner stability; potential mobility; parity bonus; square value | Hash move; weighted mobility (corners ×2); square value |
| Sorting | Lazy selection of the next best move (stops at a cutoff) | Insertion sort of all moves |
| Parallel search | YBWC on several threads | Single thread |

### 1.3 Ideas, most promising first

Each idea gives:
- what Edax does;
- where to look in the Edax source;
- what it would mean for Stello;
- the expected effect and the effort.

"Measure" means the round-2 method: the suite and FFO in Release, with alternating runs when the difference is small.

#### E1. Faster flips (largest expected gain)

- **Edax:** `flip()` in `flip_bmi2.c` handles each of the 4 lines through the square (row, column, 2 diagonals) with a table lookup.
  1. Extract the line's opponent and own bits with `pext`.
  2. Look up the outflank and the flipped pattern in two small tables (`OUTFLANK[8][64]`, `FLIPPED[8][144]`).
  3. Deposit the result back with `pdep`.

  `flip_avx_ppfill.c` does all directions at once with AVX2 and the "lowest set bit / carry" trick.
- **Stello:** `Bitboards.Flips` walks 8 rays with a `while` loop each. Flips are computed for every tried square in `SolveShallow` and for every move in `Solve`. At 16 M nodes/s this is very likely the main cost per node. Round 2 showed that the shallow solver is limited by cost per node, not by ordering.
- **In C#:**
  - `System.Runtime.Intrinsics.X86.Bmi2.X64.ParallelBitExtract` / `ParallelBitDeposit` for the BMI2 variant;
  - or `Vector256` with `Avx2.ShiftLeftLogicalVariable` / `ShiftRightLogicalVariable` for the AVX2 variant;
  - keep the current code as the fallback when `IsSupported` is false.

  The tables must be generated by Stello's own code (for example in a static constructor), not copied. A branch-free carry version without tables (the "Kogge-Stone" or "o^(o-2r)" style per line) is another option.
- **Expected:** the biggest single gain. The same function also serves the midgame search. Effort: medium.
- **Test:** perft 1–8 and all endgame tests must be unchanged. Also add a unit test that compares the new `Flips` with the old loop version on many random positions and all squares.

#### E2. Last square: count only, and a lazy cut-off

- **Edax:** `solve_1(player, alpha, x)` computes the score as 2 · discs − 64 + 2 + n, where n comes from a table (`count_last_flip`). It never builds the new board. If the side to move cannot play x, it only counts the opponent's flips when the score could still exceed α; otherwise it returns at once.
- **Stello:** `SolveLast` computes the flips bitboard (`Flips`), then `FinalScore` with two popcounts, and has no α. `SolveLast2` passes no window to it.
- **Change:** give `SolveLast` an α and apply the lazy cut-off. Count flips with a count-only routine, which could share E1's tables.
- **Expected:** small to medium. This is the most frequent node type. Effort: small once E1 exists.

#### E3. Neighbour pre-check

- **Edax:** before every `flip()` in the shallow routines, it checks `NEIGHBOUR[x] & opponent`. A move is only possible next to an opponent disc.
- **Stello:** `SolveShallow`, `SolveLast2` and `SolveLast` call `Flips` for every empty square, legal or not.
- **Change:** add a static `ulong[64]` table of neighbour masks and skip squares without an adjacent opponent disc.
- **Expected:** small to medium (many empty squares near the end are illegal). Effort: very small; a good first step.

#### E4. Unrolled solvers for 3 and 4 empty squares, with incremental parity

- **Edax:** `solve_3` and `search_solve_4` take the empty squares as parameters (x1…x4) and sort them only by swaps.
  - At 4 empty squares only the "1 1 2" quadrant pattern needs sorting.
  - At 3, it uses parity plus the square value.
  - Parity is a 4-bit mask: `parity ^= QUADRANT_ID[x]` per move, with no popcounts.
- **Stello:** round 2 did 2 empty squares. The specification lists "last 3–4 empty squares" as the next candidate. A prepared array of squares (round 2) was 25 % slower. So the squares should be passed as parameters or locals, not kept in a field array.
- **Expected:** small to medium (another few %). Effort: medium; many pass and game-over cases, covered by `Solve_MatchesMinimaxOnTheLastFewSquares` and the endgame.c suite.

#### E5. Hash table: cost-based replacement and two bounds

- **Edax** (`hash.c`, `hash.h`):
  - **4-way buckets.** The victim is the entry with the lowest `draft.u4`, which orders by date, then **search cost** (log₂ of the nodes the search took), then selectivity and depth.
  - **Two bounds per entry.** Each entry keeps a **lower and an upper bound**. When the same position is stored again at the same depth, the bounds are narrowed instead of overwritten (`data_update`).
  - **Two best moves** (`move[0]`, `move[1]`).
  - **Prefetch** of the bucket as soon as the hash code is known.
  - **Single-move nodes** are neither probed nor stored.
- **Stello:** 2 entries per slot with depth-based replacement, one value and a bound type, and one move. Round 1 showed that entries being overwritten was the biggest problem (FFO #43). In the endgame, most siblings have the same depth (empty squares), so depth hardly separates good entries from bad; the search cost does.
- **Change:** in stages, each measured on its own:
  1. replace by cost (needs a node counter per stored search, as `cost = nodes after − nodes before`);
  2. store lower and upper bounds;
  3. keep a second move;
  4. prefetch (`Sse.Prefetch0` in C#);
  5. skip single-move nodes.
- **Expected:** medium for the deep FFO positions, little for the 12-empty suite. Effort: medium. `TranspositionTableTests` must be extended.

#### E6. Better ordering key (8–14 empty squares)

- **Edax** (`movelist_evaluate_fast`):
  - wipeout first, then the 2 hash moves;
  - then: square value (JCW table) + parity bonus (8 below 12 empty squares, 4 above) + (36 − potential mobility) · 2⁵ + corner stability of the mover · 2¹¹ + (36 − weighted mobility) · 2¹⁵.
- **Stello:** hash move, then −256 · weighted mobility + `BaseScore`. Round 1 found potential mobility neutral. The **corner stability**, **parity bonus**, second hash move and wipeout are not tried yet. Edax also picks the next best move lazily (`move_next_best`), which avoids sorting moves that are never searched after a cutoff.
- **Expected:** small (a few % nodes). Effort: small.

#### E7. Stability cutoff, Edax style

- **Edax** (`search_SC_NWS`): the stability cutoff is **only tried when α is high**. The thresholds are in `NWS_STABILITY_THRESHOLD`, for example α ≥ 12 at 7 empty squares, ≥ 20 at 10, ≥ 30 at 15. The score is then at most 64 − 2 · (the opponent's stable discs). If that is ≤ α, the node fails low at once. It is used in `NWS_endgame`, `search_shallow` and `search_solve_4`, and in the midgame NWS.
- **Stello:** round 1 tried a stability cutoff at every node. The node count hardly changed and the time got slightly worse. The threshold is what keeps Edax's version cheap.
- **Change:** retry with a threshold table. This counts as a retest in a new form, not a repeat of the rejected idea.
- **Expected:** uncertain; small to medium in lopsided positions. Effort: small to medium (the round-1 stability code was reverted and would have to be written again).

#### E8. Null window inside the tree, aspiration at the root

- **Edax:** all endgame nodes are NWS. The root (`aspiration_search`) searches a small window around a guess and widens it on failure: first ±`width`, then doubling. `PVS_root` re-searches only at the root.
- **Stello:** pass 1 is already a null window (−1, 1). Pass 2 uses (result − 1, 65) after a win, and `Solve` then does PVS with full windows inside. Round 1 found MTD(f) from the win/loss/draw bound worse. The specification notes: "only worth trying again with a good first guess".
- **Change:** needs a good first guess (see E9). Then solve with an aspiration window of about ±2 discs around it, instead of pass 1 + pass 2.
- **Expected:** medium, but only together with E9. Effort: medium.

#### E9. A midgame search before the solve

- **Edax:** before solving 20+ empty squares, it runs iterative deepening to depth *n* − 8. This gives a score guess for the aspiration window and fills the hash table with best moves for the solve.
- **Stello:**
  - With `SearchLimits.Solve` (FFO tests, book learning) no midgame search runs at all.
  - In play, the midgame iterations only help the root order.
  - The midgame and endgame hash tables are separate, so the solver never sees the midgame's best moves.
  - Stello's midgame scores are in evaluation units, not discs, so they cannot be used directly as the guess.
- **Change:** either
  - let `Solve` use the midgame table's best move as the hash move where the endgame table has none (cheap), or
  - map evaluation units to a disc estimate for the aspiration window (needs a calibration run on many positions).
- **Expected:** uncertain; possibly medium for #43-like positions, where most time is spent proving the exact score. Effort: small (first part) to large (second part).

#### E10. Even scores

With the empty squares given to the winner, every final score is even. Edax relies on this: it moves odd root bounds to even and re-searches if it gets an odd score. For Stello this means pass 2 and any aspiration window can use even bounds, and a null-window test at an odd α is as strong as a two-point window. **Expected:** small. Effort: very small; a detail of E8.

#### E11. Selective endgame (iterative selectivity)

Edax solves positions with 21 or more empty squares first with ProbCut at 73 %, 87 %, … confidence, and only then exactly. Each pass fills the hash table for the next, like iterative deepening in the endgame. It depends on a statistical ProbCut model (`eval_sigma`) and on a disc-unit evaluation. **For Stello:** only realistic after G4 (section 2). Effort: large.

#### E12. Parallel search

Edax splits nodes over several threads (Young Brothers Wait Concept, `ybwc.c`, `node_split`). This can give a large speed-up on a multi-core PC, but it changes the engine's "one search at a time, single-threaded" design. The hash table would also need thread safety. Effort: large. See also G8.

#### Not worth copying

- **`USE_SOLID`** (hashing positions with full-line discs moved to one side) is switched **off** in `settings.h`.
- **The switch from hash to shallow solver at 7/8 empty squares** is within noise for Stello (round 1: thresholds 5–7 and 6–8).
- **ETC below 15 empty squares:** Edax does not use it there, but in Stello round 1 it saved about 10 % nodes, so it stays.

### 1.4 Suggested order for round 3

| Step | Idea | Why this order |
|---|---|---|
| 1 | E3 neighbour pre-check | Tiny, independent, easy to measure |
| 2 | E1 faster flips | The largest gain; everything else is measured on top of it |
| 3 | E2 counting last square with lazy cut-off | Uses E1's tables |
| 4 | E4 unrolled 3 and 4 empty squares | The remaining candidate from the specification |
| 5 | E5 hash replacement by cost, then two bounds | For the deep FFO positions |
| 6 | E6 ordering key, E7 stability cutoff with thresholds | Small, independent tests |
| 7 | E9 + E8 + E10 midgame guess and aspiration solve | Only if 1–6 are not enough for the 10 s target |

Measure each step alone, as in round 2, and keep it only if it is faster. The endgame.c suite and `Solve_MatchesMinimaxOnTheLastFewSquares` cover the shallow routines. FFO #40–#44 cover the deep ones.

---

## 2. The general (midgame) search

### 2.1 How the Edax midgame search is built

- **Root** (`root.c`):
  - **Iterative deepening in steps of 2 plies**, starting at about depth 6, so all iterations have the same parity (the odd/even effect of Othello evaluations).
  - Each iteration uses an **aspiration window** around the previous score: the width is 10 − depth, at least 1, and doubles on a fail.
  - When the result of the previous move is still in the PV hash table, the search restarts from that level (`USE_PREVIOUS_SEARCH`).
- **PV nodes** (`PVS_midgame`) search the first move with the full window and all others with a null window, re-searching on success. **Non-PV nodes** (`NWS_midgame`) are null-window only.
- **At every node:**
  1. hash probe (main table plus a small PV table near the root);
  2. stability cutoff;
  3. transposition cutoff;
  4. **ProbCut**;
  5. move sorting;
  6. **ETC**;
  7. the moves;
  8. hash store with the search cost.
- **ProbCut** (`search_probcut`) is statistical. The error of a shallow search against a deep one is `eval_sigma(empties, depth, shallow depth)`, and the margin is t · σ with t from the selectivity level (73 %…100 %). The shallow searches are only started if a static evaluation already suggests a cut (`eval_score >= eval_beta`), which saves many useless ProbCut searches. At most 2 levels of recursive ProbCut are allowed.
- **Move sorting** (`movelist_evaluate`):
  - at low depth (`MIN_DEPTH` table), the fast key of E6;
  - otherwise each move is scored by a **shallow search** (0–6 plies, deeper at PV nodes via `inc_sort_depth`), plus mobility, potential mobility, edge stability and parity, with 2 hash moves first and a bonus if the position is in the hash table.
- **The last plies** are special-cased (`search_eval_1`, `search_eval_2`), and depth ≤ 3 uses a lighter `NWS_shallow`. The shallow searches used for move sorting have their own small hash table (`shallow_table`).
- **PV extension:** near the end, the PV alone is searched to the end sooner (`depth_pv_extension`).
- **Evaluation:** learned patterns, updated **incrementally** when a move is made (`search_update_midgame`). Scores are in discs.
- **Parallel search** with YBWC.

### 2.2 Comparison with Stello

| Topic | Edax | Stello |
|---|---|---|
| PVS | At every PV node; null window at all other nodes | Only at the root; plain alpha-beta inside the tree |
| Iterative deepening | Steps of 2, from about depth 6, aspiration windows | Steps of 1, from depth 1, full window (−32 664, 32 664) |
| Previous search | Reused from the PV hash table | The hash table survives, but the search starts at depth 1 again |
| Hash use | At every node from depth 3; ETC; lower and upper bound; 2 moves; replacement by cost | Only near the root (C++ heights `_hashGetHeight`, `_hashPutHeight`); no ETC in the midgame |
| Selective search | Statistical ProbCut, error tables per empties and depth, eval pre-test, several selectivity levels | Fixed margin ½·\|bound\| + 50 at `look` 7 and 5 (C++ `SELEXT`) |
| Move ordering | Shallow search + mobility + stability + parity; 2 hash moves | Hash move, response table, static square values |
| Stability cutoff | Yes (thresholded) | No |
| Leaf code | Special 1- and 2-ply routines | Leaves evaluated in the parent's loop (as in C++) |
| Evaluation | Patterns, incremental, in discs | Edge tables + mobility, computed from scratch, in evaluation units |
| Threads | Several | One |

### 2.3 Ideas, most promising first

#### G1. Hash table in the whole tree, with ETC

- **Edax:** probes and stores at every node from depth 3, and uses the stored best moves for ordering everywhere. ETC is used at depth > 5.
- **Stello:** the midgame table is only used at the top plies (the C++ heights). Deeper nodes never get a hash move, which hurts ordering most exactly where most nodes are.
- **Change:** measure the heights first. Try using the table down to `look` ≥ 2 or 3, then add ETC for `look` ≥ 5, then (with E5) cost-based replacement.
- **Test:** node count and time at fixed depth 10–12 on the SearchEngineTests midgame position and a set of midgame positions (for example random positions after 20–30 plies). The best move and score at fixed depth may change slightly, because the hash table then gives other cutoffs.
- **Expected:** medium to large (fewer nodes at the same depth). Effort: small to medium.

#### G2. PVS / null window in the whole tree

- **Edax:** PV nodes use PVS and all other nodes a null window; a PV node re-searches only when a move beats α.
- **Stello:** plain alpha-beta inside the tree. With good ordering, most moves only need a null-window proof, which is cheaper than a full-window search.
- **Change:** use PVS at all nodes with a window wider than 1, as `Solve` already does in the endgame.
- **Expected:** medium. Effort: small; the endgame `Solve` already shows the pattern.

#### G3. Aspiration windows and iterative deepening by 2

- **Edax:** the root window is centred on the previous iteration's score and widened on a fail. The iterations go up by 2 plies, so the evaluation parity is the same each time.
- **Stello:** every iteration uses the full root window, and the iterations go up by 1 ply.
- **Change:**
  1. Add an aspiration window around the previous score. The width must be in evaluation units, so measure typical score changes between iterations first.
  2. Separately, try steps of 2.
- **Expected:** small to medium. Effort: small.

#### G4. Statistical ProbCut

- **Edax:**
  - Margins come from measured standard deviations of shallow against deep searches.
  - A cheap static-evaluation pre-test decides whether a ProbCut search is worth starting.
  - It uses several depth pairs and up to 2 recursion levels.
- **Stello:** a fixed margin of ½·|bound| + 50 at only two depths (`look` 7 and 5), from the C++ code. Buro's ProbCut paper, and Edax, show that fitted margins cut much more safely and more often.
- **Change:**
  1. Measure, for Stello's evaluator, the regression between shallow (`look` 3) and deep (`look` 7, 5) scores on many positions, in evaluation units.
  2. Use those margins, with an evaluation pre-test.
  3. Test the strength with self-play matches (old against new at the same time per move), not only speed.
- **Expected:** large for playing strength per time. Effort: large (data collection and tuning). This is also the precondition for E11.

#### G5. Better midgame move ordering

- **Edax:** sorts by a shallow search when the remaining depth is large, and by mobility, stability and parity otherwise.
- **Stello:** hash move, response table (killer-like) and static square values.
- **Change:**
  - At `look` ≥ 4 or so, try ordering by a 0- or 1-ply search (Stello's evaluation), or by −(opponent's weighted mobility) + corner stability, as in E6.
  - Keep the response table as a tie-breaker.
  - Round 1 found shallow-search ordering bad in the **endgame**. The midgame is different: there the evaluation *is* the target, so ordering by it is consistent.
- **Expected:** medium, together with G1. Effort: medium.

#### G6. Incremental evaluation and special leaf routines

Edax updates the evaluation features when a move is made instead of recomputing them. Stello's `Evaluator` recomputes the edge indices and mobility at every leaf. An incremental version (update the 4 edge indices only when a move touches an edge) could speed up leaves. Edax's `search_eval_1`/`search_eval_2` avoid the general node overhead near the horizon, which is similar to what Stello already does by evaluating in the parent's loop. **Expected:** small to medium. Effort: medium; the evaluation code must stay equal to C++ (tests exist).

#### G7. Reuse of the previous search and PV extension

- **Edax:** after its own move and the opponent's reply, it starts from the depth stored for the new position in the PV table instead of from the start. `USE_PV_EXTENSION` solves the PV to the end a bit earlier than the rest of the tree.
- **Stello:** keeps the hash tables between moves, but always starts at `look` = 0.
- **Change:** start the iterations at the depth of the stored entry for the root, if any.
- **Expected:** small (saves the first iterations in play). Effort: small.

#### G8. Parallel search

The same as E12. It gives the largest possible speed-up in both midgame and endgame, but it is also the largest change to the design. Consider it last.

### 2.4 What does not transfer directly

- **The pattern evaluation itself.** Edax's strength and its good shallow-search ordering come largely from a learned evaluation in disc units. Stello keeps the C++ evaluation on purpose, so ideas that depend on a disc-unit evaluation need their own calibration in evaluation units:
  - aspiration widths;
  - ProbCut margins;
  - a score guess for the endgame.
- **Selectivity levels and the time management** that go with them (`search_adjust_time`, `solvable_depth`) belong to Edax's tournament play and would only matter after G4.

### 2.5 Suggested order

| Step | Idea | Why this order |
|---|---|---|
| 1 | G2 PVS in the tree | Small change, well-known gain |
| 2 | G1 hash table deeper, then ETC | Largest expected node saving |
| 3 | G3 aspiration windows | Small change at the root |
| 4 | G5 ordering | Builds on G1 (hash moves everywhere) |
| 5 | G7 reuse of the previous search | Small |
| 6 | G4 statistical ProbCut | Large; needs data and self-play testing |
| 7 | G6 incremental evaluation, G8 parallel search | Large changes |

For the midgame, speed at a fixed depth is not the only measure. A change that alters which moves are cut (G1, G3, G4) should also be tested for playing strength, with matches of the old against the new version at the same time per move.
