# Findings from implementing Stello 4.0 in C++20

This file records the toolchain setup, parity gates, undefined-behavior guards, and benchmark measurements for [Stello 4.0 C++ implementation.md](Stello%204.0%20C%2B%2B%20implementation.md).

- **Baseline target:** **Stello 3.1** (`Stello.Rust` v3.1.0, `stello_version() = 30100`, ABI v3, commit `868807a`).
- **Part 1 boundary:** `Stello.Cpp` stays completely standalone through Phase 4 (`Stello.Net` and `Stello.Rust` remain unmodified) until the Phase 4 benchmark report is inspected and approved.

## Status

| Phase | State | Summary |
|---|---|---|
| 0. Toolchain & CMake skeleton | done (2026-11-09) | `.gitignore` updated; `Stello.Cpp/` CMake + 5 presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`) build with 0 warnings and pass Gate P0 (`stello_tests`, `stello_cli_version`, LLVM 23.1.5 + MSVC 19.44) |
| 1. Board, bitboards, CPUID & pattern eval | done (2026-11-09) | All 5 presets pass 10/10 unit & parity tests (`positions.txt` 6,214 positions in both auto `pext` and `--cpu portable`, `patterns.txt`, `perft.txt`, `perft(8)=390,216`, multi-member gzip + CRC-32); baseline `clang-release` isolates `pextq` strictly to `Direct::sum_pext` (2 instructions binary-wide) |
| 2. 1-thread search & endgame solver (3.1 parity) | done (2026-11-09) | All 5 presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`) + `STELLO_ABORT_EXCEPTIONS=ON` pass 27/27 tests (all 105 `stello-v3-signatures.txt`, 34-ply aging `2,156,127`) with 0 warnings and exact node-for-node parity across >31.7B nodes / 3,600+ `Stello.Benchmark --compare` searches |
| 3. Seqlock TT, Lazy SMP & YBWC | done (2026-11-09) | All 5 presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`) + `STELLO_ABORT_EXCEPTIONS=ON` pass 35/35 tests (0 warnings, 0 ASan/UBSan findings); Gate P3 passes 1-thread node parity (`3,716,389` mid d10; `1,115,666,701` FFO #40–49), FFO #40–49 (2/4/8/16t + YBWC off + exceptions), and 500 random endgames (2/4/8/16t: 2,000 exact solves, 0 score/move diffs) |
| 4. Controlled Rust vs Clang vs MSVC benchmarks | done (2026-11-09) | Complete idle-machine matrix (1/4/8/16 threads, baseline/portable/native, `Result` vs `STELLO_ABORT_EXCEPTIONS=ON`, disassembly analysis); **Gate P4 approved (Option B — proceed to Part 2)** |
| 5. Pre-FFI tuning, `.NET` FFI & `stello-v3` timed match | ready | Default `STELLO_ABORT_EXCEPTIONS=ON`, disable Clang AVX-512 SLP in `flips()`, implement `stello_ffi`, wire `Stello.Engine.csproj` + `stello-v3`, run 1,180-game match |
| 6. Emscripten WebAssembly (`Stello.Web`) | pending | Single-threaded (`wwwroot/engine`) + `-pthread` (`wwwroot/engine-mt`) C++ WASM packages, 15/15 Node tests, browser speed comparison |
| 7. Documentation & Stello 4.0 release | pending | `AboutInfo.cs` 4.0, Chapter 28 (`28-stello-4-0-the-engine-in-cpp.md`), renumber Glossary/References to 29/30 |

## Hardware & Toolchain Environment (Windows x64)

- **CPU:** AMD Ryzen 7 7800X3D (8 cores / 16 logical processors, Zen 4, AVX2 + BMI2 + VPCLMULQDQ + fast 1-cycle `pext`)
- **Host OS:** Windows (x86_64)

| Tool | Version | Notes |
|---|---|---|
| Rust (`rustc` / `cargo`) | `1.99.1` (`58cf8f9c3` 2026-10-13) | Uses **LLVM 23.1.1**, `x86_64-pc-windows-msvc`, `+crt-static`, fat LTO, `codegen-units = 1`, `panic = "abort"` |
| Clang / LLVM | `23.1.5` (`392dfff9` 2026-11-03) | `C:\Program Files\LLVM\bin` (`clang++`, `lld-link`, `llvm-ar`); matches Rust's LLVM 23.1 backend |
| MSVC (`cl.exe`) | `19.44.35229` for x64 | VS 2022 BuildTools (`14.44.35207`) |
| CMake | `3.31.6-msvc6` | VS 2022 BuildTools CMake (`Ninja 1.12.1`) |
| .NET SDK | `10.0.401` | Used read-only in Part 1 to run `Stello.Benchmark --compare` |

## Stello 3.1 Single-Thread Parity Reference (`hash_bits = 19` unless noted)

| Workload | Positions | Stello 3.1 Nodes | Reference Source |
|---|---:|---:|---|
| `perft(8)` (start position) | 1 | 390,216 | `Stello.Engine.Tests` / `stello-core/tests/parity.rs` |
| `midgame.txt` depth 10 | 100 | 3,716,389 | [`stello-v3-signatures.txt`](../Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt) |
| `midgame.txt` depth 12 | 100 | 10,579,515 | [making-stello-3-1-findings.md](making-stello-3-1-findings.md) |
| `ffo-40-59.txt` #40–44 | 5 | 93,697,164 | [`stello-v3-signatures.txt`](../Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt) |
| `ffo-40-59.txt` #40–49 | 10 | 1,115,666,701 | [making-stello-3-1-findings.md](making-stello-3-1-findings.md) |
| 34-ply persistent aging test (`hash_bits = 12`) | 34 | 2,156,127 | `stello-core/tests/search.rs` |

## Phase 0: Toolchain & Skeleton (done 2026-11-09)

- **Static CRT parity:** `Stello.Rust/.cargo/config.toml` sets `+crt-static` on `x86_64-pc-windows-msvc`. `Stello.Cpp` configures `CMAKE_MSVC_RUNTIME_LIBRARY` to `MultiThreaded` (switched to `MultiThreadedDLL` when `STELLO_SANITIZE=ON`).
- **Windows ASan DLL note:** `vcvars64.bat` puts VS 2022 BuildTools' `clang_rt.asan_dynamic-x86_64.dll` on `PATH`, which caused `0xc0000139` entrypoint failures with binaries instrumented by LLVM 23.1.5. `Stello.Cpp/CMakeLists.txt` copies LLVM's own `clang_rt.asan_dynamic-x86_64.dll` next to each `STELLO_SANITIZE=ON` executable at post-build so `ctest` loads the matching runtime regardless of `PATH`.
- **Zero-dependency eval embedding:** `Stello.Cpp/cmake/embed_eval.cmake` converts `Stello.Net/Book/stello-eval.bin` (990,568 bytes) cleanly in CMake script mode (~2.5 s when the file changes, zero external dependencies).
- **Gate P0 passed:** All 5 presets (`clang-release`, `clang-native`, `clang-sanitized`, `msvc-release`, `msvc-native`) built with 0 warnings and passed `ctest` (`stello_tests`, `stello_cli_version` reporting `stello-cpp 3.1.0 (30100)`).

## Phase 1: Board, Bitboards, CPUID & Pattern Evaluation (done 2026-11-09)

- **Zero external dependencies:** `eval/gzip.cpp` implements self-contained RFC 1951/1952 multi-member DEFLATE/gzip decompression with CRC-32 and ISIZE verification (9-bit fast Huffman LUT + canonical slow-path fallback) and decompresses embedded `stello-eval.bin` (990,568 -> 4,673,605 bytes) cleanly.
- **Baseline BMI2 isolation:** `Direct::sum_pext` lives in `src/eval/patterns_bmi2.cpp` marked `STELLO_NOINLINE` and compiled with `-mbmi2` on Clang. Disassembly of `build/clang-release/stello-cpp.exe` with `llvm-objdump -d` confirms exactly **2 `pextq` instructions** binary-wide (one for `own`, one for `opponent`), both inside `Direct::sum_pext`, with zero BMI2/AVX2 instructions emitted elsewhere in engine code despite `-O3 -flto`.
- **Gate P1 passed across all 5 presets (0 compiler warnings, 0 ASan/UBSan findings):**
  - `stello_tests`: **10/10 passed** on `clang-release`, `clang-native`, `clang-sanitized`, `msvc-release`, and `msvc-native` (`positions.txt` 6,214 positions & 30,000+ moves in both auto `pext` and `--cpu portable`, `patterns.txt` FNV-1a checksums for `classic11`, `plus15`, `egaroucid16` & `EvalWeights::built_in()`, `perft(1..8)` = `4, 12, 56, 244, 1396, 8200, 55092, 390216`, and all `perft.txt` positions).
  - `eval-split` smoke check on synthetic 200-position `.bin` (`test-d04.bin` in scratch): `stello` (Rust 3.1) and `stello-cpp` (`auto` and `--cpu portable`) produced bit-identical `eval-fnv e4f01966697f2a3c`, `score-fnv e246efc37353d0b6`, and `sum 17991`. (`Traning data/stello-bin` is not present on this machine, so Phase 4 will check `eval` throughput on fixture/test data if unavailable.)

## Phase 2: Single-Threaded Search & Endgame Solver (done 2026-11-09)

- **Register-returned `AbortResult` & exception toggle:** Under `STELLO_ABORT_EXCEPTIONS=OFF`, `AbortResultBox<int32_t>` is a trivial 8-byte aggregate verified by `static_assert` so Clang and MSVC return it in `RAX` on Win64; `ProbCutGuard` provides RAII restoration for `in_probcut` under both `AbortResult` and `STELLO_ABORT_EXCEPTIONS=ON`.
- **MSVC `/W4` cleanups:** Out-of-line helpers called by tests (`Searcher::last1`, `evaluate_leaf`) omit `STELLO_FORCEINLINE` so external linkage is preserved while `-O3`/`/Ob3` inline within the TU, and `Slot` includes an explicit 8-byte tail field (`padding_`) to eliminate MSVC `warning C4324` (`alignas(64)` tail padding).
- **Gate P2 passed with 0 compiler warnings and 0 ASan/UBSan findings:**
  - All 5 presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`) and `clang` with `-DSTELLO_ABORT_EXCEPTIONS=ON` passed **27/27 tests** (`ctest`: `stello_tests`, `stello_cli_version`), including all 105 [`stello-v3-signatures.txt`](../Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt) checks (`3,716,389` midgame d10; `93,697,164` FFO #40–44), 34-ply persistent table aging (`2,156,127` nodes at `hash_bits = 12`), and 3.1 unit regressions.
  - `Stello.Benchmark --engine stello --compare` against `Stello.Rust` 3.1 reported **100% identical moves, scores, `ScoreKind`, and node counts** on every single Gate P2 workload:
    - `midgame.txt` d10 (`3,716,389` nodes, 100 pos), d12 (`10,579,515` nodes, 100 pos)
    - `midgame-random.txt` d8 (`9,620,457` nodes, 1,000 pos)
    - `midgame-handover.txt` d12 (`321,027,249` nodes, 300 pos) and d14 (`1,850,943,261` nodes, 300 pos)
    - `endgame-random-low.txt` (`7,387,890` nodes, 1,000 pos), `endgame-random.txt` (`27,900,685,006` nodes, 500 pos), `endgame-random-pass.txt` (`418,924,297` nodes, 300 pos)
    - FFO #40–49 `solve-bench` (`1,115,666,701` nodes, 10 pos)
    - Option variants (`--selective off`: `58,742,160`; `--probcut-pct 150`: `5,504,553`; `--hash-bits 12`: `3,780,521`; `--cpu portable`: `3,716,389`) and `STELLO_ABORT_EXCEPTIONS=ON` (d10: `3,716,389`; FFO #40–44: `93,697,164`).

## Phase 3: Seqlock TT, Thread Pool, Lazy SMP & YBWC (done 2026-11-09)

- **Scoped worker pool (`search/pool.hpp`):** Lock-free `idle` fast-refusal check in `Scope::try_spawn` avoids mutex traffic at YBWC split points when workers are busy; `WaitOnDrop` guarantees task destruction (ending all borrowed references) before `ScopeTasks::finish()`, and exceptions propagate back across `ThreadPool::scope` while keeping workers alive.
- **YBWC + Lazy SMP (`search/ybwc.cpp`, `search/engine.cpp`, `endgame/search.cpp`):** Exact port of Rust's `SplitPoint` parent chain, `StopUnlessFinished` RAII guard (clearing `stop` before `WaitOnDrop` waits), `MidgameChild` (`-alpha` full re-search), `EndgameChild` (`-max(alpha, null_score)` full re-search), Lazy SMP helper depth staggering (`1 + countr_zero(i + 1)`, halved when YBWC is active), and per-worker `endgame` (`LocalTable`) clearing in `clear_hash()`. Both `AbortResult` and `STELLO_ABORT_EXCEPTIONS=ON` catch/unwind cleanly inside worker lambdas and always call `helpers.finish()`.
- **Gate P3 passed (0 compiler warnings, 0 ASan/UBSan findings):**
  - All 5 presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`) plus `STELLO_ABORT_EXCEPTIONS=ON` built with **0 warnings** and passed **35/35 tests** (including 8-thread 1M-round seqlock hammer test, pool unit/exception tests, 8-thread Move Now / Cancel latency tests < 200 ms and 150 ms time-per-move budget tests, and multi-thread endgame/midgame/handover/Lazy-SMP-only checks).
  - **1-thread parity unchanged:** `mid-bench` d10 = `3,716,389` nodes; `solve-bench` FFO #40–49 = `1,115,666,701` nodes (`AbortResult` and `STELLO_ABORT_EXCEPTIONS=ON`), matching Gate P2 move-for-move and node-for-node.
  - **FFO #40–49 multi-threaded solve exactness (`clang-release`, 1 pass each):**
    - `threads = 2`: `1,297,507,279` nodes in `16.47 s` (`78.78M nps`) — 10/10 exact scores & best moves
    - `threads = 4`: `1,643,490,497` nodes in `11.00 s` (`149.41M nps`) — 10/10 exact scores & best moves
    - `threads = 8`: `1,622,320,337` nodes in `6.47 s` (`250.65M nps`) — 10/10 exact scores & best moves (vs `--ybwc off`: `3,856,958,251` nodes in `20.50 s`, `188.13M nps`)
    - `threads = 8` (`STELLO_ABORT_EXCEPTIONS=ON`): `1,610,343,464` nodes in `6.12 s` (`263.04M nps`) — 10/10 exact scores & best moves
    - `threads = 16`: `1,739,849,389` nodes in `5.57 s` (`312.13M nps`) — 10/10 exact scores & best moves
  - **500 random endgames (`endgame-random.txt`) vs 1-thread reference (`p2-end`):**
    - `threads = 2`: `31,819,602,938` nodes in `333.32 s` (`95.46M nps`) — 500/500 exact scores, kinds, depths & optimal moves
    - `threads = 4`: `37,097,754,205` nodes in `223.00 s` (`166.35M nps`) — 500/500 exact scores, kinds, depths & optimal moves
    - `threads = 8`: `40,329,354,359` nodes in `150.28 s` (`268.36M nps`) — 500/500 exact scores, kinds, depths & optimal moves
    - `threads = 16`: `43,949,062,392` nodes in `132.81 s` (`330.92M nps`) — 500/500 exact scores, kinds, depths & optimal moves

## Phase 4: Controlled Rust vs Clang vs MSVC Benchmarks (done 2026-11-09)

### Methodology & Tested Binaries

Ten standalone binaries (all untracked under `Stello.Cpp/build/` and `Stello.Cpp/out/`) were benchmarked on an idle Ryzen 7 7800X3D using interleaved passes (`5` passes for `perft` and `eval-split`, `3` passes per thread count for `mid-bench` and `solve-bench`), recording both fastest (`min`) and `mean` across passes:

| Config | Engine / Compiler | Flags & Abort Mode | Runtime CPU Path |
|---|---|---|---|
| `rust-base` | `Stello.Rust` (`rustc 1.99.1`, LLVM 23.1.1) | `--release --locked` (fat LTO, `cgu=1`, `panic=abort`, `+crt-static`), `Result` | Runtime BMI2 (`pext`) |
| `rust-native` | `Stello.Rust` (`rustc 1.99.1`, LLVM 23.1.1) | `RUSTFLAGS="-C target-feature=+crt-static -C target-cpu=native"`, `Result` | Zen 4 native |
| `clang-base` | `Stello.Cpp` (`clang++ 23.1.5`) | `-O3 -flto` (`lld-link`, `/MT`), `AbortResult` (`OFF`) | Runtime BMI2 (`pext` isolated in `patterns_bmi2.cpp`) |
| `clang-native` | `Stello.Cpp` (`clang++ 23.1.5`) | `-O3 -flto -march=native` (`/MT`), `AbortResult` (`OFF`) | Zen 4 native |
| `clang-base-exc` | `Stello.Cpp` (`clang++ 23.1.5`) | `-O3 -flto`, `STELLO_ABORT_EXCEPTIONS=ON` (`throw Abort`) | Runtime BMI2 (`pext`) |
| `clang-native-exc` | `Stello.Cpp` (`clang++ 23.1.5`) | `-O3 -flto -march=native`, `STELLO_ABORT_EXCEPTIONS=ON` | Zen 4 native |
| `msvc-base` | `Stello.Cpp` (`cl 19.44`) | `/O2 /Ob3 /GL /Gw /LTCG` (`/MT`), `AbortResult` (`OFF`) | Runtime BMI2 (`pext` isolated in `patterns_bmi2.cpp`) |
| `msvc-native` | `Stello.Cpp` (`cl 19.44`) | `/O2 /Ob3 /GL /Gw /arch:AVX2 /LTCG`, `AbortResult` (`OFF`) | x64 + AVX2 / BMI2 / POPCNT |
| `msvc-base-exc` | `Stello.Cpp` (`cl 19.44`) | `/O2 /Ob3 /GL /Gw /LTCG`, `STELLO_ABORT_EXCEPTIONS=ON` | Runtime BMI2 (`pext`) |
| `msvc-native-exc` | `Stello.Cpp` (`cl 19.44`) | `/O2 /Ob3 /GL /Gw /arch:AVX2 /LTCG`, `STELLO_ABORT_EXCEPTIONS=ON` | x64 + AVX2 / BMI2 / POPCNT |

Plus `--cpu portable` (`--cpu portable` CLI flag forcing scalar LUT bit-extraction) on `rust-base`, `clang-base`, and `msvc-base`.

- **`eval-split` dataset note:** `Traning data/stello-bin` is not present on this machine, so `eval-split` was measured on a deterministic `1,005,001`-position split across discs `8..60` (25,000 seeded random games stored in scratch). All configs returned identical FNV-1a checksums and sums.
- **Preflight & per-run node parity:** Every 1-thread search pass matched Stello 3.1 bit-for-bit and node-for-node (`perft(8)=390,216`, `perft(9)=3,005,320`, `rand d8=9,620,457`, `mid d10=3,716,389`, `mid d12=10,579,515`, `mid d14=29,779,568`, `mid d16=77,867,024`, `FFO #40–49=1,115,666,701`).
- **Percentage convention (`Δ`):** Computed on `mean` values: `(rust-base_mean / cfg_mean - 1)` for time benchmarks and `(cfg_mean / rust-base_mean - 1)` for throughput benchmarks (`eval-split`, `M nps`), so positive `+X%` always means faster.

### 1. Microbenchmarks: `perft(8)`, `perft(9)`, and `eval-split` (1,005,001 positions)

5 interleaved idle-machine passes per binary. All configs produced identical checksums on `eval-split` (`eval-fnv 11c6fe79fd476f4b`, `score-fnv b5d384fb3835cdda`, `sum 95729301`) and `perft(8) = 390,216`, `perft(9) = 3,005,320`.

| Config | `perft(8)` min / mean (ms) | `Δ` vs `rust-base` | `perft(9)` min / mean (ms) | `Δ` vs `rust-base` | `eval-split` max / mean (M pos/s) | `Δ` vs `rust-base` |
|---|---:|---:|---:|---:|---:|---:|
| `rust-base` | 4.20 / 4.26 | +0.0% | 30.90 / 31.06 | +0.0% | 15.89 / 15.42 | +0.0% |
| `rust-native` | 3.60 / 3.66 | +16.4% | 25.90 / 26.26 | +18.3% | 15.61 / 15.45 | +0.2% |
| `clang-base` | 4.50 / 4.58 | -7.0% | 33.70 / 33.88 | -8.3% | 16.52 / 16.25 | +5.4% |
| `clang-native` | 7.40 / 7.56 | -43.7% | 55.60 / 56.56 | -45.1% | 16.22 / 16.02 | +3.9% |
| `msvc-base` | 4.80 / 4.92 | -13.4% | 36.70 / 37.14 | -16.4% | 15.07 / 14.78 | -4.1% |
| `msvc-native` | 3.90 / 4.00 | +6.5% | 29.00 / 29.46 | +5.4% | 15.97 / 15.83 | +2.7% |
| `clang-base --cpu portable` | — | — | — | — | 7.15 / 6.99 | -54.7% |
| `msvc-base --cpu portable` | — | — | — | — | 6.23 / 6.20 | -59.8% |

### 2. Single-Threaded Search & Endgame Solver Matrix (`min / mean` seconds, `Δ` on mean vs `rust-base`)

3 interleaved passes per configuration (`--hash-bits 19`). Every run reported exact Stello 3.1 node parity: `rand d8 = 9,620,457`, `d10 = 3,716,389`, `d12 = 10,579,515`, `d14 = 29,779,568`, `d16 = 77,867,024`, `FFO #40–49 = 1,115,666,701`.

| Config | `rand d8` (`s`, `Δ`) | `mid d10` (`s`, `Δ`) | `mid d12` (`s`, `Δ`) | `mid d14` (`s`, `Δ`) | `mid d16` (`s`, `Δ`) | `FFO #40–49` (`s`, `Δ`) | `FFO` mean M nps |
|---|---:|---:|---:|---:|---:|---:|---:|
| `rust-base` | 0.905 / 0.915 (+0.0%) | 0.375 / 0.381 (+0.0%) | 1.034 / 1.068 (+0.0%) | 2.835 / 2.855 (+0.0%) | 7.441 / 7.542 (+0.0%) | 23.788 / 23.845 (+0.0%) | 46.79 |
| `rust-base --cpu portable` | 1.456 / 1.471 (-37.8%) | 0.575 / 0.578 (-34.2%) | 1.590 / 1.623 (-34.2%) | 4.543 / 4.615 (-38.1%) | 11.767 / 11.804 (-36.1%) | 25.160 / 25.198 (-5.4%) | 44.28 |
| `rust-native` | 0.918 / 0.925 (-1.0%) | 0.378 / 0.381 (-0.1%) | 1.038 / 1.043 (+2.4%) | 2.868 / 2.892 (-1.3%) | 7.441 / 7.521 (+0.3%) | 23.124 / 23.167 (+2.9%) | 48.16 |
| `clang-base` | 0.902 / 0.909 (+0.6%) | 0.364 / 0.367 (+3.8%) | 0.999 / 1.008 (+5.9%) | 2.813 / 2.831 (+0.9%) | 7.388 / 7.445 (+1.3%) | 23.665 / 23.823 (+0.1%) | 46.83 |
| `clang-base --cpu portable` | 1.457 / 1.473 (-37.9%) | 0.572 / 0.579 (-34.3%) | 1.609 / 1.629 (-34.5%) | 4.513 / 4.540 (-37.1%) | 11.896 / 12.151 (-37.9%) | 25.038 / 25.059 (-4.8%) | 44.52 |
| `clang-native` | 0.874 / 0.875 (+4.6%) | 0.346 / 0.350 (+8.7%) | 0.960 / 0.978 (+9.2%) | 2.670 / 2.689 (+6.2%) | 6.969 / 7.069 (+6.7%) | 24.062 / 24.379 (-2.2%) | 45.77 |
| `clang-base-exc` | 0.892 / 0.907 (+0.9%) | 0.356 / 0.361 (+5.3%) | 0.989 / 1.000 (+6.7%) | 2.810 / 2.837 (+0.6%) | 7.315 / 7.340 (+2.7%) | 22.552 / 22.566 (+5.7%) | 49.44 |
| `clang-native-exc` | 0.882 / 0.895 (+2.2%) | 0.352 / 0.355 (+7.3%) | 0.973 / 0.991 (+7.8%) | 2.737 / 2.774 (+2.9%) | 7.060 / 7.098 (+6.3%) | 23.037 / 23.145 (+3.0%) | 48.20 |
| `msvc-base` | 1.035 / 1.039 (-11.9%) | 0.412 / 0.414 (-8.1%) | 1.176 / 1.179 (-9.5%) | 3.264 / 3.342 (-14.6%) | 8.578 / 8.681 (-13.1%) | 28.795 / 28.849 (-17.3%) | 38.67 |
| `msvc-base --cpu portable` | 1.698 / 1.710 (-46.5%) | 0.667 / 0.671 (-43.3%) | 1.881 / 1.883 (-43.3%) | 5.317 / 5.357 (-46.7%) | 13.898 / 13.911 (-45.8%) | 30.545 / 30.676 (-22.3%) | 36.37 |
| `msvc-native` | 1.007 / 1.023 (-10.6%) | 0.404 / 0.408 (-6.8%) | 1.138 / 1.147 (-6.9%) | 3.177 / 3.337 (-14.4%) | 8.205 / 8.256 (-8.7%) | 25.623 / 25.728 (-7.3%) | 43.36 |
| `msvc-base-exc` | 0.998 / 1.011 (-9.5%) | 0.399 / 0.404 (-5.7%) | 1.145 / 1.151 (-7.3%) | 3.212 / 3.221 (-11.3%) | 8.285 / 8.391 (-10.1%) | 25.713 / 25.782 (-7.5%) | 43.27 |
| `msvc-native-exc` | 0.977 / 0.981 (-6.8%) | 0.390 / 0.394 (-3.4%) | 1.084 / 1.088 (-1.9%) | 3.072 / 3.081 (-7.3%) | 7.885 / 8.044 (-6.2%) | 22.489 / 22.548 (+5.8%) | 49.48 |

### 3. `STELLO_ABORT_EXCEPTIONS=ON` vs `OFF` (`AbortResult`) Pairwise Impact (1 Thread)

100% node-identical across both modes (`3,716,389` mid d10; `77,867,024` mid d16; `1,115,666,701` FFO #40–49).

| Compiler & Target | `rand d8` (`OFF -> ON`) | `mid d10` (`OFF -> ON`) | `mid d12` (`OFF -> ON`) | `mid d14` (`OFF -> ON`) | `mid d16` (`OFF -> ON`) | `FFO #40–49` (`OFF -> ON`) |
|---|---:|---:|---:|---:|---:|---:|
| Clang baseline (`clang-base`) | 0.909 -> 0.907s (+0.3%) | 0.367 -> 0.361s (+1.5%) | 1.008 -> 1.000s (+0.8%) | 2.831 -> 2.837s (-0.2%) | 7.445 -> 7.340s (+1.4%) | 23.823 -> 22.566s (+5.6%) |
| Clang Zen 4 (`clang-native`) | 0.875 -> 0.895s (-2.3%) | 0.350 -> 0.355s (-1.3%) | 0.978 -> 0.991s (-1.3%) | 2.689 -> 2.774s (-3.1%) | 7.069 -> 7.098s (-0.4%) | 24.379 -> 23.145s (+5.3%) |
| MSVC baseline (`msvc-base`) | 1.039 -> 1.011s (+2.8%) | 0.414 -> 0.404s (+2.6%) | 1.179 -> 1.151s (+2.4%) | 3.342 -> 3.221s (+3.8%) | 8.681 -> 8.391s (+3.5%) | 28.849 -> 25.782s (+11.9%) |
| MSVC AVX2 (`msvc-native`) | 1.023 -> 0.981s (+4.3%) | 0.408 -> 0.394s (+3.6%) | 1.147 -> 1.088s (+5.4%) | 3.337 -> 3.081s (+8.3%) | 8.256 -> 8.044s (+2.6%) | 25.728 -> 22.548s (+14.1%) |

### 4. Multi-Threaded Scaling (`4`, `8`, and `16` Threads)

Lazy SMP + YBWC node counts fluctuate slightly (~1–3%) between runs due to thread scheduling, so both wall-clock time (`min / mean`) and raw throughput (`M nodes/s`) are reported across 3 passes (`--hash-bits 19`).

#### 4a. Midgame `mid-bench --depth 16` (100 positions)

| Config | `4t` min / mean (`s`, `Δ`) | `4t` M nodes / M nps (`Δ`) | `8t` min / mean (`s`, `Δ`) | `8t` M nodes / M nps (`Δ`) | `16t` min / mean (`s`, `Δ`) | `16t` M nodes / M nps (`Δ`) |
|---|---:|---:|---:|---:|---:|---:|
| `rust-base` | 3.737 / 3.864 (+0.0%) | 132.7 / 34.33 (+0.0%) | 2.714 / 2.777 (+0.0%) | 164.0 / 59.05 (+0.0%) | 3.200 / 3.276 (+0.0%) | 240.2 / 73.35 (+0.0%) |
| `rust-native` | 3.796 / 3.829 (+0.9%) | 130.9 / 34.19 (-0.4%) | 2.774 / 2.841 (-2.2%) | 165.1 / 58.13 (-1.6%) | 3.008 / 3.145 (+4.2%) | 241.9 / 76.92 (+4.9%) |
| `clang-base` | 3.726 / 3.783 (+2.2%) | 130.7 / 34.55 (+0.7%) | 2.863 / 2.869 (-3.2%) | 167.6 / 58.43 (-1.1%) | 3.136 / 3.275 (+0.0%) | 241.2 / 73.63 (+0.4%) |
| `clang-native` | 3.738 / 3.768 (+2.6%) | 134.0 / 35.56 (+3.6%) | 2.683 / 2.761 (+0.6%) | 168.4 / 60.98 (+3.3%) | 2.794 / 3.069 (+6.7%) | 237.1 / 77.22 (+5.3%) |
| `clang-base-exc` | 3.776 / 3.810 (+1.4%) | 132.3 / 34.71 (+1.1%) | 2.725 / 2.868 (-3.2%) | 171.8 / 59.89 (+1.4%) | 3.025 / 3.185 (+2.9%) | 236.7 / 74.33 (+1.3%) |
| `clang-native-exc` | 3.549 / 3.641 (+6.1%) | 128.1 / 35.18 (+2.5%) | 2.603 / 2.794 (-0.6%) | 165.1 / 59.14 (+0.1%) | 3.076 / 3.189 (+2.7%) | 248.9 / 78.07 (+6.4%) |
| `msvc-base` | 4.297 / 4.481 (-13.8%) | 130.4 / 29.11 (-15.2%) | 3.230 / 3.355 (-17.2%) | 168.4 / 50.20 (-15.0%) | 3.420 / 3.807 (-13.9%) | 250.7 / 65.83 (-10.3%) |
| `msvc-native` | 4.310 / 4.360 (-11.4%) | 131.0 / 30.05 (-12.5%) | 3.159 / 3.226 (-13.9%) | 167.1 / 51.81 (-12.3%) | 3.318 / 3.454 (-5.1%) | 234.0 / 67.76 (-7.6%) |
| `msvc-native-exc` | 4.060 / 4.137 (-6.6%) | 130.7 / 31.59 (-8.0%) | 2.914 / 3.030 (-8.3%) | 164.6 / 54.34 (-8.0%) | 3.524 / 3.545 (-7.6%) | 246.1 / 69.41 (-5.4%) |

#### 4b. Midgame Throughput Across Depths (`rand d8`, `d10`, `d12`, `d14`, `M nodes/s` mean & `%` vs `rust-base`)

| Threads & Workload | `rust-base` | `rust-native` | `clang-base` | `clang-native` | `clang-base-exc` | `clang-native-exc` | `msvc-base` | `msvc-native` | `msvc-native-exc` |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 4t `rand d8` | 29.43 | 29.03 (-1.4%) | 29.54 (+0.3%) | 30.01 (+1.9%) | 29.54 (+0.3%) | 29.92 (+1.7%) | 25.54 (-13.2%) | 25.97 (-11.8%) | 26.91 (-8.6%) |
| 4t `d10` | 29.70 | 29.12 (-1.9%) | 29.97 (+0.9%) | 30.83 (+3.8%) | 30.80 (+3.7%) | 31.09 (+4.7%) | 26.32 (-11.4%) | 26.69 (-10.1%) | 27.65 (-6.9%) |
| 4t `d12` | 32.30 | 31.01 (-4.0%) | 32.60 (+0.9%) | 33.42 (+3.5%) | 32.84 (+1.7%) | 33.03 (+2.3%) | 27.39 (-15.2%) | 28.28 (-12.4%) | 29.42 (-8.9%) |
| 4t `d14` | 33.50 | 32.82 (-2.0%) | 32.74 (-2.3%) | 34.05 (+1.6%) | 33.33 (-0.5%) | 33.98 (+1.4%) | 28.14 (-16.0%) | 29.01 (-13.4%) | 30.32 (-9.5%) |
| 8t `rand d8` | 47.49 | 47.10 (-0.8%) | 47.77 (+0.6%) | 48.98 (+3.2%) | 47.66 (+0.4%) | 48.55 (+2.3%) | 41.00 (-13.7%) | 41.89 (-11.8%) | 43.50 (-8.4%) |
| 8t `d10` | 49.49 | 48.79 (-1.4%) | 49.95 (+0.9%) | 51.36 (+3.8%) | 50.62 (+2.3%) | 48.94 (-1.1%) | 43.17 (-12.8%) | 44.18 (-10.7%) | 45.65 (-7.8%) |
| 8t `d12` | 53.12 | 52.87 (-0.5%) | 53.87 (+1.4%) | 54.48 (+2.5%) | 53.81 (+1.3%) | 53.27 (+0.3%) | 46.20 (-13.0%) | 47.42 (-10.7%) | 49.05 (-7.7%) |
| 8t `d14` | 55.57 | 55.71 (+0.3%) | 55.60 (+0.1%) | 57.91 (+4.2%) | 55.93 (+0.6%) | 57.54 (+3.5%) | 47.68 (-14.2%) | 48.97 (-11.9%) | 50.59 (-9.0%) |
| 16t `rand d8` | 71.09 | 70.92 (-0.2%) | 70.90 (-0.3%) | 73.32 (+3.1%) | 70.04 (-1.5%) | 73.07 (+2.8%) | 60.24 (-15.3%) | 63.41 (-10.8%) | 66.01 (-7.1%) |
| 16t `d10` | 69.79 | 71.84 (+2.9%) | 72.07 (+3.3%) | 74.74 (+7.1%) | 72.17 (+3.4%) | 74.11 (+6.2%) | 64.15 (-8.1%) | 64.93 (-7.0%) | 66.15 (-5.2%) |
| 16t `d12` | 71.41 | 73.70 (+3.2%) | 72.30 (+1.2%) | 76.91 (+7.7%) | 72.63 (+1.7%) | 76.96 (+7.8%) | 64.53 (-9.6%) | 64.32 (-9.9%) | 68.51 (-4.1%) |
| 16t `d14` | 68.05 | 74.03 (+8.8%) | 72.10 (+6.0%) | 76.91 (+13.0%) | 72.11 (+6.0%) | 74.36 (+9.3%) | 64.28 (-5.5%) | 66.61 (-2.1%) | 63.67 (-6.4%) |

#### 4c. Endgame Solver `solve-bench` FFO #40–49 (10 positions, exact solves)

| Config | `4t` min / mean (`s`, `Δ`) | `4t` B nodes / M nps (`Δ`) | `8t` min / mean (`s`, `Δ`) | `8t` B nodes / M nps (`Δ`) | `16t` min / mean (`s`, `Δ`) | `16t` B nodes / M nps (`Δ`) |
|---|---:|---:|---:|---:|---:|---:|
| `rust-base` | 11.136 / 11.173 (+0.0%) | 1.652 / 147.88 (+0.0%) | 6.340 / 6.431 (+0.0%) | 1.631 / 253.67 (+0.0%) | 5.376 / 5.691 (+0.0%) | 1.734 / 304.65 (+0.0%) |
| `rust-native` | 10.700 / 10.827 (+3.2%) | 1.647 / 152.10 (+2.9%) | 6.006 / 6.113 (+5.2%) | 1.633 / 267.17 (+5.3%) | 4.802 / 4.889 (+16.4%) | 1.711 / 349.89 (+14.8%) |
| `clang-base` | 10.979 / 11.095 (+0.7%) | 1.637 / 147.54 (-0.2%) | 6.318 / 6.425 (+0.1%) | 1.639 / 255.02 (+0.5%) | 5.449 / 5.512 (+3.2%) | 1.705 / 309.39 (+1.6%) |
| `clang-native` | 11.305 / 11.358 (-1.6%) | 1.648 / 145.12 (-1.9%) | 6.308 / 6.436 (-0.1%) | 1.617 / 251.22 (-1.0%) | 5.033 / 5.090 (+11.8%) | 1.724 / 338.65 (+11.2%) |
| `clang-base-exc` | 10.678 / 10.761 (+3.8%) | 1.661 / 154.36 (+4.4%) | 5.895 / 6.038 (+6.5%) | 1.619 / 268.21 (+5.7%) | 5.296 / 5.420 (+5.0%) | 1.757 / 324.18 (+6.4%) |
| `clang-native-exc` | 10.576 / 10.665 (+4.8%) | 1.640 / 153.73 (+4.0%) | 6.166 / 6.294 (+2.2%) | 1.677 / 266.43 (+5.0%) | 4.797 / 4.840 (+17.6%) | 1.717 / 354.75 (+16.4%) |
| `msvc-base` | 13.414 / 13.562 (-17.6%) | 1.640 / 120.95 (-18.2%) | 7.509 / 7.576 (-15.1%) | 1.580 / 208.52 (-17.8%) | 6.256 / 6.386 (-10.9%) | 1.727 / 270.46 (-11.2%) |
| `msvc-native` | 12.024 / 12.109 (-7.7%) | 1.653 / 136.54 (-7.7%) | 6.824 / 6.979 (-7.9%) | 1.618 / 231.88 (-8.6%) | 5.427 / 5.592 (+1.8%) | 1.701 / 304.11 (-0.2%) |
| `msvc-native-exc` | 10.328 / 10.506 (+6.3%) | 1.637 / 155.79 (+5.3%) | 5.995 / 6.039 (+6.5%) | 1.628 / 269.60 (+6.3%) | 5.023 / 5.081 (+12.0%) | 1.738 / 341.97 (+12.2%) |

### 5. Disassembly Verification & Engineering Analysis

#### Binary Instruction Audit (`llvm-objdump -d`)

| Binary | `popcnt` | `tzcnt` | `lzcnt` | `bsf` | `bsr` | `pext` | `blsr` | `ymm` refs | `zmm` refs |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `rust-base` | 0 | 71 | 0 | 0 | 16 | 3 | 0 | 59 | 59 |
| `rust-native` | 67 | 113 | 6 | 3 | 6 | 20 | 33 | 4,766 | 6,585 |
| `clang-base` | 0 | 40 | 0 | 12 | 123 | 2 | 0 | 305 | 0 |
| `clang-native` | 74 | 36 | 6 | 11 | 9 | 2 | 25 | 5,499 | 8,275 |
| `msvc-base` | 41 | 34 | 44 | 44 | 57 | 2 | 0 | 177 | 0 |
| `msvc-native` | 75 | 34 | 44 | 15 | 13 | 2 | 19 | 1,000 | 0 |

1. **Strict baseline (`x86-64` + `Result`/`AbortResult`):**
   - Because Rust (`1.99.1`) and Clang (`23.1.5`) share the LLVM 23.1 backend, `rust-base` and `clang-base` perform virtually identically in the 1-thread endgame solver (`23.845s` vs `23.823s`, `+0.1%`) and at 4–8t FFO (`11.173s` vs `11.095s`; `6.431s` vs `6.425s`).
   - `clang-base` holds a small but consistent lead in 1-thread `eval-split` (`16.25 vs 15.42 M/s`, `+5.4%`) and midgame (`+0.6%` to `+5.9%` on `d10`/`d12`/`d16`, and `-O3 -flto` C++ `std::sort`/no-bounds-check patterns).
   - `msvc-base` trails Rust/Clang by **12–21%** (`17.3%` slower in 1t FFO #40–49) in part because MSVC STL `<bit>` (`std::popcount`, `std::countl_zero`) emits a dynamic `__isa_available` branch + SWAR fallback at every call site unless `/arch:AVX2` is enabled (confirmed at `0x140005055`..`0x14000506e`).

2. **`STELLO_ABORT_EXCEPTIONS=ON` (`throw Abort` vs `AbortResult`):**
   - Zero-cost C++ exceptions eliminate `if (res.aborted()) return Abort` tag inspection along every node/pass boundary inside `last4`–`last1` and endgame search without changing a single searched node.
   - At 1 thread on `FFO #40–49`, enabling exceptions accelerates `clang-base` by **+5.6%** (`23.823 -> 22.566s`, `49.44M nps`), `clang-native` by **+5.3%** (`24.379 -> 23.145s`), `msvc-base` by **+11.9%** (`28.849 -> 25.782s`), and `msvc-native` by **+14.1%** (`25.728 -> 22.548s`, tying `clang-base-exc` as the fastest 1t solver binary overall, **+5.8%** ahead of `rust-base`).
   - Across `4`, `8`, and `16` threads on FFO #40–49, `clang-base-exc` and `msvc-native-exc` both outperform `rust-base` (`4t: 10.761s / 10.506s vs 11.173s`, `8t: 6.038s / 6.039s vs 6.431s`). In shallow midgame search (where `eval()` dominates runtime), exceptions help MSVC consistently (`+2.4%` to `+8.3%`), help `clang-base` slightly (`+0.3%` to `+2.7%` except `-0.2%` on d14), and slightly trail on `clang-native`.

3. **Native Zen 4 (`-march=native` / `target-cpu=native` / `/arch:AVX2`) & the Clang AVX-512 SLP Regression in `flips()`:**
   - **Midgame winner:** `clang-native` (`AbortResult`) is the fastest single configuration in 1-thread midgame across every depth (`rand d8 +4.6%`, `d10 +8.7%`, `d12 +9.2%`, `d14 +6.2%`, `d16 +6.7%` vs `rust-base`, and `7.069s` vs `7.521s` against `rust-native`) and holds the highest `4t`/`8t`/`16t` midgame throughput (`77.22–78.07M nps` at 16t `d16`). Note that `rust-native` gains almost nothing over `rust-base` in 1t midgame (`7.521s` vs `7.542s`, `+0.3%`).
   - **Why `clang-native` regressed on `perft` (`-43.7%`) and `1t–8t FFO` (`-2.2%`):** Inspecting `clang++ -S -O3 -march=native` on `core/board.cpp` (`board_native.s` vs `board_base.s`) and `endgame/last.cpp` (`last_native_res.s`: `773` `zmm` refs vs `0` in `last_res.s`) shows Clang 23.1.5 SLP-vectorizing the 8 ray checks in `endgame::bits::flips()` into 512-bit `zmm`/`ymm` instructions (`vpermq`, `vpternlogq`, `vplzcntq`, `vpsrlvq`, `vptestnmq`, `kmovd`), followed by 8 conditional mask branches (`testb`), vector-to-GPR extracts, and `vzeroupper` around every recursive `perft` call. `rust-native` (`-C target-cpu=native`) did not trigger that SLP pattern in `perft` and accelerated `perft(8)` (`+16.4%`) / `perft(9)` (`+18.3%`), as did `msvc-native` (`zmm=0`, `+6.5%` / `+5.4%`).

4. **Hardware `popcnt`/`lzcnt`/`blsr` & 16-Thread SMT Behavior (`FFO #40–49` vs `mid d16`):**
   - Both `rust-base` (`popcnt=0`, `lzcnt=0`) and `clang-base` (`popcnt=0`, `lzcnt=0`) execute software SWAR `popcount` and `63 ^ bsr` bit-hacks in baseline x86-64 mode, whereas native Zen 4 / AVX2 unlocks hardware `popcnt`, `lzcnt`, and `blsr`.
   - When running **16 SMT threads on 8 Zen 4 cores (`FFO #40–49`)**, SWAR instruction bloat hits shared core execution ports/front-end harder, so hardware `popcnt`/`lzcnt` drives a sharp `16t` jump across all native builds (`rust-base 304.65M -> rust-native 349.89M`, `+14.8%`; `clang-base 309.39M -> clang-native 338.65M`, `+9.5%`; `clang-native-exc` peaks overall at **`4.840s` mean / `354.75M nps`**, **+17.6% faster wall time than `rust-base`**).
   - **SMT note (`8t` vs `16t` on Lazy SMP):** On `midgame d16`, 16 SMT threads increase total search overhead nodes by **+45%** (`164.0M -> 240.2M` on `rust-base`) while SMT only adds `+24%` throughput (`59.05M -> 73.35M nps`), making 16 threads *slower* in wall time than 8 physical cores (`3.276s vs 2.777s`). On deep endgames (`FFO #40–49`), YBWC keeps node overhead small (`1.631B -> 1.734B`, `+6.3%`), so 16t cuts solve time cleanly (`6.431s -> 5.691s` on `rust-base`, `6.038s -> 5.420s` on `clang-base-exc`, `6.294s -> 4.840s` on `clang-native-exc`).

### Objective Gate P4 Summary (For Decision on Part 2)

- **Portable `x86-64` distribution build (`clang-base-exc` vs `rust-base`):** C++20 matches Rust in baseline `AbortResult` midgame/endgame parity (`+0.1%` FFO, `+1.3–5.9%` midgame, `+5.4%` eval) and — by switching one compile-time macro (`STELLO_ABORT_EXCEPTIONS=ON`) that Rust (`panic=abort`, `Result`) cannot easily use on search abort paths — gains a consistent **+5.6% (1t), +3.8% (4t), +6.5% (8t), and +5.0% (16t)** speedup in exact endgame solving (`FFO #40–49`) plus **+0.9–6.7%** in 1t midgame with **0 node difference**.
- **Native / tuned builds:** `clang-native` already leads 1t & MT midgame (**+6.7% 1t d16**, **+8.7–9.2% d10/d12**), and `clang-native-exc` sets the fastest 16t FFO mark (**4.840 s**, **+17.6%** over `rust-base`). If native `-march=native` builds are desired in Part 2, adding a targeted tweak to prevent Clang's ZMM SLP vectorization in `endgame::bits::flips()` (putting it in sync with `msvc-native`/`rust-native` scalar `blsr`/`lzcnt`) would likely combine `clang-native`'s midgame lead with fast 1t–8t endgame solving.
- **Standalone Phase 4 Gate (Approved 2026-11-09):** User approved **Option B** — proceed to **Part 2** (Phases 5–7: pre-FFI C++ tuning pass with `STELLO_ABORT_EXCEPTIONS=ON` by default and scalar `flips()` loop pragmas, `.NET` FFI + `stello-v3` preservation and 1,180-game timed match comparison, Emscripten WASM + Node/Chromium comparison vs. Rust WASM, and Chapter 28 / Stello 4.0 release).

