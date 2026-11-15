# Stello 5.0 Findings

Working notes, benchmark baselines, and gate records for [making-stello-5-0-plan.md](making-stello-5-0-plan.md) on branch `feature/stello-5.0`.

---

## Environment and Branch Setup

- **Branch:** `feature/stello-5.0`, branched from `main` at commit `ea43115` (`Stello session start document`, following `242b6eb` and tag `v4.0` at `8b0b7b7`).
- **CPU / OS:** AMD Ryzen 7 7800X3D 8-Core Processor (16 logical processors), Windows x64.
- **Toolchains:**
  - Clang `23.1.5` (`x86_64-pc-windows-msvc`), CMake `3.31.6-msvc6`, Visual Studio 2022 BuildTools (`vcvars64.bat`, `msvc-release` / `msvc-native`).
  - .NET SDK `10.0.401` (`Stello.Net/Stello.Net.slnx`).
  - Node.js `v24.19.0`.

---

## Phase 0 — Freeze the 4.0 Baseline, Decouple Tests, and Branch

Phase 0 completed and passed **Gate P0**.

### 1. Self-Contained C++ & WASM Tests (`Stello.Cpp/tests/`)

- Copied parity fixtures (`positions.txt`, `patterns.txt`, `perft.txt`) from `Stello.Rust/stello-core/tests/data/` into `Stello.Cpp/tests/data/`.
- Copied WASM Node test files (`engine.test.mjs`, `engine-mt.test.mjs`, `pool-worker.mjs`) from `Stello.Rust/stello-wasm/tests/` into `Stello.Cpp/tests/wasm/`. `Stello.Rust/` stays intact and frozen at 3.1.
- Updated `Stello.Cpp/CMakeLists.txt` (`STELLO_PARITY_DATA_DIR`, defaulting to `Stello.Cpp/tests/data`), `Stello.Cpp/tests/test_harness.hpp` (`parity_data_dir()`), `Stello.Cpp/tests/test_main.cpp`, and `Stello.Cpp/tests/parity_tests.cpp` so C++ parity tests read from `Stello.Cpp/tests/data`.
- Updated default parity `--out` path in `Stello.Net/tools/Stello.Benchmark/ParityCommands.cs` and `Program.cs` help text to `Stello.Cpp/tests/data`, and updated WASM test references in `docs/08-endgame-solver.md`, `docs/09-time-control.md`, `docs/14-app-integration.md`, and `Content and state of the Stello project as of 2026.11.13.md`.

### 2. Preserved 4.0 Reference Engine (`stello-v4`)

- Preserved `Stello.Cpp/build/clang-release/stello_engine.dll` (built from `242b6eb`, engine version `40000`, ABI `3`) as `Stello.Cpp/ref/stello_engine_v4.dll`:
  - Size: `1,373,184` bytes
  - SHA-256: `1F67F9F213677AF18E28BABA20C17B44D769A2701CCA53746F25A00F3830D29A`
- Added `Stello.Cpp/ref/` to `.gitignore`.
- Added `engine=stello-v4` / `--engine stello-v4` support to `Stello.Benchmark` (`StelloLibraryMatchPlayer.cs`, `MatchCommands.cs`, `BenchSearcher.cs`, `Program.cs`, `Stello.Benchmark.csproj`) and `docs/17-measuring-the-engine.md`, with unit test coverage in `Stello.Net/Egaroucid.Engine.Tests/StelloLibraryMatchTests.cs` (`StelloV4_SearchesWhenReferenceLibraryIsPresent` and invalid-option validation cases).
- Verified `--engine stello-v4 --compare` parity against `v4.0-ref`:
  - `mid-bench --engine stello-v4 --depth 10 --passes 1 --compare Plans\benchmark-results\v4.0-ref\midgame-depth10.json`: identical across all 100 positions (`3,716,389` nodes, `0.362 s` vs `0.353 s`, `10.25M` nps).
  - `solve-bench --engine stello-v4 --from 40 --to 44 --passes 1 --compare Plans\benchmark-results\v4.0-ref\ffo-40-49.json`: identical across all 5 positions (`93,697,164` nodes, `1.956 s` vs `1.965 s`, `47.90M` nps; all scores and moves exact).

### 3. Single-Thread 4.0 Baseline (`Plans/benchmark-results/v4.0-ref/`)

Recorded on idle CPU (`hash bits 19`, 1 thread, `stello` / `40000`):

