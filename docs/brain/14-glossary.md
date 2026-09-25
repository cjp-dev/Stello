# 14 – Glossary

[Back to the index](README.md)

The terms used in these documents. The last column gives the chapter that explains the term in detail.

<!-- Chapter numbers become links when the chapters are written. -->

| Term | Meaning | Chapter |
|---|---|---|
| Alpha | The score the side to move is already sure to get; worse moves need not be searched exactly. | 07 |
| Beta | The score the opponent will allow at most; a move that reaches beta is good enough to stop (a cutoff). | 07 |
| Bitboard | A 64-bit number with one bit per square, for example "all black discs". | 02 |
| Book value | The value stored with a move in the opening book, from the point of view of the player who makes the move. | 11 |
| Bound | A search result that is only a limit: a lower bound (the true score is at least this) or an upper bound (at most this). | 08 |
| C-square | A square on the edge next to a corner: b1, a2, g1, h2, a7, b8, h7, g8. | 05 |
| Cutoff | Stopping the search of a position because one move already reaches beta. | 07 |
| Dangerous corner move | A corner move that the edge tables mark as risky; the search looks one ply deeper instead of evaluating it. | 05 |
| Dropout expansion | Book learning step: the best move that is not yet in the book is searched and added to it. | 12 |
| Edge index | The eight squares of an edge read as a base-3 number (0 to 6560); it selects the entry in the edge tables. | 05 |
| Edge tables | Eight precomputed tables with one value per possible edge: its worth and the best edge moves. | 05 |
| Empties | The number of empty squares; the endgame code uses it instead of a depth. | 09 |
| Enhanced transposition cutoff | Before searching the moves of a position, looking up the positions after each move in the hash table to find a cutoff at once. | 09 |
| Evaluation | A heuristic score of a position for the side to move, used at the leaves of the midgame search. | 05 |
| Exact score | A search result that is the true value of the position, not just a bound. | 08 |
| Extension | Searching a move deeper than the normal depth, for example when there is only one legal reply. | 07 |
| Fail high / fail low | A search result at or above beta / at or below alpha. | 07 |
| Fail-soft | A search that may return a score outside the (alpha, beta) window, which gives a tighter bound. | 07 |
| Fastest-first | Endgame move ordering: try first the moves that leave the opponent the fewest replies. | 09 |
| FFO test suite | A standard set of endgame test positions (#40–#59) from the French Othello Federation. | 09 |
| Hash move | The best move stored in the hash table for a position; it is searched first. | 06 |
| Hash table | See transposition table. | 08 |
| Iterative deepening | Searching to depth 1, then 2, then 3, …, until the time is used; each search orders the next. | 07 |
| Legacy square number | The C++ square number 10 × row + column, both counted from 1 (a1 = 11, h8 = 88). Used by the book file and the edge tables. | 02 |
| `look` | The remaining search depth after the current move (the C++ convention). A search with `look` = n looks n + 1 plies ahead. | 07 |
| Mobility | The number of legal moves a player has. | 05 |
| Negamax | A way to write minimax where the score is always from the point of view of the side to move; a child's score is negated. | 07 |
| Normalisation | Turning a position into the form stored in the book, where black's first move is d3, by one of four symmetries. | 11 |
| Null window | A window with beta = alpha + 1. The search only tells whether the score is above or below alpha, which is fast. | 07 |
| Parity | Endgame move ordering: prefer the regions of the board with an odd number of empty squares. | 09 |
| Pass | A turn without a move, when the player has no legal move. It is stored in the game record like a move. | 03 |
| Perft | Counting all positions reachable in n plies; used to test move generation. | 03 |
| Ply | One move by one side. | 07 |
| Potential mobility | The number of (empty square, direction) pairs next to an opponent disc: places where moves may appear later. | 05 |
| Principal variation | The line of best moves for both sides found by the search. | 07 |
| Principal variation search (PVS) | Searching the first move with the full window and the others with a null window, re-searching only if one turns out better. | 07 |
| Response table | Move ordering data: for each opponent move, a score for each reply that worked well against it before. | 06 |
| Selective search | Stello's ProbCut-like pruning: at some depths a shallow search decides whether a position is clearly outside the window and can be cut. | 07 |
| Stable disc | A disc that can never be flipped again. | 05 |
| Transposition | The same position reached by different move orders. | 08 |
| Transposition table | A table that remembers search results for positions, so a transposition is not searched twice. | 08 |
| Win/loss/draw pass | The first endgame pass, which only finds out whether the side to move wins, loses or draws; it is much faster than the exact score. | 09 |
| Window | The pair (alpha, beta); only scores between them need to be exact. | 07 |
| X-square | The square diagonally next to a corner: b2, g2, b7, g7. | 05 |

---

Previous: 13 App integration (to be written) · Next: [15 References](15-references.md)
