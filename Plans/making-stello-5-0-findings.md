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
  - `solve-bench --from 40 --to 49 --threads 4 --passes 3`: `1,429,365,105` nodes, `6.540 s`, `218.57M` nps (`2.65×` vs `1t`).
  - `solve-bench --from 40 --to 49 --threads 8 --passes 3`: `1,416,798,042` nodes, `3.542 s`, `399.98M` nps (`4.88×` vs `1t`; up from `4.425 s` pre-Phase-2).
  - `solve-bench --from 40 --to 49 --threads 15 --passes 3`: `1,539,037,740` nodes, `3.127 s`, `492.19M` nps (`5.53×` vs `1t`).
  - `solve-bench --from 40 --to 49 --threads 16 --passes 3`: `1,529,375,402` nodes, `3.042 s`, `502.70M` nps (`5.69×` vs `1t`; up from `3.830 s` pre-Phase-2).
  - `mid-bench --depth 16 --passes 4`: `8t` `105,106,044` nodes `1.645 s` (`63.89M` nps); `15t` `124,361,114` nodes `1.552 s` (`80.15M` nps); `16t` `126,928,671` nodes `1.588 s` (`79.93M` nps, `-3.5 %` faster than `8t`).

### 4. Endgame Thread Scaling vs Egaroucid C++ (Ryzen 7 7800X3D)

Same-machine comparison on FFO `#40–49` (10 positions, no book, 100 % exact moves and scores). Historical `Stello 3.0` (`docs/08-endgame-solver.md`, earlier CPU) and `Stello 4.0` (`docs/28-stello-4-0-the-engine-in-cpp.md`, 7800X3D) rows are included alongside live same-day measurements of `Stello 5.0 P2` (`clang-release`, `hash-bits=19`) and `Egaroucid 7.8.2` (`Egaroucid-main`, built in scratch with Clang 23.1.5 `-O2 -march=native -mtune=native`, `-l 60 -hash 21 -nobook`, aggregated by best per-position time across 3 passes to match `Stello.Benchmark solve-bench`).

| Threads | Stello 3.0 (prev CPU) | Stello 4.0 (7800X3D) | Stello 5.0 P2 (7800X3D) | Stello 5.0 Nodes / NPS | Egaroucid 7.8.2 Clang `-hash 21` (7800X3D) | Egaroucid Nodes / NPS |
|---:|---:|---:|---:|---:|---:|---:|
| `1` | 25.6 s (`1.00×`) | 22.488 s (`1.00×`) | **17.3 s** (`1.00×`) | 1,115.7M / **64.65M** | **9.788 s** (`1.00×`) | 505.4M / 51.63M |
| `4` | 12.9 s (`1.98×`) | — | **6.540 s** (`2.65×`) | 1,429.4M / **218.57M** | **3.156 s** (`3.10×`) | 537.0M / 170.16M |
| `8` | 7.8 s (`3.29×`) | 6.132 s (`3.67×`) | **3.542 s** (`4.88×`) | 1,416.8M / **399.98M** | **1.859 s** (`5.27×`) | 556.4M / 299.29M |
| `15` | — | — | **3.127 s** (`5.53×`) | 1,539.0M / **492.19M** | **1.492 s** (`6.56×`) | 589.4M / 395.03M |
| `16` | 6.45 s (`3.97×`) | 5.307 s (`4.24×`) | **3.042 s** (`5.69×`) | 1,529.4M / **502.70M** | **1.384 s** (`7.07×`) | 549.6M / 397.09M |