| Benchmark | Positions | Limits | Passes | Nodes | Time | Speed | File |
|---|---:|---|---:|---:|---:|---:|---|
| `ffo-40-49` | 10 | solve | 1 | 1,115,666,701 | 23.552 s | 47.37M nps | [ffo-40-49.json](benchmark-results/v4.0-ref/ffo-40-49.json) |
| `endgame-random-14-24` | 500 | solve | 1 | 27,900,685,006 | 489.200 s | 57.03M nps | [endgame-random-14-24.json](benchmark-results/v4.0-ref/endgame-random-14-24.json) |
| `midgame-depth10` | 100 | depth 10 | 3 | 3,716,389 | 0.353 s | 10.53M nps | [midgame-depth10.json](benchmark-results/v4.0-ref/midgame-depth10.json) |
| `midgame-depth12` | 100 | depth 12 | 3 | 10,579,515 | 0.979 s | 10.80M nps | [midgame-depth12.json](benchmark-results/v4.0-ref/midgame-depth12.json) |
| `midgame-depth14` | 100 | depth 14 | 3 | 29,779,568 | 2.740 s | 10.87M nps | [midgame-depth14.json](benchmark-results/v4.0-ref/midgame-depth14.json) |
| `midgame-depth16` | 100 | depth 16 | 3 | 77,867,024 | 7.206 s | 10.81M nps | [midgame-depth16.json](benchmark-results/v4.0-ref/midgame-depth16.json) |
| `midgame-random-depth8` | 1,000 | depth 8 | 3 | 9,620,457 | 0.875 s | 11.00M nps | [midgame-random-depth8.json](benchmark-results/v4.0-ref/midgame-random-depth8.json) |
| `midgame-handover-depth12` | 300 | depth 12 | 3 | 321,027,249 | 6.488 s | 49.48M nps | [midgame-handover-depth12.json](benchmark-results/v4.0-ref/midgame-handover-depth12.json) |

All FFO #40–#49 and 500 `endgame-random-14-24` scores and moves verified exact.

### 4. Gate P0 Verification

- **C++ presets (`ctest --preset <P> --output-on-failure` + `stello_tests.exe`):**
  - `clang-release`: 2/2 CTest (`stello_tests` 30.56 s, `stello_cli_version` 0.01 s), `43 passed, 0 failed`.
  - `clang-native`: 2/2 CTest (`31.89 s` / `0.01 s`).
  - `msvc-release`: 2/2 CTest (`32.59 s` / `0.01 s`).
  - `msvc-native`: 2/2 CTest (`29.18 s` / `0.01 s`).
  - `clang-sanitized`: 2/2 CTest (`60.58 s` / `0.01 s`).
- **WASM Node tests (`node --test Stello.Cpp/tests/wasm/engine.test.mjs Stello.Cpp/tests/wasm/engine-mt.test.mjs`):** `19 passed, 0 failed, 0 skipped` (`13` single-thread + `6` multi-thread, `3103.8 ms`).
- **.NET Release build (`dotnet build Stello.Net/Stello.Net.slnx -c Release -p:StelloWasmEngine=false`):** succeeded with `0 Warning(s), 0 Error(s)`.
- **.NET sequential test suites (`--no-build`):**
  - `Stello.Engine.Tests`: `499 passed, 0 failed, 0 skipped`.
  - `Egaroucid.Engine.Tests`: `100 passed, 0 failed, 8 skipped` (all 8 skipped are `TaggedV30_*` reference DLL tests requiring an unbuilt `stello_engine_v3.dll`; `StelloV4_SearchesWhenReferenceLibraryIsPresent` passed).
  - `Stello.Net.Tests`: `116 passed, 0 failed, 0 skipped`.

### 5. WebAssembly 4.0 Baseline (`scratch/wasm_bench.mjs`)

- **Artifact sizes (raw / gzip / brotli):**
  - `Stello.Web/wwwroot/engine/stello_wasm.js`: `10,423` / `2,730` / `2,433` B
  - `Stello.Web/wwwroot/engine/stello_wasm_bg.wasm`: `1,174,483` / `1,062,276` / `1,048,577` B
  - `Stello.Web/wwwroot/engine-mt/stello_wasm_bg.wasm`: `1,203,142` / `1,074,555` / `1,058,149` B
- **Node benchmarks (`ST 1t` / `MT 1t` / `MT 4t`):**
  - `ST 1t`: `mid-d10` `3,716,389` nodes (`779.2/788.1 ms`, `4.72M` nps); `mid-d12` `10,579,515` nodes (`2190.3/2203.9 ms`, `4.80M` nps); `ffo-40-44` `93,697,164` nodes (`2415.5/2430.5 ms`, `38.55M` nps).
  - `MT 1t`: `mid-d10` (`780.3/786.6 ms`, `4.72M` nps); `mid-d12` (`2194.8/2216.8 ms`, `4.77M` nps); `ffo-40-44` (`2391.1/2405.6 ms`, `38.95M` nps).
  - `MT 4t`: `mid-d10` `7,425,697` nodes (`484.0/537.1 ms`, `13.83M` nps); `mid-d12` `19,442,514` nodes (`1192.7/1324.2 ms`, `14.68M` nps); `ffo-40-44` `110,478,042` nodes (`982.0/1020.5 ms`, `108.26M` nps).

---

## Phase 1 — Speed (No Change in Nodes)

### Step 1.1: Runtime `x86-64-v3` Dispatch & Explicit AVX2 `flips` / `legal_moves`

