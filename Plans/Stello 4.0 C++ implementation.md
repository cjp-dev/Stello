# Stello 4.0 C++ Implementation Plan

This plan follows [making-stello-3-plan.md](making-stello-3-plan.md) (Stello 3.0) and [making-stello-3-1-plan.md](making-stello-3-1-plan.md) / [making-stello-3-1-findings.md](making-stello-3-1-findings.md) (Stello 3.1, released on 8 November 2026). Accompanying measurements, compiler versions, parity gates, and benchmark tables will be recorded in [Stello 4.0 C++ findings.md](Stello%204.0%20C++%20findings.md) after every phase.

## Status

- **Part 1 (Phases 0–4):** Completed on 9 November 2026. All parity gates (P0–P3) and the controlled Rust vs. Clang vs. MSVC benchmark matrix (Gate P4) are recorded in [Stello 4.0 C++ findings.md](Stello%204.0%20C++%20findings.md).
- **Gate P4 Decision (user, 9 November 2026):** **Approved Option B — Proceed with Part 2 (Phases 5–7)** to integrate `Stello.Cpp` into `.NET` and `Stello.Web` as **Stello 4.0**, incorporating the Phase 4 tuning improvements (`STELLO_ABORT_EXCEPTIONS=ON` by default and disabling Clang's AVX-512 SLP vectorization in `endgame::bits::flips()`).

## Goal

Stello 3.1's game engine ([`Stello.Rust`](../Stello.Rust/Cargo.toml)) is ported **1-to-1 to modern C++20** (`Stello.Cpp`) to answer a concrete engineering question: **Is an identical Othello engine faster in C++ than in Rust?**

To make the comparison scientifically valid:
1. **Strict 1-to-1 Algorithmic & Node Parity with Stello 3.1:** For any given position and fixed search depth (or exact endgame solve) on 1 thread, the C++ engine must return **the exact same evaluation, best move, score kind, and node count** as `Stello.Rust` 3.1 (pinned by [`stello-v3-signatures.txt`](../Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt) and [`StelloV3BaselineTests.cs`](../Stello.Net/Stello.Engine.Tests/StelloV3BaselineTests.cs)).
2. **Multi-Threaded Parity:** The C++ engine ports the exact same 64-byte seqlocked transposition table (including 3.1's root-empties aging), scoped worker pool, Lazy SMP helper schedule, and YBWC split rules from `Stello.Rust` 3.1, preserving 1-thread node-for-node identity while allowing apples-to-apples multi-threaded speed comparisons at 4, 8, and 16 threads.
3. **Controlled Compiler & ISA Matrix:** On this machine (**AMD Ryzen 7 7800X3D**, 8 cores / 16 threads, Zen 4), we benchmark **Rust (`rustc` fat LTO)** head-to-head against both **Clang (`clang-cl` / `clang++` LTO — same LLVM backend as Rust)** and **MSVC (`cl.exe /O2 /GL /LTCG`)**, under both:
   - **Baseline `x86-64` + runtime BMI2 `pext` dispatch** (matching shipped Stello 3.1, plus `--cpu portable`), and
   - **Native Zen 4 (`-march=native` / `target-cpu=native` / `/arch:AVX2`)** on both Rust and C++.
4. **Two-Part Structure with a Go/No-Go Speed Gate:**
   - **Part 1 (Phases 0–4):** Standalone `Stello.Cpp` core library, unit/parity test suite, and `stello-cpp` benchmark CLI. Zero changes to `Stello.Net` or `Stello.Rust`. Ends with the full speed report.
   - **Part 2 (Phases 5–7, conditional):** Only if C++ proves meaningfully faster in Part 1 and the user approves proceeding after inspecting the Phase 4 report, integrate `Stello.Cpp` into `.NET` (`stello_engine.dll` / `libstello_engine.so`, ABI v3) and `Stello.Web` (Emscripten single-threaded and `-pthread` WebAssembly packages) as **Stello 4.0**.

```mermaid
flowchart LR
    P0["Phase 0: Toolchain & CMake Skeleton"] --> P1["Phase 1: Board, Bits & Pattern Eval"]
    P1 --> P2["Phase 2: 1-Thread Midgame & Endgame Solver (3.1)"]
    P2 --> P3["Phase 3: Seqlock TT, Lazy SMP & YBWC"]
    P3 --> P4["Phase 4: Rust vs Clang vs MSVC Benchmarks"]
    P4 --> Gate{"User Speed Gate:<br/>Inspect Phase 4 Report"}
    Gate -- "Approved -> Proceed to Stello 4.0" --> P5["Phase 5: C ABI (.NET / WPF / Tools)"]
    P5 --> P6["Phase 6: Emscripten WASM (Stello.Web)"]
    P6 --> P7["Phase 7: Docs & Stello 4.0 Release"]
    Gate -- "Stop -> Keep Stello 3.1 Rust" --> Archive["Record Findings & Conclude"]
```

## Decisions (user, 6–8 November 2026)

| Question | Decision |
|---|---|
| Baseline version | **Stello 3.1** (`Stello.Rust` 3.1.0), including all Phase 1 & Phase 2 search, time-control, table-aging, and unified node-counting fixes from [making-stello-3-1-plan.md](making-stello-3-1-plan.md). |
| Scope | Two-part plan: **Part 1** builds standalone `Stello.Cpp` (`stello_core` + `stello-cpp` CLI + parity tests) for head-to-head benchmarking with zero changes to `Stello.Net`/`Stello.Rust`; **Part 2** integrates `.NET` FFI and Emscripten WebAssembly as Stello 4.0 only if approved after Part 1. |
| Multi-threading | Port both in steps: first 1-to-1 single-threaded parity (exact evaluation, move, and node counts), then the 1-to-1 seqlock TT + scoped thread pool + Lazy SMP + YBWC so both 1-thread and 4/8/16-thread speeds are compared. |
| Compilers | Benchmark **both Clang (LLVM)** and **MSVC (`cl.exe`)** against `rustc` on Windows (installing LLVM in Phase 0). |
| CPU / ISA modes | Test **both** Baseline `x86-64` + runtime BMI2 `pext` dispatch (plus `--cpu portable`) AND Native Zen 4 (`-march=native` / `target-cpu=native` / `/arch:AVX2`) on both Rust and C++. |
| Part 2 decision gate | Pause after Phase 4 so the user inspects the full Phase 4 benchmark report directly before deciding whether to proceed to Part 2. **Approved on 9 November 2026 (Option B):** proceed with Part 2 (Phases 5–7), enabling `STELLO_ABORT_EXCEPTIONS=ON` by default and disabling Clang's AVX-512 SLP vectorization in `endgame::bits::flips()`. |
| Preserving Stello 3.x | `Stello.Benchmark` in 3.1 already supports loading any ABI-3 native library via `engine=stello-lib,lib=<path>,sha256=<hash>` ([`StelloLibraryMatchPlayer.cs`](../Stello.Net/tools/Stello.Benchmark/StelloLibraryMatchPlayer.cs)). In Part 2, we also preserve the Rust 3.1 library (`stello_engine_v3.dll` / `libstello_engine_v3.so` and `engine=stello-v3` alias) so timed matches can always be run between Stello 4.0 (C++) and Stello 3.1 (Rust). |

## The Engines (If Part 2 Proceeds)

| Name | Project | Match side | What it is |
|---|---|---|---|
| Stello 1 | `Stello.Engine.V1` | `engine=stello-v1` | The engine before 2.0, frozen |
| Stello 2.0 | `Stello.Engine.V2` | `engine=stello-v2` | The C# 2.0 engine, frozen |
| Stello 3.0 / 3.1 | `Stello.Rust` | `engine=stello-lib,lib=...,sha256=...` (plus `engine=stello-v3`) | Separate ABI-3 Rust builds (3.0 tag / 3.1 release) for matches |
| Stello 4.0 | `Stello.Cpp` + `Stello.Engine` | `engine=stello` | The C++20 engine behind the existing `StelloNative` C ABI |
| Egaroucid | `Egaroucid.Engine` | `engine=egaroucid` | The C# Egaroucid port (`time-strict=on` by default in matches) |

---

## Why C++ Could Differ from Rust (Hypotheses Isolated in Phase 4)

- **Abort Unwinding (`Result<T, Abort>` vs. Zero-Cost C++ Exceptions):** Because `Stello.Rust` compiles with `panic = "abort"`, every recursive call in [`engine.rs`](../Stello.Rust/stello-core/src/search/engine.rs), [`endgame/search.rs`](../Stello.Rust/stello-core/src/endgame/search.rs), and — since Stello 3.1's unified abort check — [`endgame/last.rs`](../Stello.Rust/stello-core/src/endgame/last.rs) (`search`, `evaluate_leaf`, `order_by_evaluation`, `ordering_value`, `end_window`, `end_deep`, `end_local`, `end_simple`, `end_fast`, `solve_shallow`, `solve_last2`, `last4`, `last3`, `last2`, `last1`) returns a tagged `Result<..., Abort>` and branches on `?` after every call. Indeed, [making-stello-3-1-findings.md](making-stello-3-1-findings.md) (Phase 2 Item 9) noted that adding `count_node()?` and `Result` propagation to `last4`–`last1` cost ~1.4% in FFO #40–49 solve time. In C++, we will first port the `Result<T, Abort>` return check 1-to-1 and then test a compile-time switch (`STELLO_ABORT_EXCEPTIONS`) using zero-cost table-driven `throw Abort` (with RAII guard for `in_probcut`), which keeps **exact node-for-node parity** while removing return-tag branches from the hot search and endgame recursion.
- **Slice Bounds Checks:** `Stello.Rust` uses `get_unchecked` in pattern weight sums, while array indexing in move ordering, local tables, and endgame tables still carries Rust bounds checks where LLVM cannot prove `< 64`.
- **Compiler Backends:** Comparing `rustc` vs. `clang++` isolates Rust-vs-C++ frontend/IR differences on LLVM; comparing `clang++` vs. `cl.exe` isolates LLVM vs. MSVC codegen on Zen 4.

---

## Proposed Changes

### Toolchain & Build Configuration

- **Folder Naming:** The repository root already contains [`Stello C++`](../Stello%20C++) (the original 1995–1998 MFC C++ 1.0 source). The new engine lives in **`Stello.Cpp/`**, matching `Stello.Net/` and `Stello.Rust/`.
- **Compilers on Windows (Ryzen 7 7800X3D):**
  - **MSVC (`cl.exe` 19.44):** Installed in Visual Studio 2022 BuildTools along with `cmake.exe` and `ninja.exe`.
  - **Clang/LLVM (`clang++.exe` / `clang-cl.exe`):** Install LLVM via `winget install -e --id LLVM.LLVM` in Phase 0, recording exact compiler versions (`rustc --version`, `clang++ --version`, `cl.exe`) in [Stello 4.0 C++ findings.md](Stello%204.0%20C++%20findings.md).
- **Zero External Dependencies:** `Stello.Rust/stello-core` depends only on `flate2` to decompress gzipped `stello-eval.bin`. `Stello.Cpp` includes a small (~250-line), self-contained RFC 1951/1952 DEFLATE/gzip decompressor (`eval/gzip.cpp`, supporting multi-member gzip streams, CRC-32, and ISIZE checks) so the entire engine builds with zero third-party packages on MSVC, Clang, Linux GCC/Clang, and Emscripten.
- **Embedded Evaluation File:** A CMake custom command converts [`Stello.Net/Book/stello-eval.bin`](../Stello.Net/Book/stello-eval.bin) into a generated source file (`embedded_eval.cpp` exposing `std::span<const uint8_t>`) in the build tree whenever `stello-eval.bin` changes — matching Rust's `include_bytes!`.

#### [MODIFY] [`.gitignore`](../.gitignore)
Add `Stello.Cpp/build/` and `Stello.Cpp/out/` so compiled binaries and CMake caches are never committed.

#### [NEW] `Stello.Cpp/CMakeLists.txt` & `Stello.Cpp/CMakePresets.json`
Define C++20 targets (`stello_core`, `stello_cli`, `stello_tests`, and in Part 2 `stello_ffi`) and Ninja build presets:

| Preset | Compiler | Optimization & LTO Flags | ISA Mode |
|---|---|---|---|
| `clang-release` | `clang-cl` / `clang++` | `-O3 -flto -DNDEBUG` | Baseline `x86-64` + runtime BMI2 `pext` |
| `clang-native` | `clang-cl` / `clang++` | `-O3 -flto -march=native -DNDEBUG` | Zen 4 native (`avx2`, `bmi2`, `lzcnt`, `popcnt`) |
| `msvc-release` | `cl.exe` | `/O2 /Ob3 /GL /Gw /DNDEBUG` + `/LTCG` | Baseline `x64` + runtime BMI2 `pext` |
| `msvc-native` | `cl.exe` | `/O2 /Ob3 /GL /Gw /arch:AVX2 /DNDEBUG` + `/LTCG` | AVX2 + BMI1/BMI2 + LZCNT/TZCNT |
| `clang-sanitized` | `clang++` | `-O1 -g -fsanitize=address,undefined` | Debug + ASan/UBSan parity check |

---

### Part 1 — `Stello.Cpp` Core Library (`stello_core`, 1-to-1 with Stello 3.1)

Every Rust module in [`Stello.Rust/stello-core/src/`](../Stello.Rust/stello-core/src/lib.rs) maps 1-to-1 to a C++20 header/source pair under `Stello.Cpp/src/core/` (namespace `stello`):

| Rust Source (`Stello.Rust/stello-core/src/`) | C++20 Target (`Stello.Cpp/src/core/`) | Key 1-to-1 Details (Including Stello 3.1 Updates) |
|---|---|---|
| [`square.rs`](../Stello.Rust/stello-core/src/square.rs) | `[NEW] square.hpp` | `Square` (`uint8_t`, 0–63), `Move` (pass = `-1` or `std::optional<Square>`), `SquareMask` bit-scan iterator (`std::countr_zero`). |
| [`bits.rs`](../Stello.Rust/stello-core/src/bits.rs) | `[NEW] bits.hpp` | `bits::legal_moves`, `bits::flips` (delegating to `endgame::bits::flips`), `bits::potential_mobility` (`std::popcount`). |
| [`board.rs`](../Stello.Rust/stello-core/src/board.rs) | `[NEW] board.hpp`, `board.cpp` | `Player` (`uint8_t`: `Black = 0`, `White = 1`), `Board` (`uint64_t black, white`), `parse`, `to_string`, `perft`. |
| [`cpu.rs`](../Stello.Rust/stello-core/src/cpu.rs) | `[NEW] cpu.hpp`, `cpu.cpp` | `std::atomic<uint8_t>` cached CPUID check (`__cpuid` / `__cpuidex`): enables `FAST_PEXT` when BMI2 bit (`EBX` bit 8 of leaf 7) is set and CPU is not pre-Zen-3 AMD/Hygon (`family < 0x19`); `set_portable_only(bool)` for `--cpu portable`. |
| *(crate `flate2`)* | `[NEW] eval/gzip.hpp`, `eval/gzip.cpp` | Zero-dependency RFC 1951/1952 multi-member gzip inflate + CRC-32/ISIZE check for `stello-eval.bin`. |
| [`eval/symmetry.rs`](../Stello.Rust/stello-core/src/eval/symmetry.rs) | `[NEW] eval/symmetry.hpp`, `eval/symmetry.cpp` | 8 board symmetries (`map_square`, `transform`). |
| [`eval/patterns.rs`](../Stello.Rust/stello-core/src/eval/patterns.rs) | `[NEW] eval/patterns.hpp`, `eval/patterns.cpp`, `eval/patterns_bmi2.cpp` | `PatternShape`, `PatternSet` (`classic11`, `plus15`, `egaroucid16`), `Direct` portable `sum_group<1..4>` multiply-gather, and `sum_pext` (`_pext_u64`). Isolating `sum_pext` in `patterns_bmi2.cpp` (compiled with `-mbmi2` on Clang and `_pext_u64` intrinsic on MSVC) ensures baseline builds never emit BMI2/AVX2 instructions outside `sum_pext`. |
| [`eval/weights.rs`](../Stello.Rust/stello-core/src/eval/weights.rs), [`eval/mod.rs`](../Stello.Rust/stello-core/src/eval/mod.rs) | `[NEW] eval/weights.hpp`, `eval/weights.cpp`, `eval/eval.hpp` | `EvalWeights` (`STELLOEV` v2 reader, delta-stage unfolding, embedded `built_in()`), `pattern_score`, `disc_score`. |
| [`endgame/bits.rs`](../Stello.Rust/stello-core/src/endgame/bits.rs) | `[NEW] endgame/bits.hpp` | `constexpr` `ALL_NEIGHBOURS` & `RAYS[64][8]`, branchless `flips` (`0x8000'0000'0000'0000ULL >> std::countl_zero((ray & not_opponent) | 1)`), Kogge-Stone `legal_moves`, `lone_empties`, `parity`, `weighted_moves`, `score_with_empties`. |
| [`endgame/local.rs`](../Stello.Rust/stello-core/src/endgame/local.rs) | `[NEW] endgame/local.hpp` | `LocalTable` (`7 * 1024` entries for 6–12 empties, exact multiplicative hash). |
| [`endgame/stable.rs`](../Stello.Rust/stello-core/src/endgame/stable.rs) | `[NEW] endgame/stable.hpp`, `endgame/stable.cpp` | One-time `std::once_flag` edge stability tables (`256 * 256` row & column), `stable::of`, `full_rows`, `full_columns` (`std::rotr`), `full_diagonals`. |
| [`endgame/last.rs`](../Stello.Rust/stello-core/src/endgame/last.rs) | `[NEW] endgame/last.cpp` | `Searcher::last4`, `last3`, `last2`, `last1`, and `last_flip_count` (`8 * 256` per-line table), with **3.1's `count_node()` abort check on every node/pass and consistent `evaluations += 1` on terminal scores**. |
| [`endgame/search.rs`](../Stello.Rust/stello-core/src/endgame/search.rs) | `[NEW] endgame/search.cpp` | `solve_child`, `end_null`, `end_window`, `end_deep`, `end_local`, `end_simple`, `end_fast`, `ordering_value`, `solve_shallow`, `solve_last2`, `solve_last`, plus **3.1's fixes**: `get_at_root`/`store_at_root`, `end_deep` ETC fail-low `best_move` retention and `hash_move_searched` split flag, `undone_first` excluding local-table-decided moves from the sort, and unified `count_node()` + terminal `evaluations += 1`. |
| [`search/limits.rs`](../Stello.Rust/stello-core/src/search/limits.rs), [`options.rs`](../Stello.Rust/stello-core/src/search/options.rs), [`time.rs`](../Stello.Rust/stello-core/src/search/time.rs) | `[NEW] search/limits.hpp`, `search/options.hpp`, `search/time.hpp` | `TimeControlMode`, `ScoreKind`, `SearchLimits`, `SearchInfo`, `SearchResult`, `SearchOptions`, `TimeBudget` (including **3.1's `with_soft_percent(percent, offset_ms)`, `for_endgame` offset by `elapsed_ms`, and `for_solver`**). |
| [`search/ordering.rs`](../Stello.Rust/stello-core/src/search/ordering.rs) | `[NEW] search/ordering.hpp`, `search/ordering.cpp` | `MoveOrdering`, dynamic corner `square_scores`, branchless rank-sort `sort_descending`. |
| [`search/table.rs`](../Stello.Rust/stello-core/src/search/table.rs) | `[NEW] search/table.hpp`, `search/table.cpp` | `alignas(64) Slot` (64-byte cache line: `std::atomic<uint64_t> sequence` + `std::atomic<uint64_t> words[6]`), acquire/release fences matching Rust's seqlock, 16-bit generation clear, **3.1's `get_at_root` / `store_at_root` aging (`entry.empty_count() <= root_empties`, overwriting stale slot 0 in place)**, `_mm_prefetch`, 64-byte aligned zeroed allocation. |
| [`search/pool.rs`](../Stello.Rust/stello-core/src/search/pool.rs) | `[NEW] search/pool.hpp` | `ThreadPool<ThreadState>`, `Scope`, lock-free `idle` fast-refusal in `try_spawn`, 1 ms `scope_polling`, pluggable `Spawner`. |
| [`search/ybwc.rs`](../Stello.Rust/stello-core/src/search/ybwc.rs) | `[NEW] search/ybwc.hpp`, `search/ybwc.cpp` | `SplitPoint` parent chain, `StopUnlessFinished` RAII guard, `Searcher::split<Child>`, `MidgameChild`. |
| [`search/engine.rs`](../Stello.Rust/stello-core/src/search/engine.rs) | `[NEW] search/engine.hpp`, `search/engine.cpp` | `SearchEngine`, `Searcher` (with `root_empties`), `SearchSignals`, PVS, **3.1's `Bound::Upper => value <= alpha` cutoff**, Multi-ProbCut error table, midgame ETC (`get_at_root`), `order_by_evaluation`, **3.1's `solve_root` stopped-pass `ScoreKind` handling**, and Lazy SMP helpers (`1 + countr_zero(i + 1)` plies ahead). |

#### Critical C++ Undefined-Behavior & Parity Guards

> [!WARNING]
> Even a single undefined behavior or subtle evaluation-order difference will break node-count parity under `-O3 -flto`. Every C++ translation unit must follow these five rules:

1. **Wrapping Signed Addition (`UB` in C++ vs. `wrapping_add` in Rust):**
   - `MoveOrdering::record_response` (`int32_t`) and `EvalWeights::read` (`int16_t`) use explicit unsigned arithmetic:
     ```cpp
     inline int32_t wrapping_add(int32_t a, int32_t b) noexcept {
         return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
     }
     ```
2. **Shift Counts Masked Explicitly (`UB` in C++ for `< 0` or `>= width`):**
   - In `stable::play_on_line`, when `place == 0`, `i` reaches `-1`; mask explicitly as `(p >> (i & 31)) & 1` (matching Rust line 104 of `stable.rs`).
   - In `ybwc::split`, handle `children.size() == 64` as `~uint64_t{0}` instead of `1ULL << 64`.
3. **Argument Evaluation Order in Recursive Calls:**
   - Rust evaluates function arguments strictly left-to-right; C++ argument evaluation order is unspecified. Never place two expressions with side effects (such as `ordering_value(...)` or `last1(...)` calls that increment `nodes` / `evaluations`) inside the same function argument list or `std::min`/`std::max` call — evaluate into a local variable first (e.g., in `last2`: `int32_t s = last1(...); v = std::min(v, s);`).
4. **Unified Stello 3.1 Node & Evaluation Counting:**
   - Every counted recursive node and explicit pass (`search`, `evaluate_leaf`, `order_by_evaluation`, `ordering_value`, `end_window`, `end_deep`, `end_local`, `end_simple`, `end_fast`, `solve_shallow`, `solve_last2`, and `last4`–`last1`) calls `Searcher::count_node()`, which increments `nodes` and calls `check_abort()` whenever `nodes >= next_abort_check` (advancing `next_abort_check = nodes + 1024`).
   - Every heuristic leaf evaluation (`evaluate_leaf`, `ordering_value` at `depth == 0`) and every terminal game-over score (`end_window`, `end_deep`, `end_local`, `end_simple`, `end_fast`, `ordering_value` double-pass, `solve_shallow`, `solve_last2`, `solve_last`, and `last4`–`last1` terminal paths) increments `evaluations += 1` in the exact same places as `Stello.Rust` 3.1.
5. **64-Byte Cache-Line Aligned Transposition Table:**
   - Allocate `Slot[]` via `::operator new[](count * sizeof(Slot), std::align_val_t{64})` and `std::memset` to 0 so every `Slot` sits on a single 64-byte hardware cache line on both MSVC and Clang.

---

### Part 1 — CLI Benchmark Tool & Parity Test Suite

#### [NEW] `Stello.Cpp/src/cli/main.cpp` & `Stello.Cpp/src/cli/bench.cpp`
1-to-1 port of [`Stello.Rust/stello-cli`](../Stello.Rust/stello-cli/src/main.rs), producing binary `stello-cpp.exe` with identical CLI syntax (including 3.1's `select_by_id_range` supporting FFO #50–59) and JSON output (`BenchRun`) so `Stello.Benchmark --compare` validates it directly:
- `stello-cpp version` (reports `3.1.0` / `30100`)
- `stello-cpp perft [--depth 8]`
- `stello-cpp eval-split [<folder>] [--max-per-discs 20000|all] [--eval <file>]`
- `stello-cpp solve-bench [--positions <file>] [--from 40] [--to 49] [--passes 1] [--hash-bits 19] [--eval <file>] [--out <file.json>] [--cpu auto|portable] [--threads N] [--ybwc on|off]`
- `stello-cpp mid-bench [--positions <file>] [--depth 10] [--passes 3] [--hash-bits 19] [--eval <file>] [--selective on|off] [--probcut-pct N] [--out <file.json>] [--cpu auto|portable] [--threads N] [--ybwc on|off]`

#### [NEW] `Stello.Cpp/tests/` (`test_main.cpp`, `parity_tests.cpp`, `search_tests.cpp`, `threads_tests.cpp`)
Reads the existing test fixtures directly from [`Stello.Rust/stello-core/tests/data/`](../Stello.Rust/stello-core/tests/parity.rs) and [`Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt`](../Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt) (no duplicate fixture files committed):
- `patterns.txt`, `perft.txt`, and all 6,214 positions / 30,000+ moves in `positions.txt`;
- All 105 recorded **Stello 3.1** [`stello-v3-signatures.txt`](../Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt) searches (100 midgame depth-10 + FFO #40–44), plus the 3.1 unit regressions (upper-bound equality cutoff, 34-ply persistent table aging = `2,156,127` nodes, `end_deep` ETC fail-low best-move retention, `undone_first` sort exclusion, `for_solver` time budgets, stopped-solve `ScoreKind`, and `count_node` abort checks in `last4`–`last1` / `ordering_value`);
- Seqlock 8-thread hammer test (1,000,000 rounds on 1,024 slots verifying zero torn reads), Move Now / Cancel latency tests, and 2/4/8/16-thread endgame exactness tests.

---

### Part 1 — Phases & Gates

1. **Phase 0 — Toolchain & Skeleton:**
   - Install LLVM (`clang++` / `clang-cl`) alongside VS 2022 BuildTools (`cl.exe`, `cmake`, `ninja`).
   - Create `Stello.Cpp/` CMake setup with all 5 presets and verify builds under both Clang and MSVC.
   - **Gate P0:** Both Clang and MSVC build `stello-cpp version` and pass an initial CTest smoke check.
2. **Phase 1 — Board, Bitboards, CPUID & Pattern Evaluation:**
   - Port `square`, `bits`, `board`, `cpu`, `endgame/bits`, `eval/gzip`, `eval/symmetry`, `eval/patterns` (portable + BMI2 `pext`), and `eval/weights`.
   - **Gate P1:** `perft(1..8)` equals `390,216` at depth 8; `parity_tests` passes 100% on `positions.txt` (6,214 positions), `patterns.txt`, and `perft.txt` in both `--cpu auto` and `--cpu portable`; `eval-split` matches FNV-1a checksums (`2228830d28881a49` on 1.04M subset; `d6d58ebf91e5794f` on full 9.97M split if training data is present).
3. **Phase 2 — Single-Threaded Midgame Search & Endgame Solver (Stello 3.1 Parity):**
   - Port `ordering`, `table`, `time`, `limits`, `options`, `search/engine`, `endgame/local`, `endgame/stable`, `endgame/last`, and `endgame/search`, plus `stello-cpp mid-bench` and `solve-bench`.
   - **Gate P2:** `Stello.Benchmark --engine stello --compare <cpp.json>` (comparing against `Stello.Rust` 3.1) reports **100% identical** moves, scores, and node counts on:
     - All 105 signatures in [`stello-v3-signatures.txt`](../Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt) (`3,716,389` midgame depth-10 nodes; `93,697,164` FFO #40–44 nodes);
     - `midgame.txt` at depth 10 (`3,716,389` nodes) and depth 12 (`10,579,515` nodes);
     - FFO #40–49 `solve-bench` (`1,115,666,701` nodes);
     - 1,000 random midgame positions at depth 8, 1,800 random endgame positions (2–13 empties, 14–24 empties, pass positions), and 300 `midgame-handover.txt` searches at depths 12 & 14;
     - 34-ply persistent hash-table aging test (`2,156,127` nodes at `hash_bits = 12`) and option variants (`--selective off`, `--probcut-pct 150`, `--hash-bits 12`, `--cpu portable`).
4. **Phase 3 — Seqlock TT, Thread Pool, Lazy SMP & YBWC:**
   - Port `search/pool` and `search/ybwc`, and wire `--threads N` and `--ybwc on|off`.
   - **Gate P3:** `threads = 1` remains 100% node-for-node identical to Gate P2; `clang-sanitized` (ASan + UBSan) passes all unit and multi-threaded tests with 0 warnings; FFO #40–49 and 500 random endgames solve with exact scores at 2, 4, 8, and 16 threads.
5. **Phase 4 — Head-to-Head Speed Benchmark & C++ Unwind Experiment (Ryzen 7 7800X3D):**
   - Also benchmark the compile-time `STELLO_ABORT_EXCEPTIONS` toggle (`Result<T, Abort>` return checks vs. zero-cost `throw Abort` unwinding, verifying node counts stay 100% identical across `mid-bench` and `solve-bench`, especially in `last4`–`last1` where 3.1 added `Result` propagation).
   - Run interleaved, idle-machine passes (fastest and mean of 3 passes) across:
     - **Engines/Compilers:** `Stello.Rust` 3.1 (`rustc` fat LTO) vs. `Stello.Cpp` (`clang++`/`clang-cl` LTO) vs. `Stello.Cpp` (`cl.exe` `/GL /LTCG`).
     - **CPU Modes:** Baseline `x86-64` + runtime `pext`, Baseline `--cpu portable`, and Native Zen 4 (`-march=native` / `target-cpu=native` / `/arch:AVX2`).
     - **Workloads:** `perft(8)` & `perft(9)`, `eval-split`, `mid-bench` depth 10 / 12 / 14 / 16, 1,000 random midgame depth 8, and `solve-bench` FFO #40–49 at **1, 4, 8, and 16 threads**.
   - **Gate P4 (User Decision Gate):** Record the complete tables in [Stello 4.0 C++ findings.md](Stello%204.0%20C++%20findings.md) and stop so the user inspects the Phase 4 benchmark report before deciding whether to proceed to Part 2.

---

### Part 2 (Approved — Option B) — Stello 4.0 Tuning & Integration

Executed following user approval of **Option B** after the Phase 4 benchmark report:

#### Phase 5 — Pre-FFI C++ Tuning, `.NET` FFI (`stello_engine.dll` / `libstello_engine.so`), `stello-v3` Preservation & Timed Matches

1. **Step 5.1 — Apply Phase 4 Tuning Recommendations in `Stello.Cpp`:**
   - **Default `STELLO_ABORT_EXCEPTIONS=ON`:** Flip `option(STELLO_ABORT_EXCEPTIONS ... ON)` in [`Stello.Cpp/CMakeLists.txt`](../Stello.Cpp/CMakeLists.txt) so all release/native/FFI/WASM builds use zero-cost table-driven C++ exception unwinding (`throw Abort`) for search aborts (`+5.6%` 1t and `+3.8%` to `+6.5%` 4t–16t FFO endgame speedup, `+0.9–6.7%` 1t midgame speedup, 100% node-for-node identical). Keep `STELLO_ABORT_EXCEPTIONS=OFF` covered by a test target or preset check.
   - **Prevent Clang AVX-512 (`zmm`) SLP Vectorization in `endgame::bits::flips()`:** In [`Stello.Cpp/src/endgame/bits.hpp`](../Stello.Cpp/src/endgame/bits.hpp), annotate both 4-iteration ray loops in `flips()` with `#if defined(__clang__)` `#pragma clang loop vectorize(disable)` (and `#pragma clang loop interleave(disable)` if needed). Verify via `llvm-objdump -d` on `clang-native` that `flips()` emits scalar GPR instructions (`lzcnt`, `blsr`, `tzcnt`) with `0` `zmm` instructions, and re-measure `perft(8..9)`, `mid-bench` d12/d16, and `solve-bench` FFO #40–49 (1t, 8t, 16t) in [Stello 4.0 C++ findings.md](Stello%204.0%20C++%20findings.md).
   - **Bump Version to `4.0.0` (`40000`):** Update `PROJECT_VERSION` in `Stello.Cpp/CMakeLists.txt` and `stello::VERSION` (`"4.0.0"`, `40000`) so Stello 4.0 identifies itself as `4.0.0` (`stello_version() == 40000`), while `ABI_VERSION` remains `3`.

2. **Step 5.2 — Implement C ABI Shared Library (`stello_ffi` -> `stello_engine.dll` / `libstello_engine.so`):**
   - `[NEW] Stello.Cpp/src/ffi/stello_ffi.hpp` & `Stello.Cpp/src/ffi/stello_ffi.cpp`: 1-to-1 port of [`Stello.Rust/stello-ffi/src/lib.rs`](../Stello.Rust/stello-ffi/src/lib.rs) implementing all 14 `extern "C"` functions and exact struct layouts expected by [`StelloNative.cs`](../Stello.Net/Stello.Engine/Native/StelloNative.cs):
     - `StelloSearchInfo` (`48` bytes, `offsetof(depth) == 24`) and `StelloSearchResult` (`40` bytes, `offsetof(mv) == 24`), verified by `static_assert`.
     - Exports: `stello_version` (`40000`), `stello_abi_version` (`3`), `stello_engine_create`, `stello_engine_destroy`, `stello_engine_clear_hash`, `stello_engine_set_threads`, `stello_engine_set_ybwc`, `stello_engine_set_iteration_done`, `stello_engine_search`, `stello_signals_create`, `stello_signals_destroy`, `stello_signals_cancel`, `stello_signals_move_now`, `stello_signals_reset`.
     - Every `extern "C"` entry point catches all C++ exceptions (`try { ... } catch (...) { return STELLO_INTERNAL_ERROR; }`) so no exception ever crosses the C ABI boundary.
   - `[NEW] Stello.Cpp/tests/ffi_tests.cpp`: Port all unit tests from `Stello.Rust/stello-ffi/src/lib.rs` (`version_and_layout`, `create_checks_its_arguments`, `search_checks_its_arguments_and_finds_a_move`, `threads_are_checked_and_searches_with_them_finish`, `callbacks_and_signals`) into `stello_tests`.
   - `[MODIFY] Stello.Cpp/CMakeLists.txt`: Add `SHARED` library target `stello_engine` (`OUTPUT_NAME "stello_engine"`) linking `stello_core`.

3. **Step 5.3 — `.NET` Integration (`Stello.Engine.csproj`) & `stello-v3` Preservation (`Stello.Benchmark`):**
   - `[MODIFY] Stello.Net/Stello.Engine/Stello.Engine.csproj`: Update `BuildStelloNative` to build `stello_engine` from `Stello.Cpp` via CMake + Ninja + Clang (`clang-release` preset by default, outputting to `Stello.Cpp/build/<os>-ffi/stello_engine.dll` or `libstello_engine.so`), copying it to the output directory just as the Cargo build did.
   - `[MODIFY] Stello.Net/tools/Stello.Benchmark/`: Preserve `Stello.Rust` 3.1 by adding an `engine=stello-v3` match engine option (backed by [`StelloLibraryMatchPlayer.cs`](../Stello.Net/tools/Stello.Benchmark/StelloLibraryMatchPlayer.cs) loading `stello_engine_v3.dll` / `libstello_engine_v3.so` built from `Stello.Rust/stello-ffi`, in addition to `engine=stello-lib,lib=...,sha256=...`).
   - `[MODIFY] Stello.Net/Stello.Engine.Tests/`: Update version assertions that check `3.1` / `30100` to `4.0` / `40000` while keeping all 105 [`stello-v3-signatures.txt`](../Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt) node-for-node baseline tests strict.
   - **Gate P5:**
     - All C++ tests (`ctest` across presets) and all .NET tests (`Stello.Engine.Tests`, `Egaroucid.Engine.Tests`, `Stello.Net.Tests`) pass with 0 warnings;
     - `Stello.Benchmark mid-bench` and `solve-bench` with `--engine stello` match `stello-v3` node-for-node on 1 thread;
     - Run a 1,180-game timed match at 500 ms per move (`engine=stello` C++ 4.0 vs. `engine=stello-v3` Rust 3.1, 1 thread per engine, 12 workers, `.games.jsonl` journal) and record the Elo / score interval and speed in [Stello 4.0 C++ findings.md](Stello%204.0%20C++%20findings.md).

#### Phase 6 — WebAssembly Engine (`Stello.Web`)

1. **Step 6.1 — C++ WebAssembly Module & Worker Glue (`Stello.Cpp/src/wasm/`):**
   - `[NEW] Stello.Cpp/src/wasm/protocol.hpp`, `protocol.cpp`, `threads.hpp`, `threads.cpp`, `wasm_exports.cpp`, and `stello_wasm.js`:
     - Port [`Stello.Rust/stello-wasm/src/lib.rs`](../Stello.Rust/stello-wasm/src/lib.rs), [`protocol.rs`](../Stello.Rust/stello-wasm/src/protocol.rs), and [`threads.rs`](../Stello.Rust/stello-wasm/src/threads.rs) 1-to-1.
     - Build two packages via Emscripten (`emcmake cmake` / `em++ -O3 -flto` — installing `emsdk` or using Clang WASM toolchain if needed):
       1. `Stello.Net/Stello.Web/wwwroot/engine/` (`stello_wasm.js` + `stello_wasm_bg.wasm`): single-threaded release build.
       2. `Stello.Net/Stello.Web/wwwroot/engine-mt/` (`stello_wasm.js` + `stello_wasm_bg.wasm`): multi-threaded release build with `-pthread` / `+atomics,+bulk-memory,+mutable-globals` and shared memory (up to 2 GiB), where pool Web Workers (`engine-pool-worker.js`) wait on `std::condition_variable` (`memory.atomic.wait32`) inside `poolWorker()` and `web_spawner` dispatches `ThreadPool` tasks to them.
     - Expose the exact ES module exports expected by [`engine-worker.js`](../Stello.Net/Stello.Web/wwwroot/js/engine-worker.js), [`engine-pool-worker.js`](../Stello.Net/Stello.Web/wwwroot/js/engine-pool-worker.js), [`engine.test.mjs`](../Stello.Rust/stello-wasm/tests/engine.test.mjs), and [`engine-mt.test.mjs`](../Stello.Rust/stello-wasm/tests/engine-mt.test.mjs): `default({ module_or_path })`, `initSync({ module, memory })`, `version()`, `init(evaluation, hashBits)`, `search(requestJson, host)`, `threaded()`, `idleThreads()`, and `poolWorker()`.

2. **Step 6.2 — Integrate into `Stello.Web.csproj`, Node Tests & CI Workflow:**
   - `[MODIFY] Stello.Net/Stello.Web/Stello.Web.csproj`: Update `BuildStelloWasm` and `BuildStelloWasmThreads` to build `Stello.Cpp` WASM targets into `wwwroot/engine` and `wwwroot/engine-mt`.
   - `[MODIFY] Stello.Rust/stello-wasm/tests/engine.test.mjs`: Update the version regex check from `/^3\.\d+\.\d+$/` to `/^4\.\d+\.\d+$/` (or Copy/Alias the test under `Stello.Cpp/tests/wasm/`).
   - `[MODIFY] .github/workflows/azure-static-web-apps.yml`: Add `Stello.Cpp/**` to trigger paths and configure the C++ WASM toolchain in CI.
   - **Gate P6:**
     - `node --test Stello.Rust/stello-wasm/tests/engine.test.mjs` and `node --test Stello.Rust/stello-wasm/tests/engine-mt.test.mjs` pass all 15/15 tests (including exact node-for-node parity on `stello-v3-signatures.txt`, 4-thread FFO #40–44 exact solves, and Move Now / cancel latency < 30 ms);
     - Benchmark C++ WASM vs. Rust 3.1 WASM (1-thread and 4-thread `mid-bench` d10/d12 and `FFO #40–44`) in Node / headless Chromium and record in [Stello 4.0 C++ findings.md](Stello%204.0%20C++%20findings.md).

#### Phase 7 — Documentation & Stello 4.0 Release

1. **Step 7.1 — Version Bump & App Verification:**
   - `[MODIFY] Stello.Net/Stello.App/AboutInfo.cs` (and corresponding unit tests): Update version display to `"Stello Version 4.0"`.
2. **Step 7.2 — Documentation (`docs/`):**
   - `[NEW] docs/28-stello-4-0-the-engine-in-cpp.md`: Write Chapter 28 covering:
     - The engineering question ("Is an identical Othello engine faster in C++20 than in Rust?");
     - The 1-to-1 C++20 architecture, zero-dependency gzip weight loader, and the 5 C++ undefined-behavior / evaluation-order guards;
     - The controlled Ryzen 7 7800X3D benchmark matrix (`rustc` vs. `clang++` vs. `cl.exe`, baseline vs. native, 1/4/8/16 threads);
     - What actually differed: why strict 1-to-1 LLVM codegen was tied in endgame (`+0.1%`), how C++ zero-cost `throw Abort` exceptions (`STELLO_ABORT_EXCEPTIONS=ON`) gained `+5.7%` in endgame solving, and how disassembly caught Clang's AVX-512 `zmm` SLP vectorization in `flips()`;
     - Native `.NET` FFI and WebAssembly speeds, and the 1,180-game timed match of Stello 4.0 (C++) against Stello 3.1 (Rust).
   - `[MODIFY] docs/28-glossary.md` -> `docs/29-glossary.md` and `docs/29-references.md` -> `docs/30-references.md`: Renumber Glossary and References to 29 and 30, and update links across `docs/README.md`, `docs/01-overview.md`, `docs/14-app-integration.md`, and `docs/27-stello-3-1-fixes-and-the-desktop-app.md`.
3. **Gate P7:** All C++, .NET (`Stello.Engine.Tests`, `Egaroucid.Engine.Tests`, `Stello.Net.Tests`), and Node WASM tests pass; `dotnet build -c Release Stello.Net/Stello.sln` succeeds with 0 warnings; findings and documentation are complete.

---

## Verification Plan

### Automated Tests & Parity Commands (Part 1)

```powershell
# 1. Build C++ engine & tests with Clang and MSVC
cmake --preset clang-release -S Stello.Cpp
cmake --build --preset clang-release
ctest --preset clang-release --output-on-failure

cmake --preset msvc-release -S Stello.Cpp
cmake --build --preset msvc-release
ctest --preset msvc-release --output-on-failure

# 2. AddressSanitizer + UndefinedBehaviorSanitizer check (0 UB, 0 races/leaks)
cmake --preset clang-sanitized -S Stello.Cpp
cmake --build --preset clang-sanitized
ctest --preset clang-sanitized --output-on-failure

# 3. Exact node-for-node parity against Stello 3.1 Rust (--engine stello)
Stello.Cpp\build\clang-release\stello-cpp.exe mid-bench --depth 10 --out scratch\cpp-mid10.json
dotnet run -c Release --project Stello.Net\tools\Stello.Benchmark -- mid-bench --engine stello --depth 10 --compare scratch\cpp-mid10.json

Stello.Cpp\build\clang-release\stello-cpp.exe mid-bench --depth 12 --out scratch\cpp-mid12.json
dotnet run -c Release --project Stello.Net\tools\Stello.Benchmark -- mid-bench --engine stello --depth 12 --compare scratch\cpp-mid12.json

Stello.Cpp\build\clang-release\stello-cpp.exe solve-bench --from 40 --to 49 --out scratch\cpp-ffo40-49.json
dotnet run -c Release --project Stello.Net\tools\Stello.Benchmark -- solve-bench --engine stello --from 40 --to 49 --compare scratch\cpp-ffo40-49.json
```

### Manual Verification

1. Review the **Phase 4 Benchmark Report** in [Stello 4.0 C++ findings.md](Stello%204.0%20C++%20findings.md) comparing Rust 3.1 vs. C++ (Clang) vs. C++ (MSVC) on the Ryzen 7 7800X3D across 1, 4, 8, and 16 threads, and decide whether to proceed to Part 2 (Stello 4.0 integration).
2. *(If Part 2 runs)* Launch the WPF desktop app (`dotnet run -c Release --project Stello.Net\Stello.Net`) and play/solve a game (including Computer vs Computer mode added in 3.1) to verify interactive responsiveness, Move Now, and thread scaling on Windows.

## Repository Rules

- **Nothing binary is committed:** `Stello.Cpp/build/`, `Stello.Cpp/out/`, `.dll`, `.so`, `.exe`, `.wasm`, and `Stello.Rust/target/` stay ignored.
- **Git stays under user control:** Commits and pushes are performed only when requested by the user.