Additional `Egaroucid 7.8.2` observations on the same 7800X3D (`FFO #40–49`, 3 passes):
- Single-pass fastest / all-3-pass totals (`Clang -hash 21`): `1t` `9.841 s` (`9.869 / 9.841 / 10.080 s`), `4t` `3.170 s` (`4.251 / 3.170 / 4.290 s`), `8t` `1.936 s` (`2.400 / 2.432 / 1.936 s`), `15t` `1.515 s` (`1.914 / 1.515 / 1.575 s`), `16t` `1.445 s` (`1.445 / 1.486 / 1.446 s`). (MSVC 19.44 `/O2 /arch:AVX2` at `8t -hash 21` ran `2.178 s` fastest pass; Clang `-hash 25` at `8t` ran `1.873 s` fastest pass.)
- **Where Stello 5.0 stands vs Egaroucid:**
  - **Raw throughput (NPS):** Stello is **`+25–34 %` faster per node** at every thread count (`64.65M` vs `51.63M` at `1t`, `399.98M` vs `299.29M` at `8t`, `502.70M` vs `397.09M` at `16t`).
  - **8-core scaling (`1t -> 8t`):** Stello reaches **`4.88×`** vs Egaroucid **`5.27×`** (and **`5.08×`** using Egaroucid's single-pass `9.841 / 1.936`), nearly closing the physical-core scaling gap from Stello 4.0 (`3.67×`).
  - **Where the remaining `1.9–2.2×` wall-time gap comes from:** Primarily **`1t` endgame pruning & move ordering** (`1,115.7M` vs `505.4M` nodes, `2.21×` — Phase 3/4 scope), plus lower parallel search-overhead inflation (`+27–38 %` extra nodes on `4–16t` in Stello vs `+6–17 %` on best passes in Egaroucid, alongside Egaroucid's Lazy SMP + YBWC scaling beyond `8` threads: `1.859 s -> 1.384 s` vs `3.542 s -> 3.042 s`).

---

## Phase 3 — Search: Item 1 (Multi-ProbCut, Rejected at Gate P3)

### 1. Error Model Refit (`Training data/mpc-values.csv`, 11,639 Clean Positions)

- Fixed the `"Traning data"` -> `"Training data"` default folder typo across `Stello.Trainer` (`ConvertCommand.cs`, `MpcCommand.cs`, `TrainerProgram.cs`, `TrainingData.cs`) and `Stello.Benchmark` (`EvalCommands.cs`, `ParityCommands.cs`, `Program.cs`), and added per-`(shallow, deep)` linear fit reporting to `Stello.Trainer mpc --fit`.
- Dropping the `61` single-legal-move all-zero rows from `Training data/mpc-values.csv` (`11,639` valid rows across `14..52` discs, depths `0..13`) exposed why the legacy fit collapsed at high search depths (`d >= 17`):

| Model | Formula ($x = \text{discs}/64$, $s = \text{shallow}$) | Fit Error (Clean Cells) | Deep Behavior ($x = 34/64$) |
|---|---|---|---|
| `old` (current default) | $1.8871 + 2.9543x + 0.0660x^2 - 0.6749s + 0.2778d$ | 9.0 % avg, 32.9 % worst | Linear negative slope collapses $\sigma$ at $d \ge 17$ ($2.12$ at $d=17$, $1.64$ at $d=25$) |
| `refit` (`mpc --fit`) | $1.4547 + 4.0093x - 1.6181x^2 - 0.4457s + 0.2048d$ | 5.0 % avg, 20.7 % worst (117 cells) | $d=20 \to 2.77$; still drifts down linearly at extreme $d$ |
| `powergap` ($d \ge 3$ fit) | $-0.0860 + 3.8174x - 1.3981x^2 + 2.1578\sqrt{\frac{d-s}{s+1}} - 0.2540\sqrt{d-s}$ | 4.6 % avg, 19.1 % worst | Captures $4k+2$ parity step ($d=14 \to 3.14$), levels out around $2.76$–$2.80$ for $d \ge 16$ |
| `pair` | Per-pair $a_d + b_d x$ for $d = 2..13$, `powergap` for $d \ge 14$ | 2.1 %–5.3 % avg per depth ($d=2..13$) | Exact empirical $d \le 13$ + `powergap` for $d \ge 14$ |

### 2. Match Evaluation (`8,340` Games, `1t` `--workers 8`, `hash-bits=19`)

Temporary `SearchOptions` / `opt=` switches (`mpc-model=`, `mpc-early=`, `mpc-late=`, linear disc ramp across `16..48` discs) were tested across `10` matches (`500 ms/move` and `2,000 ms/move`):

| Match | Games | Side A | Side B | Score (B, 95 % CI) | Elo (95 % CI) | LOS | Discs/g | Avg Depth (A / B) | First Solve (A / B) | Wall Time |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---:|
| `p3-pair-vs-old-500ms` | 1,180 | `old` (`500 ms`) | `pair` (`500 ms`) | 50.3 % (48.4–52.3 %) | +2 (−11 to +16) | 63.6 % | +0.1 ± 0.2 | 19.21 / 19.10 | 23.6 / 23.6 | [30m 53s](benchmark-results/match_p3-pair-vs-old-500ms.md) |
| `p3-powergap-vs-old-500ms` | 600 | `old` (`500 ms`) | `powergap` (`500 ms`) | 52.0 % (49.5–54.5 %) | +14 (−4 to +32) | 93.8 % | +0.3 ± 0.3 | 19.21 / 19.11 | 23.5 / 23.6 | [15m 43s](benchmark-results/match_p3-powergap-vs-old-500ms.md) |
| `p3-powergap-mpc85-vs-100-500ms` | 600 | `powergap t=1.00` | `powergap t=0.85` | 49.5 % (46.7–52.3 %) | −3 (−23 to +16) | 36.2 % | −0.1 ± 0.3 | 19.14 / 19.39 | 23.6 / 23.6 | [15m 41s](benchmark-results/match_p3-powergap-mpc85-vs-100-500ms.md) |
| `p3-powergap-mpc90-vs-100-500ms` | 600 | `powergap t=1.00` | `powergap t=0.90` | 49.1 % (46.3–51.8 %) | −6 (−25 to +13) | 25.6 % | −0.1 ± 0.3 | 19.12 / 19.29 | 23.7 / 23.7 | [15m 39s](benchmark-results/match_p3-powergap-mpc90-vs-100-500ms.md) |
| `p3-powergap-mpc110-vs-100-500ms` | 600 | `powergap t=1.00` | `powergap t=1.10` | 48.0 % (45.3–50.7 %) | −14 (−33 to +5) | 7.5 % | −0.2 ± 0.3 | 19.07 / 18.90 | 23.6 / 23.6 | [15m 42s](benchmark-results/match_p3-powergap-mpc110-vs-100-500ms.md) |
| `p3-powergap-early85-late115-500ms` | 600 | `powergap 100/100` | `powergap 85/115` | 50.4 % (47.6–53.3 %) | +3 (−17 to +23) | 61.3 % | +0.1 ± 0.3 | 19.14 / 19.17 | 23.7 / 23.7 | [15m 40s](benchmark-results/match_p3-powergap-early85-late115-500ms.md) |
| `p3-powergap-early115-late85-500ms` | 600 | `powergap 100/100` | `powergap 115/85` | 47.7 % (44.9–50.4 %) | −16 (−35 to +3) | 4.8 % | −0.1 ± 0.3 | 19.05 / 19.03 | 23.6 / 23.6 | [15m 41s](benchmark-results/match_p3-powergap-early115-late85-500ms.md) |
| `p3-powergap-early90-late110-500ms` | 600 | `powergap 100/100` | `powergap 90/110` | 49.9 % (47.2–52.7 %) | −1 (−20 to +18) | 47.6 % | +0.1 ± 0.3 | 19.53 / 19.55 | 23.9 / 23.9 | [15m 34s](benchmark-results/match_p3-powergap-early90-late110-500ms.md) |
| `p3-powergap-vs-old-1180-500ms` | 1,180 | `old` (`500 ms`) | `powergap` (`500 ms`) | 51.6 % (49.7–53.5 %) | +11 (−2 to +24) | 95.3 % | +0.1 ± 0.2 | 19.70 / 19.58 | 24.0 / 24.0 | [30m 22s](benchmark-results/match_p3-powergap-vs-old-1180-500ms.md) |
| `p3-powergap-vs-old-2s` | 1,180 | `old` (`2,000 ms`) | `powergap` (`2,000 ms`) | 50.1 % (48.4–51.8 %) | +1 (−11 to +13) | 55.8 % | +0.1 ± 0.2 | 22.20 / 22.00 | 25.8 / 25.8 | [1h 52m](benchmark-results/match_p3-powergap-vs-old-2s.md) |

- **Decision (Gate P3):** Rejected. Although `powergap` reached `51.6 %` (`+11` Elo, `95.3 %` LOS) at `500 ms`, its lower bound stayed at `49.7 %` and at `2,000 ms` its wider deep thresholds cost `−0.20` plies (`22.00` vs `22.20`), yielding `50.1 %` (`+1` Elo). Default `t = 1.00` outperformed tighter (`0.85`, `0.90`) and wider (`1.10`) thresholds, and phase-tilting `t(discs)` did not beat uniform `100/100` (while loosening late midgame `115/85` hurt at `47.7 %`, LOS `4.8 %`). Temporary `probcut_model` / `mpc-early` / `mpc-late` switches were reverted cleanly; the `"Training data"` path fixes and `MpcCommand` per-pair output were kept.

---

## Phase 3 — Search: Item 2 (Second-Best Move in the Transposition Table, Rejected at Gate P3)

- **Setup:** Added `Entry::mv2` (default `-1`, packed into unused 7 bits `26..31` and `39` of the existing 64-byte `Slot`), preserving the previous matching entry's `mv` in `mv2` on equal-or-deeper `store_at_root` writes without letting shallow results modify deeper entries. Gated behind a temporary `opt=second-move=off|mid|both` switch (`mid`: midgame `search_inc` + `order_by_evaluation`; `both`: midgame + endgame `end_window` / `end_deep`).
- **Speed probes (`off` baseline vs `mid` / `both`, `hash-bits=19`, `1t`):**
  - `mid-bench --depth 14 --passes 3`: `29,307,358` nodes in `2.026 s` (`14.46M` nps) vs `29,779,568` in `2.032 s` (`14.65M` nps) — `−1.59 %` nodes, `−0.3 %` time.
  - `mid-bench --depth 16 --passes 1`: `78,015,279` nodes in `5.691 s` (`13.71M` nps) vs `77,867,024` in `5.445 s` (`14.30M` nps) — `+0.19 %` nodes, `+4.5 %` time.
  - `solve-bench --from 40 --to 49 --threads 1 --passes 1` (`both`): `1,115,665,034` nodes in `17.1 s` (`65.09M` nps) vs `1,115,666,701` in `17.2 s` (`64.83M` nps) — `−1,667` nodes (`−0.00015 %`).
- **Match evaluation (`2,960` games, `1t` `--workers 8`, `hash-bits=19`):**

| Match | Games | Side A | Side B | Score (B, 95 % CI) | Elo (95 % CI) | LOS | Discs/g | Avg Depth (A / B) | First Solve (A / B) | Wall Time |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---:|
| `p32_secondmove_mid_500ms_1180g` | 1,180 | `off` (`500 ms`) | `mid` (`500 ms`) | 50.7 % (48.9–52.5 %) | +5 (−8 to +17) | 77.1 % | +0.1 ± 0.2 | 19.68 / 19.68 | 24.0 / 24.0 | [30m 22s](benchmark-results/match_p32_secondmove_mid_500ms_1180g.md) |
| `p32_secondmove_both_500ms_1180g` | 1,180 | `off` (`500 ms`) | `both` (`500 ms`) | 51.4 % (49.5–53.2 %) | +9 (−4 to +23) | 92.0 % | +0.1 ± 0.2 | 19.70 / 19.69 | 24.0 / 24.0 | [30m 19s](benchmark-results/match_p32_secondmove_both_500ms_1180g.md) |
| `p32_secondmove_both_2000ms_600g` | 600 | `off` (`2,000 ms`) | `both` (`2,000 ms`) | 48.7 % (46.6–50.7 %) | −9 (−24 to +5) | 10.1 % | −0.1 ± 0.2 | 22.18 / 22.16 | 25.8 / 25.8 | [57m 19s](benchmark-results/match_p32_secondmove_both_2000ms_600g.md) |

- **Decision (Gate P3):** Rejected. Neither `mid` (`48.9 %` lower bound) nor `both` (`49.5 %` lower bound) cleared Gate P3 at `500 ms`, and confirming `both` at `2,000 ms` dropped to `48.7 %` (`−9` Elo, `10.1 %` LOS) with `−0.02` plies depth (`22.16` vs `22.18`) and `−0.7 %` nps (`23.47M` vs `23.63M`). Because `order_by_evaluation` and `ordering_value` already rank non-hash moves well, forcing an older `mv2` ahead of them adds no depth while paying a small TT slot-load/NPS cost. All `second-move` changes were reverted cleanly.

---

## Phase 3 — Search: Item 3 (Soft Time Limit `soft-pct=`, Rejected at Gate P3)

- **Setup:** Used the existing `soft-pct=` (`SearchOptions.SoftTimePercent`) switch to test higher soft midgame time caps (`80 %`, `90 %`, `100 %` of move budget) against the default `2/3` (`66.7 %`) rule, where Stello 5.0 averaged `261 / 501 ms` (`52 %`) per searched move at `500 ms/move`.
- **Match evaluation (`3,540` games, `500 ms/move`, `1t` `--workers 8`, `hash-bits=19`):**

| Match | Games | Side A | Side B | Score (B, 95 % CI) | Elo (95 % CI) | LOS | Avg Depth (A / B) | First Solve (A / B) | Move Time (A / B) | Wall Time |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---:|
| `p33_soft80_500ms_1180g` | 1,180 | `soft67` (`67 %`) | `soft80` (`80 %`) | 50.0 % (48.5–51.6 %) | 0 (−10 to +11) | 52.1 % | 19.75 / 19.86 | 24.0 / 24.0 | 261 / 275 ms | [31m 11s](benchmark-results/match_p33_soft80_500ms_1180g.md) |
| `p33_soft90_500ms_1180g` | 1,180 | `soft67` (`67 %`) | `soft90` (`90 %`) | 51.2 % (49.5–52.9 %) | +9 (−3 to +20) | 92.0 % | 19.73 / 19.88 | 24.0 / 24.1 | 261 / 281 ms | [31m 28s](benchmark-results/match_p33_soft90_500ms_1180g.md) |
| `p33_soft100_500ms_1180g` | 1,180 | `soft67` (`67 %`) | `soft100` (`100 %`) | 50.0 % (48.2–51.8 %) | 0 (−12 to +12) | 50.0 % | 19.70 / 19.85 | 24.0 / 24.0 | 261 / 282 ms | [31m 36s](benchmark-results/match_p33_soft100_500ms_1180g.md) |

- **Decision (Gate P3):** Rejected. Raising `soft-pct` to `80–100 %` increased average move time by `+14–21 ms` (`+5–8 %`) and average midgame depth by `+0.11–0.15` plies, but none cleared the `50 %` lower-bound gate (`soft80` `50.0 %`, `soft90` `51.2 %` CI `49.5–52.9 %`, `soft100` `50.0 %`). Default `2/3` (`66.7 %`) stays.

---

## Phase 3 — Search: Item 4 (Aspiration Windows, Rejected at Gate P3)

- **Setup:** Gated behind a temporary `opt=aspiration=<N>` (`0 = off`, `<N>` = half-width in `1/100` disc units starting from `look >= 3` / `depth >= 4`, doubling on fail-low/fail-high up to `1600` before falling back to `[-ROOT_WINDOW, +ROOT_WINDOW]` with fail-low `best`/`improvements`/`split` preservation).
- **Speed probes (`mid-bench --passes 1`, `hash-bits=19`, `1t`):**
  - `d=14`: `off` `29,779,568` (`2.017 s`), `100` (`±1d`) `30,312,539` (`+1.79 %`), `150` (`±1.5d`) `29,398,598` (`−1.28 %`), `200` (`±2d`) `29,543,148` (`−0.79 %`), `300` (`±3d`) `29,517,484` (`−0.88 %`).
  - `d=16`: `off` `77,867,024` (`5.319 s`), `100` `81,009,359` (`+4.04 %`), `150` `79,000,875` (`+1.46 %`), `200` `77,571,337` (`−0.38 %`), `300` `77,518,185` (`−0.45 %`), `400` (`±4d`) `76,239,011` (`−2.09 %`, `5.135 s`), `500` (`±5d`) `77,126,721` (`−0.95 %`, `5.376 s`).
  - `d=18`: `off` `212,290,236` (`14.5 s`), `200` `208,938,543` (`−1.58 %`, `14.1 s`), `300` `212,577,991` (`+0.14 %`, `14.3 s`), `400` `209,112,936` (`−1.50 %`, `14.1 s`).
- **Match evaluation (`2,360` games, `500 ms/move`, `1t` `--workers 8`, `hash-bits=19`):**

| Match | Games | Side A | Side B | Score (B, 95 % CI) | Elo (95 % CI) | LOS | Discs/g | Avg Depth (A / B) | First Solve (A / B) | Wall Time |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---:|
| `p34_asp400_500ms_1180g` | 1,180 | `asp_off` (`500 ms`) | `asp_400` (`±4d`) | 50.3 % (48.7–52.0 %) | +2 (−9 to +14) | 65.7 % | 0.0 ± 0.2 | 19.74 / 19.74 | 24.0 / 24.0 | [30m 22s](benchmark-results/match_p34_asp400_500ms_1180g.md) |
| `p34_asp200_500ms_1180g` | 1,180 | `asp_off` (`500 ms`) | `asp_200` (`±2d`) | 50.3 % (48.5–52.2 %) | +2 (−10 to +15) | 64.1 % | +0.1 ± 0.2 | 19.76 / 19.72 | 24.1 / 24.1 | [30m 17s](benchmark-results/match_p34_asp200_500ms_1180g.md) |

- **Decision (Gate P3):** Rejected. Narrow aspiration windows (`±1` to `±1.5` discs) *increased* `mid-bench` nodes at depth 16 (`+4.04 %` and `+1.46 %`) due to odd/even parity and Multi-ProbCut score swings causing re-searches. Wider windows (`±2` and `±4` discs) cut only `0.4–2.1 %` of `mid-bench` nodes at depths 16–18 and produced `50.3 %` (`+2` Elo) in both `1,180`-game `500 ms` matches with zero effective depth gain (`19.74` vs `19.74`; `19.72` vs `19.76`), failing Gate P3 (`lower bound > 50 %`). All temporary `aspiration` changes were reverted cleanly.

---

## Phase 3 — Search: Item 5 (Transposition Table Size at 2 s with 8 Threads, Rejected)

- **Setup:** Tested doubling the transposition table capacity by 4× (`hash-bits=21`, `256 MB`: `2 × 2^21 × 64 B` slots) against the default (`hash-bits=19`, `64 MB`: `2 × 2^19 × 64 B` slots) at `2,000 ms/move` with `8` threads (`--workers 1`, `600` games).
- **Match evaluation (`600` games, `2,000 ms/move`, `8t` `--workers 1`):**

| Match | Games | Side A | Side B | Score (B, 95 % CI) | Elo (95 % CI) | LOS | Discs/g | Avg Depth (A / B) | First Solve (A / B) | NPS (A / B) | Wall Time |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `p35_hash21_vs_19_2000ms_8t_600g` | 600 | `hash19` (`64 MB`) | `hash21` (`256 MB`) | 49.0 % (46.6–51.4 %) | −7 (−23 to +9) | 20.3 % | 0.0 ± 0.2 | 25.48 / 25.09 | 27.8 / 27.7 | 205.94M / 186.72M | [6h 07m + 41m](benchmark-results/match_p35_hash21_vs_19_2000ms_8t_600g.md) |

- **Decision (Gate P3):** Rejected (`hash-bits=19` stays default). Even at `2 s/move` with `8` threads, increasing the TT from `64 MB` (`hash-bits=19`) to `256 MB` (`hash-bits=21`) reduced NPS by `−9.3 %` (`186.72M` vs `205.94M`), reduced average midgame depth by `−0.39` plies (`25.09` vs `25.48`), and scored `49.0 %` (`−7` Elo, LOS `20.3 %`). On the 7800X3D (`96 MB` L3 V-Cache), `64 MB` fits in L3 while `256 MB` spills to DRAM.

---

## Phase 3 — Search: Item 6 (Root Clog Search, Rejected at Gate P3)

- **Setup:** Implemented a pure-bitboard root clog search behind temporary `opt=clog=<N>` (`0 = off`, `1..16` max plies; capped on timed searches to `1/8` of remaining hard budget; ascending opponent-mobility ordering, immediate `flips == opponent` `+64` wipeout, lazy `endgame::stable::of` `beta` bound, excluding solved clog moves from midgame iterations and seeding `alpha` / `best_move` with `WIN_SCORE` scaling or immediate `ScoreKind::Exact` if all moves or `+64` are solved).
- **Speed probes (`mid-bench --passes 1`, `hash-bits=19`, `1t`):**
  - `d=14`: `off` `29,779,568` (`1.998 s`), `clog=4` `31,467,681` (`2.023 s`, `+0.25 ms/pos`), `clog=5` `49,677,844` (`2.141 s`, `+1.43 ms/pos`).
  - `d=16`: `off` `77,867,024` (`5.235 s`), `clog=5` `97,765,300` (`5.481 s`, `+2.46 ms/pos`).
- **Match evaluation (`1,180` games, `500 ms/move`, `1t` `--workers 8`, `hash-bits=19`):**

| Match | Games | Side A | Side B | Score (B, 95 % CI) | Elo (95 % CI) | LOS | Discs/g | Avg Depth (A / B) | First Solve (A / B) | Wall Time |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---:|
| `p36_clog5_500ms_1180g` | 1,180 | `clog_off` (`500 ms`) | `clog_5` (`clog=5`) | 49.2 % (47.7–50.6 %) | −6 (−16 to +4) | 12.1 % | −0.1 ± 0.2 | 19.73 / 19.71 | 24.1 / 24.1 | [30m 20s](benchmark-results/match_p36_clog5_500ms_1180g.md) |

- **Decision (Gate P3):** Rejected. With no selective endgame and exact solves already starting around `24.1` empties on average (`~19.7` plies midgame depth), pre-searching `5` plies at the root pays a small depth tax (`−0.02` plies) and scores `49.2 %` (`−6` Elo, LOS `12.1 %`), failing Gate P3 (`lower bound > 50 %`). All temporary `clog` changes were reverted cleanly.

---

## Phase 4 — Endgame & Transition: Item 1 (`end_local` Pattern Move Ordering, Rejected at Gate P4)

- **Setup:** Added a `4`-pattern endgame feature set `end4` (`edge+2x`, `triangle`, `corner+block`, `cross`, `16` features across `4 × 59,049` raw indices) and disc-range filtering (`--min-discs`, `--max-discs`) in `Stello.Trainer`. Trained `1` stage (`--min-discs 52 --max-discs 54 --smooth 1 --max-per-discs 2000000 --units 16 --min-appear 15`) on `10,000,000` positions from `Training data/stello-bin` (`44 s`: train MAE `5.556`, validation MAE `5.610`, RMSE `7.161`, `92,691` bytes gzip as `stello-end-local.bin`). Implemented incremental `16`-feature `LocalPatternState` (portable, BMI2 `state_pext`, single-`YMM` AVX2 update) for `end_local` at `11–13` empties behind temporary `opt=end-local=<N>` (`0 = off`, weighted mobility + pattern disc estimate).
- **Speed probes (`solve-bench`, `hash-bits=19`, `100 %` exact moves and scores):**
  - Weight sweep on `FFO #40–49` (`1t`, `1` pass, before `state_pext`): baseline `off` `1,115,666,701` nodes in `17.3 s` (`64.35M` nps); `w=8` minimized time/nodes at `913.8M` / `15.9 s` (while `12–13` empties only at `w=8` was slower at `970,691,078` / `16.4 s`).
  - `FFO #40–49`, `1t` (`3` passes, with `state_pext`, `w=8`): `913,845,572` nodes (`−18.1 %`) in `15.7 s` (`−9.6 %`), `58.32M` vs `64.35M` nps.
  - `FFO #40–49`, `8t` (`3` passes, `w=8`): `1,178,186,120` nodes (`−20.9 %` vs `1,489,727,223`) in `3.348 s` (`−11.1 %` vs `3.767 s`), `351.88M` vs `395.44M` nps.
  - `FFO #40–59`, `8t` (`1` pass, `w=8`): `27,227,038,461` nodes (`−16.3 %` vs `32,523,926,257`) in `84.3 s` (`−5.2 %` vs `88.9 s`), `323.09M` vs `365.78M` nps.
- **Match evaluation (`1,180` games, `500 ms/move`, `1t` `--workers 8`, `hash-bits=19`):**

| Match | Games | Side A | Side B | Score (B, 95 % CI) | Elo (95 % CI) | LOS | Discs/g | Avg Depth (A / B) | First Solve (A / B) | NPS (A / B) | Wall Time |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `p41_endlocal8_500ms_1180g` | 1,180 | `endlocal_off` (`500 ms`) | `endlocal_8` (`end-local=8`) | 48.3 % (46.8–49.8 %) | −12 (−22 to −1) | 1.4 % | −0.1 ± 0.2 | 19.71 / 19.72 | 24.0 / 24.1 | 21.67M / 20.17M | [30m 18s](benchmark-results/match_p41_endlocal8_500ms_1180g.md) |

- **Technical diagnosis:**
  - `end_local` is never called during midgame or Multi-ProbCut (`midgame_iterations -> search_root -> search_inc / try_probcut_inc`); it runs solely inside `solve_iteration` (`solve_root -> end_window / end_deep -> end_null` at `empties <= 13`).
  - Match `NodesPerSecond` (`20.17M` vs `21.67M`, `−6.9 %`) blends midgame (`~15M` nps) and solver (`~58–64M` nps). Because `end_local` enters from `end_deep` with `eval_state == nullptr` at `13` empties and computes `state_of_local(own, opponent)` from scratch, pattern-state extraction/evaluation overhead eats roughly half of the `−18.1 %` node reduction (`58.32M` vs `64.35M` nps, `−9.6 %` wall time on `FFO #40–49`). Searching `~18 %` fewer `60M`-nps solver nodes (at `58.3M` vs `64.4M` nps during `24–27`-empty WLD/exact solves, including partial WLD solves that hit the `500 ms` hard cap) reduces the solver share of total game nodes, pulling blended whole-game NPS down.
  - Because each additional empty square multiplies the endgame solver tree by `~2.7×`, a `9.6 %` (`1.10×`) FFO speedup shifts the solve horizon by only `log_{2.7}(1.10) ≈ +0.1` empties (`24.0 -> 24.1`), yielding no Elo gain at `500 ms` (`48.3 %`, `−12` Elo, `95 % CI 46.8–49.8 %`) while adding a second embedded weights file (`stello-end-local.bin`, hurting WebAssembly bundle size) and `~400` lines of plumbing across `19` files. By contrast, Phase 4, Item 2 (`mpc-end` selective endgame + estimated solve-time handover) directly targets the `25–30`-empty transition zone where full WLD solves time out.
- **Decision (Gate P4):** Rejected. All engine, book, and benchmark `end_local` changes were reverted cleanly; `--min-discs` / `--max-discs` support in `Stello.Trainer` was kept.

---

## Phase 4 — Endgame & Transition: Item 2 (Selective Endgame `mpc-end` & Solve Handover, Accepted)

- **Error model (`Stello.Trainer mpc-end` on `plus15`):** Solved `1,040` positions (`38–50` discs, `80`/disc count, `15 s` solve cap -> `972` solved in `3m 46s`). Restricted to Egaroucid's same-parity shallow depth `s = ((empties / 3) & ~1) + (empties & 1)`, the RMSE of `exact − shallow` is `5.43` discs at `14` empties (`s = 4`) down to `4.20` at `26` empties (`s = 8`), fitted linearly by `σ_end = max(2.0, 1.0376 + 5.4960·x − 0.0378·s)` (`x = discs / 64`).
- **Implementation:**
  - Added `try_end_probcut` inside `end_window` / `end_deep` (`empties >= 16`) in [search_impl.hpp](../../Stello.Cpp/src/endgame/search_impl.hpp), probing `search_inc` (`in_probcut = true`, `look = s - 1`, `WIN_SCORE` terminal scores normalized to disc units) against precomputed `EndProbCutErrors` (`stages 1..6`: `74 % / 1.13`, `88 % / 1.55`, `93 % / 1.81`, `98 % / 2.32`, `99 % / 2.57`, `99.9 % / 3.29`). Selective entries store `depth = stage` (`1..6`) in `endgame_table`, while exact solves store `depth = empties` (`>= 7`), so ascending stages share `hash_move` and never corrupt or overwrite exact entries. `TimeControlMode::Solve` stays 100 % exact (`end_mpc_stage = 0`).
  - After midgame reaches `empties - 8` (`TimePerMove` and `TimePerGame` when `selective` is enabled and `empties >= 16`), `iterate()` runs `3` full-window `[-64, +64]` selective stages (`74 % -> 88 % -> 98 %`) within the midgame soft budget before entering exact `WLD -> Exact`; if the soft budget expires during selective passes it returns the last completed stage's disc-maximizing `Heuristic` move cleanly. Live `ScoreKind::Heuristic` progress callbacks in `solve_root` / `Kernel::split` are scaled by `PATTERN_UNITS_PER_DISC` (`100`). In `TimePerGame`, `for_solver` (`rest / 2`) is also guarded by `estimate_wld_ms` so long-clock games do not burn half the remaining clock on unsolvable `30+`-empty exact solves.
- **`mid-quality` ablations (`midgame-exact.txt`, `28`/`26`/`24` empties):**
  - `500 ms` (`300` positions): `hybrid` (`74 -> 88 -> 98 %`) cut disc loss from `0.52 ± 0.16` (`84.3 %` best move) to `0.33 ± 0.12` (`89.0 %` best move; paired `base − hybrid = +0.19 ± 0.12`), driven by `24` empties dropping from `0.68 ± 0.35` (`82.0 %` best, `WLD` `+2` vs `+18` cutoff) to `0.08 ± 0.10` (`97.0 %` best). Skipping `mpc-end` via `solve-est` (`smart`, `smart2`, `mpc_est`) reverted part of that gain by jumping straight into `[-1, +1]` WLD too early.
  - `2,000 ms` (`150` positions): `hybrid` halved disc loss from `0.27 ± 0.17` (`92.7 %` best move, `1,744 ms`) to `0.13 ± 0.13` (`96.7 %` best move, `1,606 ms`; paired `+0.13 ± 0.12`), cutting `26`-empty loss `9×` (`0.04 ± 0.08`, `98.0 %` best vs `0.36 ± 0.33`, `90.0 %`) while keeping `24` empties at `92 %` solved (`0.04` vs `0.08`).
- **Match evaluation (`2,960` games, `1t` `--workers 8`, `hash-bits=19`):**

| Match | Games | Side A | Side B | Score (B, 95 % CI) | Elo (95 % CI) | LOS | Discs/g | Avg Depth (A / B) | First Solve (A / B) | Wall Time |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---:|
| `p42_hybrid_500ms_1180g` | 1,180 | `mpcend_off` (`500 ms`) | `mpcend_hybrid` (`500 ms`) | 51.5 % (49.9–53.0 %) | +10 (0 to +21) | 97.1 % | +0.5 ± 0.2 | 19.68 / 20.25 | 23.9 / 22.9 | [30m 37s](benchmark-results/match_p42_hybrid_500ms_1180g.md) |
| `p42_smart_500ms_1180g` | 1,180 | `mpcend_off` (`500 ms`) | `mpcend_smart` (`500 ms`) | 50.3 % (48.9–51.8 %) | +2 (−8 to +12) | 67.9 % | +0.1 ± 0.2 | 19.68 / 19.72 | 24.0 / 23.9 | [30m 22s](benchmark-results/match_p42_smart_500ms_1180g.md) |
| `p42_hybrid_2000ms_600g` | 600 | `mpcend_off` (`2,000 ms`) | `mpcend_hybrid` (`2,000 ms`) | 53.2 % (51.5–54.8 %) | +22 (+11 to +34) | 100.0 % | +0.5 ± 0.1 | 22.17 / 23.00 | 25.7 / 25.1 | [57m 45s](benchmark-results/match_p42_hybrid_2000ms_600g.md) |

- **Decision (Gate P4):** Accepted. `mpcend_hybrid` (`74 -> 88 -> 98 %` selective endgame + `TimePerGame` `for_solver` guard) improved `500 ms` (`51.5 %`, `+10` Elo, `LOS 97.1 %`, `+0.5 ± 0.2` discs/g; winning `30–5` on equal-`FirstSolve` pairs) and decisively passed `2,000 ms` (`53.2 %`, `95 % CI 51.5–54.8 %`, `+22` Elo, `LOS 100.0 %`, `+0.5 ± 0.1` discs/g), while leaving exact `Solve` (`FFO #40–59`) untouched. Temporary `opt=mpc-end=`, `opt=end-dist=`, and `opt=solve-est=` switches were removed cleanly.

---

## Phase 5 — Evaluation: Item 1 (`egaroucid16 × 60`, `10 M`, Unified Desktop & WASM Model, Accepted)

- **Trainer & smoothing findings (`Stello.Trainer`):**
  - Scoped stage arrays inside `TrainStage` (`TrainCommand.cs`) to release each stage's `~6.4 GB` encoded data before loading the next stage, eliminated block-modulo and bounds-check overhead in `Fit1`, and parallelized `Error(validation)`.
  - On `60` single-disc stages, smoothing across adjacent disc counts mixes opposite Othello parity: on stage 30 (`35` discs, `209,214` validation positions), `--smooth 2` (`50 M` rows) scored `5.445` MAE / `7.125` RMSE and `--smooth 1` (`30 M` rows, `67 %` opposite parity) scored `5.635` / `7.324`, whereas `--smooth 0` (`10 M` exact-disc rows, `--rate 0.15 --patience 15`) reached **`5.359` MAE / `7.051` RMSE** in `0:02:05`.
  - Trained all `60` stages (`egaroucid16`, `64` features, `320,193` folded weights/stage) in `0:31:12` (`--min-discs 12 --smooth 0 --rate 0.15 --patience 15 --loops 1000 --units 16 --min-appear 10`) -> `8,885,734` bytes gzip ([eval-p5-e16-s60-10m.md](benchmark-results/eval-p5-e16-s60-10m.md)).
  - **Test split (`1,040,276` positions):** **MAE `5.07`, RMSE `6.87`** (vs `plus15 × 10` `5.47` / `7.31` and Egaroucid `5.05` / `6.90` in [v2-p2-eval-p15-s10-m30-u8.md](benchmark-results/v2-p2-eval-p15-s10-m30-u8.md)).
- **MPC & MPC-end fits (`Stello.Trainer --model`, `engine.cpp`):**
  - Midgame (`3,900` positions, `14–52` discs, depths `0–12`): $\sigma = 0.9172 + 5.0552x - 2.4951x^2 - 0.5569s + 0.2567d$ (`6.0 %` avg relative error).
  - Endgame (`967/1,040` solved, `38–50` discs, `15 s` cap, linear fit on `EndShallowDepth`): $\sigma_{\text{end}} = \max(2.0,\; 6.5937 - 0.2985x - 0.2975s)$.
  - Dispatched in `SearchEngine` (`Stello.Cpp/src/search/engine.cpp`) automatically by `PatternSet` (`"egaroucid16"` vs `"plus15"`).
- **Native & WASM speed / bundle measurements:**
  - `clang-release` `mid-bench --depth 12` (`100` pos): `9,496,154` nodes (`−10.2 %`) in `0.664 s` (`14.30M` nps) vs `10,579,515` nodes in `0.684 s` (`15.48M` nps); `mid-bench --depth 10`: `3,144,099` nodes (`−15.4 %`) in `0.245 s` (`12.83M` nps); `solve-bench` (`FFO #40–49`): `1,057,623,116` nodes (`−5.2 %`) in `15.9 s` (`66.35M` nps).
  - WebAssembly bundles (`Stello.Web/wwwroot`): `engine` `9,121,683` B raw / `8,966,487` B gzip / `8,948,321` B brotli (vs `1,210,594` / `1,070,418` / `1,054,760` B); `engine-mt` `9,152,030` B raw / `8,978,382` B gzip / `8,957,898` B brotli. In Node `v24.19.0` (ST WASM, `hash=19`), `init(19)` took `390.2 ms` (vs `68.3 ms`), linear memory was `221.3 MiB` (vs `94.8 MiB`), `mid-d12` ran `8,541,092` nodes in `0.970 s` (`8.81M` nps vs `10.54M` nps in `0.996 s`), and `FFO #40–44` ran `95,540,217` nodes in `2.361 s` (`40.46M` nps).
- **Timed matches (`1t`, `--workers 8`, `hash-bits=19`, vs `plus15 × 10`):**

| Match | Games | Side A | Side B | Score (B, 95 % CI) | Elo (95 % CI) | LOS | Discs/g | Avg Depth (A / B) | First Solve (A / B) | NPS (A / B) | Wall Time |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| `p51_e16_500ms_1180g` | 1,180 | `p15_base` (`500 ms`) | `e16_10m` (`500 ms`) | **52.9 %** (50.8–55.0 %) | **+20** (+6 to +35) | **99.7 %** | +0.3 ± 0.2 | 20.25 / 20.34 | 23.0 / 23.0 | 21.21M / 20.40M | [30m 35s](benchmark-results/match_p51_e16_500ms_1180g.md) |
| `p51_e16_2000ms_1180g` | 1,180 | `p15_base` (`2,000 ms`) | `e16_10m` (`2,000 ms`) | **52.0 %** (50.1–53.9 %) | **+14** (+1 to +27) | **98.2 %** | +0.3 ± 0.2 | 22.94 / 22.93 | 25.1 / 25.1 | 21.96M / 21.20M | [56m 04s](benchmark-results/match_p51_e16_2000ms_1180g.md) |

- **Decision & landing (Gate P5 Item 1):** Landed `e16-s60-10m.bin` as the single unified [stello-eval.bin](../../Stello.Net/Book/stello-eval.bin) across desktop C++, `.NET`, and WebAssembly (`engine` + `engine-mt`). Updated parity fixtures (`Stello.Cpp/tests/data/`), `test_main.cpp`, `parity_tests.cpp`, `search_tests.cpp` (`1,846,697` nodes), `stello-v3-signatures.txt`, `engine.test.mjs`, `NativeEngineTests.cs` (`24,670` nodes and shared-model parity vs frozen `V2`), and `BookLearnerTests.cs`. Verified `clang-release` CTest (`2/2`, `29.48 s`), `msvc-release` CTest (`2/2`, `32.71 s`), WASM Node tests (`19/19`), `Stello.Engine.Tests` (`499/499`), `Stello.Net.Tests` (`116/116`), and `Egaroucid.Engine.Tests` (`100 passed, 8 skipped`).

---

## Phase 5 — Evaluation: Item 2 (`egaroucid16 + mobility × 60`, Format v3, Accepted)

- **Ablation & leaf speed probe:**
  - Joint Adam ablation (`10 M` train rows, `d25` / `d35` / `d45`): adding legal mobility (`+mob`) improved held-out Test MAE by `−0.106` / `−0.126` / `−0.133` (`−0.130` / `−0.159` / `−0.179` RMSE), capturing `~85–90 %` of full `+all` extra features (`+pot + quad_parity + disc_diff`).
  - C++ `look == 0` leaf probe (`mid-bench` fixed nodes): `+mob` cost `−2.3 %` NPS at `depth 12` (`13.69M` vs `14.01M` nps) and `−0.8 %` NPS at `depth 14` (`14.07M` vs `14.19M` nps), versus `−4.3 %` at `depth 14` for `+mob+pot`.
- **Format v3 & training (`Stello.Trainer --mobility on`):**
  - Extended `EvalWeights` across C# ([EvalWeights.cs](../../Stello.Net/Stello.Engine/Evaluation/EvalWeights.cs)) and C++ ([weights.hpp](../../Stello.Cpp/src/eval/weights.hpp), [weights.cpp](../../Stello.Cpp/src/eval/weights.cpp)) to version `3` with `96` mobility weights per stage (`48` own + `48` opponent legal-move count slots). Wired `eval_after_move_score` / `eval_state_score` into `Kernel<UseAvx2>` ([kernel.hpp](../../Stello.Cpp/src/search/kernel.hpp)), `search_root`/`search_inc`, `order_by_evaluation` (reusing `replies_bb`), `try_probcut_inc` ([engine_impl.hpp](../../Stello.Cpp/src/search/engine_impl.hpp)), and `try_end_probcut` ([search_impl.hpp](../../Stello.Cpp/src/endgame/search_impl.hpp)).
  - Trained `e16-mob-s60-10m.bin` (`66` features, `320,289` weights/stage, `--min-discs 12 --smooth 0 --rate 0.15 --patience 15 --loops 1000 --units 16 --min-appear 10 --mobility on`) in `1:20:54` -> `8,892,085` bytes gzip (`+6,351` B vs Item 1; [eval-p5-e16-mob-s60-10m.md](benchmark-results/eval-p5-e16-mob-s60-10m.md)).
  - **Test split (`1,040,276` positions):** **MAE `4.97`, RMSE `6.74`** (down from `5.07 / 6.87` for Item 1, `5.47 / 7.31` for Stello 4.0 `plus15 × 10`, and `5.05 / 6.90` for Egaroucid).
- **MPC & MPC-end fits (`engine.{hpp,cpp}`):**
  - Midgame (`3,900` positions): $\sigma = 0.9972 + 5.1252x - 2.8746x^2 - 0.5219s + 0.2280d$ (`5.6 %` avg relative error).
  - Endgame (`970/1,040` solved, linear on `EndShallowDepth`): $\sigma_{\text{end}} = \max(2.0,\; 8.0571 - 1.8599x - 0.3890s)$.
  - Dispatched in `SearchEngine` ([engine.cpp](../../Stello.Cpp/src/search/engine.cpp)) by `(PatternSet, has_mobility)` (`egaroucid16 + mobility` vs plain `egaroucid16` vs `plus15`).
  - Also fixed the `TimePerGame` `estimate_wld_ms` handover in [engine_impl.hpp](../../Stello.Cpp/src/search/engine_impl.hpp) by snapshotting `mid_elapsed_ms` and `mid_total_nodes` before `kSelectiveStages` (avoiding `3×` NPS inflation from selective endgame probes) and tightening the growth factor (`1.35 -> 1.6`, threshold `> capped_rem_ms`).
- **Native benchmarks & WASM bundle size (`Stello.Web/wwwroot`):**
  - `clang-release` `mid-bench --depth 12` (`100` pos): `8,201,458` nodes (`−3.5 %` vs Item 1's `8,501,049`) in `0.602 s` (`13.62M` nps vs `0.607 s` / `14.01M` nps); `mid-bench --depth 10`: `3,083,842` nodes in `0.232–0.265 s` (`11.63–13.28M` nps); `solve-bench` (`FFO #40–49`): `1,070,469,673` nodes in `16.4 s` (`65.12M` nps), all exact.
  - WebAssembly bundles (`Stello.Web/wwwroot`): `engine` `9,146,966` B raw / `8,977,277` B gzip / `8,957,275` B brotli (`+8,954` B brotli vs Item 1); `engine-mt` `9,177,430` B raw / `8,988,280` B gzip / `8,967,037` B brotli (`+9,139` B brotli vs Item 1).
- **Timed matches (`1t`, `--workers 8`, `hash-bits=19`, vs Item 1 `e16_item1`):**

| Match | Games | Side A | Side B | Score (B, 95 % CI) | Elo (95 % CI) | LOS | Discs/g | Avg Depth (A / B) | First Solve (A / B) | NPS (A / B) | Wall Time |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| `p52_e16mob_500ms_1180g` | 1,180 | `e16_item1` (`500 ms`) | `e16_mob` (`500 ms`) | **51.8 %** (49.9–53.7 %) | **+13** (−1 to +26) | **96.9 %** | +0.2 ± 0.2 | 20.32 / 20.41 | 23.0 / 23.0 | 20.48M / 19.91M | [30m 42s](benchmark-results/match_p52_e16mob_vs_e16_500ms_1180g.md) |
| `p52_e16mob_2000ms_600g` | 600 | `e16_item1` (`2,000 ms`) | `e16_mob` (`2,000 ms`) | **52.9 %** (50.3–55.6 %) | **+20** (+2 to +39) | **98.4 %** | +0.2 ± 0.2 | 22.96 / 23.13 | 25.1 / 25.1 | 21.28M / 20.72M | [57m 41s](benchmark-results/match_p52_e16mob_vs_e16_2000ms_600g.md) |

- **Decision & landing (Gate P5 Item 2):** Accepted (`4.97 MAE / 6.74 RMSE` beating Item 1 `5.07 / 6.87`, Stello 4.0 `5.47 / 7.31`, and Egaroucid `5.05 / 6.90`; `+13` Elo at `500 ms` `LOS 96.9 %` and `+20` Elo at `2,000 ms` `LOS 98.4 %`; `+9 KB` WASM brotli). Landed `e16-mob-s60-10m.bin` as unified [stello-eval.bin](../../Stello.Net/Book/stello-eval.bin). Embedded Stello 2.0's frozen `plus15 × 10` [stello-eval.bin](../../Stello.Net/Stello.Engine.V2/stello-eval.bin) (`990,568` B) in [Stello.Engine.V2.csproj](../../Stello.Net/Stello.Engine.V2/Stello.Engine.V2.csproj) so frozen `V2` continues to load v2 weights cleanly on clean builds. Updated parity/test fixtures (`Stello.Cpp/tests/data/`, `test_main.cpp`, `parity_tests.cpp`, `search_tests.cpp`, `engine.test.mjs`, `ParityCommands.cs`, `PatternTests.cs`, `NativeEngineTests.cs`, `BookLearnerTests.cs`, `stello-v3-signatures.txt`). Verified `clang-release` CTest (`2/2`, `31.75 s`), `msvc-release` CTest (`2/2`, `35.14 s`), WASM Node tests (`19/19`), `Stello.Engine.Tests` (`500/500`), `Stello.Net.Tests` (`116/116`), and `Egaroucid.Engine.Tests` (`100 passed, 8 skipped`).





