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
