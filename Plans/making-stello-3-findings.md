# Findings from making Stello 3.0

What the work on Stello 3.0 teaches us, written down as we go: the decisions, the measurements, what worked and what did not, and the ideas for later.

- **The plan:** [making-stello-3-plan.md](making-stello-3-plan.md). This file is updated after every phase.
- **Where it starts:** Stello 2.0 as released on 2 November 2026 ([docs chapter 24](../docs/24-results-and-what-comes-next.md)), and the C++/C# speed comparison ([cpp-vs-csharp-speed.md](cpp-vs-csharp-speed.md)).

## Status

| Phase | State | What it showed |
|---|---|---|
| 0 Set-up and the frozen Stello 2.0 | done (2026-11-02) | Stello.Engine.V2 searches node for node like 2.0; the Rust workspace builds for Linux, Windows and WASM, and .NET and Node call it |
| 1 The core in Rust | done (2026-11-02) | Board, evaluation, midgame search and endgame solver node-for-node identical to C# 2.0 (mid-bench, FFO #40–49, 3,100 random midgame and endgame positions, the whole test split); 1.27–1.37× faster on one thread |
| 2 Desktop and tools on Rust | done (2026-11-02) | The C# `SearchEngine` calls Rust, identical to 2.0 (mid-bench, random positions, FFO, handover, 500 random endgames, a whole book recalculation); after a speed pass 2.8–3.1× faster in the midgame and 1.75× on FFO; 3.0 beats 2.0 at 500 ms with 59.6 % (+68 Elo) |
| 3 The web app on Rust | done (2026-11-02) | The browser searches with the Rust engine in WebAssembly, node for node like 2.0; in Chromium 2.3–2.4× faster than the .NET AOT worker in the midgame and 1.59× on FFO #40–44 (1.86× overall); the worker starts in 92 ms instead of 680, Stop and Move Now answer in under 1 ms, and the first visit is 1.07 MB smaller |
| 4 Release (3.0 preview) | done (2026-11-02) | About shows 3.0; docs Part 1 describes the Rust engine and Part 3 begins (chapter 25); main = c9aa054, deploy 41504303682 succeeded; the live site plays, stops and solves on the Rust engine |
| 5 Threads and a shared hash table | done (2026-11-02) | A thread pool and a seqlocked shared hash table (64-byte slots); `Threads` from C# to Rust, the CLI and the Benchmark; threads = 1 identical to 2.0 on every parity run, at most +2.2 % slower; 2–16 threads give exact solves; no speed-up yet (phase 6) |
| 6 Lazy SMP | done (2026-11-02) | Helpers run their own iterative deepening 1 + ctz(i+1) plies ahead, and up to 3 help each solve pass; 4 threads beat 1 with 55.3 % (+37 Elo) and 8 threads with 53.0 % (+21) at 500 ms; time to depth 1.53× (4 threads) and 1.66–1.76× (8); FFO only about 10 % faster (YBWC is phase 7) |
| 7 YBWC | done (2026-11-03) | YBWC (after Egaroucid's ybwc.hpp) plus Lazy SMP on half the workers; FFO #40–49 1.98×/3.29×/3.97× faster with 4/8/16 threads (Egaroucid C++ 2.61×/4.15×/4.86×); depth 16 2.09× with 8 threads; at 8 threads beats Lazy SMP alone with 54.4 % at 500 ms and 54.7 % at 2 s |
| 8 Threads in the browser | done (2026-11-03) | A threaded WebAssembly package (pinned nightly, shared memory) whose threads are Web Workers, loaded only when more than 1 thread is asked for and the page is cross-origin isolated; in Chromium FFO #40–44 3.5× faster and midgame 1.93× with 8 threads, all exact; Move Now ≤ 7 ms; the single-threaded package is the fallback |
| 9 Threads setting and the 3.0 release | part A done (2026-11-03) | A Threads setting in both apps (Automatic: cores − 1 on the desktop, all in the browser); the book recalculation stays at 1 thread per worker (user); final matches wait for the user's go |

## Baseline (Stello 2.0, C#, 1 thread)

| What | Value | Source |
|---|---|---|
| FFO #40–49, exact | 46.8 s, 1.10 G nodes | [v2-final-solve-bench-40-49.json](benchmark-results/v2-final-solve-bench-40-49.json) |
| Midgame speed in matches, 500 ms | about 4.2–4.5 M nodes/s | [docs chapter 24](../docs/24-results-and-what-comes-next.md) |
| Browser, 22 empty squares | solved in 0.49 s (live check) | [improve-stello-strength-findings.md](improve-stello-strength-findings.md) |
| Book recalculation, 60 s per position, 8 workers | about 14 h (Ryzen 7 7800X3D) | [docs chapter 23](../docs/23-phase-5-the-search.md) |

## Phase 0: set-up and the frozen Stello 2.0 (2026-11-02)

### Toolchain
Installed in the sandbox (Ubuntu 26.04). The PATH is set in `/etc/sandbox-persistent.sh` via `~/.cargo/env`.

| Tool | Version | Used for |
|---|---|---|
| rustup, stable Rust | rustc 1.99.1, cargo 1.99.1 | everything |
| Targets | `x86_64-unknown-linux-gnu`, `x86_64-pc-windows-gnu`, `wasm32-unknown-unknown` | tools and tests, WPF, web |
| MinGW-w64 (apt `gcc-mingw-w64-x86-64`) | GCC 13 (win32 threads) | linker for the Windows DLL (`Stello.Rust/.cargo/config.toml`) |
| wasm-bindgen-cli | 0.2.130 (`cargo install --locked`) | JS glue for the WASM package; the crate is pinned to `=0.2.130` |
| Node | 22.22.1 (already installed) | quick WASM checks |

### Stello.Engine.V2
- **Contents:** a copy of `SearchEngine` (with `Endgame/`), `Search/`, `Evaluation/` and `ComputerPlayer`, in the namespaces `Stello.Engine.V2.*`. It has its own embedded `stello-eval.bin` (resource `Stello.Engine.V2.stello-eval.bin`).
- **Shared with `Stello.Engine`:** `Board`, `Move`, the limits, the results and the opening book. `Stello.Engine` gives it `InternalsVisibleTo`.
- **Match and benchmark keys:** `engine=stello-v2`, `--engine stello-v2` (`solve-bench`, `mid-bench`, with `--eval`), and `stello-v2` in `mid-quality --engines`. `V2MatchPlayer` plays from the current book.
- **Gate:** identical to today's engine. It visited the same nodes and found the same moves and scores on all 100 `mid-bench` positions at depth 10 (3,719,468 nodes) and on FFO #40–49 (1.10 G nodes).
- **Tests:** `StelloV2Tests`, with 105 recorded signatures (`Data/stello-v2-signatures.txt`): the 100 midgame positions at depth 10 and FFO #40–44 solved, each with move, score, kind, depth and nodes.
  - The frozen engine must keep matching them.
  - So must the engine being developed: once `Stello.Engine.SearchEngine` calls Rust, this is the parity test.
  - Engine tests: 469 (259 + 2 × 105), all green.

### Stello.Rust
- **Location:** a Cargo workspace (edition 2024, GPL-3.0-only, version 3.0.0) at the repository root.
- **Crates:** `stello-core` (the engine, empty so far), `stello-ffi` (the `cdylib` `stello_engine`), `stello-wasm` (wasm-bindgen) and `stello-cli` (the binary `stello`).
- **Release profile:** fat LTO, one codegen unit, `panic = "abort"`, so no panic unwinds into .NET or JavaScript.
- **Builds:**
  - Linux `libstello_engine.so`: 333 KB;
  - Windows `stello_engine.dll`: 324 KB. It imports only system DLLs (KERNEL32, msvcrt, ntdll, api-ms-win-core-synch), so no MinGW runtime has to ship;
  - `stello_wasm.wasm`: 36 KB, plus the wasm-bindgen package (`--target web`).
- **Checks:**
  - a .NET program with `[LibraryImport("stello_engine")]` calls `stello_version()` and gets 30000;
  - Node loads the WASM package and gets "3.0.0".
- **Not committed:** `target/` and `pkg/` (`.gitignore`).

### Noted
- The host's Visual Studio and the sandbox share the `obj/` folders. After a restore on Windows, the Linux build failed with "Unable to find fallback package folder 'C:\Program Files (x86)\…\NuGetPackages'". A `dotnet restore` here fixes it, and the reverse may happen on Windows.

## Phase 1, steps 1–2: board, rules and evaluation (2026-11-02)

### What was ported
About 1,780 lines in `Stello.Rust/stello-core/src/`. Each item names its C# original.
- **Board and rules:** `square.rs` (`Square`, `Move`), `board.rs` (`Player`, `Board`, perft) and `bits.rs` (`Bitboards`).
- **The endgame's move generator** (`endgame/bits.rs`, C# `EndgameBits`), ported now because the midgame ordering already uses its weighted moves and potential mobility.
- **Evaluation:** `eval/` holds the three pattern sets, symmetries, folding, the `stello-eval.bin` reader (gzip with `flate2`, pure Rust), `evaluate`, and the search's scale (`pattern_score`: × 100 / units, clamped to ±64 discs).
- **Default evaluation:** the shipped file is embedded with `include_bytes!`.

### Parity
- **Fixtures** (`stello-core/tests/data`, 1.5 MB, written by the new `Stello.Benchmark parity-data` command from `Stello.Engine.V2`):
  - 6,214 positions from 30 random games, `midgame.txt`, the FFO positions and edge cases, including 78 positions where the side to move must pass and 170 finished games;
  - for each position: legal moves for both sides, the potential mobilities, parity, stage, the evaluation, `PatternScore`, the endgame's ordering value, and for every move its flips and the evaluation after it (over 30,000 moves);
  - pattern checksums (indices, folding, weights).

  Regenerating gives byte-identical files.
- **`cargo test -p stello-core`:** 12 unit tests and 3 parity tests pass. Changing a single value made each parity test fail, so the tests catch differences.
- **Perft:** depths 1–8 from the start position are equal (390,216 at depth 8), as are 14 `midgame.txt` and FFO positions at depth 5.
- **The test split:** evaluated by both sides with identical checksums.

| Set | Positions | Checksum of the scores (FNV-1a) |
|---|---:|---|
| The trainer report's subset (20,000 per disc count) | 1,040,276 | `2228830d28881a49` |
| The whole test split | 9,973,199 | `d6d58ebf91e5794f` |

- **Other builds:** the WASM build gives the fixture's evaluations and the right perft(8) under Node.

### Speed (informational, 1 thread)
| | Rust | C# 2.0 |
|---|---:|---:|
| Evaluations per second, whole test split | 4.89 M | 3.67 M (1.33×) |
| Evaluations per second, report subset | 4.6–4.8 M | 3.3–3.6 M |
| perft(8) | 8.9–9.6 ms | 38–41 ms (4.2×) |
| perft(8), WASM in Node | 11 ms | — |

C#'s move generation is slower partly because `Bitboards.Flips` calls its eight `Ray` helpers without `AggressiveInlining`. The frozen engine stays as it is.

### Notes
- **The test-split numbers:** the plan's "1.04 M test positions" is the trainer report's subset. Both it and the whole split were checked.
- **Two move generators:** the C# has two with identical results, Stello's `Bitboards` for the midgame and Egaroucid's `EndgameBits` for the solver. Both are ported and both are checked.
- **Not yet tested:** the endgame's tables (flip neighbours, quadrants, stability thresholds) were copied by script and are untested until the endgame step's node parity.
- **Size in the browser:** the embedded evaluation makes the WASM module about 1.1 MB. Phase 3 may pass the bytes to `init` instead.

## Phase 1, steps 3 and 5: the midgame search, stopping and progress (2026-11-02)

### What was ported
About 1,900 lines in `stello-core/src/search/` (`engine.rs`, `limits.rs`, `options.rs`, `ordering.rs`, `table.rs`, `time.rs`) and `endgame/search.rs`.
- **The search itself:** iterative deepening with PVS at the root and in every node, game-over scores, `onlyMoves`, the single-move shortcut and passes.
- **Selectivity and ordering:** Multi-ProbCut with its error table, the evaluation ordering from look 4 with the response table, and midgame ETC.
- **Tables and options:** both transposition tables (two-entry slots, the same hash and replacement), the 2/3 time rule with `SoftTimePercent`, and the options (another evaluation, `Selective`, `ProbCutPercent`).
- **The solver's surroundings,** ported now because they live in C#'s `SearchEngine.cs`: `SolveRoot`, `SolveShallow`, `SolveLast2`, `SolveLast`, the 7-ply hand-over rule and the 4-look presearch in solve mode. Only the deeper solver remains a stub, `solve_child`, filled in by step 4.
- **Stopping and progress:**
  - abort checks at exactly C#'s node counts;
  - progress reports with 100 ms heartbeats;
  - cancel and Move Now through a `StopSignal` trait. `SearchSignals` holds two atomic flags that another thread can set; the browser can read a `SharedArrayBuffer` instead.
- **Structure:** `SearchEngine` keeps the state between searches. A `Searcher` holds one running search, and the endgame is added to it as another `impl` block.

### Parity
C#'s `Stello.Benchmark mid-bench --engine stello-v2 --compare <the Rust JSON>` reported "identical" (same nodes, moves and scores) in every run:

| Run | Searches | Nodes |
|---|---:|---:|
| `midgame.txt`, depth 10 | 100 | 3,719,468 |
| `midgame.txt`, depth 12 | 100 | 10,582,535 |
| 1,000 random positions, depth 8 (`Data/midgame-random.txt`, from `parity-data --midgame`) | 1,000 | 9,624,573 |
| `--selective off`, depth 8, `midgame.txt` and the random positions | 100 + 1,000 | |
| `--eval` with two grid models (`plus15` × 30 stages, `egaroucid16` × 30 stages), depth 10 | 2 × 100 | |
| `--probcut-pct 150`, depth 10 | 100 | |
| `--hash-bits 12`, depth 10 | 100 | |

- **Tests:** `cargo test` runs 36 tests, including:
  - 20 recorded 2.0 signatures;
  - the single-move shortcut, passes and `only_moves`;
  - progress and heartbeats;
  - the soft rule, with a clock the test moves by hand;
  - the hard limit;
  - cancel and Move Now from another thread;
  - `SolveShallow`/`SolveLast2`/`SolveLast` against a plain minimax on 300 random endings.
- **Lint and builds:** clippy (including wasm32) and `cargo fmt --check` are clean, and the Windows, WASM and .NET builds pass.

### Speed (1 thread, Release, fastest of 3 passes)
| | Rust | C# 2.0 | Rust faster by |
|---|---:|---:|---:|
| mid-bench depth 10 | 0.84 s, 4.45 M nodes/s | 1.06 s, 3.50 M nodes/s | 1.27× |
| mid-bench depth 12 | 2.35 s, 4.50 M nodes/s | 3.07 s, 3.45 M nodes/s | 1.31× |

The midgame is limited by the evaluation, which runs at 4.9 M evaluations per second. The plan's target is 1.5×, so speed work after parity starts there: index extraction, SIMD on the desktop, and incremental updates if they keep the nodes. Clearing the hash tables is a single `memset`, since an all-zero entry is the empty one.

### Notes from the C#
- **Where the solver code lives:** `SolveRoot`, `SolveShallow`, `SolveLast2`, `SolveLast` and `Quadrants` are in `SearchEngine.cs`, not in the endgame file.
- **Two abort-check schemes share one node counter:**
  - the midgame and `SolveShallow` check when `(++_nodes & 1023) == 0`;
  - `CountEndNode` checks at `_nextAbortCheck`.

  Leaf evaluations also count as nodes, so the midgame's checks fall irregularly but deterministically. Both schemes are kept exactly.
- **Ordering counts:** `OrderByEvaluation`'s evaluations count as nodes and evaluations.
- **Multi-ProbCut and the table:** its shallow search runs at the same ply, so it reads and writes the hash table.
- **Hash cutoffs are asymmetric:** an upper bound cuts if its value is below α (strictly), a lower bound if it is at least β.
- **The ProbCut flag:** `_inProbCut` is reset in a `finally`; Rust resets it on the stop path too.
- **Overflow:** the response table is an `int` that can overflow in very long searches. Rust uses wrapping addition, as C#'s unchecked arithmetic does.
- **The clock in the browser:** `std::time::Instant` is always zero on `wasm32-unknown-unknown`, so the web build must set its own clock (`performance.now()`).

## Phase 1, step 4: the endgame solver, and the phase 1 gate (2026-11-02)

### What was ported
About 1,480 lines in `stello-core/src/endgame/`:
- `search.rs`: `solve_child`, `EndWindow`, `EndDeep`, `EndLocal`, `EndSimple`, `EndFast`, the endgame ETC, the ordering with `OrderingValue`, and the stable-disc cutoff;
- `last.rs`: `Last4`–`Last1`;
- `local.rs`: the local table;
- `stable.rs`: the stable-disc edge tables, built once per process (about 2 ms).

`stello-cli solve-bench` writes the benchmark's JSON.

### Parity
C#'s `--compare` reported "identical" (same nodes, moves and scores) in every run:

| Run | Searches |
|---|---:|
| FFO #40–49, `solve-bench` | 10 (1,099,769,823 nodes) |
| Random endgames, 14–24 empty squares (`Data/endgame-random.txt`, from `parity-data --endgame`) | 500 (27.7 G nodes) |
| Random endgames, 2–13 empty squares (`endgame-random-low.txt`) | 1,000 |
| Random endgames with a pass within 4 plies (`endgame-random-pass.txt`) | 300 |
| The midgame's hand-over to the solver, `mid-bench` depth 12 and 14 on `midgame-handover.txt` (65 and 136 of them went through the win/loss/draw and exact passes) | 2 × 300 |
| Also `mid-bench` depth 8 and depth 3 on the endgame files | 300 + 1,000 |

- **The endgame tables:** the four copied in step 1 are equal to C#'s value for value.
- **Tests:** 40 Rust tests pass in debug and release, including the 5 FFO signatures, random endings against a plain minimax, and Move Now and cancel during the solve of FFO #49 from another thread.
- **In the browser's engine:** WebAssembly solves FFO #40 in 0.44 s under Node, with the same nodes.

### The phase 1 gate: passed
| Gate item | Result |
|---|---|
| Identical on `mid-bench` depth 10 and 12 | yes |
| Identical on FFO #40–49 | yes |
| Identical on 1,000 random positions at depth 8 | yes, plus 1,800 random endgames and the hand-over |
| Evaluation equal on the whole test split | yes, 9,973,199 positions |
| Speed measured | below |

### Speed (1 thread, Release)
| | Rust | C# 2.0 | Rust faster by |
|---|---:|---:|---:|
| FFO #40–49 | 34.0–34.4 s, 32.3 M nodes/s | 47.1–47.8 s, 23.3 M nodes/s | 1.37× |
| mid-bench depth 10 / 12 | 0.84 s / 2.35 s | 1.06 s / 3.07 s | 1.27× / 1.31× |
| Random endgames, 14–24 empty squares | 715 s | 929 s | 1.30× (rough: other jobs ran at the same time) |

This is a straight port with no speed work. The plan's phase 2 target is 1.5× natively, so the next work is profiling, starting with the evaluation, plus AVX2/BMI2 paths on the desktop, all with the nodes unchanged.

### Notes from the C#
- **Inconsistent evaluation counts:** `EndWindow`/`EndDeep` count an evaluation at a final score, but `EndLocal`, `EndSimple` and `EndFast` don't. `OrderingValue` counts a node without an abort check, and `Last4`–`Last1` count each pass level as a node. All are kept.
- **Sorting:** in `EndLocal`/`EndSimple`, moves already decided by the local table stay in the sort and can change the order of the rest. Kept.
- **A shift by −1:** `StableDiscs.PlayOnLine` shifts by −1 when place = 0. C# masks the shift count to 31; Rust does the same explicitly.

### Environment notes
- **Incremental builds:** Cargo's incremental builds fail on the host mount ("unable to copy .o"). Build with `CARGO_INCREMENTAL=0`.
- **`git stash`** is unreliable on the mount (a stash pop failed once with "unable to read tree", then worked).
- **A test fix:** two cancel tests measured their time from after the signalling thread started and failed in release builds (96 ms against an asserted 100 ms). The clock now starts before the thread.

## Phase 2, part 1: the wrapper (2026-11-02)

### Design
- **The C ABI** (`stello-ffi`, library `stello_engine`, 629 lines):
  - an engine handle (hash bits, the options, and optionally the bytes of another evaluation file);
  - a signals handle for cancel and Move Now;
  - `search` with the position, the limits, `onlyMoves`, a progress callback and an iteration-done callback, returning a status code;
  - `clear_hash` and `stello_version`.

  No panic can cross the boundary (`panic = "abort"`, and every argument is checked).
- **`Stello.Engine.SearchEngine`** (278 lines) is now a wrapper over a `SafeHandle`, using `[LibraryImport]` (`Native/StelloNative.cs`, 181 lines). It keeps the same API:
  - progress through an `[UnmanagedCallersOnly]` callback;
  - the cancellation tokens set the signals, and a cancelled search still throws `OperationCanceledException`;
  - exceptions thrown inside the progress or iteration callbacks reach the caller.
- **The C# search was removed from `Stello.Engine`:** the midgame part of `SearchEngine.cs`, `Endgame/` and `Search/{MoveOrdering,TimeControl,TranspositionTable}.cs`. `Stello.Engine.V2` keeps it. `Evaluation/`, `SearchOptions` and the board stay, for the trainer and the tools. The tests of the C# internals (`EndgameBitsTests`, `TranspositionTableTests`) now test V2.
- **MSBuild** (`Stello.Engine.csproj`, target `BuildStelloNative`):
  - Cargo runs before the build when the Rust sources or the evaluation file are newer than the library;
  - each OS has its own target folder (`Stello.Rust/target/linux`, `target/windows`);
  - Cargo is found on the PATH or in rustup's folder, and the build stops with a clear message if Cargo or the C++ linker is missing;
  - the library flows to the output of every project that references `Stello.Engine`: apps, tools and tests;
  - `-p:StelloCargoTarget=x86_64-pc-windows-gnu` cross-builds the DLL here as a check, and `StelloNativeEngine=false` skips Rust;
  - on Windows the MSVC DLL links the C runtime statically (`+crt-static`), so no Visual C++ redistributable is needed.
- **The web app** keeps the frozen V2 engine in its .NET worker until phase 3, so the live site is unchanged. Its build keeps the native library out of the browser output.
- **On the user's PC:** rustup with the MSVC toolchain and Visual Studio's C++ build tools. `cargo build --release -p stello-ffi --target-dir target\windows` works there.

### Checks
- **Tests:** `Stello.Engine.Tests` has 483 tests, all green. They include the 105 recorded 2.0 signatures through the wrapper and 14 new wrapper tests (versions, options, progress and iterations, exceptions in callbacks, cancel and Move Now, engines on several threads, rejected options). `Egaroucid.Engine.Tests` (34) and `cargo test` pass too.
- **Parity through the wrapper,** `--engine stello` against `--engine stello-v2 --compare`: identical in every run.

| Run | C# 2.0 | Rust through the wrapper | Faster by |
|---|---:|---:|---:|
| mid-bench depth 10 | 1.28 s | 0.93 s | 1.39× |
| mid-bench depth 12 | 3.50 s | 2.62 s | 1.34× |
| 1,000 random positions, depth 8 | 2.97 s | 2.36 s | 1.26× |
| FFO #40–49 | 48.9 s | 35.6 s | 1.37× |

- **The book recalculation:** the whole master book at `--depth 10` with 4 workers, using the C# engine (a worktree at 972d317) and then Rust. The books and the reports are identical (13,064 values changed). It took 50 s in C# and 44 s in Rust. The gain is smaller because each of the roughly 8,000 short searches starts with cleared hash tables and pays the call overhead.
- **Tools:** each ran on Rust: Benchmark `match` (`stello` against `stello-v2`), BookTool `match`, and the trainer's `mpc` (with too few positions to fit, as expected).
- **Builds:** the solution builds with 0 warnings. WPF gets `stello_engine.dll` (checked with the MinGW cross-build), and the web publish contains no native library.

## Phase 2, part 2: the speed pass (2026-11-02, 8220705)

Every change keeps the nodes identical. It was checked through the wrapper after each step, with `--compare` against the C# 2.0 results.

### Where the time went (perf)
- **Midgame:** the evaluation took 58 %. About a third of that was waiting on weight-table loads; the rest built the indices (16 symmetry transforms, then about 120 parts). After it came ordering at 10 % (mostly the two insertion sorts), `bits::flips` at 6 %, the TT store at 3.6 %, and the 48 MB memset at the start of each search.
- **Endgame:** `flips` took 45 %, because its four downward directions branched on a bit scan and mispredicted. Then came `end_simple` at 15 %, `last2` at 9 % and `legal_moves` at 8 %.

### Changes
1. **The evaluation reads each feature's squares straight from the board** (`eval/patterns.rs`), so the symmetry transforms are gone.
   - **BMI2:** one `pext` per side and feature plus a shared digit table, in a `#[target_feature(enable = "bmi2")]` function. It is chosen at run time by the new `cpu.rs`, which refuses AMD and Hygon before family 19h, where `pext` runs in microcode.
   - **Portable:** parts that gather squares from different columns with one multiply; WASM uses this path.
   - **Effect:** depth 10 0.84 → 0.49 s with `pext`; the portable path alone gives 0.84 → 0.70 s. Tests check both paths against `indices()` for all three pattern sets.
2. **Midgame flips** use the endgame's mask flips (C# walks each ray square by square): depth 10 −8 %, perft(9) 64 → 50 ms.
3. **The TT is cleared by generation.** A u16 in the entry's padding keeps it at 24 bytes. An entry from an older generation counts as empty, also for replacement, and the table is memset only when the counter wraps. Search time is the same; the wall time of 5 × 100 searches goes from 2.99 to 2.37 s.
4. **Branchless downward rays** in the endgame flips (`1<<63 >> lzcnt(x|1)`): FFO #40–44 −14 %, depth 10 −17 %.
5. **A rank sort** in `sort_descending`, with no data-dependent branches: depth 12 −3 %.
6. **Prefetch** of the hash slot at nodes that store but do not probe: depth 12 −3 %, random positions −10 %.
7. **`last_flip_count` from per-line tables** (row, column and both diagonals into a byte each, then an 8×256 count table), checked against `flips` on 20,000 boards × 64 squares: FFO −6 %.

**Tried and reverted:** `#[inline(always)]` on the endgame flips (8–10 % slower), one packed u64 per move for the insertion sort (8 % slower), and a 16-lane rank comparison (35 % slower, because it did not vectorize).

### Speed (one pinned CPU, fastest of 3 passes, same session)
| Run | C# 2.0 | Rust at 8fd3f98 | Rust now, wrapper | Faster than C# |
|---|---:|---:|---:|---:|
| mid-bench depth 10 | 1.074 s | 0.831 s | 0.351 s | 3.06× |
| mid-bench depth 12 | 3.077 s | 2.395 s | 1.062 s | 2.90× |
| 1,000 random positions, depth 8 | 2.590 s | 2.102 s | 0.920 s | 2.82× |
| FFO #40–49 | 45.7 s | 34.4 s | 26.1 s | 1.75× |
| Whole-book recalc, depth 10, 4 workers | 50 s | 47 s | 32 s | 1.56× |

- **The wrapper costs nothing measurable:** the CLI gives the same times.
- **This VM hides the P- and E-cores,** so the runs were pinned with `taskset`.
- **A recheck after the commit** (mid-bench depth 12, unpinned) gave the same result: identical, 1.074 s against 3.282 s.

### Parity, all identical against C# 2.0
- **Through the wrapper:** mid-bench depth 10 and 12, random positions at depth 8, FFO #40–49, `midgame-handover.txt` at depth 12 (300 searches), and `endgame-random.txt` (500 solves, 27.7 G nodes; 540 s against C#'s 974 s).
- **The portable code natively** (`stello-cli --cpu portable`): FFO #40–49, depth 12 and random positions at depth 8.
- **The book recalc:** books and reports byte-identical before and after the pass (13,064 values changed).
- **WASM under Node:** the same moves, scores and nodes as before the pass. Speed: mid-bench depth 10 1.42 → 0.86 s, FFO #40 0.335 → 0.255 s, FFO #41–42 1.20 → 0.98 s.
- **Tests and lint:** `cargo test` (28 unit tests plus parity and search) and the 483 engine tests pass; clippy and `cargo fmt` are clean; the WASM and windows-gnu builds pass.

### What more would take (none of it changes nodes)
- **Endgame** (now 38 % in flips): AVX2 flips and legal moves. Without an AVX2 build target, the recursive solver has to be compiled twice (a `#[target_feature]` copy and a portable one), a refactor of about 1,200 lines.
- **Midgame:** the evaluation now mostly waits on weight-table loads. Incremental indices through make and unmake would help, but are invasive.
- **WASM:** a `simd128` build, which needs WASM SIMD in the browser.

### The match and the phase 2 gate: passed
**3.0 (Rust, `engine=stello`) against 2.0 (C#, `engine=stello-v2`):** 500 ms per move, no books, 1,180 games from `starts.txt`. Full result: [match_v3p2-vs-v2-500ms](benchmark-results/match_v3p2-vs-v2-500ms.md).

| | Stello 2.0 (C#) | Stello 3.0 (Rust) |
|---|---:|---:|
| Points | 476.5 | **703.5 (59.6 %)** |
| Wins / draws / losses | +389 =175 −616 | +616 =175 −389 |
| Midgame depth (average) | 16.67 | 18.58 |
| First solve (empty squares) | 22.2 | 23.2 |
| Nodes per second | 4.25 M | 10.38 M |

- **Strength:** 3.0 scores 59.6 % (57.3–61.9 %), Elo +68 (+51 to +84). The same search is about two plies deeper in the same time, and starts solving one empty square earlier.
- **Speed in play:** 2.4× the nodes per second over whole games. The bench ratios are higher, because games include the endgame, where the gain is 1.75×.

| Gate item | Result |
|---|---|
| Tests pass on the wrapper | 483 engine tests, 34 Egaroucid tests, `cargo test` |
| ≥ 1.5× native speed (target) | 2.8–3.1× midgame, 1.75× FFO, 1.56× whole-book recalc |
| 500 ms match of 3.0 against 2.0 > 50 % | 59.6 % (57.3–61.9) |
| Book recalc sample gives identical values | the whole book at depth 10: identical books and reports, both before and after the speed pass |
| WPF builds | yes (`-p:EnableWindowsTargeting=true`, Windows DLL cross-built with MinGW), with `stello_engine.dll` in its output. The user tested it on Windows: built in VS Code with `dotnet run`, Cargo built the MSVC DLL, and the app plays, stops and solves |

**Environment note:** a cross-built DLL that Cargo had hard-linked into `target/.../release` gave "Stale file handle" on the mount. Deleting both links and rebuilding fixed it.

## Phase 3: the web app on Rust (2026-11-02, 458c50a)

### Design
- **`stello-wasm`** (wasm-bindgen 0.2.130, `--target web`) exports `init(evaluation?, hashBits)`, `search(request, host)` and `version()`:
  - `search` takes and returns the JSON of the web app's `EngineProtocol` (the same shapes). Rust reads and writes it (`protocol.rs`), because `JSON.parse` would round the 64-bit boards;
  - `host` is `{ progress(json), signal() }`. The engine reads `signal()` (an `Atomics.load` of the page's `SharedArrayBuffer`: 1 Move Now, 2 cancel) at every abort check, every 1,024 nodes. The .NET worker only read it in its progress reports;
  - the clock is `performance.now()`, imported and set with `SearchEngine::set_clock`;
  - progress is throttled as before: a new depth or best move at once, otherwise every 100 ms;
  - a request with other hash bits makes a new engine, as the .NET worker did.
- **`engine-worker.js`** loads the wasm-bindgen module. It has two commands, `init` and `search`, and needs no book.
- **`WebEngineHost`** looks up the book on the page with `ComputerPlayer.TryChooseBookMove`, the book step of `ChooseMove` moved out unchanged, so a book move no longer waits for the worker. Games are added to the book on the page too (`BookLearner.AddGame`), as in `LocalEngineHost`. The book tracker is reset when the hash bits change or the worker restarts, as the .NET worker's new `ComputerPlayer` did. The protocol, the cooperative stop and the 300 ms fallback are unchanged.
- **The evaluation is embedded** in the module (`include_bytes!`). The module is 1,194,262 bytes, 1,065,123 with brotli; 990,568 bytes of it are the gzip file, which brotli cannot shrink. A separate file would transfer the same plus a request, and start-up does not change. `init` still accepts the bytes of another file.
- **The build:** a `BuildStelloWasm` target in `Stello.Web.csproj` runs Cargo (`--target wasm32-unknown-unknown`, always release) and wasm-bindgen into `wwwroot/engine` (git-ignored) when the Rust sources are newer. It stops with a clear message if Cargo, the wasm32 target (checked in `rustc --print sysroot`) or wasm-bindgen is missing. `StelloWasmEngine=false` skips it.
- **The deploy workflow** installs Rust stable with the target, and the prebuilt wasm-bindgen 0.2.130 from its GitHub release. It publishes with `-p:StelloNativeEngine=false`, since the web app does not need the native library. Its Rust steps were run in a clean `ubuntu:24.04` container, and the module built there passes the Node tests.
- **The web app no longer references Stello.Engine.V2.**
- **Stello.Engine no longer embeds `stello-eval.bin`** (follow-up after 458c50a). Only the Benchmark's `parity-data` and the trainer's `mpc` evaluated with `EvalWeights.Default`; they load the file from their `Data` folder instead. `Stello.Engine.wasm` goes from 1,031,245 to 41,135 bytes with brotli.

### Gate P3 (Release AOT publish in headless Chromium, served with COOP/COEP and brotli)
Speed: the old build is b132d6f (the .NET AOT worker running Stello 2.0), and each search runs on a fresh engine.

| Run | .NET AOT worker | Rust worker | Faster by |
|---|---:|---:|---:|
| mid-bench depth 10, 100 positions (3.72 M nodes) | 2.149 s, 1.73 M nodes/s | 0.933 s, 3.99 M nodes/s | 2.30× |
| mid-bench depth 12, every 5th position (2.44 M nodes) | 1.389 s | 0.580 s | 2.40× |
| FFO #40–44 (92.5 M nodes) | 4.141 s, 22.3 M nodes/s | 2.609 s, 35.4 M nodes/s | 1.59× |
| All three | 7.68 s | 4.12 s | 1.86× |

| Gate item | Result |
|---|---|
| The Release build plays | a whole game, computer against computer, 1 s per move with the book (19 book moves, 41 searched); Add Game to Book saved the book; no page errors |
| Stop and Move Now within 100 ms | in the engine (signal to answer): 0.2–0.9 ms, against 6–38 ms for .NET. In the app (click until the board changes): Move Now 25–27 ms, cancel (New Game) 28–31 ms, against 34–57 ms |
| 22 empty squares solved | FFO #41 in 340 ms (.NET 569 ms); a 22-empty game position in the app in 0.26 s at 26.5 M nodes/s (.NET 0.49 s, 12.9 M nodes/s) |
| Browser speed ≥ 1.9× (target) | midgame 2.30–2.40×, FFO 1.59×, overall 1.86× |
| Worker start-up | 89–97 ms, against 658–713 ms for the second .NET runtime |
| Transferred size, first visit | 7,913,010 bytes (2.0) → 7,829,307 (458c50a) → 6,844,294 bytes without the evaluation in `Stello.Engine` |

- **Start-up, measured under Node:** compiling the module takes 3–7 ms. Decoding the evaluation takes about 40 ms, and the first growth of memory for the 48 MB of hash tables about 35 ms.
- **Parity:**
  - the browser engine matches the 2.0 signatures on all 100 depth-10 searches and the 5 solves;
  - the .NET and the Rust worker agree on all 125 searches;
  - the browser and native `stello mid-bench --depth 12` agree on 20 of 20.
- **Tests:**
  - 9 Node tests of the built package (`node --test Stello.Rust/stello-wasm/tests/engine.test.mjs`): signatures, progress throttling, time per move with the JavaScript clock, Move Now and cancel in the midgame and in a solve, other hash bits, rejected requests, the evaluation as bytes;
  - 3 Rust unit tests of the protocol;
  - 484 engine tests;
  - `cargo test`, clippy (also for wasm32) and `cargo fmt` are clean.

### The solver in WebAssembly (follow-up)
- **Profile** (`node --cpu-prof`, FFO #40–44, debug symbols):

  | Function | Share of the time |
  |---|---:|
  | `endgame::bits::flips` | 28 % |
  | `end_simple` | 17 % |
  | `end_fast` | 10 % |
  | `legal_moves` | 9 % |
  | `evaluate` (the ordering searches) | 9 % |
  | `last1`–`last3` | 13 % |

  The JavaScript clock and the signal reads take under 1 %.
- **WebAssembly is already close to native:** the solves take 2.59 s under Node, against 2.08 s natively (default code) and 2.17 s with `--cpu portable`, the same code WebAssembly runs. `i64.clz`, `ctz` and `popcnt` are single instructions in WebAssembly, and V8 compiles them to `lzcnt`/`tzcnt`/`popcnt`.
- **Why the target is out of reach:** .NET's AOT build is nearly as fast in the browser as C# natively (22.3 against 23.3 M nodes/s). So 1.9× in the browser would need the solver itself to get about 25 % faster than after phase 2's speed pass (1.75× natively).
- **Tried, nodes unchanged, none kept:**

  | Change | Browser (Node) | Native |
  |---|---|---|
  | A copy of `flips` for each square, with the rays as constants and the rays that cannot flip removed | 2.59 → 3.23 s | 2.08 → 2.54 s |
  | `#[inline(always)]` on `flips` (as in phase 2) | 2.59 → 2.77 s | 2.08 → 2.29 s |
  | Branchless upward rays | unchanged (LLVM already makes them branchless) | unchanged |
  | `opt-level` 2 | unchanged | — |
  | `opt-level` s | 15 % slower | — |
  | `wasm-opt -O3` | 0–4 % slower | — |
  | `simd128` | about 2 % faster | — |
  | `target-cpu=bleeding-edge` | about 3 % faster | — |

  `bleeding-edge` needs features that not every browser has.
- **What could still help:**
  - `simd128` (2 %). The .NET AOT build already requires WebAssembly SIMD, so this adds no new browser requirement;
  - a different flip algorithm, such as Edax's per-square `flip_carry` functions with outflank tables (about 1,500 lines);
  - SIMD flips for the four upward rays. WebAssembly SIMD has no per-lane variable shifts and no 64-bit `clz`, so the downward rays stay scalar.

## Phase 4: the 3.0 preview release (2026-11-02, c9aa054)

- **About** shows "Stello Version 3.0", and its text says the brain is written in Rust since 3.0.
- **The docs:**
  - Part 1 (01–15) describes the Rust engine: code links, names, constants and the 3.0 numbers, plus new sections on `pext`, the generation-cleared table, the flips, the stop signals and clock, and how each app gets the engine.
  - Part 3 begins with [chapter 25](../docs/25-phases-0-4-the-engine-in-rust.md). The glossary and references are now 26 and 27.
  - The link check finds 0 broken links in `docs/`, and all 46 Mermaid diagrams render in the docs viewer.
  - A new result file, [v3p2-solve-bench-40-49.json](benchmark-results/v3p2-solve-bench-40-49.json), has FFO #40–49 with 3.0 (26.1 s).
- **The release:** main was fast-forwarded to c9aa054. Deploy run 41504303682 succeeded, including the new Rust steps (rustup with wasm32, prebuilt wasm-bindgen 0.2.130).
- **The live check** (headless Chromium, polite-sky-0e9899910.3.azurestaticapps.net):
  - `crossOriginIsolated` is true, and `engine/stello_wasm_bg.wasm` and `stello_wasm.js` are served;
  - About shows 3.0, and chapter 25 opens in the docs viewer;
  - a whole game, computer against computer at 1 s per move with the book, gave 19 book moves and 41 searches. Add Game to Book saved 22,916 positions;
  - Move Now answered in 28–29 ms and New Game during a search in 29–33 ms, click to board;
  - a 22-empty position was solved in 0.26 s at 25.2 M nodes/s (2.0 in the browser: 0.49 s at 12.9 M);
  - there were no page errors.

## Phase 5: threads and a shared hash table (2026-11-02)

### The hash table: a seqlock per slot
- **The slot** is one 64-byte cache line: a sequence word plus the two entries. Each entry is three 64-bit words (own discs, opponent discs, and one packed word with value, depth, bound, move, tag and generation).
- **A store** takes the slot's lock with a compare-and-swap, keeps the old replacement rules, and releases with sequence + 2.
- **A probe** returns a miss if the slot is locked. On a hit it reads the sequence again, and a read that overlapped a write counts as a miss. So a torn entry is never taken for a valid one, and with 1 thread nothing changes.
- **The variants measured on 1 thread** (CLI, one pinned CPU, fastest of 3 passes; mid-bench depth 10 / depth 12 / random depth 8 / FFO #40–49):

  | Variant | Time | Cost |
  |---|---|---|
  | Unchanged (single-threaded) | 0.346 / 1.001 / 0.856 / 25.0 s | — |
  | **Seqlock, 64-byte slot (chosen)** | 0.345 / 1.007 / 0.855 / 24.9 s | 0–0.6 % |
  | Spin lock per slot on probe and store (Egaroucid) | 0.351 / 1.021 / 0.865 / 25.0 s | 1.5–2 % |
  | Hyatt XOR check, 48-byte slot | 0.341 / 1.008 / 0.848 / 25.1 s | about 0 %, but only probabilistic: two positions that differ in one disc's colour can let a torn read pass |

- **The cost is memory:** a third more, so the default 19 hash bits take 64 MB instead of 48. The Settings labels in both apps now say 4, 16, 64, 256 and 1,024 MB. The docs tables (chapters 07 and 15, the glossary) are updated with the 3.0 docs in phase 9.

### The thread pool (`search/pool.rs`), shaped like Egaroucid's `thread_pool.hpp`
- **The workers** are fixed, and each has its own state: the solver's local table and the move-ordering killers. The pool is started at the first search with N > 1 threads and kept for later searches with the same N.
- **Tasks run in a scope,** which, as with `std::thread::scope`, waits for them (also if its body panics) and passes a task's panic on. So tasks may borrow the position, the shared tables and stop flags.
- **`try_spawn`** hands a task to an idle worker, or refuses at once when all are busy. That is what YBWC needs (phase 7): split only where a thread is free.
- **A `Spawner` hook,** `std::thread` by default, is for Web Workers in phase 8. On wasm32 without atomics the engine always uses 1 thread and starts no thread.
- **The interim N-thread search:** N−1 helpers each run the same root search with the root moves rotated, into the shared tables. They stop when the main search ends (a drop guard makes sure of that even if it panics), and their nodes and evaluations are counted.
- **This is not faster:** FFO #40–49 takes 27–31 s against 25 s on 1 thread, and mid-bench depth 12 with 8 threads takes the same time as with 1 (1.03 against 1.11 s). Speed-up is the work of phases 6 and 7.

### `Threads` through every layer
- **Rust:** `SearchLimits::with_threads(n)`, 1–256, default 1.
- **FFI:** ABI version 2 adds `stello_engine_set_threads`. The version-1 functions are unchanged, and C# requires version 2.
- **C#:** `SearchLimits.Threads` (init, validated 1–256, default 1). The wrapper sets it only when it changes, and V1 and V2 ignore it.
- **Web:** `EngineProtocol.MoveRequest.Threads`. stello-wasm checks it and searches with 1 thread until phase 8.
- **Tools:** `stello-cli` and `Stello.Benchmark` `mid-bench`/`solve-bench --threads N`, the match side key `threads=` (engine=stello only), and the threads of each side in the match header.

### Gate P5: passed

| Gate | Result |
|---|---|
| threads = 1 identical | Identical to the V2 reference JSONs through the wrapper: mid-bench depth 10 and 12, random depth 8, FFO #40–49, handover (300) and random endgames (500); also with `--cpu portable`. Re-checked after the review: mid-bench depth 12 identical. The 105 signatures pass |
| N threads give the right results | FFO #40–49 with 2, 4, 8 and 16 threads, and 500 random endgames with 4 and 8 threads: every score exact. Midgame with 4–16 threads: no wrong score kind or depth, and the WLD and exact solves agree |
| Overhead on 1 thread ≤ 3 % | Through the wrapper, one pinned CPU, best of 2 interleaved rounds: depth 10 +1.5 %, depth 12 +0.7 %, random depth 8 +2.2 %, FFO −0.4 % |

- **The tests:** 66 `cargo test` tests, including a hammer test (8 threads on 1,024 slots, every field checked against its key; with the check disabled it fails 2 runs in 3), 8 threads solving different positions on one small table, FFO and random endings with 2–8 threads, and Move Now and cancel stopping 8 threads in under 200 ms. Also 487 engine tests, 34 Egaroucid tests and 10 Node tests.
- **Checked with Miri and ThreadSanitizer** (nightly, only for these checks): the table and pool tests pass under Miri, and the library and thread tests give no ThreadSanitizer reports.
- **Not verified:** Windows with MSVC (only the MinGW cross-build), and matches with threads.

### Notes for the next phases
- **Phase 6** replaces the helper closure with per-depth helper searches (depth + ctz(i+1), as in Egaroucid), started in a scope around each main iteration and stopped by its flag.
- **Matches with threads** must set `--workers` so that workers × threads ≤ cores, because the default number of workers is cores / 2.
- **The docs for the 3.0 release:** chapter 07 (slot layout and concurrency), 14 (ABI 2), and 06 and 08 (the shared table).

## Phase 6: Lazy SMP (2026-11-02)

### Design
- **Midgame:** one pool scope covers all the midgame iterations. Helper i (from 0) runs its own iterative deepening on the root, 1 + ctz(i+1) plies ahead of the main thread (+1, +2, +1, +3, +1, +2, +1). It reads the main depth from an atomic and jumps ahead when the main thread catches up, capped at the last midgame depth. Helpers use the main thread's selectivity and stop through a flag when the midgame iterations end (a drop guard, so also on a panic). Their nodes and evaluations are counted; the result is the main thread's.
- **Solve:** each solve pass (win/loss/draw, then exact) gets at most 3 helpers, which solve the same window with the root moves rotated into the shared endgame table. The ordering iterations in Solve mode run without helpers.
- **Few empty squares:** with too few empty squares for a midgame iteration no helper is started (a guard added in review, with a test: 1–10 empties, depth and time limits, 8 threads give the single-threaded score).
- **The helpers' response killers** are cleared at the start of each search. The phase 5 interim helpers are removed.
- **Not as planned:** the plan said "helpers at main depths up to 10", as in Egaroucid. Here they run at all midgame depths, because the limit gave no gain without YBWC: the deep iterations take nearly all the time, and Egaroucid relies on YBWC there.

### What was measured before choosing (CLI, mid-bench time to depth; 1 thread 3.03 s at depth 14)
| Scheme | 4 threads | 8 threads |
|---|---|---|
| Egaroucid's: per-iteration helpers at depth + ctz(i+1) with an MPC ladder, main depth ≤ 10 | 2.93–3.14 s (no gain) | |
| The same at all depths | 2.22 s | 2.27 s (1.35×) |
| Deeper-only, or rotated same-depth helpers | about 1.4× | about 1.4× |
| **Helpers with their own iterative deepening ahead (chosen)** | **2.00 s** | **1.83 s** (1.5–1.65×) |

Other ways of choosing how far ahead a helper goes were the same or worse.

- **The machine limits the scaling:** N single-threaded copies in parallel at depth 12 take 1.16 s alone, 1.31 s each with 4, 1.50 s with 8 and 2.03 s with 16 (12 %, 27 % and 75 % slower). The VM's P/E cores and SMT are the cause.
- **Endgame helpers** (FFO #40–47, 1 thread 10.1 s): 3 or 7 rotated helpers on the same window take 9.0–9.4 s with 4 threads and 6.5–6.7 s with 8. An exact window is no better. On FFO #40–49, 7 helpers made #49 take 15 s instead of 8.8 s by filling the table with less useful entries. With 8 threads: 0 helpers 26.1 s, 3 helpers 23.5 s with no position slower, 5 helpers 25.7 s. So 3 helpers.

### Numbers (through the wrapper, unpinned, machine idle)
Time to depth, the mean wall time of 3 mid-bench passes (100 positions):

| Threads | Depth 14 | Speed-up | Nodes/s | Depth 16 | Speed-up | Nodes/s |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 3.22 s | 1.00× | 9.9 M | 8.48 s | 1.00× | 9.5 M |
| 2 | 2.57 s | 1.25× | 18.0 M | 6.89 s | 1.23× | 17.7 M |
| 4 | 2.10 s | 1.53× | 34.0 M | 5.31 s | 1.60× | 33.7 M |
| 8 | 1.94 s | 1.66× | 58.0 M | 4.81 s | 1.76× | 57.9 M |
| 16 | 1.97 s | 1.63× | 83.7 M | 5.01 s | 1.69× | 81.5 M |

FFO #40–49: 1 thread 25.4 s (identical to 2.0), 4 threads 23.6 s, 8 threads 23.1 s, every score and move correct.

### Gate P6: passed
500 ms per move, 1,180 games, engine=stello on both sides, hash bits 19, no book ([4 threads](benchmark-results/match_v3p6-4t-vs-1t-500ms.md), [8 threads](benchmark-results/match_v3p6-8t-vs-1t-500ms.md)):

| Match | Games at a time | Score of N threads (95 %) | Elo | Midgame depth | Nodes/s | Wall time |
|---|---:|---|---|---|---|---:|
| 4 threads vs 1 | 5 | **55.3 %** (53.1–57.4), +552 =200 −428 | +37 (+22 to +52) | 19.41 vs 18.50 | 39.4 M vs 10.4 M | 50 min |
| 8 threads vs 1 | 2 | **53.0 %** (50.9–55.0), +524 =202 −454 | +21 (+6 to +35) | 20.20 vs 18.97 | 70.2 M vs 13.4 M | 2 h 04 min |

- **8 threads passes, but only just,** and scores below 4 threads. The two matches are not directly comparable: with only 2 games at a time, the 1-thread side ran faster (13.4 M against 10.4 M nodes/s). The confidence intervals overlap. Accepted, since the gate is a lower bound above 50 %. The real test of 8 threads comes with YBWC.
- **Parity with 1 thread:** mid-bench depth 12 and FFO identical to 2.0 (re-checked after the review).
- **Tests:** 69 `cargo test` tests (new: a time per move with 8 threads, the handover with 4 threads with Multi-ProbCut on and off, and few empty squares with 8 threads), 487 engine tests and 10 Node tests; clippy clean, also for wasm32. Miri and ThreadSanitizer were not re-run, because the pool and the table did not change.
- **Not verified:** Windows with MSVC, and matches at 2 s.

### Notes for phase 7
- The Lazy SMP helpers hold N−1 workers for the whole midgame, so YBWC tasks would find no idle worker. Phase 7 will probably cap the helpers by depth (as Egaroucid does: helpers up to depth 10, YBWC deeper) or give them fewer workers.
- In the solve, 4 of 8 threads are idle today, ready for YBWC.

## Phase 7: YBWC (2026-11-03)

### Design
- **The pool's `scope_polling`** waits for its tasks and calls a check about every millisecond while it waits. That way a thread waiting at a split still sees Move Now, cancel and its time limit.
- **`search/ybwc.rs`, after Egaroucid's `ybwc.hpp`:**
  - Split points form a chain, and a split-off search stops when any split point above it is stopped.
  - `Searcher::split`, called after the first child, gives every younger brother a null window at alpha. Each brother except the last goes to an idle worker if there is one, otherwise it is searched here.
  - A fail high sets the split's cut and stop flags. A brother at ≥ beta cuts the node off; the other fail highs are searched again with the full window, in order. Brothers that were stopped go into another round at the new alpha.
  - If tasks are missing without a cut, the stop came from above, and the split returns the right abort reason.
  - A split-off search copies the stage of the iteration and the time limit, and may split again.
- **Where nodes split:**
  - midgame `search`, at the second child, when the children are searched at depth ≥ 6 (Egaroucid's minimum), at PVS and null-window nodes;
  - endgame `end_window` and `end_deep`, when the children have ≥ 16 empty squares (15 and 16 measured best).
- **Lazy SMP together with YBWC:** half of the workers, rounded up, run Lazy SMP helpers (2 threads → 1, 4 → 2, 8 → 4, 16 → 8), and the rest are free for YBWC. The solve uses no Lazy SMP helpers when YBWC is on (3 solve helpers made FFO with 8 threads take 11.2 s instead of 8.0 s).
- **Not as planned:** the plan said "Lazy SMP at shallow main depths, as in Egaroucid". Helpers only up to depth 10 were slower here (8 threads, depth 14: 2.79 s against 1.82 s for Lazy SMP alone). YBWC alone is weak in this midgame (51k tasks averaging 415 nodes, workers 26 % busy; 8 threads at depth 14: 3.30 s, slower than 1 thread's 3.07 s). Half the workers as helpers at all depths, with YBWC on the rest, won.
- **An internal switch** (`SearchOptions.ybwc`, FFI `stello_engine_set_ybwc` in ABI 3, `stello-cli --ybwc off`, match key `ybwc=on|off`) gives exactly phase 6 with `off`. It is kept for later comparisons, like the `selective` switch.

### Choosing the number of Lazy SMP helpers (CLI, mean of 3 passes, depth 16, 1 thread about 8.1–8.5 s)
| Threads | Helpers | Time | Lazy SMP alone |
|---:|---|---:|---:|
| 2 | 0 / 1 | 7.13 / 6.66 s | |
| 4 | 1 / 2 | 5.08 / 4.89 s | 5.49 s |
| 8 | 3 / **4** / 5 | 4.02 / **3.82** / 4.08 s | 4.80 s |
| 16 | 4 / 8 / 12 | 4.92 / 4.96 / 5.26 s | 5.18 s |

At depth 17 with 8 threads: 4 helpers 6.39 s, 2 helpers 6.62 s, Lazy SMP alone 7.68 s, YBWC alone 9.69 s (1 thread 13.4 s).

### Numbers (through the wrapper, machine idle)
FFO #40–49, every score and move correct:

| Threads | Stello 3.0 | Speed-up | Lazy SMP alone | Egaroucid C++ (hash 21) | Speed-up |
|---:|---:|---:|---:|---:|---:|
| 1 | 25.6 s | 1.00× | | 11.46 s | 1.00× |
| 4 | 12.9 s | 1.98× | 24.3 s | 4.39 s | 2.61× |
| 8 | 7.8 s | 3.29× | 23.5 s | 2.76 s | 4.15× |
| 16 | 6.45 s | 3.97× | 23.7 s | 2.36 s | 4.86× |

The C# Egaroucid port is single-threaded (26.2 s), so it has no thread numbers.

Time to depth 16, mean wall time of 3 mid-bench passes:

| Threads | YBWC + Lazy SMP | Speed-up | Lazy SMP alone |
|---:|---:|---:|---:|
| 1 | 8.13 s | 1.00× | |
| 4 | 4.85 s | 1.68× | 5.28 s |
| 8 | 3.89 s | 2.09× | 4.72 s |
| 16 | 5.39 s | 1.51× | 4.97 s |

At 16 threads this VM saturates (16 single-threaded copies each run 75 % slower), and YBWC + Lazy SMP is slower than Lazy SMP alone there.

### Gate P7: passed
| Gate | Result |
|---|---|
| FFO #40–49 correct and faster with N threads | 1.98×, 3.29× and 3.97× with 4, 8 and 16 threads, all correct |
| 500 ms, 8 threads, YBWC + Lazy SMP against Lazy SMP alone: not worse | **54.4 %** (52.2–56.5), +30 Elo (+16 to +45), 1,180 games, 2 at a time; midgame depth 20.51 vs 19.89, first solve at 24.9 vs 23.6 empties; 2 h 02 min ([results](benchmark-results/match_v3p7-ybwc-vs-lazy-8t-500ms.md)) |
| 2 s, the same: better | **54.7 %** (52.0–57.3), +33 Elo (+14 to +51), **600 games** (300 starts), 2 at a time; depth 23.14 vs 22.26, first solve at 26.7 vs 25.3; 3 h 50 min ([results](benchmark-results/match_v3p7-ybwc-vs-lazy-8t-2s.md)) |

- **600 games at 2 s** instead of 1,180, because 1,180 would have taken about 8 hours. The lower bound is above 50 % anyway.
- **Review checks:** 500 random endgames with 8 and with 16 threads gave every score and move equal to 2.0 (180 s and 150 s, against 974 s for 2.0). The 300 handover positions with 8 threads gave no wrong score kind or depth. With 1 thread, mid-bench depth 12 and FFO #40–49 are identical to 2.0.
- **Tests:** 72 `cargo test` tests (new: split solves with 3, 8 and 16 threads on small tables, Multi-ProbCut on and off; Lazy SMP alone solves exactly; the polling scope; `set_ybwc`), 488 engine tests, 34 Egaroucid tests, 10 Node tests; clippy clean, also for wasm32.
- **Miri** passes the pool tests, including the polling scope (the split code is too slow under Miri). **ThreadSanitizer** found no reports. Only the time-per-move test fails under it, because of its slowdown (370 ms against 190). In release the worst overshoot at 150 ms with 8 threads was 4.9 ms.
- **Not verified:** Windows with MSVC.

### Not done, and ideas for later
- **No root splitting:** `search_root` and `solve_root` still go through the root moves one by one. Egaroucid splits at the root too, which could close part of the FFO gap to it (3.29× against 4.15× with 8 threads).
- **The thread that split is not interrupted:** when a task fails high, the brother this thread is searching itself runs to the end (found in review). Correct, but the work is wasted.
- **No heartbeats while waiting:** a thread waiting at a split sends no progress reports, so the apps may see a stale report for as long as the wait.
- **`end_deep`:** a stored best move that was searched but did not improve on best does not count as searched, so that node splits one move later. This costs only parallelism.
- **16 threads in the midgame** are slower than Lazy SMP alone on this VM. Measure on the 7800X3D (8 cores, 16 threads, no E-cores) before choosing the default Threads value in phase 9.

## Phase 8: threads in the browser (2026-11-03)

### Design
- **Our own small pool,** not wasm-bindgen-rayon:
  - the engine already has its pool and the `Spawner` hook, and Rayon would add two crates only to run long-lived bodies;
  - with ours, the engine worker decides when the Web Workers start (asynchronously, before a search), so Rust never waits for a worker the browser has not started yet;
  - it is about 70 lines of Rust and 60 of JavaScript.
- **Rust (`stello-wasm/src/threads.rs`, only with `target_feature = "atomics"`):** `web_spawner` hands the body to a Web Worker waiting in `poolWorker()`, or fails if none waits, and the pool then has fewer threads. It waits briefly (at most 200 ms) for a worker that is on its way back from the previous pool.
  - **Found in review:** that wait had no limit. If fewer Web Workers started than the pool asked for, it would have waited for the pool's own threads, which end only with the pool: the search would hang. Now it gives up after 200 ms.
- **JavaScript:**
  - `engine-worker.js` is still the search thread. At `init` it loads `../engine-mt` only if more than 1 thread is asked for, the page is `crossOriginIsolated` and `SharedArrayBuffer` exists; otherwise, or if the load fails, it loads `../engine`. Only the package used is downloaded.
  - In the threaded mode it compiles the module once and starts the pool workers (`engine-pool-worker.js`) at `init`. Each instantiates the module on the shared memory and waits for bodies with `Atomics.wait`, which a worker may do. The page's main thread never blocks.
  - Before each search the threads are capped at `navigator.hardwareConcurrency`.
- **The clock** is `performance.timeOrigin + performance.now()` in both builds, because `performance.now()` counts from each worker's own start and the threads compare their time with the search's start.
- **For testing until phase 9:** the page address query `?threads=N` (1–256), read by `WebEngineHost.ThreadsFromQuery`. The default is 1. `engine-client.js` `info()` returns `{ version, mode: 'threaded' | 'single', threads }`. Switching between 1 and more threads will need a worker restart in phase 9, because the package is chosen at `init`.
- **Shared memory** may grow to 2 GiB, so the 1,024 MB hash setting fits. Where a device cannot reserve that, the threaded load fails and the single-threaded package plays.

### Build
- **The MSBuild target `BuildStelloWasmThreads`** builds into `wwwroot/engine-mt` (git-ignored) with the nightly pinned by the property `StelloWasmNightly` (`nightly-2026-11-02`, not a rust-toolchain file, so the rest of the workspace stays on stable). It uses `-Z build-std=std,panic_abort` and `+atomics,+bulk-memory,+mutable-globals`, with shared, imported memory and its own target folder.
- **`StelloWasmThreads`:**
  - `auto` (the default): a missing nightly or rust-src gives a warning with the rustup command, and only the single-threaded engine is built. A Windows build without the nightly still works;
  - `required`: a missing nightly is an error (the deploy workflow);
  - `false`: no threaded build. `StelloWasmEngine=false` still skips everything.
  - The probe runs with `RUSTUP_AUTO_INSTALL=0`, so a build never downloads a toolchain.
- **The deploy workflow** installs the pinned nightly with rust-src and publishes with `-p:StelloWasmThreads=required`. It first runs on GitHub at the phase 9 release.

### Gate P8 (local): passed
Headless Chromium, Release AOT publish served with COOP/COEP, `hardwareConcurrency` 24; engine times:

| Threads | FFO #40–44 (exact) | Speed-up | Midgame depth 14, 20 positions | Speed-up | Move Now / cancel |
|---:|---:|---:|---:|---:|---|
| 1 (single-threaded package) | 2.87 s | 1.00× | 1.72 s | 1.00× | 0.1–1.2 ms |
| 2 | 1.93 s | 1.49× | 1.62 s | 1.06× | ≤ 2.1 ms |
| 4 | 1.24 s | 2.31× | 1.06 s | 1.62× | ≤ 1.5 ms |
| 8 | 0.82 s | 3.50× | 0.89 s | 1.93× | ≤ 6.5 ms |
| 24 | 0.89 s | 3.22× | 0.90 s | 1.91× | ≤ 15.6 ms |

- **Results:** every solve exact, and every midgame move legal and at depth 14.
- **The threaded package with 1 thread** searches the same nodes, 3–8 % slower.
- **Re-checked after the review fix:** 8 threads, FFO #40–44 exact, 0.82 s against 2.79 s (3.4×); Move Now and cancel ≤ 7 ms; no page errors.
- **In the UI with `?threads=8`:** the engine runs threaded and plays at 1 s per move at depth 25 and about 21 M nodes/s. Move Now takes 83 ms through the UI.
- **Fallback:**
  - served without COOP/COEP with `?threads=8`, it runs single-threaded, plays, and Move Now works;
  - with `engine-mt` missing, it runs single-threaded with a console warning.
- **First visit (brotli):** 6.85 MB with 1 thread (only `engine/`), and 6.88 MB with `?threads=8` (only `engine-mt/`).
- **Node tests:** `engine.test.mjs` 10/10 on the single-threaded package. The new `engine-mt.test.mjs`, with `worker_threads` pool workers, passes 5/5: 1-thread parity with the 2.0 signatures, FFO #40–44 exact with 4 threads, a legal midgame move, and Move Now and cancel under 30 ms.
- **Checks:** fmt clean; clippy clean natively, for wasm32 single-threaded and for wasm32 threaded (nightly, build-std); 72 `cargo test` tests; the .NET build has 0 warnings; 488 engine tests. The engine core did not change, so native parity holds.
- **Not verified:**
  - the Windows build with the nightly;
  - the CI workflow itself (it first runs at the release);
  - Firefox and Safari (nested workers and shared memory);
  - mobile devices;
  - the live check with threads (phase 9).

## Phase 9, part A: the Threads setting (2026-11-03)

### The setting
- **`GameSettings.Threads`**, stored with the other settings. 0 means Automatic, so the default follows the device the settings are used on. Old settings files read as Automatic.
- **`ThreadCounts(Device, Default)`:**
  - desktop: `Environment.ProcessorCount`, with Automatic = cores − 1 (at least 1);
  - browser: `navigator.hardwareConcurrency` for both (user decisions).
  - A chosen number is capped at the device's.
- **The UI:**
  - WPF: a "Threads" box in the Settings dialog ("Automatic (N threads)", then 1 up to the device's count, "of N on this computer");
  - web: the same choice in the settings form.
- **Desktop:** the threads reach the Rust engine through `SearchLimits.Threads` on the existing path (`MainViewModel` → `ComputerPlayer` → `SearchEngine`). Book learning stays on 1 thread.
- **Web:**
  - the worker starts with the saved setting, so the first search finds the right package loaded;
  - going between 1 thread and more restarts the worker, keeping the book tracker; another number above 1 does not;
  - `?threads=N` is removed.
- **The analysis panel** already shows nodes per second over all threads.
- **Checks:**
  - in headless Chromium, the setting at 1 runs the single-threaded package (3.7 M nodes/s), 8 runs the threaded one (22 M), and Automatic (24) gives 33 M. A reload keeps the setting, and there were no console errors;
  - the solution builds with 0 warnings, including WPF and Stello.Net.Tests (Windows-only, not run here; new tests for the setting and `ThreadCounts`);
  - 488 engine tests pass.
  - **WPF itself needs the user's check on Windows.**

### Threads in the tools (user decisions)
- **The book recalculation has no `--threads`.** It already searches many positions in parallel, one search per core, which scales almost perfectly. Threads within a search scale far less (midgame 1.7–2.1× and endgame about 3.3× at 8 threads). So at the same number of cores, 1 thread with more time per position always beats several threads. A measurement made before the decision confirms it (the whole master book at depth 14, 7,534 positions):

  | Workers × threads | Positions/min |
  |---|---:|
  | 24 × 1 | 11,191 |
  | 12 × 1 (the default) | 8,969 |
  | 12 × 2 | 6,831–8,056 |
  | 8 × 1 | 6,683 |
  | 6 × 4 | 5,993 |
  | 3 × 8 | 4,160 |
  | 2 × 12 | 3,129 |
  | 1 × 24 | 1,816 |

  The default stays at 12 workers. 24 × 1 was 25 % faster at depth 14, but with a time per position, hyperthreads and E-cores give each search less CPU.
- **Threads stay where engines are compared:** `Stello.Benchmark match` (`threads=`), `mid-bench`/`solve-bench --threads`, and `Stello.BookTool match --threads` (new: default 1, shown in the header).

## Phase 9, part B: the final matches (started 2026-11-03)

Each match starts on the user's go. They run one at a time on an otherwise idle machine, with no books, hash bits 19 and Stello 3.0 as side A.

| # | Match | Games | Stello 3.0 score (95 %) | Elo | W / D / L | Midgame depth | Nodes/s | Wall time |
|---|---|---:|---|---|---|---|---|---:|
| 1 | 1 thread vs Stello 2.0, 100 ms | 1,180 | **63.8 %** (61.5–66.1) | +99 (+81 to +116) | +682 =142 −356 | 15.76 vs 13.83 | 10.1 M vs 4.2 M | 4 min 38 s |
| 2 | 1 thread vs Stello 2.0, 500 ms | 1,180 | **61.1 %** (58.9–63.3) | +78 (+62 to +94) | +636 =169 −375 | 18.59 vs 16.70 | 10.4 M vs 4.3 M | 21 min 28 s |
| 3 | 1 thread vs Stello 2.0, 2 s | 600 | **61.7 %** (58.7–64.6) | +83 (+61 to +105) | +318 =104 −178 | 21.03 vs 19.20 | 10.8 M vs 4.6 M | 40 min 36 s |

| 4 | 1 thread vs Egaroucid port, 100 ms | 1,180 | **82.8 %** (80.9–84.7) | +273 (+251 to +297) | +925 =104 −151 | 15.73 vs 8.69 | 9.7 M vs 3.8 M | 5 min 14 s |
| 5 | 1 thread vs Egaroucid port, 500 ms | 1,180 | **65.2 %** (63.1–67.4) | +109 (+93 to +126) | +689 =161 −330 | 18.58 vs 13.42 | 10.3 M vs 4.2 M | 22 min 27 s |
| 6 | 1 thread vs Egaroucid port, 2 s | 600 | **45.7 %** (42.8–48.5) | −30 (−50 to −10) | +217 =114 −269 | 20.95 vs 21.45 | 10.7 M vs 5.0 M | 42 min 16 s |

- On 1 thread, 3.0 searches about 1.9 plies deeper than 2.0 at every time per move, and starts the exact solve one empty square earlier.
- The phase 2 match at 500 ms gave 59.6 % (57.3–61.9). The final 61.1 % lies within that interval; the single-threaded engine has not changed since.
- **Against the Egaroucid port** (C#, 1 thread, table bits 21; its resources are the ones in `Egaroucid-main/bin/resources`, as in the 2.0 matches), compared with 2.0's final matches:

  | Time per move | Stello 2.0 | Stello 3.0 | Gain |
  |---|---|---|---|
  | 100 ms | 75.6 % (+196 Elo) | 82.8 % (+273) | +77 Elo |
  | 500 ms | 58.8 % (+62) | 65.2 % (+109) | +47 Elo |
  | 2 s | 40.8 % (−64), 1,180 games | 45.7 % (−30), 600 games | +34 Elo |

  At short times the port reaches far less depth (8.7 at 100 ms), so Stello wins clearly. At 2 s the port's evaluation and deeper search (21.45 against 20.95 plies, at half the nodes per second) still win. The gap to the port is now 30 Elo instead of 64.
