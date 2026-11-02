# 05 – Move ordering

[Back to the index](README.md)

## In short

Alpha-beta search (chapter 06) is fastest when the best move in each position is searched first: then the other moves are refuted quickly and whole branches are cut off. The engine cannot know the best move in advance, so it guesses. The move stored in the hash table from an earlier search always comes first. For the other moves the midgame search has two orderings:

- **Near the horizon** (`look` < 4, the last 4 plies before the leaves) it must be cheap: replies that worked well against the opponent's last move before (the *response table*), then a fixed value per square that prefers corners and avoids the squares next to empty corners.
- **Further from the horizon** (`look` ≥ 4) a good order saves whole subtrees, so each move is scored by the opponent's mobility, potential mobility and the pattern evaluation after it, as Egaroucid does.

This chapter describes the ordering of the midgame search; the endgame solver has its own ordering (chapter 08).

## Data structures

| Name | Type | Contents |
|---|---|---|
| `BASE_SQUARE_SCORES` | `[i8; 64]` (constant) | The static value of each square (C++ `scores[100]`) |
| `CORNER_AREAS` | 4 × `CornerArea` | For each corner: the two C-squares, the edge square beyond each C-square, and the X-square |
| `MoveOrdering::responses` | `[i32; 2 × 64 × 64]` | For each replying colour, each previous move and each reply: a score (C++ `humres`/`comres`) |
| `EVALUATION_ORDER_MIN_LOOK` | 4 (in `search/engine.rs`) | From this `look` on, `order_by_evaluation` replaces `order` |
| `ORDER_OFFSET`, `ORDER_MOBILITY`, `ORDER_POTENTIAL_MOBILITY`, `ORDER_VALUE` | 38, 35, 17, 269 (in `endgame/search.rs`) | Egaroucid's weights of the evaluation ordering (`move_ordering.hpp`), shared with the solver |

The static square values: corners 127, X-squares −64, C-squares −32, the other edge squares 22 or 11, and small values inside the board.

![Two heat maps. Left: the static square values, 127 on the corners, −64 on the X-squares, −32 on the C-squares. Right: the values after the squares next to the corners are recomputed for Black, with Black on a1 and White on h8 and f8.](images/square-values.svg)

## Algorithm

### Ordering the moves of one position

`MoveOrdering::order(moves, hash_move, previous_move, player, board)` in [search/ordering.rs](../Stello.Rust/stello-core/src/search/ordering.rs) sorts the list of legal moves in place:

```mermaid
flowchart TD
    Start["Legal moves, in square order a1 ... h8"] --> Hash{"Hash move given<br/>and in the list?"}
    Hash -- "yes" --> Front["Swap it to the front"]
    Hash -- "no" --> Resp
    Front --> Resp{"Previous move known?"}
    Resp -- "yes" --> Sort1["Sort the rest by their response score<br/>for (player, previous move)"]
    Sort1 --> Keep["Keep the moves with a score > 0 in that order"]
    Keep --> Squares
    Resp -- "no" --> Squares["Sort the remaining moves<br/>by square value for this position"]
    Squares --> Done["Ordered list"]
```

All sorting uses `sort_descending`, a stable sort: moves with the same key keep their square order. This makes the search deterministic. It is a *rank sort*: each move goes straight to its place, which is the number of keys that are higher than its own, or equal and before it in the list. That takes $n^2$ comparisons, but the comparisons are added up without branches, so the processor never mispredicts one, and the lists of Othello are short (usually fewer than 20 moves). The result is the same order as the insertion sort of C# 2.0 (`SortDescending`).

### Square values for this position

`square_scores` starts from the static values and recomputes the squares next to each corner, from the point of view of the side to move:

| Corner is … | C-squares | X-square |
|---|---|---|
| owned by the side to move | 34 each | 24 |
| owned by the opponent | 32 if the opponent also owns the next edge square beyond the C-square, otherwise −32 | −16 |
| empty | −16 each | −64 |

Next to an own corner, the C- and X-squares are safe and become good moves. Next to an empty corner they are dangerous, because they can give the opponent the corner. Next to an opponent's corner a C-square is good only if it connects to the opponent's edge discs, so that the move cannot easily be flipped back.

