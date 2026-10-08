# Improve the Stello opening book

The Opening book in Stello was developed 20 years ago, and the format and content might be improved.

There are several sections we can look into:

1. Can the format be improved, meaning the way the book is stored and the information for each node.
2. Can we develop a book update tool that runs through every ending leaf and recalculates its value using the Stello engine at a timed interval (default 60 sec)
3. Can we develop a book update tool that implements the  Drop-Out Expansion algorithm to expand the book

Look into the Checkers folder for inspiration and information, files worth mentioning are:

- [Checkers/docs/brain/10-opening-book.md](Checkers/docs/brain/10-opening-book.md) (how the Checkers book is built and used: Multi-PV generation, full-width early plies + DOE, text format, random choice within 10 cp, CLI reference, measured results)
- [Checkers/Opening book.md](Checkers/Opening%20book.md) (the Checkers book spec)
- [Checkers/Opening book Drop-Out Expansion.md](Checkers/Opening%20book%20Drop-Out%20Expansion.md) (DOE background)
- [Checkers/tools/Checkers.Benchmark/BookCommands.cs](Checkers/tools/Checkers.Benchmark/BookCommands.cs) (CLI: `book generate`, `book expand-doe`, `book verify`)
- [Checkers/src/Checkers.Core/AI/Book/BookBuilder.cs](Checkers/src/Checkers.Core/AI/Book/BookBuilder.cs) (`ExpandDropOut`: parallel workers, virtual visits, checkpoints), [BookFile.cs](Checkers/src/Checkers.Core/AI/Book/BookFile.cs) (text format), [OpeningBook.cs](Checkers/src/Checkers.Core/AI/Book/OpeningBook.cs) (random choice within a margin)

The current Stello book is described in [docs/brain/11-opening-book.md](docs/brain/11-opening-book.md) and [docs/brain/12-book-learning.md](docs/brain/12-book-learning.md).

---

## Review

### Scope

