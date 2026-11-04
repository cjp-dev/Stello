# 27 – Glossary

[Back to the index](README.md)

The terms used in these documents. The last column gives the chapter that explains the term in detail. A few terms of Stello 1 that Part 2 uses are marked "Stello 1" and link to the [Stello 1 documentation](stello-1/README.md), which has its own [glossary](stello-1/16-glossary.md).

| Term | Meaning | Chapter |
|---|---|---|
| Alpha | The score the side to move is already sure to get; worse moves need not be searched exactly. | [06](06-midgame-search.md#negamax-with-alpha-beta) |
| Beta | The score the opponent will allow at most; a move that reaches beta is good enough to stop (a cutoff). | [06](06-midgame-search.md#negamax-with-alpha-beta) |
| Bitboard | A 64-bit number with one bit per square, for example "all black discs". | [02](02-board-and-squares.md) |
| BMI2, `pext` | An x86-64 instruction set extension; its `pext` gathers the bits of a mask into the low bits of a number. The evaluation reads a feature's squares with one `pext` per side when the processor has it; the portable path does the same with shifts and multiplies. | [04](04-evaluation.md#reading-the-features-in-the-engine) |
| Book effort | `BookEffort`: how hard a book value was searched for: the search limit (time, depth or solve), the depth the search reached and the engine version. | [10](10-opening-book.md#bookentry) |
| Book origin | `BookOrigin`: where a book value comes from: `Unknown`, `Heuristic` (a midgame search), `WinLossDraw`, `Exact` or `BackedUp` (minimax from the position after the move). | [10](10-opening-book.md#bookentry), [11](11-book-learning.md#values-origin-and-effort) |
| Book tracker | `BookTracker`: stops asking the book after three misses in a row, until it is reset (new game, Open, Back, after learning). | [10](10-opening-book.md#when-to-ask-the-book-booktracker) |
| Book value | The value stored with a move in the opening book, from the point of view of the player who makes the move: a search score in 1/100 disc, or a solved or game result. | [10](10-opening-book.md#bookentry), [11](11-book-learning.md#values-origin-and-effort) |
| Bound | A search result that is only a limit: a lower bound (the true score is at least this) or an upper bound (at most this). | [07](07-transposition-table.md#data-structures) |
| C ABI | The calling convention of C functions, which almost every language can call. The native library `stello_engine` exports the Rust engine through it, and `Stello.Engine` calls it with `[LibraryImport]`. | [14](14-app-integration.md#how-each-app-gets-the-engine) |
| C-square | A square on the edge next to a corner: b1, a2, g1, h2, a7, b8, h7, g8. | [05](05-move-ordering.md#data-structures) |
| Calculated | The C++ book flag of a move whose value comes from a search; now a book move with a search origin (`IsSearched`), which book learning does not search again. | [11](11-book-learning.md#values-origin-and-effort) |
| Canonical form | The form in which a position is stored in the book: the smallest of its four mirror images that keep the start position. Turning a position into it is **normalisation**. | [10](10-opening-book.md#canonical-form) |
| Cargo, crate | Rust's build tool and package manager, and a Rust package. `Stello.Rust` is a Cargo workspace of four crates; the .NET builds run Cargo. | [01](01-overview.md#the-projects), [14](14-app-integration.md#how-each-app-gets-the-engine) |
| Clog search | Egaroucid's 5-ply full-width search, before a selective search, for moves that end the game soon. | [18](18-the-egaroucid-port.md#how-egaroucid-finds-a-move) |
| Competitive integrity | The unranked status of a game; taking back moves, switching sides, or modifying settings mid-match turns STATUS crimson red (#E74C3C). | [15](15-gui-and-settings.md#1-status-header--competitive-integrity) |
| Cross-origin isolation | A page served with the COOP and COEP headers (`staticwebapp.config.json`), so that it may use `SharedArrayBuffer`. The web app needs it for the stop signal and for the threaded engine; `self.crossOriginIsolated` tells. | [14](14-app-integration.md#threads-in-the-browser) |
| Cutoff | Stopping the search of a position because one move already reaches beta. | [06](06-midgame-search.md#negamax-with-alpha-beta) |
| Dangerous corner move | Stello 1: a corner move that the edge tables mark as risky; the search looks one ply deeper instead of evaluating it. | [Stello 1: 04](stello-1/04-evaluation.md#dangerous-corner-moves) |
| Dropout expansion | Book learning step: the best move that is not yet in the book is searched and added to it. | [11](11-book-learning.md#evaluating-the-book-evaluatepositions) |
| Dumb7Fill | Broadword parallel bitboard fill technique (Delorme, Edax) used for directional ray casting in move generation. | [03](03-rules-and-move-generation.md#algorithm) |
| Edge tables | Stello 1: eight precomputed tables with one value per possible edge, its worth and the best edge moves; the core of Stello 1's evaluation. | [Stello 1: 04](stello-1/04-evaluation.md#the-edge-tables) |
| Egaroucid | A very strong open-source Othello program by Takuto Yamana. Its engine 7.9 is ported to C# in `Egaroucid.Engine`, used only to measure Stello against. | [18](18-the-egaroucid-port.md) |
| Empties | The number of empty squares; the endgame code uses it instead of a depth. | [08](08-endgame-solver.md#the-layers) |
| Endgame layers | The chain of solver routines, one for each range of empty squares: `end_deep` (14 or more), `end_local` (11–13), `end_simple` (7–10), `end_fast` (5–6) and `last4` … `last1`; `end_window` and `solve_shallow` for the principal variation. | [08](08-endgame-solver.md#the-layers) |
| Endgame solver | The search that plays every line to the end of the game and finds the true result, first win/loss/draw, then the exact score. It takes over when the next midgame iteration would end with 7 or fewer empty squares. | [08](08-endgame-solver.md) |
| Engine host | `IEngineHost`: runs the searches and owns the book for the view model; in-process in the desktop app; in the browser it plays the book moves on the page and runs the searches in a Web Worker. | [14](14-app-integration.md#services) |
| Engine version | `OpeningBook.EngineVersion`, stored with every searched book value. It is raised when the evaluation or the search changes the values; Stello 2.0's is 4. | [10](10-opening-book.md#bookentry), [12](12-book-tool.md#recalculating-the-leaves-recalc) |
| Enhanced transposition cutoff | Before searching the moves of a position, looking up the positions after each move in the hash table to find a cutoff at once. Used in the midgame from `look` = 4 and in the solver from 12 empty squares. | [06](06-midgame-search.md#one-node), [07](07-transposition-table.md#use-in-the-midgame-search), [08](08-endgame-solver.md#above-13-empty-squares) |
| ETC | Short for enhanced transposition cutoff. | [08](08-endgame-solver.md#above-13-empty-squares) |
| Evaluation | A heuristic score of a position for the side to move, used at the leaves of the midgame search. Stello's is a trained pattern evaluation that estimates the final disc difference. | [04](04-evaluation.md) |
| Evaluation file | `stello-eval.bin`: the pattern set, the stages and the weights of the evaluation, built into the Rust engine (0.99 MB). | [04](04-evaluation.md#the-file) |
| Exact score | A search result that is the true value of the position, not just a bound. | [07](07-transposition-table.md#data-structures) |
| Extension | Searching a move deeper than the normal depth, for example when there is only one legal reply. | [06](06-midgame-search.md#one-node) |
| Fail high / fail low | A search result at or above beta / at or below alpha. | [06](06-midgame-search.md#negamax-with-alpha-beta) |
| Fail-soft | A search that may return a score outside the (alpha, beta) window, which gives a tighter bound. | [06](06-midgame-search.md#negamax-with-alpha-beta), [08](08-endgame-solver.md#data-structures) |
| Fastest-first | Endgame move ordering: try first the moves that leave the opponent the fewest replies. | [08](08-endgame-solver.md#7-to-13-empty-squares) |
| Feature | One pattern in one of its places on the board. Its weight comes from the pattern's table. The evaluation has 62. | [04](04-evaluation.md#places-and-symmetries) |
| FFI | Foreign function interface: calling code written in another language, here C# calling Rust through the C ABI. | [14](14-app-integration.md#how-each-app-gets-the-engine) |
| FFO test suite | A standard set of endgame test positions (#40–#59) with known exact scores, from the French Othello Federation. | [08](08-endgame-solver.md#the-ffo-positions), [17](17-measuring-the-engine.md#the-endgame-benchmark-solve-bench) |
| Folding | Giving two pattern indices one weight when a symmetry maps the pattern onto itself, such as a row read backwards. | [04](04-evaluation.md#folding) |
| Game loop | The app's `RunAsync`: plays passes and computer moves until the human is to move or the game is over. | [14](14-app-integration.md#the-game-loop) |
| Generation | A 16-bit counter in the hash table; clearing the table only raises it, and an entry of an older generation counts as empty. | [07](07-transposition-table.md#clearing-by-generation) |
| Half-turn trick | Egaroucid numbers the squares the other way round (a1 = bit 63). Reversing the bits turns the board half a turn, under which Othello and all the solver's tables are the same, so Egaroucid's endgame code works on Stello's bitboards unchanged. | [08](08-endgame-solver.md#egaroucids-tables-on-stellos-bitboards) |
| Hard limit | The time after which the search is stopped in the middle and the best move so far is played. | [09](09-time-control.md#how-the-search-uses-the-limits) |
| Hash move | The best move stored in the hash table for a position; it is searched first. | [05](05-move-ordering.md#ordering-the-moves-of-one-position) |
| Hash table | See transposition table. | [07](07-transposition-table.md) |
| Index | The filling of a pattern's squares (empty, own, opponent's) read as a number in base 3; it selects the pattern's weight. | [04](04-evaluation.md#patterns-and-indices) |
| Iterative deepening | Searching to depth 1, then 2, then 3, …, until the time is used; each search orders the next. | [06](06-midgame-search.md#iterative-deepening) |
| Lazy SMP | A parallel search in which helper threads search the same root and help only through the shared hash table. Stello's helpers run their own iterative deepening 1 + ctz(i + 1) plies ahead of the main thread; with YBWC on, half the other threads are helpers. | [06](06-midgame-search.md#lazy-smp), [26](26-phases-5-9-threads.md#phase-6-lazy-smp) |
| Legacy square number | The C++ square number 10 × row + column, both counted from 1 (a1 = 11, h8 = 88). Used only by the C++ opening book file, which is read to import it. | [02](02-board-and-squares.md#legacy-square-numbers) |
| Local table | The solver's small table of bounds for 6 to 12 empty squares, 1,024 slots per number of empty squares, from Egaroucid. Each search thread has its own. | [08](08-endgame-solver.md#data-structures) |
| Lone empty square | An empty square with no empty neighbour; the last layers of the solver try these first. | [08](08-endgame-solver.md#5-and-6-empty-squares) |
| `look` | The remaining search depth after the current move (the C++ convention). A search with `look` = n looks n + 1 plies ahead. | [06](06-midgame-search.md#data-structures) |
| Master book | The opening book shipped with the apps: the text file `Stello.Net/Book/opening-book.txt`, built to `opening-book.bin`. Its positions were learned by the C++ program; its values were recalculated by Stello 2.0. | [10](10-opening-book.md#the-text-file-the-master-book), [12](12-book-tool.md#recalculating-the-master-book) |
| Match side | One of the two engine settings of a benchmark `match`, written as keys such as `engine=egaroucid,time-ms=100,book=on`. | [17](17-measuring-the-engine.md#sides) |
| Mobility | The number of legal moves a player has. The solver orders its moves by the opponent's mobility after them, with corners counted twice. | [08](08-endgame-solver.md#move-ordering-above-13-empty-squares) |
| Move Now | The command that stops the search at once and plays the best move found so far. | [09](09-time-control.md#move-now) |
| Multi-ProbCut | Stello's selective search (Buro): a shallow search predicts the deep one, and the deep search is skipped when the prediction is outside the window by more than t times the expected error. The error σ is a model fitted on Stello's own searches, by the number of discs and both depths. | [06](06-midgame-search.md#selective-search) |
| Negamax | A way to write minimax where the score is always from the point of view of the side to move; a child's score is negated. | [06](06-midgame-search.md#negamax-with-alpha-beta) |
| Node-for-node parity | Two engines that visit exactly the same positions, so they give the same moves, scores and node counts. The Rust engine has it with Stello 2.0's C# engine; it is checked with `--compare` and recorded signatures. (Not to be confused with **parity**, the endgame ordering.) | [25](25-phases-0-4-the-engine-in-rust.md#how-parity-was-checked) |
| Normalisation | See canonical form. | [10](10-opening-book.md#canonical-form) |
| Null window | A window with beta = alpha + 1. The search only tells whether the score is above or below alpha, which is fast. The endgame solver is built from null-window searches. | [06](06-midgame-search.md#the-root-principal-variation-search), [08](08-endgame-solver.md#the-layers) |
| Parity | Endgame move ordering: prefer the quadrants of the board with an odd number of empty squares. | [08](08-endgame-solver.md#7-to-13-empty-squares) |
| Pass | A turn without a move, when the player has no legal move. It is stored in the game record like a move. | [03](03-rules-and-move-generation.md#pass-and-game-over) |
| Pattern | A fixed list of squares whose filling (empty, own, opponent's) selects one trained weight; the evaluation sums the weights of all patterns in all their places. | [04](04-evaluation.md#patterns-and-indices) |
| Pattern set | The patterns an evaluation uses. Stello's is `plus15`: 11 classic patterns and 4 of Egaroucid's, 62 features. | [04](04-evaluation.md#the-patterns) |
| Perft | Counting all positions reachable in n plies; used to test move generation. | [03](03-rules-and-move-generation.md#worked-example-perft) |
| Ply | One move by one side. | [06](06-midgame-search.md#data-structures) |
| Portable path | The engine's code without processor-specific instructions such as `pext`; WebAssembly runs it, and `--cpu portable` forces it natively. It gives the same results as the fast path. | [04](04-evaluation.md#reading-the-features-in-the-engine) |
| Potential mobility | The empty squares next to a player's discs: places where the opponent may get moves later. The solver uses it to order moves above 13 empty squares. | [08](08-endgame-solver.md#move-ordering-above-13-empty-squares) |
| Principal variation | The line of best moves for both sides found by the search. | [06](06-midgame-search.md#the-root-principal-variation-search) |
| Principal variation search (PVS) | Searching the first move with the full window and the others with a null window, re-searching only if one turns out better. Stello does it in every node. | [06](06-midgame-search.md#the-root-principal-variation-search), [06](06-midgame-search.md#one-node) |
| Procedural sound | Synthesized 16-bit PCM WAV audio feedback generated at runtime by `SoundWaves` without external audio files. | [15](15-gui-and-settings.md#procedural-sound-system) |
| Response table | Move ordering data: for each opponent move, a score for each reply that worked well against it before. | [05](05-move-ordering.md#the-response-table) |
| Rust | A compiled systems language (LLVM, like Clang), in which Stello 3.0's search is written. | [25](25-phases-0-4-the-engine-in-rust.md#why-rust) |
| rustup | The installer and toolchain manager of Rust; it also adds targets such as `wasm32-unknown-unknown`. | [14](14-app-integration.md#how-each-app-gets-the-engine) |
| Score scale | The unit of the midgame search: 1/100 disc, with evaluations capped at ±64 discs. A finished game scores ±(32,600 + discs). | [04](04-evaluation.md#from-weights-to-search-scores) |
| Selective search | Pruning where a shallow search decides whether a position is clearly outside the window and can be cut. Stello's is Multi-ProbCut. | [06](06-midgame-search.md#selective-search) |
| Seqlock | A sequence lock: a writer makes a counter odd while it writes and even (and larger) when it is done; a reader that sees the counter change takes what it read as a miss. Each slot of Stello's hash tables has one, so a torn entry is never taken for a valid one. | [07](07-transposition-table.md#sharing-the-table-between-threads) |
| Self-play | Book learning where the engine plays games against itself with the book and adds them to it. | [11](11-book-learning.md#self-play-playgame-and-selfplay) |
| SharedArrayBuffer | Memory that a page and its Web Workers share. The web app writes the stop signal into 4 bytes of it, and the engine reads it with `Atomics.load`. The threaded WebAssembly engine runs all its threads on one shared memory. Only available when the page is cross-origin isolated. | [14](14-app-integration.md#the-engine-in-the-browser), [14](14-app-integration.md#threads-in-the-browser) |
| Signature | A recorded search: position, limit, move, score, kind, depth and nodes. The 105 signatures of Stello 2.0 check both the frozen C# engine and the Rust engine. | [25](25-phases-0-4-the-engine-in-rust.md#phase-0-the-set-up-and-the-frozen-stello-20) |
| Soft limit | The time after which no new iteration of the search is started: two thirds of the hard limit. | [09](09-time-control.md#how-the-search-uses-the-limits) |
| Spawner | The function the thread pool starts its workers with: `std::thread` by default, and in the threaded WebAssembly engine one that hands the worker's body to a waiting Web Worker. | [06](06-midgame-search.md#the-thread-pool), [14](14-app-integration.md#threads-in-the-browser) |
| Split point | A node whose younger brothers YBWC searches on other threads. It has a stop and a cut flag, and points to the split point above it, so a stop reaches every search below. | [06](06-midgame-search.md#ybwc) |
| Stable disc | A disc that can never be flipped again. If the opponent's stable discs already decide the window, the solver stops at once (the stable-disc cutoff). | [08](08-endgame-solver.md#the-stable-disc-cutoff) |
| Stage | A range of disc counts with its own pattern weights; the evaluation has 10. | [04](04-evaluation.md#stages) |
| Stello 1, Stello 2.0, Stello 3.0 | Stello 1 is the C# port of the C++ program, the engine before 2.0, frozen in `Stello.Engine.V1` as the baseline. Stello 2.0 is the C# engine of the first 2.0 release (1 November 2026) and the final 2.0 (2 November 2026), frozen in `Stello.Engine.V2`. Stello 3.0 is the engine of the apps: the same search in Rust (`Stello.Rust`), behind `Stello.Engine`'s `SearchEngine`, on one thread or more (Lazy SMP and YBWC). | [01](01-overview.md), [16](16-the-goal-and-the-plan.md), [25](25-phases-0-4-the-engine-in-rust.md), [26](26-phases-5-9-threads.md) |
| Thread pool | `ThreadPool` in `search/pool.rs`: a fixed set of worker threads, started at the first search with more than 1 thread and kept. Each worker has its own state; tasks run in a scope that waits for them; `try_spawn` refuses at once when no worker is idle. | [06](06-midgame-search.md#the-thread-pool) |
| Threads | The threads of one search (`SearchLimits.Threads`, 1–256), and the setting of that name in both apps. Automatic (stored as 0) is the cores − 1 on the desktop and `navigator.hardwareConcurrency` in the browser. | [15](15-gui-and-settings.md#3-threads), [14](14-app-integration.md#search-threads) |
| Training data | Egaroucid's self-play and match games with their final scores, on which the evaluation was trained. Its terms forbid redistribution, so it is not in the repository. | [20](20-phases-0-2-the-evaluation.md) |
| Transposition | The same position reached by different move orders. | [07](07-transposition-table.md#a-transposition) |
| Transposition table | A table that remembers search results for positions, so a transposition is not searched twice. | [07](07-transposition-table.md) |
| Transposition table capacity | The configurable size of the midgame and endgame tables together: 65,536 to 16,777,216 entries per table, 4 MB to 1,024 MB (2^bits slots × 64 bytes × 2 tables). | [07](07-transposition-table.md#the-sizes-in-the-settings) |
| wasm-bindgen | The Rust tool and crate that let WebAssembly and JavaScript call each other; it writes the JavaScript glue of `stello-wasm`. Its command-line version must equal the crate's (0.2.130). | [14](14-app-integration.md#how-each-app-gets-the-engine) |
| Web Worker | A browser thread without a page; the web app runs the Rust engine, compiled to WebAssembly, in one so the page stays responsive. | [14](14-app-integration.md#the-engine-in-the-browser) |
| WebAssembly (WASM) | A portable binary format that browsers run at near-native speed. The web app's .NET code is compiled to it ahead of time, and the Rust engine is compiled to it for the worker. | [14](14-app-integration.md#the-engine-in-the-browser) |
| Weight | The trained value, in 1/8 disc, of one filling of a pattern in one stage. | [04](04-evaluation.md#patterns-and-indices) |
| Win/loss/draw pass | The first endgame pass, which only finds out whether the side to move wins, loses or draws; it is much faster than the exact score. | [08](08-endgame-solver.md#two-passes) |
| Window | The pair (alpha, beta); only scores between them need to be exact. | [06](06-midgame-search.md#negamax-with-alpha-beta) |
| Wrapper | `Stello.Engine.SearchEngine` since 3.0: a C# class with Stello 2.0's API that calls the Rust engine and owns it through a `SafeHandle`. | [01](01-overview.md#the-engine-at-a-glance), [25](25-phases-0-4-the-engine-in-rust.md#the-wrapper) |
| YBWC | Young Brothers Wait Concept (Feldmann and others): after the first child of a node has been searched, its younger brothers may be searched in parallel. Stello splits midgame nodes whose children are searched 6 or more plies deep, and endgame nodes whose children have 16 or more empty squares. | [06](06-midgame-search.md#ybwc), [08](08-endgame-solver.md#threads-in-the-solver) |
| X-square | The square diagonally next to a corner: b2, g2, b7, g7. | [05](05-move-ordering.md#data-structures) |

---

Previous: [26 Phases 5–9: threads](26-phases-5-9-threads.md) · Next: [28 References](28-references.md)