- **Implementation:**
  - Extended `stello::cpu` (`cpu.hpp`, `cpu.cpp`) with `has_avx2_bmi2()` (`AVX2 + BMI1 + BMI2 + POPCNT + LZCNT`) and `--cpu portable` override.
  - Added 256-bit AVX2 `flips_avx2` and `legal_moves_avx2` (`Stello.Cpp/src/endgame/bits_avx2.hpp`) with unit tests in `Stello.Cpp/tests/bits_avx2_tests.cpp` and parity assertions in `parity_tests.cpp`.
  - Isolated `Kernel<true>` (`Stello.Cpp/src/search/search_v3.cpp`, compiled with `-mavx2 -mbmi -mbmi2 -mpopcnt -mlzcnt -mno-avx512f` on Clang/GCC, `/arch:AVX2` on MSVC) from `Kernel<false>` (`Stello.Cpp/src/search/search_portable.cpp`).
  - **ODR / LTO note:** Baseline TUs (`engine.cpp`, `endgame/search.cpp`, `endgame/last.cpp`, `search/ybwc.cpp`) previously included `*_impl.hpp` and emitted non-AVX2 `linkonce_odr` COMDAT copies of `Kernel<true>`, which LTO selected over `search_v3.cpp` everywhere except `last4..1`. Moving `#ifdef __AVX2__` out of `kernel.hpp` into explicit specializations in `search_v3.cpp` and restricting baseline TUs to `#include "search/kernel.hpp"` (`extern template struct Kernel<false>` / `Kernel<true>`) resolved this cleanly.
- **Disassembly check (`llvm-objdump -d` on `clang-release`):**
  - `stello-cpp.exe`: `zmm=0`, `ymm=2257`, `vpsllvq=91`, `vpsrlvq=184`, `popcnt=66`, `tzcnt=75`, `lzcnt=0`, `blsr=22`, `shlx=23`.
  - `stello_engine.dll`: `zmm=0`, `ymm=2245`, `vpsllvq=91`, `vpsrlvq=184`, `popcnt=66`, `tzcnt=73`, `lzcnt=0`, `blsr=22`, `shlx=23`.
- **Verification (0 compiler warnings across all builds):**
  - `clang-release`: 2/2 CTest (`stello_tests` `33.75 s`, `stello_cli_version` `1.10 s`).
  - `msvc-release`: 2/2 CTest (`stello_tests` `29.71 s`, `stello_cli_version` `1.04 s`).
  - `clang-native`: 2/2 CTest (`stello_tests` `26.73 s`, `stello_cli_version` `0.56 s`).
  - `clang-sanitized`: 2/2 CTest (`stello_tests` `62.28 s`, `stello_cli_version` `0.53 s`, 0 ASan/UBSan findings).
  - `msvc-native`: 2/2 CTest (`stello_tests` `27.58 s`, `stello_cli_version` `8.60 s`).
  - `.NET` Release (`-p:StelloWasmEngine=false`): `0 Warning(s), 0 Error(s)`; `Stello.Engine.Tests` `499 passed, 0 failed`; `Stello.Net.Tests` `116 passed, 0 failed`; `Egaroucid.Engine.Tests` `100 passed, 8 skipped` (`TaggedV30_*`).
- **1-thread parity & speed (`Stello.Benchmark --compare v4.0-ref`, 100% identical nodes/move/score on every position):**

| Benchmark | Positions | Nodes | v4.0 Time | Step 1.1 Time | Step 1.1 Speed | Gain |
|---|---:|---:|---:|---:|---:|---:|
| `midgame-depth10` | 100 | 3,716,389 | 0.353 s | 0.313 s | 11.88M nps | +12.8 % |
| `midgame-depth12` | 100 | 10,579,515 | 0.979 s | 0.897 s | 11.80M nps | +9.2 % |
| `midgame-depth14` | 100 | 29,779,568 | 2.740 s | 2.478 s | 12.02M nps | +10.6 % |
| `midgame-depth16` | 100 | 77,867,024 | 7.206 s | 6.401 s | 12.17M nps | +12.6 % |
| `midgame-random-depth8` | 1,000 | 9,620,457 | 0.875 s | 0.789 s | 12.19M nps | +10.8 % |
| `midgame-handover-depth12` | 300 | 321,027,249 | 6.488 s | 5.132 s | 62.56M nps | +26.4 % |
| `ffo-40-49` | 10 | 1,115,666,701 | 23.552 s | 17.5 s | 63.79M nps | +34.7 % |
| `endgame-random-14-24` | 500 | 27,900,685,006 | 489.2 s | 361.7 s | 77.13M nps | +35.2 % |

- **4-thread comparison (`stello` vs `stello-v4`, `hash-bits=19`):**
  - `midgame-depth12` (3 passes): `stello-v4` `16,655,987` nodes `0.533 s` (`31.23M` nps) vs `stello` `16,491,580` nodes `0.472 s` (`34.95M` nps).
  - `midgame-depth14` (3 passes): `stello-v4` `46,015,720` nodes `1.398 s` (`32.92M` nps) vs `stello` `45,812,696` nodes `1.250 s` (`36.64M` nps).
  - `midgame-depth16` (3 passes): `stello-v4` `113,189,215` nodes `3.255 s` (`34.78M` nps) vs `stello` `110,157,524` nodes `2.888 s` (`38.15M` nps).
  - `ffo-40-49` (1 pass): `stello-v4` `1,633,549,190` nodes `11.0 s` (`148.96M` nps) vs `stello` `1,642,317,511` nodes `7.928 s` (`207.16M` nps, `+39.1 %` nps).
- **`stello-cpp` `--cpu portable` vs `--cpu auto` (exact JSON node/score/move parity):**
  - `mid-bench d12`: `portable` `10,579,515` nodes `1.446 s` (`7.32M` nps) vs `auto` `10,579,515` nodes `0.860 s` (`12.30M` nps).
  - `solve-bench` FFO #40–49: `portable` `1,115,666,701` nodes `23.9 s` (`46.61M` nps) vs `auto` `1,115,666,701` nodes `16.8 s` (`66.31M` nps).

