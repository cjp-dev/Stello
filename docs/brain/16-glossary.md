# 16 – Glossary

[Back to the index](README.md)

The terms used in these documents. The last column gives the chapter that explains the term in detail.

| Term | Meaning | Chapter |
|---|---|---|
| Alpha | The score the side to move is already sure to get; worse moves need not be searched exactly. | [06](06-midgame-search.md#negamax-with-alpha-beta) |
| Beta | The score the opponent will allow at most; a move that reaches beta is good enough to stop (a cutoff). | [06](06-midgame-search.md#negamax-with-alpha-beta) |
| Bitboard | A 64-bit number with one bit per square, for example "all black discs". | [02](02-board-and-squares.md) |
| Book tracker | `BookTracker`: stops asking the book after three misses in a row, until it is reset (new game, Open, Back, after learning). | [10](10-opening-book.md#when-to-ask-the-book-booktracker) |
| Book value | The value stored with a move in the opening book, from the point of view of the player who makes the move. | [10](10-opening-book.md#bookentry) |
| Bound | A search result that is only a limit: a lower bound (the true score is at least this) or an upper bound (at most this). | [07](07-transposition-table.md#data-structures) |
| C-square | A square on the edge next to a corner: b1, a2, g1, h2, a7, b8, h7, g8. | [04](04-evaluation.md#the-four-edges) |
| Calculated | The C++ book flag of a move whose value comes from a search; now a book move with a search origin (`IsSearched`), which book learning does not search again. | [11](11-book-learning.md#values-origin-and-effort) |
| Competitive integrity | The unranked status of a game; taking back moves, switching sides, or modifying settings mid-match turns STATUS crimson red (#E74C3C). | [15](15-gui-and-settings.md#1-status-header--competitive-integrity) |
| Cutoff | Stopping the search of a position because one move already reaches beta. | [06](06-midgame-search.md#negamax-with-alpha-beta) |
| Dangerous corner move | Stello 1: a corner move that the edge tables mark as risky; the search looks one ply deeper instead of evaluating it. Stello 2 dropped it. | [04](04-evaluation.md#dangerous-corner-moves) |
| Dropout expansion | Book learning step: the best move that is not yet in the book is searched and added to it. | [11](11-book-learning.md#evaluating-the-book-evaluatepositions) |
| Dumb7Fill | Broadword parallel bitboard fill technique (Delorme, Edax) used for directional ray casting in move generation. | [03](03-rules-and-move-generation.md#algorithm) |
| Edge index | The eight squares of an edge read as a base-3 number (0 to 6560); it selects the entry in the edge tables. | [04](04-evaluation.md#the-edge-index) |
| Edge tables | Stello 1's eight precomputed tables with one value per possible edge: its worth and the best edge moves. | [04](04-evaluation.md#the-edge-tables) |
| Empties | The number of empty squares; the endgame code uses it instead of a depth. | [08](08-endgame-solver.md#which-routine-at-how-many-empty-squares) |
| Engine host | `IEngineHost`: runs the searches and owns the book for the view model; in-process in the desktop app, in a Web Worker in the browser. | [14](14-app-integration.md#services) |
| Enhanced transposition cutoff | Before searching the moves of a position, looking up the positions after each move in the hash table to find a cutoff at once. | [08](08-endgame-solver.md#solve-7-or-more-empty-squares), [21](21-stello-2-evaluation-and-solver.md#above-13-empty-squares-new-shaped-like-egaroucids) |
| ETC | Short for enhanced transposition cutoff. | [21](21-stello-2-evaluation-and-solver.md#above-13-empty-squares-new-shaped-like-egaroucids) |
| Evaluation | A heuristic score of a position for the side to move, used at the leaves of the midgame search. Stello 2 uses a trained pattern evaluation, in 1/100 disc. | [21](21-stello-2-evaluation-and-solver.md#the-pattern-evaluation), [04](04-evaluation.md) (Stello 1) |
| Exact score | A search result that is the true value of the position, not just a bound. | [07](07-transposition-table.md#data-structures) |
| Extension | Searching a move deeper than the normal depth, for example when there is only one legal reply. | [06](06-midgame-search.md#one-node) |
| Fail high / fail low | A search result at or above beta / at or below alpha. | [06](06-midgame-search.md#negamax-with-alpha-beta) |
| Fail-soft | A search that may return a score outside the (alpha, beta) window, which gives a tighter bound. | [06](06-midgame-search.md#negamax-with-alpha-beta), [21](21-stello-2-evaluation-and-solver.md#the-layers) |
| Fastest-first | Endgame move ordering: try first the moves that leave the opponent the fewest replies. | [08](08-endgame-solver.md#solve-7-or-more-empty-squares) |
| Feature | One pattern in one of its places on the board. Stello 2's evaluation has 62. | [21](21-stello-2-evaluation-and-solver.md#what-a-pattern-is) |
| FFO test suite | A standard set of endgame test positions (#40–#59) from the French Othello Federation. | [08](08-endgame-solver.md#worked-example-the-ffo-positions) |
| Folding | Giving two pattern indices one weight when a symmetry maps the pattern onto itself, such as a row read backwards. | [21](21-stello-2-evaluation-and-solver.md#places-symmetries-and-folding) |
| Frontier disc | A disc adjacent to at least one empty square; potential mobility measures empty squares bordering opponent frontier discs. | [04](04-evaluation.md#mobility-m-and-potential-mobility-p) |
| Game loop | The app's `RunAsync`: plays passes and computer moves until the human is to move or the game is over. | [14](14-app-integration.md#the-game-loop) |
| Hard limit | The time after which the search is stopped in the middle and the best move so far is played. | [09](09-time-control.md#how-the-search-uses-the-limits) |
| Hash move | The best move stored in the hash table for a position; it is searched first. | [05](05-move-ordering.md#ordering-the-moves-of-one-position) |
| Hash table | See transposition table. | [07](07-transposition-table.md) |
| Iterative deepening | Searching to depth 1, then 2, then 3, …, until the time is used; each search orders the next. | [06](06-midgame-search.md#iterative-deepening) |
| Legacy square number | The C++ square number 10 × row + column, both counted from 1 (a1 = 11, h8 = 88). Used by the book file and the edge tables. | [02](02-board-and-squares.md#legacy-square-numbers) |
| `look` | The remaining search depth after the current move (the C++ convention). A search with `look` = n looks n + 1 plies ahead. | [06](06-midgame-search.md#data-structures) |
| Master book | The opening book shipped with the apps: the text file `Stello.Net/Book/opening-book.txt`, built to `opening-book.bin` (`Data/OPENING` in the WPF app). It was learned by the C++ program. | [10](10-opening-book.md#the-text-file-the-master-book) |
| Mobility | The number of legal moves a player has. | [04](04-evaluation.md#mobility-m-and-potential-mobility-p) |
| Move Now | The command that stops the search at once and plays the best move found so far. | [09](09-time-control.md#move-now) |
| Negamax | A way to write minimax where the score is always from the point of view of the side to move; a child's score is negated. | [06](06-midgame-search.md#negamax-with-alpha-beta) |
| Normalisation | Turning a position into the form stored in the book, its canonical form: the smallest of its four mirror images that keep the start position. | [10](10-opening-book.md#canonical-form) |
| Null window | A window with beta = alpha + 1. The search only tells whether the score is above or below alpha, which is fast. Stello 2's endgame solver is built from null-window searches. | [06](06-midgame-search.md#the-root-principal-variation-search), [21](21-stello-2-evaluation-and-solver.md#the-layers) |
| Parity | Endgame move ordering: prefer the regions of the board with an odd number of empty squares. | [08](08-endgame-solver.md#solveshallow-3-to-6-empty-squares) |
| Pass | A turn without a move, when the player has no legal move. It is stored in the game record like a move. | [03](03-rules-and-move-generation.md#pass-and-game-over) |
| Pattern | A fixed list of squares whose filling (empty, own, opponent's) selects one trained weight; the evaluation sums the weights of all patterns in all their places. | [21](21-stello-2-evaluation-and-solver.md#what-a-pattern-is) |
| Perft | Counting all positions reachable in n plies; used to test move generation. | [03](03-rules-and-move-generation.md#worked-example-perft) |
| Ply | One move by one side. | [06](06-midgame-search.md#data-structures) |
| Potential mobility | The number of (empty square, direction) pairs next to an opponent disc: places where moves may appear later. | [04](04-evaluation.md#mobility-m-and-potential-mobility-p) |
| Principal variation | The line of best moves for both sides found by the search. | [06](06-midgame-search.md#the-root-principal-variation-search) |
| Principal variation search (PVS) | Searching the first move with the full window and the others with a null window, re-searching only if one turns out better. | [06](06-midgame-search.md#the-root-principal-variation-search) |
| Procedural sound | Synthesized 16-bit PCM WAV audio feedback generated at runtime by `SoundWaves` without external audio files. | [15](15-gui-and-settings.md#procedural-sound-system) |
| Response table | Move ordering data: for each opponent move, a score for each reply that worked well against it before. | [05](05-move-ordering.md#the-response-table) |
| Selective search | Stello's ProbCut-like pruning: at some depths a shallow search decides whether a position is clearly outside the window and can be cut. | [06](06-midgame-search.md#selective-search) |
| Self-play | Book learning where the engine plays games against itself with the book and adds them to it. | [11](11-book-learning.md#self-play-playgame-and-selfplay) |
| Soft limit | The time after which no new iteration of the search is started. | [09](09-time-control.md#how-the-search-uses-the-limits) |
| Stable disc | A disc that can never be flipped again. | [04](04-evaluation.md#corner-stability-t) |
| Stage | A range of disc counts with its own pattern weights; Stello 2's evaluation has 10. | [21](21-stello-2-evaluation-and-solver.md#stages) |
| Stello 1, Stello 2 | Stello 1 is the engine before version 2.0, frozen in `Stello.Engine.V1` as the baseline; Stello 2 is the engine of the apps, in `Stello.Engine`. | [21](21-stello-2-evaluation-and-solver.md#the-engines) |
| Transposition | The same position reached by different move orders. | [07](07-transposition-table.md#a-transposition) |
| Transposition table | A table that remembers search results for positions, so a transposition is not searched twice. | [07](07-transposition-table.md) |
| Transposition table capacity | The configurable memory size across midgame and endgame tables (65K to 16.7M entries, 3 MB to 768 MB). | [07](07-transposition-table.md#configurable-capacity-options) |
| Web Worker | A browser thread without a page; the web app runs the engine in one so the page stays responsive. | [14](14-app-integration.md#the-engine-in-the-browser) |
| Win/loss/draw pass | The first endgame pass, which only finds out whether the side to move wins, loses or draws; it is much faster than the exact score. | [08](08-endgame-solver.md#two-passes) |
| Window | The pair (alpha, beta); only scores between them need to be exact. | [06](06-midgame-search.md#negamax-with-alpha-beta) |
| X-square | The square diagonally next to a corner: b2, g2, b7, g7. | [04](04-evaluation.md#the-four-edges) |

---

Previous: [15 GUI and settings](15-gui-and-settings.md) · Next: [17 References](17-references.md)
