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