## Phase 1.2: Incremental Pattern Indices in Midgame Search

- **Design:** Added `eval::PatternState` (`alignas(32) uint16_t black[64], white[64]`) and incremental feature-index algebra to `Stello.Cpp/src/eval/patterns.hpp`, `src/eval/patterns.cpp`, `src/eval/eval.hpp`, `src/search/kernel.hpp`, `src/search/kernel_impl.hpp`, `src/search/search_v3.cpp`, `src/search/ybwc_impl.hpp`, `src/search/engine_impl.hpp`, and `tests/parity_tests.cpp` (`EvalWeights::evaluate` left untouched).
  - Precomputes dense `square_deltas_[64][64]`, sparse `(feature, delta)` slices per square (`square_sparse_offsets_`, `square_sparse_deltas_`), and dense `feature_offsets_[64]`.
  - Child local index satisfies `other + 2 * own + 2 * delta(square) + sum(delta(flipped))`, giving 0-cost passes and single-vector leaf (`look == 0`) & move-ordering (`order_by_evaluation`) evaluations without copying `Board` or calling `play_flips` at `look == 0`.
  - `Kernel<true>` (`search_v3.cpp`) specializes `advance_eval_state`, `build_eval_base`, `sum_eval_state`, and `sum_eval_after_move` with 256-bit AVX2 intrinsics; YBWC (`ybwc_impl.hpp`) initializes `PatternState` once on task entry.
- **L1 scalar weight loads vs AVX2 gather (`_mm256_i32gather_epi32`):**
  - Tested both in `search_v3.cpp` (`STELLO_V3_USE_GATHER=0` vs `1`). Scalar 4-way unrolled `weights[indices[f]]` after AVX2 index computation beat microcoded AVX2 gather on every benchmark (and avoids 2-byte unaligned `int*` UB on odd-length stage tables such as `plus15`/`classic11`/`egaroucid16`), so `STELLO_V3_USE_GATHER=0` is kept:
    - `midgame-depth12` (3 passes): `0.718 s` (`14.74M` nps) vs gather `0.734 s` (`14.41M` nps).
    - `midgame-depth14` (3 passes): `2.007 s` (`14.84M` nps) vs gather `2.024 s` (`14.71M` nps).
    - `midgame-depth16` (3 passes): `5.268 s` (`14.78M` nps) vs gather `5.313 s` (`14.66M` nps).
    - `midgame-random-depth8`: `0.630 s` (`15.26M` nps) vs gather `0.634 s` (`15.18M` nps).
- **1-thread parity & speed (`Stello.Benchmark --compare v4.0-ref`, 100% identical nodes/move/score on every position):**

| Benchmark | Positions | Nodes | v4.0 Time | Step 1.2 Time | Step 1.2 Speed | Gain vs v4.0 |
|---|---:|---:|---:|---:|---:|---:|
| `midgame-depth10` | 100 | 3,716,389 | 0.353 s | 0.261 s | 14.23M nps | +35.1 % |
| `midgame-depth12` | 100 | 10,579,515 | 0.979 s | 0.718 s | 14.74M nps | +36.4 % |
| `midgame-depth14` | 100 | 29,779,568 | 2.740 s | 2.007 s | 14.84M nps | +36.5 % |
| `midgame-depth16` | 100 | 77,867,024 | 7.206 s | 5.268 s | 14.78M nps | +36.8 % |
| `midgame-random-depth8` | 1,000 | 9,620,457 | 0.875 s | 0.630 s | 15.26M nps | +38.8 % |
| `midgame-handover-depth12` | 300 | 321,027,249 | 6.488 s | 4.787 s | 67.07M nps | +35.5 % |
| `ffo-40-49` | 10 | 1,115,666,701 | 23.552 s | 17.2 s | 64.69M nps | +36.6 % |

- **`stello-cpp` `--cpu portable` vs `--cpu auto` (`mid-bench d12`, exact JSON node/score/move parity):**
  - `portable`: `10,579,515` nodes `0.910 s` (`11.62M` nps, up from `7.32M` nps in Step 1.1).
  - `auto`: `10,579,515` nodes `0.709 s` (`14.92M` nps).
- **Toolchains & Tests (final Step 1.2 code, 0 warnings):**
  - `parity_incremental_eval`: bit-exact parity across all 6,214 `positions.txt` positions and `> 30,000` legal moves (`built_in`, `classic11`, `plus15`, `egaroucid16`).
  - `clang-release`: 2/2 CTest (`stello_tests` `25.72 s`, `stello_cli_version` `0.49 s`).
  - `clang-native`: 2/2 CTest (`stello_tests` `24.61 s`, `stello_cli_version` `0.40 s`).
  - `msvc-release`: 2/2 CTest (`stello_tests` `27.90 s`, `stello_cli_version` `0.52 s`).
  - `msvc-native`: 2/2 CTest (`stello_tests` `26.97 s`, `stello_cli_version` `0.49 s`).
  - `clang-sanitized`: 2/2 CTest (`stello_tests` `48.40 s`, `stello_cli_version` `0.35 s`, 0 ASan/UBSan findings).
  - `.NET` Release (`-p:StelloWasmEngine=false`): `0 Warning(s), 0 Error(s)`; `Stello.Engine.Tests` `499 passed, 0 failed`; `Egaroucid.Engine.Tests` `100 passed, 0 failed, 8 skipped` (`TaggedV30_*`); `Stello.Net.Tests` `116 passed, 0 failed`.

