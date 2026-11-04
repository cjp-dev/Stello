# 25 – Phases 0–4: the engine in Rust

[Back to the index](README.md)

## In short

Part 1 of the plan for Stello 3.0, [making-stello-3-plan.md](../Plans/making-stello-3-plan.md), rewrote Stello 2.0's search in Rust, node for node, and made the apps and tools use it. It took five phases, all on 2 November 2026; the measurements are in [making-stello-3-findings.md](../Plans/making-stello-3-findings.md).

- **Why:** the same Egaroucid search is 1.5 times faster in C++ than in C# natively, and 1.9–2.7 times in WebAssembly. Rust compiles through LLVM like Clang, so it should get close to C++. Speed is worth playing strength: twice the time gave +84 Elo at 100 ms and +38 at 500 ms ([chapter 23](23-phase-5-the-search.md)).
- **What moved:** the search core: board and rules, the evaluation, the midgame search, the endgame solver, the hash tables and the time control, about 3,600 lines of C#. The book, the game record, the apps and the tools stay in C#, behind a thin `SearchEngine` wrapper with the same API. The C# engine is frozen as Stello 2.0 in `Stello.Engine.V2`.
- **Parity first:** every step was checked against the frozen C# engine before the next one started: the same moves, scores and node counts on the benchmarks, on thousands of random positions, and on a whole-book recalculation.
- **Speed:** a straight port was 1.27–1.37 times faster. A speed pass that kept every node made it **2.8–3.1 times faster in the midgame and 1.75 times on FFO #40–49** (26.1 s against 45.7 s).
- **In play:** Stello 3.0 beat Stello 2.0 with **59.6 %** (+68 Elo) at 500 ms per move, about two plies deeper in the same time.
- **In the browser:** the web worker is now Rust compiled to WebAssembly instead of a second .NET runtime. It is 2.30–2.40 times faster in the midgame and 1.59 times in the solver, starts in 92 ms instead of 680 ms, and answers Move Now in under 1 ms.
- **Next:** Part 2 of the plan, threads: a shared hash table, Lazy SMP and YBWC, as in Egaroucid.

## Why Rust

[cpp-vs-csharp-speed.md](../Plans/cpp-vs-csharp-speed.md) measured the same Egaroucid search in C++ and in C#, with node counts within 2 %:

| | C++ faster than C# by |
|---|---|
| Native, C++ without AVX2 | 1.5 times |
| Native, C++ with AVX2 | 2.3 times |
| WebAssembly (C++ with Emscripten, C# with .NET's AOT) | 1.9–2.7 times |

Three more reasons came from Stello 2.0's results ([chapter 24](24-results-and-what-comes-next.md)):

- **Speed pays.** The time-odds matches put twice the time at +84 Elo at 100 ms and +38 at 500 ms.
- **Threads multiply with speed.** The desktop machines have 16–24 threads, and the engine used one. A faster core makes every thread faster.
- **Where 2.0 stood.** It beat the Egaroucid port at 100 and 500 ms, but lost at 2 s, where depth counts most.

Chapter 24 had listed threads first and a new core second. The plan takes them in the other order: the core first (Part 1, this chapter), then the threads (Part 2).

## The decisions

The user made these decisions on 2 November 2026 ([the plan](../Plans/making-stello-3-plan.md#decisions-user-2-november-2026)):

| Question | Decision |
|---|---|
| Toolchain | rustup with stable Rust, the `wasm32-unknown-unknown` target and `wasm-bindgen-cli`. Nightly only in Part 2, for threads in the browser. An exception to the ".NET only" rule, for this plan |
| The web engine | Rust compiled to WebAssembly with wasm-bindgen, in a small JavaScript Web Worker. The Blazor UI keeps its engine host; the .NET worker goes |
| The boundary | The search core moves to Rust. A thin C# `SearchEngine` wrapper keeps today's API, so the book code, the game records, the apps and the tools need no changes. The C# engine is frozen as Stello 2.0 (`Stello.Engine.V2`) |
| Releases | The apps get the Rust engine after Part 1 (this preview) and the threaded one after Part 2. About shows 3.0 once Part 1 is live; 3.0 is final after Part 2 |
| The Windows DLL | Built by Cargo on the user's PC, called from MSBuild as in the sandbox. Each OS has its own Cargo target folder, because the repository folder is shared. Nothing binary is committed |
| The gate of Part 1 | Node for node identical to the C# 2.0 engine: the same moves, scores and node counts. Speed-ups must keep the node counts |

### The engines

| Name | Project | Match side | What it is |
|---|---|---|---|
| Stello 1 | `Stello.Engine.V1` | `engine=stello-v1` | The engine before 2.0, frozen |
| Stello 2.0 | `Stello.Engine.V2` | `engine=stello-v2` | The C# engine as released on 2 November 2026, frozen |
| Stello 3.0 | `Stello.Engine` (the C# API) and `Stello.Rust` (the core) | `engine=stello` | The Rust engine |
| Egaroucid | `Egaroucid.Engine` | `engine=egaroucid` | The C# port of Egaroucid 7.9, the reference ([chapter 18](18-the-egaroucid-port.md)) |

### What moved and what stayed

Every user of the engine reaches the search through `SearchEngine`: directly, or through `ComputerPlayer`, `BookSearch`, `BookRecalculator`, `BookLearner` or `BookMatch`. So a wrapper with the same API moves all of them to Rust at once: the WPF app, the book tool's `recalc` and `match`, the benchmark tool, the trainer's `mpc` and `mpc-end`, and the tests.

- **To Rust:** `SearchEngine.cs` (809 lines), `Endgame/` (about 1,470), `Evaluation/` (about 610), `Search/` (about 310), and `Bitboards` and `Board`'s move generation (about 250).
- **Stays in C#:** `Board`, `Move`, `Square`, `Game`, `SearchLimits`, `SearchResult`, the opening book and its formats, `BookRecalculator`, `BookLearner`, `BookMatch`, `ComputerPlayer`, and the apps. The C# `Evaluation/` stays too, for the trainer's `train` and `report`, which read the same `stello-eval.bin`.
- **Only the web app changes more:** its worker is replaced, and the book moves to the page.

## Phase 0: the set-up and the frozen Stello 2.0

- **The toolchain** ([findings](../Plans/making-stello-3-findings.md#toolchain)): Rust 1.99.1 with the targets `x86_64-unknown-linux-gnu`, `x86_64-pc-windows-gnu` and `wasm32-unknown-unknown`, MinGW-w64 (GCC 13) as the linker for a check build of the Windows DLL, and `wasm-bindgen-cli` 0.2.130, to which the crate is pinned.
- **`Stello.Engine.V2`:** a copy of `SearchEngine` with `Endgame/`, `Search/`, `Evaluation/` and `ComputerPlayer`, in the namespaces `Stello.Engine.V2.*`, with its own embedded `stello-eval.bin`. It shares the board, the limits, the results and the book with `Stello.Engine`. The match and benchmark tools take it as `engine=stello-v2` and `--engine stello-v2`.
- **105 recorded signatures** (`Stello.Engine.Tests/Data/stello-v2-signatures.txt`): the 100 midgame positions at depth 10 and FFO #40–44 solved, each with move, score, kind, depth and nodes. `StelloV2Tests` checks them for the frozen engine, and later for the Rust engine through the wrapper. That became the parity test that runs with every build.
- **`Stello.Rust`:** a Cargo workspace (edition 2024, GPL-3.0-only, version 3.0.0) with four crates: `stello-core` (the engine), `stello-ffi` (the C ABI library `stello_engine`), `stello-wasm` (the wasm-bindgen package) and `stello-cli` (the tool `stello`). The release profile has fat LTO, one codegen unit and `panic = "abort"`, so no panic unwinds into .NET or JavaScript.
- **The gate:** V2 visited the same nodes and found the same moves and scores as the engine it was copied from, on all 100 `mid-bench` positions at depth 10 (3,719,468 nodes) and on FFO #40–49 (1.10 G nodes). An empty library built for Linux, Windows and WebAssembly, and .NET and Node called it.

## Phase 1: the core in Rust, parity first

### The order of the port

The core was ported in dependency order, and each step had to match C# before the next one started:

1. **Bitboards, move generation and flips** (`board.rs`, `bits.rs`, `square.rs`), and the endgame's move generator (`endgame/bits.rs`), which the midgame ordering already uses.
2. **The evaluation** (`eval/`): the three pattern sets, the symmetries, the folding, the reader of `stello-eval.bin` (gzip with the pure-Rust `flate2`), and the search's scale `pattern_score`.
3. **The midgame search** (`search/`): PVS, Multi-ProbCut with the same error table, the move ordering with the response table and the evaluation ordering from look 4, ETC, the two-entry hash tables with the same hash function and replacement, iterative deepening, the 2/3 time rule, and the switches of `SearchOptions`.
4. **The endgame solver** (`endgame/`): `end_window`, `end_deep`, `end_local`, `end_simple`, `end_fast`, `last4`–`last1`, the stable-disc tables, the local table and the two passes.
5. **Stopping and progress:** the abort checks at exactly C#'s node counts, the 100 ms heartbeats, and cancel and Move Now through a `StopSignal` trait. `SearchSignals` holds two atomic flags that another thread can set; the browser reads a `SharedArrayBuffer` instead.

The structure follows the C#, with one change: `SearchEngine` keeps what lives between searches (the hash tables, the response table, the options), and a `Searcher` holds one running search. The endgame solver is another `impl Searcher` block. Instead of exceptions, a stopped search returns `Abort::Stop` or `Abort::Cancelled` up the recursion with `?`.

### How parity was checked

| Check | How | What was compared |
|---|---|---|
| Fixtures | The new `Stello.Benchmark parity-data` writes them from `Stello.Engine.V2` into `stello-core/tests/data` (1.5 MB): 6,214 positions from 30 random games, `midgame.txt`, the FFO positions and edge cases (78 positions with a pass, 170 finished games) | For each position: the legal moves of both sides, the potential mobilities, parity, stage, the evaluation, `pattern_score`, the endgame's ordering value, and for every move its flips and the evaluation after it (over 30,000 moves); checksums of the pattern indices, the folding and the weights. Changing a single value made each parity test fail |
| Perft | `stello perft`, against C# | Depths 1–8 from the start (390,216 at depth 8), and 14 positions at depth 5 |
| The evaluation | `stello eval-split` and `parity-data --split`, FNV-1a checksums over the trainer's test split | All 9,973,199 positions (`d6d58ebf91e5794f`), and the report's subset of 1,040,276 |
| The searches | `stello mid-bench` and `solve-bench --out` write the benchmark's JSON; `Stello.Benchmark mid-bench --engine stello-v2 --compare <file>` (and `solve-bench`) compare them entry by entry, as for Stello 1 and the Egaroucid port ([chapter 17](17-measuring-the-engine.md)) | Nodes, moves and scores of every search |
| Random positions | `parity-data --midgame` and `--endgame` write them to `Stello.Benchmark/Data` | 1,000 midgame positions at depth 8; 500 endgames with 14–24 empty squares, 1,000 with 2–13, 300 with a pass within 4 plies |
| The hand-over | `midgame-handover.txt` at depth 12 and 14 | 300 searches each, through the win/loss/draw and exact passes |
| The options | `--selective off`, `--eval` with two other evaluation files, `--probcut-pct 150`, `--hash-bits 12` | 100 or 1,100 searches each |

Every run was "identical" ([findings, the phase 1 gate](../Plans/making-stello-3-findings.md#the-phase-1-gate-passed)): for example 3,719,468 nodes at depth 10, 10,582,535 at depth 12, and 1,099,769,823 on FFO #40–49.

### What the C# taught

Node-for-node parity meant keeping details that a fresh design would not have:

- **Two abort-check schemes share one node counter:** the midgame checks when `(++nodes & 1023) == 0`, the solver at a next-check count. Leaf evaluations count as nodes too, so the midgame's checks fall irregularly, but deterministically.
- **Inconsistent counts:** `EndWindow` and `EndDeep` count an evaluation at a final score, the other solver layers do not; the ordering counts a node without an abort check; `Last4`–`Last1` count each pass level as a node.
- **Hash cutoffs are asymmetric:** an upper bound cuts if its value is below α, strictly; a lower bound if it is at least β.
- **Arithmetic:** the response table can overflow in very long searches, so Rust adds with wrapping, as C#'s unchecked arithmetic does. `StableDiscs.PlayOnLine` shifts by −1 when the place is 0; C# masks the shift count to 31, and Rust does the same explicitly.
- **Sorting:** in the solver's ordering, moves already decided by the local table stay in the sort and can change the order of the rest.
- **The browser's clock:** `std::time::Instant` is always zero on `wasm32-unknown-unknown`, so the engine takes its clock as a function (`set_clock`).

### Speed before the speed work

The straight port, with no speed work, was faster on one thread ([findings](../Plans/making-stello-3-findings.md#speed-1-thread-release)):

| | Rust | C# 2.0 | Faster by |
|---|---:|---:|---:|
| FFO #40–49 | 34.0–34.4 s | 47.1–47.8 s | 1.37 times |
| `mid-bench` depth 10 / 12 | 0.84 s / 2.35 s | 1.06 s / 3.07 s | 1.27 / 1.31 times |
| perft(8) | 8.9–9.6 ms | 38–41 ms | 4.2 times |

## Phase 2: the desktop and the tools on Rust

### The wrapper

- **The C ABI** (`stello-ffi`, 629 lines): an engine handle (hash bits, the options, and optionally the bytes of another evaluation file), a signals handle for cancel and Move Now, `stello_engine_search` with the position, the limits, the only moves, a progress callback and an iteration-done callback, returning a status code, `stello_engine_clear_hash`, and the version functions. Every argument is checked, and no panic crosses the boundary.
- **`Stello.Engine.SearchEngine`** is now a wrapper over a `SafeHandle`, with `[LibraryImport]` (`Native/StelloNative.cs`). It keeps the API: the progress comes through an `[UnmanagedCallersOnly]` callback, the cancellation tokens set the signals, a cancelled search still throws `OperationCanceledException`, and an exception in a callback reaches the caller.
- **The C# search left `Stello.Engine`:** the midgame part of `SearchEngine.cs`, `Endgame/` and `Search/{MoveOrdering,TimeControl,TranspositionTable}.cs`. `Stello.Engine.V2` keeps them; the tests of the C# internals now test V2.
- **The build** (target `BuildStelloNative`, [chapter 14](14-app-integration.md#how-each-app-gets-the-engine)): Cargo runs when a Rust source or the evaluation file is newer than the library, into a target folder per OS. The library flows to every project that references `Stello.Engine`. On Windows the MSVC DLL links the C runtime statically.
- **Parity through the wrapper:** `--engine stello` against `--engine stello-v2 --compare` was identical in every run. **The whole master book** was recalculated at `--depth 10` with 4 workers, once with the C# engine and once with Rust: identical books and reports, 13,064 values changed by both.
- **The tools:** each ran once on Rust with no change: the benchmark's `match`, the book tool's `match` and the trainer's `mpc`.

### The speed pass

A profile with `perf` showed where the time went ([findings](../Plans/making-stello-3-findings.md#where-the-time-went-perf)):

- **Midgame:** the evaluation took 58 %: a third waiting on weight-table loads, the rest building the indices (16 symmetry transforms, then about 120 parts). Then the ordering (10 %, mostly two insertion sorts), `flips` (6 %), the table store (3.6 %), and the 48 MB `memset` at the start of every search.
- **Endgame:** `flips` took 45 %, because its four downward rays branched on a bit scan and mispredicted. Then `end_simple` (15 %), `last2` (9 %) and `legal_moves` (8 %).

The changes, each checked with `--compare` against C# 2.0 ([findings](../Plans/making-stello-3-findings.md#changes)):

| Change | How it works now | Gain |
|---|---|---|
| The evaluation reads each feature's squares straight from the board | No symmetry transforms. With BMI2, one `pext` per side and feature plus a shared digit table, chosen at run time by `cpu.rs` (which refuses AMD and Hygon before family 19h, where `pext` runs in microcode). Otherwise a portable path that gathers squares from different columns with one multiply; WebAssembly runs it ([chapter 04](04-evaluation.md)) | Depth 10: 0.84 → 0.49 s with `pext`, 0.84 → 0.70 s portable |
| The midgame uses the endgame's mask flips | C# walked each ray square by square ([chapter 03](03-rules-and-move-generation.md)) | Depth 10 −8 %, perft(9) 64 → 50 ms |
| The hash table is cleared by generation | A 16-bit counter in the entry's padding keeps it at 24 bytes; an older entry counts as empty, also for replacement; the table is zeroed only when the counter wraps ([chapter 07](07-transposition-table.md)) | The same search time; 5 × 100 searches 2.99 → 2.37 s wall time |
| Branchless downward rays in the endgame flips | `1 << 63 >> lzcnt(x \| 1)` | FFO #40–44 −14 %, depth 10 −17 % |
| A rank sort in `sort_descending` | No data-dependent branches ([chapter 05](05-move-ordering.md)) | Depth 12 −3 % |
| Prefetch of the hash slot | At nodes that store but do not probe | Depth 12 −3 %, random positions −10 % |
| `last_flip_count` from per-line tables | The row, column and both diagonals into a byte each, then an 8 × 256 count table, checked against `flips` on 20,000 boards × 64 squares ([chapter 08](08-endgame-solver.md)) | FFO −6 % |

Tried and reverted: `#[inline(always)]` on the endgame flips (8–10 % slower), one packed `u64` per move for the insertion sort (8 % slower), and a 16-lane rank comparison (35 % slower, because it did not vectorize).

The result, on one pinned CPU, fastest of 3 passes, through the wrapper ([findings](../Plans/making-stello-3-findings.md#speed-one-pinned-cpu-fastest-of-3-passes-same-session)):

| Run | C# 2.0 | Rust before the pass | Rust after the pass | Faster than C# |
|---|---:|---:|---:|---:|
| `mid-bench` depth 10 | 1.074 s | 0.831 s | 0.351 s | 3.06 times |
| `mid-bench` depth 12 | 3.077 s | 2.395 s | 1.062 s | 2.90 times |
| 1,000 random positions, depth 8 | 2.590 s | 2.102 s | 0.920 s | 2.82 times |
| FFO #40–49 | 45.7 s | 34.4 s | 26.1 s | 1.75 times |
| Whole-book recalculation, depth 10, 4 workers | 50 s | 47 s | 32 s | 1.56 times |

The per-position FFO times are in [v3p2-solve-bench-40-49.json](../Plans/benchmark-results/v3p2-solve-bench-40-49.json) and in [chapter 08](08-endgame-solver.md). The wrapper costs nothing measurable: the command-line tool gives the same times. The book recalculation gains less, because each of its roughly 8,000 short searches starts with cleared tables and pays the call.

### The match: 3.0 against 2.0

1,180 games from `starts.txt`, 500 ms per move, no books ([match_v3p2-vs-v2-500ms.md](../Plans/benchmark-results/match_v3p2-vs-v2-500ms.md)):

| | Stello 2.0 (C#) | Stello 3.0 (Rust) |
|---|---:|---:|
| Points | 476.5 | **703.5 (59.6 %)** |
| Wins / draws / losses | +389 =175 −616 | +616 =175 −389 |
| Midgame depth (average) | 16.67 | 18.58 |
| First solve (empty squares, average) | 22.2 | 23.2 |
| Nodes per second | 4.25 M | 10.38 M |

Stello 3.0 scored 59.6 % (57.3–61.9 %), **+68 Elo** (+51 to +84). The same search reaches about two plies deeper and starts solving one empty square earlier. Over whole games it searches 2.4 times the nodes per second; the benchmarks show more, because games include the endgame, where the gain is 1.75 times.

### The gate of phase 2

| Gate item | Result |
|---|---|
| The tests pass on the wrapper | 483 engine tests (with the 105 signatures through the wrapper and 14 new wrapper tests), 34 Egaroucid tests, `cargo test` |
| At least 1.5 times the native speed (target) | 2.8–3.1 times in the midgame, 1.75 times on FFO, 1.56 times on the whole-book recalculation |
| 3.0 against 2.0 at 500 ms above 50 % | 59.6 % (57.3–61.9) |
| The book recalculation gives identical values | the whole book at depth 10: identical books and reports, before and after the speed pass |
| WPF builds | yes, with `stello_engine.dll`. The user built and ran it on Windows: Cargo built the MSVC DLL, and the app plays, stops and solves |

## Phase 3: the web app on Rust

### The design

Stello 2.0's web worker was a second .NET runtime with its own `ComputerPlayer`, engine and copy of the book. In 3.0 ([chapter 14](14-app-integration.md#the-engine-in-the-browser)):

- **`stello-wasm`** exports `init(evaluation?, hashBits)`, `search(request, host)` and `version()`. `search` takes and returns the same JSON as the .NET worker (`EngineProtocol`); Rust reads and writes it itself, because `JSON.parse` would round the 64-bit boards.
- **The stop signal is read at every abort check,** every 1,024 nodes, with `Atomics.load` on the page's `SharedArrayBuffer`. The .NET worker read it only at its progress reports, up to 100 ms apart.
- **The clock** is `performance.now()`, and progress is throttled as before: a new depth or best move at once, otherwise every 100 ms.
- **`engine-worker.js`** has two commands, `init` and `search`, and needs no book.
- **The book moved to the page.** `WebEngineHost` plays the book move with `ComputerPlayer.TryChooseBookMove` (the book step of `ChooseMove`, moved out unchanged), so a book move no longer waits for the worker, and it adds games to the book itself. It resets the book tracker when the worker's engine is made again, as the .NET worker's new `ComputerPlayer` did.
- **The evaluation is embedded in the module.** The module is 1,194,262 bytes, 1,065,123 with brotli; 990,568 of them are the gzip file, which brotli cannot shrink. A separate file would transfer the same plus a request. `init` still accepts the bytes of another file.
- **`Stello.Engine` no longer embeds the evaluation.** Only two tool commands still used `EvalWeights.Default`; they now load `Data/stello-eval.bin`. The browser's `Stello.Engine.wasm` went from 1,031,245 to 41,135 bytes with brotli.
- **The build and the deploy:** `BuildStelloWasm` in `Stello.Web.csproj` runs Cargo and wasm-bindgen into `wwwroot/engine/` (git-ignored), and stops with a clear message if Cargo, the target or wasm-bindgen is missing. The deploy workflow installs Rust stable with the target and the prebuilt wasm-bindgen 0.2.130; its Rust steps were tried in a clean `ubuntu:24.04` container.

### The gate of phase 3

The Release publish in headless Chromium, with COOP/COEP and brotli, against the .NET AOT worker running Stello 2.0, each search on a fresh engine ([findings](../Plans/making-stello-3-findings.md#gate-p3-release-aot-publish-in-headless-chromium-served-with-coopcoep-and-brotli)):

| Run | .NET AOT worker | Rust worker | Faster by |
|---|---:|---:|---:|
| `mid-bench` depth 10, 100 positions | 2.149 s, 1.73 M nodes/s | 0.933 s, 3.99 M nodes/s | 2.30 times |
| `mid-bench` depth 12, every 5th position | 1.389 s | 0.580 s | 2.40 times |
| FFO #40–44 | 4.141 s, 22.3 M nodes/s | 2.609 s, 35.4 M nodes/s | 1.59 times |
| All three | 7.68 s | 4.12 s | 1.86 times |

| Gate item | Result |
|---|---|
| The Release build plays | a whole game, computer against computer, 1 s per move with the book (19 book moves, 41 searched); Add Game to Book saved the book; no page errors |
| Stop and Move Now within 100 ms | in the engine 0.2–0.9 ms (the .NET worker 6–38 ms); in the app, click to board, Move Now 25–27 ms and cancel 28–31 ms (.NET 34–57 ms) |
| 22 empty squares solved | FFO #41 in 340 ms (.NET 569 ms); a 22-empty game position in the app in 0.26 s (.NET 0.49 s) |
| At least 1.9 times the browser speed (target) | 2.30–2.40 times in the midgame, 1.59 times on FFO, 1.86 times overall: just short |
| Worker start-up | 89–97 ms, against 658–713 ms for the second .NET runtime |
| Transferred size, first visit | 7,913,010 bytes (2.0) → 6,844,294 bytes |

The browser engine matched the 2.0 signatures on all 105 searches, and agreed with native `stello mid-bench --depth 12` on 20 of 20.

### Why the browser stays short of 1.9 times

A profile of the solver in WebAssembly showed it is already close to native: FFO #40–44 take 2.59 s under Node against 2.08 s natively, and 2.17 s natively with `--cpu portable`, the same code WebAssembly runs. `i64.clz`, `ctz` and `popcnt` are single WebAssembly instructions that V8 compiles to `lzcnt`, `tzcnt` and `popcnt`. The gap is elsewhere: .NET's AOT build is nearly as fast in the browser as C# natively (22.3 against 23.3 M nodes/s), so 1.9 times in the browser would need the solver itself to be about 25 % faster than after the speed pass.

Tried in WebAssembly, with the nodes unchanged, and not kept ([findings](../Plans/making-stello-3-findings.md#the-solver-in-webassembly-follow-up)): a copy of `flips` per square (25 % slower), `#[inline(always)]` (7 % slower), branchless upward rays (no change), `opt-level` 2 (no change) and `s` (15 % slower), `wasm-opt -O3` (0–4 % slower), `simd128` (about 2 % faster) and `target-cpu=bleeding-edge` (about 3 % faster, but not every browser has its features).

## Phase 4: the 3.0 preview

Phase 4 releases Part 1: About shows "Stello Version 3.0" ([chapter 15](15-gui-and-settings.md)), the documentation's Part 1 describes the Rust engine and this Part 3 begins, and main is fast-forwarded and deployed. 3.0 is called a preview until Part 2 adds the threads.

## What comes next

Part 2 of the plan is parallel search, modelled on Egaroucid's `ai.hpp`, `ybwc.hpp` and its thread pools. The Egaroucid port did not port this part ([chapter 18](18-the-egaroucid-port.md)).

| Phase | What | Gate |
|---|---|---|
| 5 | A thread pool in `stello-core`, a `Threads` field in `SearchLimits` (default 1), and a thread-safe transposition table whose torn reads cannot be taken for valid entries | 1 thread stays node for node identical; N threads give correct results; at most 3 % overhead on 1 thread |
| 6 | Lazy SMP: at main depths up to 10, helper searches of the root at deeper depths or higher selectivity fill the shared table | 4 and 8 threads beat 1 thread at 500 ms (lower bound above 50 %) |
| 7 | YBWC: after the first child of a node, the other children go to idle threads, in the midgame and in `end_window`/`end_deep` above the local table; Lazy SMP and YBWC together | FFO #40–49 correct and faster with 4, 8 and 16 threads; not worse than Lazy SMP alone at 500 ms, better at 2 s |
| 8 | Threads in the browser: nightly Rust with atomics and a pool of Web Workers sharing the WASM memory; the single-threaded module as the fallback | Faster and correct with N threads; the fallback works |
| 9 | A "Threads" setting in both apps, `threads=` and `--threads` in the tools, the final measurements against 2.0 and Egaroucid at 100 ms, 500 ms and 2 s, and the 3.0 release | — |

The speed pass also left ideas that keep the nodes ([findings](../Plans/making-stello-3-findings.md#what-more-would-take-none-of-it-changes-nodes)): AVX2 flips and legal moves in the solver (which needs the recursive solver compiled twice), incremental pattern indices through make and unmake, `simd128` in the browser, and a different flip algorithm such as Edax's per-square `flip_carry` functions.

## Design notes

- **Parity first, speed second.** With the frozen engine as an oracle, every difference is a bug, found at the step that made it. Speed work started only after the whole core matched, and every speed change had to keep the node counts, so the parity runs checked it too.
- **The boundary is narrow.** A position, the limits and the stop signals go in; progress and a result come out. Everything else (the book, the game record, `ComputerPlayer`, the apps, the tools) did not change, so the C# tests of those parts still hold.
- **The same engine in both apps.** The desktop app and the browser run the same Rust code; only the boundary differs (a C ABI, or wasm-bindgen), and WebAssembly takes the portable paths.
- **Nothing binary is committed.** The native library and the WebAssembly module are built by Cargo during the .NET build, on each machine, into a target folder per OS.
- **Kept quirks.** The node-counting quirks and the sorting detail above are kept, so that 3.0 stays comparable with 2.0 node for node. Part 2 keeps that for one thread: its phase 5 gate is that 1 thread stays identical.

## Where in the code

| File | What |
|---|---|
| [Stello.Rust/Cargo.toml](../Stello.Rust/Cargo.toml), [.cargo/config.toml](../Stello.Rust/.cargo/config.toml) | The workspace, the release profile, the pinned wasm-bindgen; the Windows linkers |
| [stello-core/src](../Stello.Rust/stello-core/src) | The engine; each module names its C# original ([chapter 01](01-overview.md#the-engine-at-a-glance)) |
| [stello-core/src/cpu.rs](../Stello.Rust/stello-core/src/cpu.rs) | The run-time choice of the BMI2 path |
| [stello-ffi/src/lib.rs](../Stello.Rust/stello-ffi/src/lib.rs) | The C ABI for .NET |
| [stello-wasm/src/lib.rs](../Stello.Rust/stello-wasm/src/lib.rs), [protocol.rs](../Stello.Rust/stello-wasm/src/protocol.rs) | The WebAssembly engine and its JSON |
| [stello-cli/src/main.rs](../Stello.Rust/stello-cli/src/main.rs), [bench.rs](../Stello.Rust/stello-cli/src/bench.rs) | `perft`, `eval-split`, `solve-bench`, `mid-bench` |
| [Stello.Engine/SearchEngine.cs](../Stello.Net/Stello.Engine/SearchEngine.cs), [Native/StelloNative.cs](../Stello.Net/Stello.Engine/Native/StelloNative.cs), [Stello.Engine.csproj](../Stello.Net/Stello.Engine/Stello.Engine.csproj) | The wrapper, the `[LibraryImport]` functions, `BuildStelloNative` |
| [WebEngineHost.cs](../Stello.Net/Stello.Web/Services/WebEngineHost.cs), [engine-worker.js](../Stello.Net/Stello.Web/wwwroot/js/engine-worker.js), [Stello.Web.csproj](../Stello.Net/Stello.Web/Stello.Web.csproj) | The book on the page, the worker, `BuildStelloWasm` |
| [Stello.Engine.V2](../Stello.Net/Stello.Engine.V2) | The frozen C# engine of Stello 2.0, and `V2MatchPlayer` |
| [Stello.Benchmark](../Stello.Net/tools/Stello.Benchmark) | `parity-data`, `--engine stello-v2`, `--compare` |
| [azure-static-web-apps.yml](../.github/workflows/azure-static-web-apps.yml) | The deploy with Rust and wasm-bindgen |

## Tests

- **Rust** (`cargo test` in `Stello.Rust`): [tests/parity.rs](../Stello.Rust/stello-core/tests/parity.rs) against the fixtures written by `parity-data` (the positions, the pattern sets, perft); [tests/search.rs](../Stello.Rust/stello-core/tests/search.rs) with 20 of the recorded midgame searches and the 5 FFO signatures, the single-move shortcut, passes, `only_moves`, progress and heartbeats, the time rules with a clock the test moves by hand, cancel and Move Now from another thread, and random endings against a plain minimax; and the unit tests in the modules (for example both evaluation paths against the plain indices, the per-line flip counts against `flips`).
- **.NET:** [StelloV2Tests.cs](../Stello.Net/Stello.Engine.Tests/StelloV2Tests.cs) checks the 105 signatures for both the frozen engine and the Rust engine; [NativeEngineTests.cs](../Stello.Net/Stello.Engine.Tests/NativeEngineTests.cs) tests the wrapper. The other engine tests run through the wrapper unchanged.
- **WebAssembly:** 9 Node tests of the built package, [engine.test.mjs](../Stello.Rust/stello-wasm/tests/engine.test.mjs) ([chapter 14](14-app-integration.md#tests)).

## Further reading

- [making-stello-3-plan.md](../Plans/making-stello-3-plan.md): the plan, with its Status section.
- [making-stello-3-findings.md](../Plans/making-stello-3-findings.md): every measurement of phases 0–3.
- [cpp-vs-csharp-speed.md](../Plans/cpp-vs-csharp-speed.md): C++ against C#, natively and in WebAssembly.
- [References](28-references.md#25-phases-04-the-engine-in-rust): Rust, the Rustonomicon on FFI, wasm-bindgen, BMI2.

---

Previous: [24 Results and what comes next](24-results-and-what-comes-next.md) · Next: [26 Phases 5–9: threads](26-phases-5-9-threads.md)
