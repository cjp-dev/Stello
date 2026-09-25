# 14 – Glossary

[Back to the index](README.md)

The terms used in these documents. The last column gives the chapter that explains the term in detail.

<!-- Chapter numbers become links when the chapters are written. -->

| Term | Meaning | Chapter |
|---|---|---|
| Alpha | The score the side to move is already sure to get; worse moves need not be searched exactly. | [07](07-midgame-search.md#negamax-with-alpha-beta) |
| Beta | The score the opponent will allow at most; a move that reaches beta is good enough to stop (a cutoff). | [07](07-midgame-search.md#negamax-with-alpha-beta) |
| Bitboard | A 64-bit number with one bit per square, for example "all black discs". | [02](02-board-and-squares.md) |
| Book value | The value stored with a move in the opening book, from the point of view of the player who makes the move. | 11 |
| Bound | A search result that is only a limit: a lower bound (the true score is at least this) or an upper bound (at most this). | [08](08-transposition-table.md#data-structures) |
| C-square | A square on the edge next to a corner: b1, a2, g1, h2, a7, b8, h7, g8. | [05](05-evaluation.md#the-four-edges) |
| Cutoff | Stopping the search of a position because one move already reaches beta. | [07](07-midgame-search.md#negamax-with-alpha-beta) |
| Dangerous corner move | A corner move that the edge tables mark as risky; the search looks one ply deeper instead of evaluating it. | [05](05-evaluation.md#dangerous-corner-moves) |
| Dropout expansion | Book learning step: the best move that is not yet in the book is searched and added to it. | 12 |
| Edge index | The eight squares of an edge read as a base-3 number (0 to 6560); it selects the entry in the edge tables. | [05](05-evaluation.md#the-edge-index) |
| Edge tables | Eight precomputed tables with one value per possible edge: its worth and the best edge moves. | [05](05-evaluation.md#the-edge-tables) |
| Empties | The number of empty squares; the endgame code uses it instead of a depth. | [09](09-endgame-solver.md#which-routine-at-how-many-empty-squares) |
| Enhanced transposition cutoff | Before searching the moves of a position, looking up the positions after each move in the hash table to find a cutoff at once. | [09](09-endgame-solver.md#solve-7-or-more-empty-squares) |
| Evaluation | A heuristic score of a position for the side to move, used at the leaves of the midgame search. | [05](05-evaluation.md) |
| Exact score | A search result that is the true value of the position, not just a bound. | [08](08-transposition-table.md#data-structures) |
| Extension | Searching a move deeper than the normal depth, for example when there is only one legal reply. | [07](07-midgame-search.md#one-node) |
| Fail high / fail low | A search result at or above beta / at or below alpha. | [07](07-midgame-search.md#negamax-with-alpha-beta) |
| Fail-soft | A search that may return a score outside the (alpha, beta) window, which gives a tighter bound. | [07](07-midgame-search.md#negamax-with-alpha-beta) |
| Fastest-first | Endgame move ordering: try first the moves that leave the opponent the fewest replies. | [09](09-endgame-solver.md#solve-7-or-more-empty-squares) |
| FFO test suite | A standard set of endgame test positions (#40–#59) from the French Othello Federation. | [09](09-endgame-solver.md#worked-example-the-ffo-positions) |
| Hash move | The best move stored in the hash table for a position; it is searched first. | [06](06-move-ordering.md#ordering-the-moves-of-one-position) |
| Hash table | See transposition table. | [08](08-transposition-table.md) |
| Iterative deepening | Searching to depth 1, then 2, then 3, …, until the time is used; each search orders the next. | [07](07-midgame-search.md#iterative-deepening) |
| Legacy square number | The C++ square number 10 × row + column, both counted from 1 (a1 = 11, h8 = 88). Used by the book file and the edge tables. | [02](02-board-and-squares.md#legacy-square-numbers) |
| `look` | The remaining search depth after the current move (the C++ convention). A search with `look` = n looks n + 1 plies ahead. | [07](07-midgame-search.md#data-structures) |
| Mobility | The number of legal moves a player has. | [05](05-evaluation.md#mobility-m-and-potential-mobility-p) |
| Negamax | A way to write minimax where the score is always from the point of view of the side to move; a child's score is negated. | [07](07-midgame-search.md#negamax-with-alpha-beta) |
| Normalisation | Turning a position into the form stored in the book, where black's first move is d3, by one of four symmetries. | 11 |
| Null window | A window with beta = alpha + 1. The search only tells whether the score is above or below alpha, which is fast. | [07](07-midgame-search.md#the-root-principal-variation-search) |
| Parity | Endgame move ordering: prefer the regions of the board with an odd number of empty squares. | [09](09-endgame-solver.md#solveshallow-2-to-6-empty-squares) |
| Pass | A turn without a move, when the player has no legal move. It is stored in the game record like a move. | [03](03-rules-and-move-generation.md#pass-and-game-over) |
| Perft | Counting all positions reachable in n plies; used to test move generation. | [03](03-rules-and-move-generation.md#worked-example-perft) |
| Ply | One move by one side. | [07](07-midgame-search.md#data-structures) |
| Potential mobility | The number of (empty square, direction) pairs next to an opponent disc: places where moves may appear later. | [05](05-evaluation.md#mobility-m-and-potential-mobility-p) |
| Principal variation | The line of best moves for both sides found by the search. | [07](07-midgame-search.md#the-root-principal-variation-search) |
| Principal variation search (PVS) | Searching the first move with the full window and the others with a null window, re-searching only if one turns out better. | [07](07-midgame-search.md#the-root-principal-variation-search) |
| Response table | Move ordering data: for each opponent move, a score for each reply that worked well against it before. | [06](06-move-ordering.md#the-response-table) |
| Selective search | Stello's ProbCut-like pruning: at some depths a shallow search decides whether a position is clearly outside the window and can be cut. | [07](07-midgame-search.md#selective-search) |
| Stable disc | A disc that can never be flipped again. | [05](05-evaluation.md#corner-stability-t) |
| Transposition | The same position reached by different move orders. | [08](08-transposition-table.md#a-transposition) |
| Transposition table | A table that remembers search results for positions, so a transposition is not searched twice. | [08](08-transposition-table.md) |
| Win/loss/draw pass | The first endgame pass, which only finds out whether the side to move wins, loses or draws; it is much faster than the exact score. | [09](09-endgame-solver.md#two-passes) |
| Window | The pair (alpha, beta); only scores between them need to be exact. | [07](07-midgame-search.md#negamax-with-alpha-beta) |
| X-square | The square diagonally next to a corner: b2, g2, b7, g7. | [05](05-evaluation.md#the-four-edges) |

---

Previous: 13 App integration (to be written) · Next: [15 References](15-references.md)