### Step 1.3: WebAssembly `-msimd128` & Abort-Mode Check

- **Implementation:**
  - Enabled `-msimd128` in `Stello.Cpp/CMakeLists.txt` under `if(EMSCRIPTEN AND STELLO_BUILD_WASM)` (`stello_core PUBLIC`, `stello_wasm PRIVATE`, and both single-thread / pthreaded link options).
  - Added `<wasm_simd128.h>` 128-bit vector paths under `#ifdef __wasm_simd128__` in `Stello.Cpp/src/eval/patterns.cpp` (`advance_state`, `build_base`, `sum_local_wasm`, `sum_weights_from_state`, `sum_weights_after_move` using 8× `wasm_i16x8` + `wasm_u32x4_extend_*_u16x8` + `wasm_i32x4_add` + 4-way scalar L1 weight loads).
  - Added `<wasm_simd128.h>` 2-lane vector upper-ray `flips` (`detail::UPPER_RAYS_128`, 4 upper rays in 2 `v128_t` ops without shifts, keeping `std::countl_zero` scalar lower rays) in `Stello.Cpp/src/endgame/bits.hpp`.
  - Tested a 2-lane bit-reversed (`rbit_u64x2`) SIMD `legal_moves`; benchmarked slower than scalar Kogge-Stone (`ST 1t ffo-40-44` `2344.6 ms` / `39.96M` nps vs scalar `2257.2 ms` / `41.51M` nps, and `+8 KiB` raw wasm size) and reverted it.
- **`STELLO_ABORT_EXCEPTIONS=ON` vs `OFF` (`-msimd128` active in both):**
  - `ON` matched `OFF` on ST midgame (`398.6` / `1124.6 ms` vs `397.9` / `1115.2 ms`), beat `OFF` on `ST 1t ffo-40-44` (`2257.2 ms` / `41.51M` vs `2332.4 ms` / `40.17M`, `+3.3 %`), beat `OFF` across all `MT 1t` benchmarks (`400.1` / `1135.2` / `2257.0 ms` vs `447.4` / `1210.4` / `2517.2 ms`, `+6.6–11.8 %`), and beat `OFF` on `MT 4t ffo-40-44` (`959.2 ms` vs `1039.5 ms`) with smaller binaries (`1,204,118` vs `1,207,819` B ST). Kept `STELLO_ABORT_EXCEPTIONS=ON` for WASM.
- **Final WebAssembly (`STELLO_ABORT_EXCEPTIONS=ON`, `-msimd128`) vs Step 1.2 scalar WASM & 4.0 WASM baseline (`scratch/wasm_bench.mjs`):**
  - **Artifact sizes (raw / gzip / brotli):**
    - `engine/stello_wasm_bg.wasm`: `1,204,118` / `1,068,747` / `1,053,785` B (vs 4.0: `1,174,483` / `1,062,276` / `1,048,577` B)
    - `engine-mt/stello_wasm_bg.wasm`: `1,233,820` / `1,080,982` / `1,063,717` B (vs 4.0: `1,203,142` / `1,074,555` / `1,058,149` B)

| Suite | Nodes (1t) | 4.0 WASM Mean | Step 1.2 Scalar Mean | Step 1.3 SIMD128 Mean | Step 1.3 Speed | Gain vs 4.0 |
|---|---:|---:|---:|---:|---:|---:|
| `ST 1t mid-d10` | 3,716,389 | 788.1 ms | 553.7 ms | 393.8 ms | 9.44M nps | +100.1 % |
| `ST 1t mid-d12` | 10,579,515 | 2203.9 ms | 1535.0 ms | 1110.4 ms | 9.53M nps | +98.5 % |
| `ST 1t ffo-40-44` | 93,697,164 | 2430.5 ms | 2392.6 ms | 2257.5 ms | 41.51M nps | +7.7 % |
| `MT 1t mid-d10` | 3,716,389 | 786.6 ms | 555.9 ms | 404.9 ms | 9.18M nps | +94.3 % |
| `MT 1t mid-d12` | 10,579,515 | 2216.8 ms | 1550.2 ms | 1141.2 ms | 9.27M nps | +94.3 % |
| `MT 1t ffo-40-44` | 93,697,164 | 2405.6 ms | 2378.1 ms | 2252.3 ms | 41.60M nps | +6.8 % |
| `MT 4t mid-d10` | — | 537.1 ms | 381.6 ms | 286.2 ms (7,659,257 n) | 26.76M nps | +87.7 % |
| `MT 4t mid-d12` | — | 1324.2 ms | 963.2 ms | 691.1 ms (19,511,833 n) | 28.23M nps | +91.6 % |
| `MT 4t ffo-40-44` | — | 1020.5 ms | 1021.9 ms | 988.4 ms (105,521,717 n) | 106.76M nps | +3.2 % |

### Gate P1 Verification (Final Phase 1 State, 0 Warnings)