### The response table

After each move `move` has been searched, the search records the opponent's best reply `reply` in the table (`record_response`):

- **+4** if the reply refuted the move (the move's score did not rise above alpha);
- **+1** otherwise.

The row is chosen by the replying colour and the move it replied to, so the table learns "after this move, that reply is strong". When the same move appears again elsewhere in the tree, the reply is tried early (`order` with `previous_move`). The table is cleared at the start of every search (`clear_responses`), so it only holds knowledge from the current search. This idea is often called a *killer response* or *counter-move* heuristic.

### Evaluation ordering in deeper nodes

At nodes with `look` ≥ 4 (`EVALUATION_ORDER_MIN_LOOK`), `order_by_evaluation` in [search/engine.rs](../Stello.Rust/stello-core/src/search/engine.rs) replaces `order`. The hash move goes first. Every other move gets a key from the position after it, with Egaroucid's weights (`move_ordering.hpp` at depth 0):

$$\text{key} = 35\,(38 - r) + 17\,(38 - p) - 2.69\,v$$

- $r$ is the opponent's mobility: 2 per legal reply, plus 1 more for each corner reply (`endgame::bits::weighted_moves`, Egaroucid's `get_weighted_n_moves`);
- $p$ is the opponent's potential mobility: the empty squares next to the mover's discs, where a disc on an edge counts only along that edge, the only direction in which it can be flipped (`endgame::bits::potential_mobility`, Egaroucid's `get_potential_mobility`);
- $v$ is the pattern evaluation for the opponent, in 1/100 disc (`evaluate_leaf`, so each key counts as a node and an evaluation); $2.69\,v$ is 269 per disc.

Fewer replies, less potential mobility and a worse position for the opponent give a higher key. The hash move gets the largest possible key, and the moves are sorted by key, highest first, with the stable `sort_descending`. The response table is not used in these nodes, although the replies are still recorded there: with the response moves before the evaluated moves the search was weaker. Below `look` 4 the cheap ordering is used, because an evaluation per move costs too much so near the leaves. Why these limits were chosen, and the other orderings that were tried, is told in [chapter 23](23-phase-5-the-search.md).

### Where the ordering is used

- At every node of the midgame search with `look` < 4, with the hash move and the opponent's last move (chapter 06); deeper nodes use the evaluation ordering above.
- At the root, `order` runs once at the start of a search, without a hash move or a previous move, so only by square value; after that, the root list is reordered by the search itself: each move that becomes the best is moved to the front for the next iteration (chapter 06).
- The endgame solver does not use `MoveOrdering`. It uses Egaroucid's keys with the same weights: mobility, potential mobility and the evaluation after a small search in the larger nodes, mobility and parity near the end (chapter 08).

## Worked example

In the midgame position of [chapter 04](04-evaluation.md#a-midgame-position), Black has eight legal moves. Suppose the position is a node near the horizon (`look` < 4), where `order` is used. The square values for this position (a1 is White's, h1 and h8 are empty, a8 is empty):

| Move | e2 | f4 | g5 | a6 | f6 | f7 | b8 | c8 |
|---|---|---|---|---|---|---|---|---|
| Square value | −5 | 1 | −5 | 22 | 12 | −8 | −16 | 22 |

- **Without a hash move or a previous move** (as at the root): `a6 c8 f6 f4 e2 g5 f7 b8`. a6 and c8 have the same value and keep their square order; so do e2 and g5.
- **With hash move f4, and responses recorded after White's last move a4** (e2 refuted a move before: 4; g5 was the best reply once: 1): `f4 e2 g5 a6 c8 f6 f7 b8`. The hash move comes first, then the moves with a response score, then the rest by square value.

b8 is last: it is a C-square next to the empty corner a8.

## Design notes

- **Cheap and simple near the leaves.** `order` uses no search and no evaluation, only a hash lookup, a table lookup and a few comparisons, because it runs at most nodes. Only the deeper nodes, where a good order saves a whole subtree, pay for an evaluation per move.
- **Bug fixed.** In C++, after sorting by response score, the code read the response of the wrong square (the −11 offset of the legacy numbering was missing). The .NET port read the right entry, and the Rust engine does too.
- **One table per colour.** C++ had one table for the computer and one for the human. Since the .NET port there is one per colour, which is the same while one side is the computer, and also correct when the engine searches for both sides (self-play, book learning).
- **No global state.** The dynamic square values are computed into a local array instead of overwriting a global table, and the response table belongs to the search engine (a `MoveOrdering` in each `SearchEngine`).
- **The same weights as the solver.** The evaluation ordering and the endgame solver's ordering share the constants `ORDER_OFFSET`, `ORDER_MOBILITY`, `ORDER_POTENTIAL_MOBILITY` and `ORDER_VALUE` and the counting functions in `endgame::bits`, so both follow Egaroucid's `move_ordering.hpp` in the same way.

## Where in the code

| File | Main members |
|---|---|
| [search/ordering.rs](../Stello.Rust/stello-core/src/search/ordering.rs) | `BASE_SQUARE_SCORES`, `CORNER_AREAS`, `MoveOrdering` (`order`, `record_response`, `clear_responses`), `square_scores`, `sort_descending` |
| [search/engine.rs](../Stello.Rust/stello-core/src/search/engine.rs) | `EVALUATION_ORDER_MIN_LOOK`; the calls to `order` and `record_response` in `search` and `search_root`; `order_by_evaluation` |
| [endgame/search.rs](../Stello.Rust/stello-core/src/endgame/search.rs), [endgame/bits.rs](../Stello.Rust/stello-core/src/endgame/bits.rs) | The weights `ORDER_OFFSET`, `ORDER_MOBILITY`, `ORDER_POTENTIAL_MOBILITY`, `ORDER_VALUE`; `weighted_moves`, `potential_mobility` |
| C# 2.0 (frozen): [Search/MoveOrdering.cs](../Stello.Net/Stello.Engine.V2/Search/MoveOrdering.cs), [SearchEngine.cs](../Stello.Net/Stello.Engine.V2/SearchEngine.cs) | `Order`, `SquareScores`, `RecordResponse`, `SortDescending` (an insertion sort); `OrderByEvaluation` |
| C++: [Sort.cpp](../Stello%20C++/BRAIN/Sort.cpp) | `sortlist`, `simsort`, `scores` |

## Tests

- [search/ordering.rs](../Stello.Rust/stello-core/src/search/ordering.rs) (unit tests): `sort_descending` is stable and sorts highest first; `order` puts the hash move first, then the moves with a response score (a refutation before a best reply), then the rest by square value.
- Through the search: in a plain alpha-beta search the ordering only affects speed, not the result; with Multi-ProbCut and the hash table it can change which cuts are made, and so the score a little, but never the legality of the move. Any change of the order would change the node counts, so the recorded 2.0 signatures (move, score and nodes of 100 midgame searches and 5 solves, in [StelloV2Tests.cs](../Stello.Net/Stello.Engine.Tests/StelloV2Tests.cs) through the wrapper; 20 of the searches and the 5 solves also in [tests/search.rs](../Stello.Rust/stello-core/tests/search.rs)) check the whole ordering, `order_by_evaluation` included. The other search tests add the same result every time (`FixedDepth_IsDeterministic` in [SearchEngineTests.cs](../Stello.Net/Stello.Engine.Tests/SearchEngineTests.cs)), the strength tests in [StrengthTests.cs](../Stello.Net/Stello.Engine.Tests/StrengthTests.cs), and the endgame positions in [EndgameTests.cs](../Stello.Net/Stello.Engine.Tests/EndgameTests.cs), which only finish in time with good ordering.

## Further reading

- [Alpha-Beta – Chess Programming Wiki](https://www.chessprogramming.org/Alpha-Beta): the section "Savings" explains why the best move first gives the most cutoffs.
- [Writing an Othello program – Gunnar Andersson](http://radagast.se/othello/howto.html): the section "Move ordering" describes killer responses in Othello (after an X-square move, try the corner first).
- [Egaroucid on GitHub](https://github.com/Nyanyan/Egaroucid): `src/engine/move_ordering.hpp`, the source of the evaluation ordering and its weights.

---

Previous: [04 Evaluation](04-evaluation.md) · Next: [06 Midgame search](06-midgame-search.md)
