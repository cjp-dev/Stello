# Findings from making Stello 3.0

What the work on Stello 3.0 teaches us, written down as we go: the decisions, the measurements, what worked and what did not, and the ideas for later.

- **The plan:** [making-stello-3-plan.md](making-stello-3-plan.md). This file is updated after every phase.
- **Where it starts:** Stello 2.0 as released on 2 November 2026 ([docs chapter 24](../docs/24-results-and-what-comes-next.md)), and the C++/C# speed comparison ([cpp-vs-csharp-speed.md](cpp-vs-csharp-speed.md)).

## Status

| Phase | State | What it showed |
|---|---|---|
| 0 Set-up and the frozen Stello 2.0 | done (2026-11-02) | Stello.Engine.V2 searches node for node like 2.0; the Rust workspace builds for Linux, Windows and WASM, and .NET and Node call it |
| 1 The core in Rust | in progress | Steps 1–2 (board and rules, evaluation): identical to C# on 6,214 fixture positions, perft 1–8 and the whole test split; Rust perft 4.2× and evaluation 1.3–1.4× faster. Steps 3 and 5 (midgame search, stopping, progress): node-for-node identical in 9 mid-bench comparisons; 1.27–1.31× faster |
| 2 Desktop and tools on Rust | not started | |
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