- **C++ presets (`ctest --preset <P> --output-on-failure`):**
  - `clang-release`: 2/2 (`stello_tests` `41.11 s`, `stello_cli_version` `2.53 s`).
  - `clang-native`: 2/2 (`stello_tests` `40.37 s`, `stello_cli_version` `0.58 s`).
  - `msvc-release`: 2/2 (`stello_tests` `43.58 s`, `stello_cli_version` `8.36 s`).
  - `msvc-native`: 2/2 (`stello_tests` `28.01 s`, `stello_cli_version` `0.64 s`).
  - `clang-sanitized`: 2/2 (`stello_tests` `53.06 s`, `stello_cli_version` `1.18 s`, 0 ASan/UBSan findings).
- **WASM Node tests (`node --test Stello.Cpp/tests/wasm/engine.test.mjs Stello.Cpp/tests/wasm/engine-mt.test.mjs`):** `19 passed, 0 failed, 0 skipped` (`2971.2 ms`).
- **Native `--cpu portable` vs `--cpu auto` (`stello-cpp.exe` `clang-release` JSON exact match):**
  - `mid-bench d12`: `100` positions exact (`10,579,515` nodes; `auto` `0.740 s` / `14.30M` nps vs `portable` `0.937 s` / `11.29M` nps).
  - `solve-bench` FFO #40–49: `10` positions exact (`1,115,666,701` nodes; `auto` `17.0 s` / `65.71M` nps vs `portable` `24.0 s` / `46.48M` nps).
- **`Stello.Benchmark --compare v4.0-ref` (100% node/move/score identical):**
  - `mid-bench --depth 12 --passes 1`: `10,579,515` nodes, `0.737 s` vs `0.979 s` (`+32.8 %`).
  - `solve-bench --from 40 --to 49 --passes 1`: `1,115,666,701` nodes, `17.2 s` vs `23.6 s` (`+37.1 %`).
- **.NET Release build & sequential `--no-build` tests (`-p:StelloWasmEngine=false`):** `0 Warning(s), 0 Error(s)`; `Stello.Engine.Tests` `499 passed, 0 failed`; `Egaroucid.Engine.Tests` `100 passed, 0 failed, 8 skipped` (`TaggedV30_*`); `Stello.Net.Tests` `116 passed, 0 failed`.

### Pre-Phase-2 Cross-TU Disassembly Optimizations (After Gate P1)

- **1. Inline `TranspositionTable` (`Stello.Cpp/src/search/table.hpp`, `Stello.Cpp/src/search/table.cpp`):**
  - **Root cause:** `table.cpp` compiles without `-mpopcnt`, so LLVM LTO could not inline `get_at_root`, `store_at_root`, or `prefetch` into `Kernel<true>` (`search_v3.cpp`, compiled with AVX2/BMI2/`-mpopcnt`), forcing cross-TU calls and a 12-instruction SWAR `popcount` in `Entry::empty_count()`.
  - **Change:** Moved `Entry::pack`, `Entry::unpack`, `TranspositionTable::slot_index`, `get_at_root`, `store_at_root`, and `prefetch` (plus `detail` bit-packing and seqlock helpers) inline into `table.hpp`, keeping `AlignedDeleter`, constructor, and `clear` in `table.cpp`. `Kernel<true>` inlines TT access with hardware `popcntq`, while `Kernel<false>` (`search_portable.cpp`) stays baseline-safe (`--cpu portable`).
  - **Disassembly (`stello_engine.dll` `clang-release`):** `popcnt` instructions increased `66 -> 69` and `0x5555555555555555` SWAR constants dropped `53 -> 50`.
- **2. Remove TLS guard & SEH frame from `endgame::stable::of` (`Stello.Cpp/src/endgame/stable.hpp`, `Stello.Cpp/src/endgame/stable.cpp`):**
  - **Root cause:** `stable::init()` and `stable::of()` lacked `noexcept`, and `edges()` returned `static const EdgeTables` (`std::vector<uint64_t>`), emitting a `%gs:0x58` TLS init check and Windows SEH unwind frame on every call.
  - **Change:** Marked `init()` and `of()` `noexcept`; replaced function-local `static const EdgeTables` with `.bss` `std::array<uint64_t, 65536>` tables (`g_row_tables`, `g_column_tables`), a lock-free 3-state `std::atomic<uint8_t> g_edges_state` (`0/1/2`) `acquire` fast path, and `STELLO_NOINLINE void build_edges_slow() noexcept` (preserving zero `.rdata`/`.wasm` binary bloat and thread safety when unit tests call `stable::of` without constructing `Engine`).
  - **Disassembly & binary sizes:** `gs:0x58` TLS guards dropped `12 -> 10` (`stello_engine.dll` `1,463,808` B); WASM raw sizes slightly decreased (`engine/stello_wasm_bg.wasm` `1,204,118 -> 1,203,295` B; `engine-mt/stello_wasm_bg.wasm` `1,233,820 -> 1,232,838` B).
- **Benchmark impact (`Stello.Benchmark` `v4.0-ref`, 100% node/move/score parity):**
  - `mid-bench --depth 12 --passes 3`: `0.731 s` (`14.47M` nps, `+33.9 %` vs `v4.0-ref`) -> `0.706 s` (`14.98M` nps after Item 1) -> `0.703 s` (`15.05M` nps, `+39.3 %` vs `v4.0-ref` after Items 1+2, ~`+4.0 %` midgame throughput).
  - `solve-bench --from 40 --to 49 --passes 1`: `17.3 s` (`64.60M` nps) -> `17.4 s` (`64.20M` nps, `+35.5 %` vs `v4.0-ref`; within single-pass timing noise).