- **Now: phase 1 (book format) and phase 2 (recalculate the leaves)**, items 1 and 2 above.
- **Postponed: phase 3 (Drop-Out Expansion, item 3) and phase 4 (varied play from the book).** Their notes are kept under [Postponed](#postponed-phases-3-and-4) at the end. Phases 1 and 2 must not block them: the file format has a version number so later fields can be added.

### Status

- **Phase 1: done (2026-10-07).** Described in [docs/brain chapter 11](docs/brain/11-opening-book.md), [chapter 12](docs/brain/12-book-learning.md), the new [chapter 16](docs/brain/16-book-tool.md) and [Stello porting documentation.md](Stello%20porting%20documentation.md), phase 10. Decisions taken while implementing it are marked *(phase 1)* below.
- **Phase 2: done (2026-10-08).** `recalc`, `compare` and `match` are in the book tool ([chapter 16](docs/brain/16-book-tool.md#the-result-october-2026)) and in [Stello porting documentation.md](Stello%20porting%20documentation.md), phase 11. The master book was recalculated with 60 s per position (8 workers, about a day): 10 531 leaf values searched, exact values from 1 091 to 2 990, 2 163 positions with another first move. In the match from those positions the recalculated book scored 52.8 % (95 % interval 51.6–54.0 %) against the C++ book, so it is shipped.

### Decisions (answers to the review questions)

| Question | Decision |
|---|---|
| Must the C++ program still read the book? | **No.** New format; the legacy `OPENING` reader is kept only to import the old book (and old user books) once. |
| Storage | **Text as the source in git, binary built for the apps** (WPF `Data/`, web `wwwroot/data/`, user books in `%AppData%` and localStorage). |
| Where do the tools run? | **A new console tool**, `Stello.Net/tools/Stello.BookTool`, like `Checkers.Benchmark`. The book algorithms live in `Stello.Engine` so they can be tested. |
| Search limit for the tools | **Both**: default 60 s per position, `--depth N` as an alternative. |
| When to solve exactly | **As the computer does in a game**: the normal `SearchEngine.Search` switches to the endgame solver by itself (finding 4); no separate threshold. |
| Where the master book lives | **`Stello.Net/Book/opening-book.txt`**; `Stello C++/OPENING` stays unchanged as the C++ program's book and the import source. |
| The first recalc run | **All ~10 000 leaves** in one (resumable) run. |
| Checking the recalculated book | **The end-of-run report and a match between the old and the new book** (`match`, section 2). |
| Key of a line in the text book | **The move line** that reaches the position (readable); see [why](#why-the-move-line-is-the-key). |
| Play in phases 1 and 2 | **Unchanged**: the computer plays the first legal reply of the sorted list, as today. Phase 2 changes *which* move is first, not how it is chosen. |
| How the binary book is built *(phase 1)* | **By `Stello.BookTool build`, and committed** next to the text book. Engine tests fail if the binary does not hold the text book or the text is not in normal form. An MSBuild target was not used: it would have to run the tool inside the WPF and web builds and the GitHub workflow. |
| What `NodeCount` means *(phase 1)* | The number of **book moves** (22 878); `PositionCount` is the number of positions (11 200). The About box now says "more than 11,000 opening positions". |
| Imported values that are inconsistent *(phase 1)* | **Kept as imported** (104 moves not backed up, 18 positions not sorted, from positions the C++ tree stored twice), so the computer plays the same moves; `verify` warns, and phase 2 backs up and sorts. |

### Facts about the current book

- 23 389 nodes, 11 978 leaves, 11 214 positions with replies, longest line 57 plies ([chapter 11](docs/brain/11-opening-book.md#the-master-book)).
- 11 972 nodes are `Calculated`; 1 217 of them solved exactly and 3 500 solved win/loss/draw. So about 7 250 leaves have a heuristic value of unknown quality (the C++ learning used 2 minutes per position on a 20-year-old PC; the C# app uses the user's "time per move").
- A node is 6 bytes: move, value, flags. The file has no magic number or version, and the header is a C++ allocation counter, not the node count.

### Findings

1. **No record of how a value was found.** A value can come from an added game (±32 665), a heuristic search, a win/loss/draw solve or an exact solve, and the flags only partly tell which. The search depth or time is not stored, so a tool cannot tell a 1-second value from a 2-minute value. Phase 2 needs this to skip leaves that are already good enough and to resume after a stop.
2. **Tree, not graph.** Transpositions are stored once per move order; the index takes the first one, and minimax needs 10 rounds so that values travel through transpositions. Storing each position once (a DAG keyed by the position) makes back-up exact in one pass, avoids searching the same leaf position twice in phase 2, and is what DOE will work on later.
3. **"60 seconds" is not 60 seconds.** `SearchLimits.TimePerMove(60 s)` has a soft limit of 40 s (no new iteration) and a hard limit of 60 s ([TimeControl.cs](Stello.Net/Stello.Engine/Search/TimeControl.cs)). That is fine, but the tool output and the stored effort should say "time per move 60 s", and the time estimates below use ~50 s.
4. **The search already decides when to solve exactly.** `SearchEngine.Search` deepens the midgame search until the depth reaches the number of empties − 8 (`EndgameDistance` = 7), then solves win/loss/draw and then the exact score, all within the time limit ([chapter 09](docs/brain/09-endgame-solver.md)). If the solver does not finish in time, the result is the last complete one (heuristic or win/loss/draw), and `SearchResult.Kind` says which. So with 60 s the late leaves are solved exactly (FFO #40–44, 20–24 empties, take 19 s together), and the origin is stored from `Kind`. A later run with more time can upgrade a heuristic or win/loss/draw value.
5. **The engine is single-threaded.** The tool must run several `SearchEngine` instances in parallel (one per worker, each with its own hash table), as `BookBuilder` does in Checkers. With timed searches, the worker count changes how deep each search gets (shared L3 cache, SMT), so the default should be the number of physical cores.
6. **Cost of phase 2.** About 10 000 leaves that are not exactly solved × ~50 s ≈ 140 hours on one core, ≈ 18 hours with 8 workers (less, since late leaves are solved in seconds). The tool must be stoppable and resumable, and does the most important leaves (nearest the root) first, so a stopped run has already improved the most-played lines.
7. **Book size matters for the web version.** The user's book is stored base64-encoded in localStorage (about 5 MB per site), and the shipped book is downloaded on start. The binary format must stay about as compact as today's 8 bytes per move.
8. **Data loss in the legacy file.** The first node of every chain has its value 32 600 written as 0 (chapter 11). On import, such values and the ±32 665 game values must be marked as "unknown" so they are recalculated, not trusted.
9. **Evaluate Book and Self-play must move to the new model.** They stay in the apps, so `BookLearner` must work on the new in-memory book and record origin and effort for its searches. Its tests (`BookLearnerTests`) are the safety net.

---

## 1. Book format (phase 1)

### In-memory model

- One entry per **position**, keyed by the canonical position: the smallest of the 4 start-preserving symmetries (identity, both diagonals, half turn) of `(Black, White, ToMove)`. The other 4 symmetries of the square do not keep the start position and are not needed.
- Each entry has a list of **book moves** (in the canonical frame) and is a node in a DAG; the child of a move is looked up by its canonical position.
- Each book move stores:

| Field | Type | Meaning |
|---|---|---|
| `Move` | `Move` | The move in the canonical frame, or a pass |
| `Value` | int16 | Value for the player who makes the move; backed up from the child if the child is in the book |
| `Origin` | enum | `Unknown` (imported, game result), `Heuristic`, `WinLossDraw`, `Exact`, `BackedUp` |
| `Effort` | `BookEffort` | For searched values: the limit kind and amount (time per move in ms, fixed depth, solve, or unknown), the depth reached, and the `EngineVersion` (bumped by hand when a change to the evaluation or search changes values; starts at 1) |

- Statistics such as the number of games through a position are left out (YAGNI); a later version can add them.

### Text format (source, in git)

One line per position, sorted by the shortest line that reaches it, keyed by that line in the d3 frame so it is readable and can be checked by replaying it. The syntax as implemented *(phase 1)*:

```text
# Stello opening book, format 2
# 11200 positions, 22878 moves
# <line> <move>:<value>:<origin>[:<limit>:d<depth reached>:v<engine version>] ...
# origin: U unknown, H heuristic, W win/loss/draw, X exact, B backed up; limit: time per move (s, ms), depth (ply), solve, - unknown
d3 c5:-39:B c3:-40:B e3:-110:B
d3c3 c4:40:B e6:-18:W
...
```

- A searched value with a known effort looks like `e6:12:H:60s:d14:v1`. A pass is `--`. Lines of equal length are sorted alphabetically, and of several shortest lines the alphabetically first is the key.
- The tool writes the file deterministically (stable order, invariant culture, LF) so a diff shows only what changed.
- The file is the checkpoint: the tool saves it (temporary file + rename) every N searches, and a re-run skips moves whose effort already meets the target. No separate `.partial` file is needed, unlike Checkers.

#### Why the move line is the key

The key does not affect transpositions: on load every line becomes a position, and the in-memory model is keyed by the canonical position, so all move orders to a position share one entry whatever the text key is. The move line was chosen over a position hash (as in Checkers) or the bitboards because:

- **Readable.** A line is an opening you can recognise and play on a board ("tiger" is `d3c5f6f5e6`), and a git diff of the book shows *which* openings changed.
- **Self-contained.** The positions are rebuilt by replaying the lines from the start, which also checks that every move is legal. A hash cannot be turned back into a position.
- **No hash function in the file format.** Stello has no Zobrist hashing (its hash table stores full positions); a hash key would fix a random table and the symmetry normalisation in the format forever, and 64-bit keys can collide.
- **Same structure as the binary format**, which is written from the start position with moves only.
- Costs: the key changes when a shorter line to the same position is added later (shows as a moved line in the diff), and the text file is larger (~0.5–1 MB, only in git, not shipped).

### Binary format (built for the apps)

- Magic `STBK`, version (2), position count, move count *(phase 1: the engine version is stored per move, not in the header)*.
- The DAG written depth-first from the start position with moves only (positions are implicit and rebuilt on load, as now), and back-references by index for transpositions. The master book takes about 7.5 bytes per move (172 KB, against 187 KB for the C++ file).
- Built from the text file by `Stello.BookTool build` and committed (see Decisions); WPF links it as `Data/OPENING`, the web build copies it to `wwwroot/data/OPENING.bin`.
- `OpeningBook.Load` detects the format by the first bytes (binary, text, or else the C++ file); a C++ file is converted. This covers the existing user books in `%AppData%\Stello\OPENING` and localStorage: they are saved in the new format on the next save, under the same name.

### Tool commands (phase 1)

- `import <OPENING> <out.txt>`: legacy file → text, origins as in finding 8. A position stored under several move orders gets the union of their move lists; for a move in more than one list, the value with the best origin wins. The legacy index used only the first line's list. *(Phase 1: a leaf move whose position another line continues from becomes `BackedUp`; the play differs from the old book in 1 position, which C++ stored in two mirrored frames.)*
- `format <in.txt>` *(phase 1)*: rewrites the text book in its normal form, e.g. after a hand edit.
- `build <in.txt> <out.bin>`: text → binary.
- `verify <file.txt> [<file.bin>]`: every line legal, every position reachable (errors); normal form, values backed up and sorted (warnings); binary and text give the same book (error).
- `stats <file>`: the table in chapter 11 (positions, leaves per ply, origins, effort histogram).

### In the apps (phase 1)

- `OpeningBook`, `BookLearner` (Add Game to Book, Evaluate Book, Self-play) and the book stores (`FileBookStore`, `LocalStorageBookStore`, `StartupBook`) work on the new model.
- `BookLearner` stores origin, effort and engine version for every search it does.
- Play is unchanged (first legal reply). *(Phase 1: the import keeps the first line's move order, so the computer plays the same book moves as before, except in the 1 position C++ stored in two mirrored frames.)*

---

## 2. Recalculate the leaves: `Stello.BookTool recalc` (phase 2)

```text
Stello.BookTool recalc <book.txt> [--time-s 60 | --depth N]
                       [--workers <physical cores>] [--save-every 10] [--hash-bits 19]
```

1. Collect the **leaf moves**: book moves whose child position has no entry. Leaves that reach the same position (transpositions) are searched once.
2. Skip what is already good enough: `Exact` always; `Heuristic` and `WinLossDraw` with effort at or above the target and the current engine version.
3. Order the work by ply (nearest the root first), so a stopped run has improved the moves that are played most.
4. Search each position after the leaf move with the normal `SearchEngine.Search` and `--time-s` (default 60) or `--depth`, one `SearchEngine` per worker. The engine switches to the endgame solver by itself (finding 4); the origin is taken from `SearchResult.Kind`. As in `BookLearner.SearchValue` (which can be reused), the moves are passed as `onlyMoves` so that a position with a single legal move is searched too; a position where the side to move must pass is searched for the opponent, and a finished game gets its exact disc count.
5. Store value, origin, effort and engine version; save every `--save-every` searches and on Ctrl+C.
6. At the end: back up the values through the DAG (one pass in reverse topological order), sort every move list best first, run `verify`, save.

*(Phase 2, as implemented:)*

- Each search starts with a cleared hash table, so the values are the same with any number of workers (tested with 1 and 4).
- A `WinLossDraw` value is kept, with the new effort, when the new search could not solve the position; otherwise the 3 384 proven results of the C++ book would become heuristic values.
- Back-up and sort also run after Ctrl+C, so the saved book is always consistent; `verify` then gives no warnings.
- Leaves that are not legal cannot exist (every reader checks legality), so nothing is removed.

Progress line per search: done/total, ply, line, value, origin, depth reached, search time, estimated time left.

**End-of-run report** (also written to `<book>.report.txt`): positions searched, values changed by origin, and the positions whose best move changed, sorted by ply, with the old and new move and value. The changed positions are the start positions for `match`. *(Phase 2: also available on its own as `compare <old> <new> [--out <report>]`, for the report against the book in git after a run that was stopped and continued.)*

After the run and the match, the recalculated master book is built to binary and shipped with the apps.

### Comparing the old and the new book: `Stello.BookTool match`

```text
Stello.BookTool match <book A> <book B> (--starts <report.txt> | --starts-ply N) [--max-starts N]
                      [--depth 10 | --time-s S] [--workers ...] [--hash-bits 19]
```

- **Start positions.** By default the positions from the report whose best move changed: only there can the books play differently, so no games are wasted on lines where they agree. `--starts-ply N` takes all book positions at ply N instead.
- **Books.** Book A is the master book as imported in phase 1 (from git, before the run), book B the recalculated one.
- **Pairs of games.** From each start position two games are played with the colours swapped: book A plays the side to move and book B the opponent, then the other way round. Each side is a `ComputerPlayer` with its own `SearchEngine`, its own book and the same search limits, so only the books differ. The book is used as in a real game (`BookTracker` reset at the start).
- **No repeated games.** The engine and the book choice are deterministic, so playing a pair again gives the same games; the variety comes from the start positions. If both books agree all the way, the pair scores 1–1 and cancels out.
- **Result.** Points for B (win 1, draw ½), average disc difference, and a 95 % interval over the pairs; B is better if the interval lies above 50 %. A list of the pairs where B lost points, to inspect by hand.
- **Fixed depth recommended**, so the result does not depend on machine load and the number of workers. *(Phase 2: the default is `--depth 10`; each game starts with cleared hash tables.)*
- **Cost.** About a minute per game at 2 s per move (book moves are free, late moves are solved fast). 500 changed positions = 1 000 games ≈ 17 hours on one core, ≈ 2 hours with 8 workers.

---

## Testing

- Import of `Stello C++/OPENING`: same positions and book replies as the legacy index; the same 16-move "tiger" line for `new Random(0)`.
- Text → binary → text round trip is identical; legacy user books load and convert.
- `recalc` on a small book with `--depth 4`: searches only what is needed, searches a transposed leaf once, resumes after cancellation without repeating work, back-up equals a full minimax, single-move and pass positions get a value, the report lists the changed positions.
- `match`: two identical books score exactly 50 %; a pair is played with the colours swapped.
- Existing `OpeningBookTests`, `BookLearnerTests`, `ComputerPlayerTests` and view-model tests keep passing on the new model.

## Documentation

- Update [docs/brain/11-opening-book.md](docs/brain/11-opening-book.md) (format) and [docs/brain/12-book-learning.md](docs/brain/12-book-learning.md) (new model, origin and effort); add a chapter for the book tool (`import`, `build`, `verify`, `stats`, `recalc`, `match`).
- Record the change in [Stello porting documentation.md](Stello%20porting%20documentation.md) as a new phase ("changed from C++: book format").

## Phases

1. **Format**: in-memory DAG model, text and binary format, `import`/`build`/`verify`/`stats`, legacy conversion on load, port `BookLearner` and the app's book stores; the master book moves to `Stello.Net/Book/opening-book.txt` and the apps ship it converted. No change in play.
2. **Recalc**: `recalc` with workers and resume, the end-of-run report and `match`; run it on all ~10 000 leaves of the master book (≈ 14 h with 12 workers, measured), compare the old and the new book with `match`, and ship the result. *(Done: 52.8 % for the recalculated book.)*

## Open questions (phases 1 and 2)

None; all answered (see Decisions).

---

## Postponed (phases 3 and 4)

Kept for later; not part of the current work.

### Decisions already taken

| Question | Decision |
|---|---|
| DOE and the existing "dropout expansion" | **Alongside.** Evaluate Book and Self-play stay in the apps; DOE is a new tool command. |
| Choosing a book move when playing | **Random among the moves within a margin of the best** (a setting), instead of always the first move. |

### Findings for later

- **Mixed value scales.** Heuristic values are in evaluation units; solved values are ±(32 600 + discs). The chapter 05 evaluation has no known "units per disc". DOE (a drop-out threshold) and random move choice (a margin) both compare values of different moves, so they need one scale.
- **The existing "dropout expansion" is Buro's, not Lincke's.** `EvaluatePositions` adds *one* best deviation (best move not in the book, found with `onlyMoves`) to a list that has no calculated reply, and only once. Lincke's DOE ([Checkers/Opening book Drop-Out Expansion.md](Checkers/Opening%20book%20Drop-Out%20Expansion.md)) repeatedly picks the most promising leaf in the whole book and expands it. Both need only one search per position, *not* Multi-PV as in Checkers.
- **Deviation per position.** DOE needs, for every position, the value of the best move *not* in the book. A later format version can store it per position instead of adding it as a book move.
- **Book size.** DOE can grow the book without limit, so `expand` needs a position limit that fits the web version.

### Phase 3: Drop-Out Expansion, `Stello.BookTool expand`

```text
Stello.BookTool expand --book <file.txt> [--omega <units per ply>] [--max-ply 40]
                       [--max-positions N] [--max-time-min M] [--iterations N]
                       [--time-s 60 | --depth N] [--workers ...]
```

- **Priority.** Lincke (*Strategies for the Automatic Construction of Opening Books*, CG 2000, and his 2002 ETH thesis): every leaf and every deviation gets an expansion priority from the path to it, ω × depth + the sum of the value losses along the path; the lowest is expanded next (formula to be checked against the paper). The Checkers implementation ([chapter 10, section 5](Checkers/docs/brain/10-opening-book.md#5-full-width-early-plies--drop-out-expansion-expanddropout)) uses a simpler variant that is proven in practice: a hard threshold δ per ply (15 cp at plies 0–5, 10 cp at 6–9, 6 cp from 10) and the least-visited active child.
- **Expansion.** Expanding a leaf: search its position and its deviation. Expanding a deviation: add the move, search its position and its deviation. Then back up values along all parent paths.
- **Both colours.** The book plays either side, so value losses of both players count.
- **Full-width early plies** (`--full-width-plies N`, from Checkers phase A): store all legal moves in the first N plies, so the computer stays in book against any human reply early on.
- **Drop-out vs. play margin.** The DOE threshold must be at least the play margin of phase 4 (Checkers: δ 15 cp ≥ margin 10 cp).
- **Parallel.** Workers reserve leaves with "virtual visits", as in `BookBuilder.ExpandDropOut`.
- **Limits.** `--max-positions`, `--max-time-min`, `--iterations`, `--max-ply`; the saved book is valid at any time.
- **Seed.** Start from the master book after phase 2.

### Phase 4: Varied play from the book

- `TryGetMove` chooses uniformly at random among the legal book moves whose value is within a margin of the best; the margin is a setting (0 = always the best).
- `Unknown` moves are played only if nothing else is in the book; a solved win is never left for a heuristic move, and a solved loss is never chosen when a heuristic move exists.
- "Child transposition" fallback from the Checkers `TryGetMove`: if the position is not in the book but a legal move leads to a book position, play that move.
- Measure in self-play matches that the wider book does not lose strength.

### Open questions for later

1. **Value scale.** How many evaluation units is one disc? Proposal: measure it once (regress the evaluation against the solved disc difference over many book positions), or choose ω, δ and the margin by experiment.
2. **Size limit.** The largest book the web version should ship and keep in localStorage (e.g. 1 MB binary).
3. **Search effort for DOE.** 60 s as in recalc, or shorter to grow faster?
4. **Depth of the book.** Should DOE stop at a ply (e.g. 40) where the engine solves the game anyway during play?
5. **Which DOE.** Lincke's priority or the Checkers variant? Proposal: start with the Checkers variant, since its code can be ported.
