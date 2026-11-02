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
| 3 The web app on Rust | not started | |
| 4 Release (3.0 preview) | not started | |
| 5 Threads and a shared hash table | not started | |
| 6 Lazy SMP | not started | |
| 7 YBWC | not started | |
| 8 Threads in the browser | not started | |
| 9 Threads setting and the 3.0 release | not started | |

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
| WPF builds | yes (`-p:EnableWindowsTargeting=true`, Windows DLL cross-built with MinGW), with `stello_engine.dll` in its output; the user's own test on Windows is pending |

**Environment note:** a cross-built DLL that Cargo had hard-linked into `target/.../release` gave "Stale file handle" on the mount. Deleting both links and rebuilding fixed it.