- **Full verification (after both optimizations, 0 warnings):**
  - All 5 C++ presets 2/2 CTest (`clang-release` `33.72 s`, `clang-native` `26.71 s`, `msvc-release` `36.62 s`, `msvc-native` `35.61 s`, `clang-sanitized` `64.40 s` with 0 ASan/UBSan findings).
  - `stello-cpp.exe mid-bench --depth 10 --passes 1`: `--cpu portable` (`11.02M` nps) and `--cpu auto` (`14.13M` nps) matched at `3,716,389` nodes.
  - `.NET` Release (`Stello.Net.slnx`, `-p:StelloWasmEngine=false` and `Stello.Web` WASM build): `0 Warning(s), 0 Error(s)`; `Stello.Engine.Tests` `499 passed, 0 failed`; `Egaroucid.Engine.Tests` `100 passed, 0 failed, 8 skipped` (`TaggedV30_*`); `Stello.Net.Tests` `116 passed, 0 failed`.
  - WASM Node tests: `19 passed, 0 failed, 0 skipped` (`2978.2 ms`).

---

## Phase 2 — Threads

Phase 2 completed and passed **Gate P2**.

### 1. Implementation & Tuned Defaults

- **Root splitting (`Stello.Cpp/src/search/engine_impl.hpp`):** Enabled YBWC splits after the first root move in `search_root` (`look >= YBWC_MID_MIN_DEPTH`) and `solve_root` (`empties > YBWC_END_MIN_EMPTIES`).
- **Brother cut (`Stello.Cpp/src/search/engine.cpp`, `src/search/ybwc_impl.hpp`):** Attached the active `SplitPoint` (`s.split_point = &point`) while the splitting thread searches brothers sequentially so `Searcher::check_abort()` interrupts superseded local brothers immediately when a helper fails high. Propagated external aborts out of the `#if STELLO_ABORT_EXCEPTIONS` catch block via `std::optional<Abort> abort_caught` + `STELLO_RETURN_ABORT_IN_STATUS(*abort_caught)` outside the catch block to avoid a bare `throw;` crash in `VCRUNTIME140.dll` under Win64 Clang ASan.
- **Split-wait progress heartbeats (`Stello.Cpp/src/search/engine.cpp`, `src/search/ybwc_impl.hpp`):** Extracted `Searcher::maybe_report_heartbeat()` and invoked it from `scope_polling`'s `poll` callback so waiting threads continue reporting every ~100 ms during long root/deep splits.
- **Tuned permanent parameters (temporary switches removed before Gate P2):**
  - `YBWC_MID_MIN_DEPTH = 6` (`mid-bench d16` confirmed `6` remains optimal at both `8t` and `16t`).
  - `YBWC_END_MIN_EMPTIES = 14` (reduced from `16`; `FFO #40–49` 3 passes improved from `3.889 s` -> `3.695 s` @ `8t` and `3.212 s` -> `3.103 s` @ `16t`; 500 `endgame-random-14-24` 2 passes improved from `86.3 s` -> `78.2 s` (`-9.4 %`) @ `8t` and `76.7 s` -> `68.8 s` (`-10.3 %`) @ `16t`).
  - Pure YBWC in `solve` (`0` Lazy SMP solve helpers when `s.ybwc` is active; `0` helpers beat `1/2/3` on `FFO #40–49`: `3.824 s` vs `4.166 s` / `4.604 s` / `5.247 s` @ `8t`).
  - Scaled midgame Lazy SMP helpers: `lazy_smp_helpers(workers) = (workers + 2U) / 4U` (`2` at `8t`, `4` at `15t`/`16t`). Flat `lazy-cap=2` gained `+23 Elo` at `8t`, but made `15t` (`1.767 s`) and `16t` (`1.823 s`) slower than `8t` (`1.737 s`) on `mid-bench d16`, whereas `4` at `15t`/`16t` ran `1.649 s` / `1.617 s`.
  - Kept `ThreadCounts.ForThisProcess()` at `ProcessorCount - 1` (`15` on the 7800X3D) in `Stello.Net/Stello.App/Models/ThreadCounts.cs` after `15t` (`cap=4`) tied `8t` (`cap=2`) within statistical noise at 500 ms (`-2 Elo`, `-15 to +12`) while solving `+0.2` empties earlier.
- **Tests:** Added `phase2_root_split_brother_cut_and_split_heartbeats` in `Stello.Cpp/tests/threads_tests.cpp` covering 8-thread `FFO #40–#44` parity, 8-thread 19–21 empty brother-cut recovery, and `<= 250 ms` split-wait heartbeat cadence (`<= 600 ms` under ASan). Kept generic `opt=` key=value support in `MatchCommands.cs` (covered by `MatchAdapterTests` and `StelloLibraryMatchTests`).

### 2. Reference Library & Gate P2 Matches (`--workers 1`, `hash-bits=19`, no book)

- Preserved Phase 1 baseline (`0ef7526`, `clang-release`) as `Stello.Cpp/ref/stello_engine_p1.dll` (`SHA-256 4ADB15E26874C52D43ECBA35EDA2387448D24152C1D3F0BF7DEAD765444C242F`).

| Match | Games | Side A | Side B | B Score (95 % CI) | Elo (95 % CI) | LOS | Discs | Depth (A / B) | First Solve (A / B) | File |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---|
| `p2-auto-15t-vs-8t-500ms` | 1,180 | `8t, cap=4` | `15t, cap=4` | **52.1 %** (50.1–54.1 %) | **+15** (+1 to +29) | 98.1 % | +0.2 ± 0.2 | 22.60 / 23.03 | 26.2 / 26.4 | [match_p2-auto-15t-vs-8t-500ms.md](benchmark-results/match_p2-auto-15t-vs-8t-500ms.md) |
| `p2-lazy-cap-2-vs-4-500ms` | 600 | `8t, cap=4` | `8t, cap=2` | **53.3 %** (50.6–56.1 %) | **+23** (+4 to +42) | 99.2 % | +0.2 ± 0.2 | 22.60 / 22.95 | 26.1 / 26.2 | [match_p2-lazy-cap-2-vs-4-500ms.md](benchmark-results/match_p2-lazy-cap-2-vs-4-500ms.md) |
| `v5p2-gate-500ms` | 1,180 | `P1 8t` | `P2 8t (cap=2)` | **51.1 %** (49.3–53.0 %) | **+8** (−5 to +21) | 88.3 % | +0.2 ± 0.2 | 22.34 / 22.80 | 26.0 / 26.2 | [match_v5p2-gate-500ms.md](benchmark-results/match_v5p2-gate-500ms.md) |
| `v5p2-gate-2s` | 600 | `P1 8t` | `P2 8t (cap=2)` | **54.3 %** (51.7–57.0 %) | **+30** (+12 to +49) | 99.9 % | +0.3 ± 0.2 | 24.94 / 25.37 | 27.8 / 28.0 | [match_v5p2-gate-2s.md](benchmark-results/match_v5p2-gate-2s.md) |
| `v5p2-auto-15t-vs-8t-500ms` | 1,180 | `P2 8t (cap=2)` | `P2 15t (cap=4)` | **49.7 %** (47.8–51.7 %) | **−2** (−15 to +12) | 39.7 % | −0.1 ± 0.2 | 22.91 / 22.90 | 26.2 / 26.4 | [match_v5p2-auto-15t-vs-8t-500ms.md](benchmark-results/match_v5p2-auto-15t-vs-8t-500ms.md) |

### 3. Gate P2 Verification (Final Phase 2 Code, Switches Removed, 0 Warnings)

- **C++ presets (`ctest --preset <P> --output-on-failure`):**
  - `clang-release`: 2/2 (`stello_tests` `28.20 s`, `stello_cli_version` `0.50 s`).
  - `clang-native`: 2/2 (`stello_tests` `26.81 s`, `stello_cli_version` `0.89 s`).
  - `msvc-release`: 2/2 (`stello_tests` `29.05 s`, `stello_cli_version` `0.58 s`).
  - `msvc-native`: 2/2 (`stello_tests` `27.62 s`, `stello_cli_version` `0.46 s`).
  - `clang-sanitized`: 2/2 (`stello_tests` `53.20 s`, `stello_cli_version` `8.72 s`, 0 ASan/UBSan findings).
- **.NET Release build & sequential `--no-build` tests (`-p:StelloWasmEngine=false`):** `0 Warning(s), 0 Error(s)`; `Stello.Engine.Tests` `499 passed, 0 failed, 0 skipped`; `Egaroucid.Engine.Tests` `100 passed, 0 failed, 8 skipped` (`TaggedV30_*`); `Stello.Net.Tests` `116 passed, 0 failed, 0 skipped`.
- **`Stello.Web` Release WASM build & Node tests:** `0 Warning(s), 0 Error(s)`; `node --test Stello.Cpp/tests/wasm/engine.test.mjs Stello.Cpp/tests/wasm/engine-mt.test.mjs`: `19 passed, 0 failed, 0 skipped` (`2924.3 ms`).
- **Final idle-CPU benchmarks (`Stello.Benchmark`, `hash-bits=19`, 100 % exact moves and scores):**
  - `solve-bench --from 40 --to 49 --threads 1 --passes 1`: `1,115,666,701` nodes, `17.3 s`, `64.65M` nps.
  - `solve-bench --from 40 --to 49 --threads 8 --passes 3`: `1,416,798,042` nodes, `3.542 s`, `399.98M` nps (`4.88×` vs `1t`; up from `4.425 s` pre-Phase-2).
  - `solve-bench --from 40 --to 49 --threads 15 --passes 3`: `1,539,037,740` nodes, `3.127 s`, `492.19M` nps (`5.53×` vs `1t`).
  - `solve-bench --from 40 --to 49 --threads 16 --passes 3`: `1,529,375,402` nodes, `3.042 s`, `502.70M` nps (`5.69×` vs `1t`; up from `3.830 s` pre-Phase-2).
  - `mid-bench --depth 16 --passes 4`: `8t` `105,106,044` nodes `1.645 s` (`63.89M` nps); `15t` `124,361,114` nodes `1.552 s` (`80.15M` nps); `16t` `126,928,671` nodes `1.588 s` (`79.93M` nps, `-3.5 %` faster than `8t`).
