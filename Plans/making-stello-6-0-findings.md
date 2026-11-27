# Stello 6.0 Findings

Working notes, benchmark baselines, and gate records for [making-stello-6-0-plan.md](making-stello-6-0-plan.md) on branch `feature/stello-6.0`.

---

## Environment and Branch Setup

- **Branch:** `feature/stello-6.0`, branched from `main` at commit `caffe06` (`Updated making-stello-6-0-plan.md`, following `v5.0` at `473d0dc`).
- **Primary machine:** AMD Ryzen 7 7800X3D 8-Core Processor (16 logical processors, Zen 4, 96 MB L3, AVX-512VL/CD/DQ/VNNI) + NVIDIA GeForce RTX 4080, Windows x64.
- **Toolchains:**
  - Clang `23.1.5` (`x86_64-pc-windows-msvc`), CMake `3.31` + Ninja, Visual Studio 2022 BuildTools (`vcvars64.bat`).
  - .NET SDK `10.0.401` (`Stello.Net/Stello.Net.slnx`).
  - Node.js `v24.19.0`.

---

## Phase 0 — Set-up, the 5.0 Baseline, and Native External Match Harnesses

*(Gate P0 complete: Items 1–5, `v5.0-ref/` benchmarks, WASM 5.0 ST/MT baseline, external FFO `#40–59` benchmarks, and all `30/30` `starts-v6.txt` baseline matches [`35,220` games] completed and verified.)*

### 1. Preserved 5.0 Reference Engine (`stello-v5`)

- Preserved the release 5.0 `stello_engine.dll` (`stello_version() = 50000`, ABI `3`) as `Stello.Cpp/ref/stello_engine_v5.dll` (git-ignored):
  - Size: `9,418,752` bytes
  - SHA-256: `15DDA751EC045B89F63493F7CBB16B2CBC2C064F5052EFAE79406657458B78F3`
- Added `engine=stello-v5` / `--engine stello-v5` across [StelloLibraryMatchPlayer.cs](../Stello.Net/tools/Stello.Benchmark/StelloLibraryMatchPlayer.cs) (`StelloLibrary.ResolveV5`), [Stello.Benchmark.csproj](../Stello.Net/tools/Stello.Benchmark/Stello.Benchmark.csproj), [MatchCommands.cs](../Stello.Net/tools/Stello.Benchmark/MatchCommands.cs), and [BenchSearcher.cs](../Stello.Net/tools/Stello.Benchmark/BenchSearcher.cs), with validation and live search tests in [StelloLibraryMatchTests.cs](../Stello.Net/Egaroucid.Engine.Tests/StelloLibraryMatchTests.cs).
- Smoke match (`1` start / `2` games, `100 ms`, `1t`, `--workers 1`): `v5` vs `stello-v5` scored `1.0/2` (`50.0 %`, `21.07M` vs `21.42M` nps, avg depth `16.56 / 16.59`, `56 / 100 ms` vs `57 / 100 ms`, `6 s`).

### 2. Deterministic `IDisposable` Match-Player & Benchmark Cleanup

- Wrapped worker-local `playerA` / `playerB` in `try/finally` disposal inside `EngineMatch.Work()` ([BookMatch.cs](../Stello.Net/Stello.Engine/BookMatch.cs)), wrapped `players` in `QualityCommands.MidQuality()` ([QualityCommands.cs](../Stello.Net/tools/Stello.Benchmark/QualityCommands.cs)), implemented `IDisposable` on `BenchSearcher` ([BenchSearcher.cs](../Stello.Net/tools/Stello.Benchmark/BenchSearcher.cs)), and disposed `searcher` via `using` in `BenchCommands.SolveBench` and `BenchCommands.MidBench` ([BenchCommands.cs](../Stello.Net/tools/Stello.Benchmark/BenchCommands.cs)).
- Verified deterministic cleanup on both normal completion and mid-match cancellation in `EngineMatch_DisposesPlayersOnCompletionAndCancellation` ([BenchmarkTests.cs](../Stello.Net/Stello.Engine.Tests/BenchmarkTests.cs)).

### 3. Direct Native C Edax Harness (`engine=edax`)

- Benchmarked Clang `-march=native` builds of `edax-reversi-master` (zero upstream source edits) on `-bench 20 -n 1 -eval-file ..\bin\data\eval.dat` (`1,337,611,062` nodes total):
  - Default `-march=native` (AVX2 `MOVE_GENERATOR_AVX_ACEPCK` + `COUNT_LAST_FLIP_BMI2`): `15.970 s` (`83.76M` nps)
  - `-march=native -D__AVX512__` (`MOVE_GENERATOR_AVX512CD` + `COUNT_LAST_FLIP_BMI2`): **`15.549 s` (`86.03M` nps)**
  - `-march=native -D__AVX512__ -DCOUNT_LAST_FLIP=COUNT_LAST_FLIP_AVX512CD`: `15.860 s` (`84.34M` nps)
- Built PGO-optimized `edax-reversi-master/bin/wEdax-native.exe` with `-march=native -D__AVX512__` (`15.891 s` / `84.17M` nps on `-bench 20 -n 1`, git-ignored).
- Added [EdaxMatchPlayer.cs](../Stello.Net/tools/Stello.Benchmark/EdaxMatchPlayer.cs) speaking `-xboard -q -book-usage off -ponder off -h <bits> -n <threads>` (default `hash-bits=21`, ~54 MB; TT-preserving `usermove` sync within a game and `setboard` reset on `NewGame()`) and wired `engine=edax` into `MatchCommands`, `BenchSearcher`, and `Program.cs`.
- Verified in `EdaxPlayer_SearchesMovetimeDepthSolvePassAndDisposesCleanly` (FFO `#40` `+38 a2`, `TimePerMove`, `usermove` continuity, `threads=2`, clean `quit` shutdown) and `100 ms` smoke matches (`1t`: `45.33M` nps, avg depth `20.50`, `57 / 109 ms`; `8t`: `209.02M` nps, avg depth `23.84`, `53 / 109 ms`; zero lingering processes).

### 3B. Direct Native C Edax AVX Harness (`engine=edax-avx`)

- Benchmarked `edax-reversi-AVX-master` (Okuhara v4.5.7, zero source edits, `-eval-file ..\..\edax-reversi-master\bin\data\eval.dat`) on `-bench 20 -n 1` (`1,357,814,664` nodes across candidates 1–5; `1,532,305,569` on GFNI+highcut):
  - Clang `v4` (`-march=native -mprefer-vector-width=256`): `15.437 s` (`87.96M` nps) → PGO re-bench `15.312 s` (`88.68M` nps)
  - `clang-cl` (`-march=native -mprefer-vector-width=256 /arch:AVX512`): `15.438 s` (`87.95M` nps)
  - Clang `v4` + `-DLASTFLIP_HIGHCUT`: `15.889 s` (`85.46M` nps)
  - MSVC `vc-x64-v4` (`/GL /arch:AVX512 /vlen=256`): `15.969 s` (`85.03M` nps)
  - Clang `v4` + `-DMOVE_GENERATOR=MOVE_GENERATOR_AVXGFNI`: `16.374 s` (`82.93M` nps)
  - Clang `v4` + `GFNI` + `LASTFLIP_HIGHCUT`: `19.157 s` (`79.99M` nps)
  - **Clang `v4` PGO (`-fprofile-instr-generate` trained on `fforum-20-39.obf` + `-bench 18`, then `-fprofile-instr-use`): `14.827 s` (`91.58M` nps)** (`+8.8 %` NPS over `edax-reversi-master`'s `15.891 s` / `84.17M` nps).
- Saved winner as git-ignored `edax-reversi-AVX-master/bin/wEdax-avx-native.exe` (`701,952` bytes), extended [EdaxMatchPlayer.cs](../Stello.Net/tools/Stello.Benchmark/EdaxMatchPlayer.cs) (`EdaxVariant.Avx`, `EDAX_AVX_BIN`, default `hash-bits=21`), and wired `engine=edax-avx` / `--engine edax-avx` into `MatchCommands`, `BenchSearcher`, and `Program`.
- Verified in `EdaxAvxPlayer_SearchesMovetimeDepthSolvePassAndDisposesCleanly` and `100 ms` smoke matches (`1t`: `46.68M` nps, avg depth `20.88`, `58 / 116 ms`, `6 s`; `8t`: `238.66M` nps, avg depth `24.42`, `53 / 113 ms`, `5 s`; zero lingering processes).

### 4. Direct Native C++ Egaroucid Harness (`engine=egaroucid-cpp`)

- Created standalone driver [Stello.Net/tools/Egaroucid.Runner/](../Stello.Net/tools/Egaroucid.Runner/README.md) (`runner.cpp`, `build.cmd`, `README.md`, zero upstream edits to `Egaroucid-main/`) and built `bin/egaroucid-runner.exe` (git-ignored).
- Benchmarked `solve-bench --engine egaroucid-cpp --from 40 --to 44 --passes 1` (`82,691,350` nodes, all exact, `hash-bits=21`):
  - `-march=native -DHAS_AVX512 -DHAS_AMD_PROCESSOR`: **`1.390 s` (`59.51M` nps)**
  - `-march=native -DHAS_AMD_PROCESSOR` (AVX2-only): `1.479 s` (`55.91M` nps)
- Added [EgaroucidCppMatchPlayer.cs](../Stello.Net/tools/Stello.Benchmark/EgaroucidCppMatchPlayer.cs) (default `hash-bits=21`, ~48 MB) and wired `engine=egaroucid-cpp` into `MatchCommands`, `BenchSearcher`, and `Program.cs`.
- Verified in `EgaroucidCppPlayer_SearchesMovetimeLevelSolveMtAndDisposesCleanly` (FFO `#40` `+38 a2`, `movetime`, `level`, `threads=2`, clean `quit` shutdown) and `100 ms` smoke matches (`1t`: `14.42M` nps, avg depth `11.35`, `60 / 115 ms`; `8t`: `87.92M` nps, avg depth `17.67`, `54 / 112 ms`; zero lingering processes).

### 5. Prefix-Free `starts-v6.txt` Match Start Suite

- Added `--prefix-free` to `Stello.Benchmark make-starts` ([MatchCommands.cs](../Stello.Net/tools/Stello.Benchmark/MatchCommands.cs), [Program.cs](../Stello.Net/tools/Stello.Benchmark/Program.cs)), switched `MatchCommands.LoadStarts(null)` default to `Data/starts-v6.txt` (keeping `starts.txt` untouched and explicit default for `make-positions` and `make-exact`).
- Generated [Stello.Net/tools/Stello.Benchmark/Data/starts-v6.txt](../Stello.Net/tools/Stello.Benchmark/Data/starts-v6.txt) via `make-starts --plies 10-14 --max-value 220 --prefix-free` (**`587` starts / `1,174` games**, `0 %` ancestor–descendant overlap).
- Verified in `StartsV6File_HoldsPrefixFreeBalancedBookLines` and `MakeStarts_PrefixFree_RemovesAncestorsAndKeepsDeepest`.

### 5B. `stello-cpp microbench` Per-Primitive Cycle Baseline

- Added [microbench.hpp](../Stello.Cpp/src/search/microbench.hpp), [microbench_impl.hpp](../Stello.Cpp/src/search/microbench_impl.hpp), `stello-cpp microbench` ([main.cpp](../Stello.Cpp/src/cli/main.cpp)), `microbench_portable_and_v3_parity_tests` ([search_tests.cpp](../Stello.Cpp/tests/search_tests.cpp)), and `stello_cli_microbench` CTest (`repeats = 500,000`, `warmup = 1,000`, `64` samples, `__rdtsc()` CPU cycles, `clang-release` on 7800X3D):

| Primitive | `--cpu auto` (`v3/avx2`) Cycles (min < mean ± sd < max) | `--cpu portable` Cycles (min < mean ± sd < max) | Checksum (`v3 == portable`) |
|---|---|---|---|
| `flips` | `5.98 < 6.35 ± 0.50 < 9.20` | `17.46 < 17.73 ± 0.18 < 18.15` | `da4c9d7fdfcb5660` |
| `last_flip_count` | `4.36 < 4.54 ± 0.14 < 5.01` | `4.38 < 4.51 ± 0.11 < 4.80` | `71bce4877bbeb380` |
| `solve_1` | `7.76 < 7.99 ± 0.16 < 8.40` | `11.59 < 12.03 ± 0.22 < 12.72` | `71bce48788869ce0` |
| `legal_moves` | `8.74 < 9.28 ± 0.47 < 10.90` | `33.48 < 34.02 ± 0.69 < 38.90` | `fbd24755bc7c7d20` |
| `potential_mobility` | `6.21 < 6.50 ± 0.29 < 8.18` | `17.61 < 18.01 ± 0.30 < 18.63` | `71bce487a9412720` |
| `legal_and_potential` | `15.59 < 15.86 ± 0.20 < 16.49` | `40.65 < 41.14 ± 0.31 < 41.93` | `fbd2475603ea7fe0` |
| `stable_of` | `30.64 < 31.08 ± 0.26 < 31.64` | `30.56 < 31.14 ± 0.27 < 31.71` | `3a7999385805bc58` |
| `eval_sum` | `160.58 < 162.28 ± 3.98 < 191.36` | `166.16 < 167.40 ± 0.84 < 170.16` | `71bce487f75f90e0` |
| `eval_after_move` | `161.49 < 162.96 ± 0.82 < 165.29` | `204.45 < 206.19 ± 0.97 < 211.19` | `71bce48810567f00` |
| `eval_advance` | `27.75 < 28.60 ± 0.38 < 29.23` | `70.33 < 72.30 ± 4.41 < 105.77` | `71bce48dc4d33360` |

### 5C. Stello 5.0 Reference Benchmarks (`Plans/benchmark-results/v5.0-ref/`)

Recorded on idle CPU (`hash-bits=19`, `stello` / `50000`, all FFO and random-endgame solves verified 100 % exact):

| Benchmark | Positions | Limits | Threads | Passes | Nodes | Time | Speed | Scaling vs 1t | File |
|---|---:|---|---:|---:|---:|---:|---:|---:|---|
| `midgame-depth10` | 100 | depth 10 | 1 | 3 | 3,083,842 | 0.237 s | 13.02M nps | — | [midgame-depth10.json](benchmark-results/v5.0-ref/midgame-depth10.json) |
| `midgame-depth12` | 100 | depth 12 | 1 | 3 | 8,120,201 | 0.631 s | 12.86M nps | — | [midgame-depth12.json](benchmark-results/v5.0-ref/midgame-depth12.json) |
| `midgame-depth14` | 100 | depth 14 | 1 | 3 | 22,875,227 | 1.780 s | 12.85M nps | — | [midgame-depth14.json](benchmark-results/v5.0-ref/midgame-depth14.json) |
| `midgame-depth16` | 100 | depth 16 | 1 | 3 | 60,566,022 | 4.549 s | 13.31M nps | 1.00× | [midgame-depth16.json](benchmark-results/v5.0-ref/midgame-depth16.json) |
| `midgame-depth16-8t` | 100 | depth 16 | 8 | 3 | 83,837,665 | 1.526 s | 54.95M nps | 2.98× (+38.4 % nodes) | [midgame-depth16-8t.json](benchmark-results/v5.0-ref/midgame-depth16-8t.json) |
| `midgame-depth16-16t` | 100 | depth 16 | 16 | 3 | 99,072,287 | 1.426 s | 69.50M nps | 3.19× (+63.6 % nodes) | [midgame-depth16-16t.json](benchmark-results/v5.0-ref/midgame-depth16-16t.json) |
| `midgame-random-depth8` | 1,000 | depth 8 | 1 | 3 | 8,699,892 | 0.640 s | 13.59M nps | — | [midgame-random-depth8.json](benchmark-results/v5.0-ref/midgame-random-depth8.json) |
| `midgame-handover-depth12` | 300 | depth 12 | 1 | 3 | 303,631,734 | 4.531 s | 67.02M nps | — | [midgame-handover-depth12.json](benchmark-results/v5.0-ref/midgame-handover-depth12.json) |
| `ffo-40-49` | 10 | solve | 1 | 1 | 1,070,469,669 | 16.664 s | 64.24M nps | 1.00× | [ffo-40-49.json](benchmark-results/v5.0-ref/ffo-40-49.json) |
| `ffo-40-49-8t` | 10 | solve | 8 | 1 | 1,170,579,879 | 3.114 s | 375.96M nps | 5.35× (+9.4 % nodes) | [ffo-40-49-8t.json](benchmark-results/v5.0-ref/ffo-40-49-8t.json) |
| `ffo-40-49-16t` | 10 | solve | 16 | 1 | 1,258,992,039 | 2.629 s | 478.91M nps | 6.34× (+17.6 % nodes) | [ffo-40-49-16t.json](benchmark-results/v5.0-ref/ffo-40-49-16t.json) |
| `ffo-50-59-8t` | 10 | solve | 8 | 1 | 37,576,557,005 | 105.095 s | 357.55M nps | 5.33× (+23.1 % nodes)* | [ffo-50-59-8t.json](benchmark-results/v5.0-ref/ffo-50-59-8t.json) |
| `ffo-50-59-16t` | 10 | solve | 16 | 1 | 42,951,931,459 | 93.144 s | 461.14M nps | 6.01× (+40.8 % nodes)* | [ffo-50-59-16t.json](benchmark-results/v5.0-ref/ffo-50-59-16t.json) |
| `endgame-random-14-24` | 500 | solve | 1 | 1 | 27,755,239,908 | 364.674 s | 76.11M nps | — | [endgame-random-14-24.json](benchmark-results/v5.0-ref/endgame-random-14-24.json) |

\* `ffo-50-59` 8t/16t scaling is computed against the identical-binary 2026-11-22 Stello 5.0 1t reference ([p7_v5_ffo50-59_1t.json](benchmark-results/p7_v5_ffo50-59_1t.json): `30,513,267,013` nodes, `559.648 s`).

### 5D. WebAssembly 5.0 ST/MT Baseline

- **Artifact sizes (raw / gzip / brotli):**
  - `Stello.Web/wwwroot/engine/stello_wasm.js`: `10,423` / `2,726` / `2,433` B
  - `Stello.Web/wwwroot/engine/stello_wasm_bg.wasm`: `9,136,543` / `8,974,551` / `8,954,923` B
  - `Stello.Web/wwwroot/engine-mt/stello_wasm.js`: `10,423` / `2,726` / `2,433` B
  - `Stello.Web/wwwroot/engine-mt/stello_wasm_bg.wasm`: `9,167,007` / `8,985,554` / `8,964,314` B
- **Node.js `v24.19.0` benchmarks (`hash-bits=19`, `3` passes midgame, `2` passes `ffo-40-44`, all exact; reported as `best-per-pos sum / median pass`):**

| Variant | `mid-d10` (100 pos) | `mid-d12` (100 pos) | `ffo-40-44` (5 pos) |
|---|---|---|---|
| `ST 1t` (`engine`) | `3,083,842` nodes, `376.3 / 384.9 ms` (`8.20M / 8.01M` nps) | `8,120,201` nodes, `992.1 / 999.1 ms` (`8.18M / 8.13M` nps) | `122,045,591` nodes, `2995.1 / 3000.1 ms` (`40.75M / 40.68M` nps) |
| `MT 1t` (`engine-mt`) | `3,083,842` nodes, `385.8 / 391.2 ms` (`7.99M / 7.88M` nps) | `8,120,201` nodes, `1022.2 / 1027.9 ms` (`7.94M / 7.90M` nps) | `122,045,591` nodes, `3003.5 / 3018.9 ms` (`40.63M / 40.43M` nps) |
| `MT 4t` (`engine-mt`) | `5,031,656` nodes, `291.1 / 302.9 ms` (`17.29M / 16.61M` nps) | `12,583,610` nodes, `649.4 / 679.8 ms` (`19.38M / 18.51M` nps) | `139,599,178` nodes, `1106.2 / 1129.3 ms` (`126.19M / 123.62M` nps) |

### 5E. Same-Machine FFO `#40–49` and `#50–59` Comparisons (`solve-bench`, 1 Pass, All Exact)

Defaults: `hash-bits=19` for Stello (`stello`, `stello-v4`) and `hash-bits=21` for external engines (`egaroucid-cpp`, `edax`, `edax-avx`).

#### FFO `#40–49` (`10` positions)

| Engine | Threads | Hash Bits | Total Nodes | Total Time | Nodes/s | Scaling vs 1t | File |
|---|---:|---:|---:|---:|---:|---:|---|
| `Stello 5.0` (`stello`) | 1 | 19 | 1,070,469,669 | 16.664 s | 64.24M | 1.00× | [ffo-40-49.json](benchmark-results/v5.0-ref/ffo-40-49.json) |
| `Stello 4.0` (`stello-v4`) | 1 | 19 | 1,115,666,701 | 23.409 s | 47.66M | 1.00× | [p0_v4_ffo40-49_1t.json](benchmark-results/p0_v4_ffo40-49_1t.json) |
| `Egaroucid` (`egaroucid-cpp`) | 1 | 21 | **512,627,776** | **9.470 s** | 54.13M | 1.00× | [p0_egaroucid_cpp_ffo40-49_1t.json](benchmark-results/p0_egaroucid_cpp_ffo40-49_1t.json) |
| `Edax` (`edax`) | 1 | 21 | 939,357,096 | 11.906 s | 78.90M | 1.00× | [p0_edax_ffo40-49_1t.json](benchmark-results/p0_edax_ffo40-49_1t.json) |
| `Edax AVX` (`edax-avx`) | 1 | 21 | 933,274,231 | 11.496 s | **81.18M** | 1.00× | [p0_edax_avx_ffo40-49_1t.json](benchmark-results/p0_edax_avx_ffo40-49_1t.json) |
| `Stello 5.0` (`stello`) | 8 | 19 | 1,170,579,879 | 3.114 s | 375.96M | 5.35× (+9.4 % nodes) | [ffo-40-49-8t.json](benchmark-results/v5.0-ref/ffo-40-49-8t.json) |
| `Stello 4.0` (`stello-v4`) | 8 | 19 | 1,625,810,698 | 6.385 s | 254.63M | 3.67× (+45.7 % nodes) | [p0_v4_ffo40-49_8t.json](benchmark-results/p0_v4_ffo40-49_8t.json) |
| `Egaroucid` (`egaroucid-cpp`) | 8 | 21 | **728,998,978** | 2.380 s | 306.28M | 3.98× (+42.2 % nodes) | [p0_egaroucid_cpp_ffo40-49_8t.json](benchmark-results/p0_egaroucid_cpp_ffo40-49_8t.json) |
| `Edax` (`edax`) | 8 | 21 | 1,014,232,221 | 2.158 s | 469.99M | 5.52× (+8.0 % nodes) | [p0_edax_ffo40-49_8t.json](benchmark-results/p0_edax_ffo40-49_8t.json) |
| `Edax AVX` (`edax-avx`) | 8 | 21 | 987,761,820 | **2.034 s** | **485.57M** | 5.65× (+5.8 % nodes) | [p0_edax_avx_ffo40-49_8t.json](benchmark-results/p0_edax_avx_ffo40-49_8t.json) |
| `Stello 5.0` (`stello`) | 16 | 19 | 1,258,992,039 | 2.629 s | 478.91M | 6.34× (+17.6 % nodes) | [ffo-40-49-16t.json](benchmark-results/v5.0-ref/ffo-40-49-16t.json) |

#### FFO `#50–59` (`10` positions)

| Engine | Threads | Hash Bits | Total Nodes | Total Time | Nodes/s | Scaling vs 1t | File |
|---|---:|---:|---:|---:|---:|---:|---|
| `Stello 5.0` (`stello`)* | 1 | 19 | 30,513,267,013 | 559.648 s | 54.52M | 1.00× | [p7_v5_ffo50-59_1t.json](benchmark-results/p7_v5_ffo50-59_1t.json) |
| `Stello 4.0` (`stello-v4`)* | 1 | 19 | 25,419,468,677 | 586.495 s | 43.34M | 1.00× | [p7_v4_ffo50-59_1t.json](benchmark-results/p7_v4_ffo50-59_1t.json) |
| `Egaroucid` (`egaroucid-cpp`) | 1 | 21 | **15,543,366,497** | 325.701 s | 47.72M | 1.00× | [p0_egaroucid_cpp_ffo50-59_1t.json](benchmark-results/p0_egaroucid_cpp_ffo50-59_1t.json) |
| `Edax` (`edax`) | 1 | 21 | 24,086,394,724 | 265.765 s | 90.63M | 1.00× | [p0_edax_ffo50-59_1t.json](benchmark-results/p0_edax_ffo50-59_1t.json) |
| `Edax AVX` (`edax-avx`) | 1 | 21 | 23,637,594,214 | **253.005 s** | **93.43M** | 1.00× | [p0_edax_avx_ffo50-59_1t.json](benchmark-results/p0_edax_avx_ffo50-59_1t.json) |
| `Stello 5.0` (`stello`) | 8 | 19 | 37,576,557,005 | 105.095 s | 357.55M | 5.33× (+23.1 % nodes) | [ffo-50-59-8t.json](benchmark-results/v5.0-ref/ffo-50-59-8t.json) |
| `Stello 4.0` (`stello-v4`) | 8 | 19 | 39,256,125,746 | 146.450 s | 268.05M | 4.00× (+54.4 % nodes) | [p0_v4_ffo50-59_8t.json](benchmark-results/p0_v4_ffo50-59_8t.json) |
| `Egaroucid` (`egaroucid-cpp`) | 8 | 21 | **15,859,140,638** | 50.333 s | 315.09M | 6.47× (+2.0 % nodes) | [p0_egaroucid_cpp_ffo50-59_8t.json](benchmark-results/p0_egaroucid_cpp_ffo50-59_8t.json) |
| `Edax` (`edax`) | 8 | 21 | 24,986,454,798 | 39.434 s | 633.63M | 6.74× (+3.7 % nodes) | [p0_edax_ffo50-59_8t.json](benchmark-results/p0_edax_ffo50-59_8t.json) |
| `Edax AVX` (`edax-avx`) | 8 | 21 | 24,535,845,905 | **38.487 s** | **637.50M** | 6.57× (+3.8 % nodes) | [p0_edax_avx_ffo50-59_8t.json](benchmark-results/p0_edax_avx_ffo50-59_8t.json) |
| `Stello 5.0` (`stello`) | 16 | 19 | 42,951,931,459 | 93.144 s | 461.14M | 6.01× (+40.8 % nodes) | [ffo-50-59-16t.json](benchmark-results/v5.0-ref/ffo-50-59-16t.json) |

\* `Stello 5.0` and `Stello 4.0` `FFO #50–59` `1t` rows reference the identical-binary 2026-11-22 `p7_*` JSONs.

### 6. Phase 0 Baseline Matches (`starts-v6.txt`, `1,174` Games Each, `book=off`, Side B = `Stello 5.0`)

*(Complete: all `30/30` matches = `35,220` games recorded on `starts-v6.txt`.)*

| # | Opponent | Time | Threads | Score (95 % CI) | W / D / L | Elo (95 % CI) | LOS | Discs/g | Avg Depth (A / B) | First Solve (A / B) | NPS (A / B) | Wall Time |
|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 1 | `Stello 4.0` (`v4`) | 100 ms | 1t | **60.6 %** (58.2–63.1 %) | `+620 =184 −370` | **+75** (+57 to +93) | 100.0 % | +1.6 ± 0.3 | 16.23 / 17.37 | 21.3 / 20.4 | 15.16M / 19.90M | [6m 12s](benchmark-results/match_p0_v5_vs_v4_100ms_1t.md) |
| 2 | `Stello 2.0` (`v2`) | 100 ms | 1t | **72.4 %** (70.2–74.6 %) | `+773 =154 −247` | **+168** (+149 to +187) | 100.0 % | +3.6 ± 0.3 | 14.34 / 17.43 | 20.6 / 20.5 | 6.69M / 20.31M | [6m 17s](benchmark-results/match_p0_v5_vs_v2_100ms_1t.md) |
| 3 | `Stello 1` (`v1`) | 100 ms | 1t | **98.2 %** (97.6–98.9 %) | `+1143 =20 −11` | **+696** (+641 to +776) | 100.0 % | +17.6 ± 0.5 | 11.06 / 17.41 | 19.2 / 20.9 | 5.06M / 20.40M | [6m 29s](benchmark-results/match_p0_v5_vs_v1_100ms_1t.md) |
| 4 | `Egaroucid` (`egaroucid-cpp`) | 100 ms | 1t | **73.3 %** (71.1–75.4 %) | `+781 =158 −235` | **+175** (+156 to +195) | 100.0 % | +3.3 ± 0.3 | 11.54 / 17.25 | 20.0 / 20.5 | 11.26M / 18.69M | [6m 44s](benchmark-results/match_p0_v5_vs_egaroucid_cpp_100ms_1t.md) |
| 5 | `Edax` (`edax`) | 100 ms | 1t | **38.5 %** (36.3–40.8 %) | `+342 =221 −611` | **−81** (−98 to −65) | 0.0 % | −1.3 ± 0.3 | 21.63 / 17.25 | 21.7 / 20.3 | 38.83M / 18.99M | [6m 23s](benchmark-results/match_p0_v5_vs_edax_100ms_1t.md) |
| 6 | `Stello 4.0` (`v4`) | 100 ms | 8t | **60.4 %** (58.1–62.8 %) | `+608 =203 −363` | **+74** (+57 to +91) | 100.0 % | +1.4 ± 0.2 | 18.60 / 20.28 | 23.2 / 22.8 | 105.56M / 121.54M | [45m 59s](benchmark-results/match_p0_v5_vs_v4_100ms_8t.md) |
| 7 | `Egaroucid` (`egaroucid-cpp`) | 100 ms | 8t | **48.0 %** (45.7–50.4 %) | `+445 =238 −491` | **−14** (−30 to +3) | 5.0 % | −0.3 ± 0.2 | 20.41 / 20.25 | 23.0 / 22.7 | 83.29M / 118.87M | [48m 17s](benchmark-results/match_p0_v5_vs_egaroucid_cpp_100ms_8t.md) |
| 8 | `Edax` (`edax`) | 100 ms | 8t | **37.5 %** (35.5–39.6 %) | `+311 =259 −604` | **−89** (−104 to −73) | 0.0 % | −1.2 ± 0.2 | 25.02 / 20.26 | 24.0 / 22.6 | 214.73M / 119.69M | [46m 20s](benchmark-results/match_p0_v5_vs_edax_100ms_8t.md) |
| 9 | `Stello 4.0` (`v4`) | 500 ms | 1t | **59.9 %** (57.8–62.1 %) | `+594 =219 −361` | **+70** (+54 to +86) | 100.0 % | +1.4 ± 0.2 | 19.05 / 20.47 | 23.4 / 22.9 | 16.30M / 21.13M | [28m 20s](benchmark-results/match_p0_v5_vs_v4_500ms_1t.md) |
| 10 | `Stello 2.0` (`v2`) | 500 ms | 1t | **68.9 %** (66.7–71.1 %) | `+708 =201 −265` | **+138** (+120 to +156) | 100.0 % | +2.5 ± 0.2 | 17.25 / 20.45 | 22.7 / 23.0 | 7.19M / 21.17M | [28m 43s](benchmark-results/match_p0_v5_vs_v2_500ms_1t.md) |
| 11 | `Stello 1` (`v1`) | 500 ms | 1t | **97.8 %** (97.1–98.6 %) | `+1138 =21 −15` | **+661** (+610 to +734) | 100.0 % | +14.1 ± 0.5 | 12.90 / 20.48 | 20.8 / 23.4 | 5.08M / 21.69M | [30m 13s](benchmark-results/match_p0_v5_vs_v1_500ms_1t.md) |
| 12 | `Egaroucid` (`egaroucid-cpp`) | 500 ms | 1t | **55.0 %** (52.7–57.3 %) | `+531 =229 −414` | **+35** (+18 to +51) | 100.0 % | +0.6 ± 0.2 | 19.07 / 20.37 | 22.8 / 22.9 | 14.66M / 20.41M | [29m 26s](benchmark-results/match_p0_v5_vs_egaroucid_cpp_500ms_1t.md) |
| 13 | `Edax` (`edax`) | 500 ms | 1t | **37.7 %** (35.6–39.9 %) | `+310 =266 −598` | **−87** (−103 to −71) | 0.0 % | −1.0 ± 0.2 | 25.87 / 20.39 | 23.8 / 22.9 | 41.81M / 20.57M | [28m 37s](benchmark-results/match_p0_v5_vs_edax_500ms_1t.md) |
| 14 | `Stello 4.0` (`v4`) | 500 ms | 8t | **61.9 %** (59.8–64.0 %) | `+596 =262 −316` | **+84** (+69 to +100) | 100.0 % | +1.3 ± 0.2 | 21.65 / 23.80 | 25.3 / 25.6 | 128.71M / 149.41M | [3h 25m](benchmark-results/match_p0_v5_vs_v4_500ms_8t.md) |
| 15 | `Egaroucid` (`egaroucid-cpp`) | 500 ms | 8t | **44.8 %** (42.8–46.8 %) | `+366 =320 −488` | **−36** (−50 to −22) | 0.0 % | −0.5 ± 0.1 | 26.58 / 23.77 | 25.4 / 25.4 | 105.04M / 146.90M | [3h 29m](benchmark-results/match_p0_v5_vs_egaroucid_cpp_500ms_8t.md) |
| 16 | `Edax` (`edax`) | 500 ms | 8t | **37.8 %** (35.8–39.9 %) | `+299 =290 −585` | **−86** (−102 to −71) | 0.0 % | −1.0 ± 0.1 | 29.49 / 23.76 | 26.2 / 25.4 | 288.18M / 147.22M | [3h 24m](benchmark-results/match_p0_v5_vs_edax_500ms_8t.md) |
| 17 | `Stello 4.0` (`v4`) | 2,000 ms | 1t | **59.3 %** (57.2–61.4 %) | `+559 =274 −341` | **+65** (+50 to +81) | 100.0 % | +1.0 ± 0.2 | 21.51 / 23.16 | 25.2 / 25.2 | 17.81M / 22.00M | [1h 44m](benchmark-results/match_p0_v5_vs_v4_2000ms_1t.md) |
| 18 | `Stello 2.0` (`v2`) | 2,000 ms | 1t | **65.5 %** (63.4–67.6 %) | `+643 =252 −279` | **+111** (+95 to +128) | 100.0 % | +1.8 ± 0.2 | 19.76 / 23.18 | 24.5 / 25.2 | 7.88M / 22.43M | [1h 45m](benchmark-results/match_p0_v5_vs_v2_2000ms_1t.md) |
| 19 | `Edax AVX` (`edax-avx`) | 100 ms | 1t | **35.6 %** (33.4–37.9 %) | `+312 =213 −649` | **−103** (−120 to −86) | 0.0 % | −1.6 ± 0.3 | 21.65 / 17.17 | 21.6 / 20.2 | 39.14M / 18.35M | [6m 24s](benchmark-results/match_p0_v5_vs_edax_avx_100ms_1t.md) |
| 20 | `Edax AVX` (`edax-avx`) | 500 ms | 1t | **37.2 %** (35.0–39.4 %) | `+313 =247 −614` | **−91** (−108 to −75) | 0.0 % | −1.1 ± 0.2 | 26.05 / 20.39 | 23.9 / 22.9 | 43.65M / 20.38M | [28m 34s](benchmark-results/match_p0_v5_vs_edax_avx_500ms_1t.md) |
| 21 | `Edax AVX` (`edax-avx`) | 100 ms | 8t | **36.8 %** (34.6–39.0 %) | `+306 =253 −615` | **−94** (−110 to −77) | 0.0 % | −1.3 ± 0.2 | 25.20 / 20.27 | 24.1 / 22.6 | 230.21M / 120.01M | [46m 16s](benchmark-results/match_p0_v5_vs_edax_avx_100ms_8t.md) |
| 22 | `Stello 1` (`v1`) | 2,000 ms | 1t | **97.3 %** (96.5–98.1 %) | `+1128 =28 −18` | **+621** (+575 to +683) | 100.0 % | +12.5 ± 0.4 | 14.19 / 23.14 | 22.1 / 25.6 | 5.01M / 23.02M | [1h 52m](benchmark-results/match_p0_v5_vs_v1_2000ms_1t.md) |
| 23 | `Egaroucid` (`egaroucid-cpp`) | 2,000 ms | 1t | **42.2 %** (40.1–44.2 %) | `+348 =294 −532` | **−55** (−70 to −40) | 0.0 % | −0.6 ± 0.2 | 25.90 / 23.00 | 24.8 / 25.0 | 17.13M / 21.02M | [1h 46m](benchmark-results/match_p0_v5_vs_egaroucid_cpp_2000ms_1t.md) |
| 24 | `Edax` (`edax`) | 2,000 ms | 1t | **37.5 %** (35.5–39.5 %) | `+287 =306 −581` | **−89** (−104 to −74) | 0.0 % | −1.0 ± 0.2 | 29.39 / 23.04 | 25.6 / 25.0 | 43.83M / 21.15M | [1h 45m](benchmark-results/match_p0_v5_vs_edax_2000ms_1t.md) |
| 25 | `Edax AVX` (`edax-avx`) | 2,000 ms | 1t | **37.0 %** (35.0–38.9 %) | `+275 =318 −581` | **−93** (−108 to −78) | 0.0 % | −1.0 ± 0.1 | 29.60 / 23.17 | 25.7 / 25.0 | 47.33M / 22.43M | [1h 45m](benchmark-results/match_p0_v5_vs_edax_avx_2000ms_1t.md) |
| 26 | `Edax AVX` (`edax-avx`) | 500 ms | 8t | **37.1 %** (35.2–39.1 %) | `+289 =294 −591` | **−91** (−106 to −77) | 0.0 % | −0.9 ± 0.1 | 29.65 / 23.79 | 26.2 / 25.5 | 307.02M / 147.83M | [3h 23m](benchmark-results/match_p0_v5_vs_edax_avx_500ms_8t.md) |
| 27 | `Stello 4.0` (`v4`) | 2,000 ms | 8t | **63.1 %** (61.1–65.1 %) | `+604 =273 −297` | **+93** (+78 to +108) | 100.0 % | +1.1 ± 0.2 | 23.36 / 25.95 | 26.5 / 27.2 | 92.13M / 113.11M | [12h 50m](benchmark-results/match_p0_v5_vs_v4_2000ms_8t.md) |
| 28 | `Egaroucid` (`egaroucid-cpp`) | 2,000 ms | 8t | **48.1 %** (46.2–50.0 %) | `+389 =351 −434` | **−13** (−27 to 0) | 2.5 % | −0.1 ± 0.1 | 29.19 / 26.63 | 26.9 / 27.5 | 111.03M / 154.09M | [12h 50m](benchmark-results/match_p0_v5_vs_egaroucid_cpp_2000ms_8t.md) |
| 29 | `Edax` (`edax`) | 2,000 ms | 8t | **41.1 %** (39.4–42.8 %) | `+295 =375 −504` | **−63** (−75 to −50) | 0.0 % | −0.7 ± 0.1 | 32.80 / 26.86 | 27.8 / 27.5 | 353.39M / 166.89M | [12h 35m](benchmark-results/match_p0_v5_vs_edax_2000ms_8t.md) |
| 30 | `Edax AVX` (`edax-avx`) | 2,000 ms | 8t | **41.7 %** (39.8–43.6 %) | `+308 =363 −503` | **−58** (−72 to −45) | 0.0 % | −0.7 ± 0.1 | 32.90 / 26.87 | 27.8 / 27.6 | 369.00M / 169.43M | [12h 33m](benchmark-results/match_p0_v5_vs_edax_avx_2000ms_8t.md) |

### 7. Test Verification (Gate P0 Complete)

- `.NET Release build` (`-p:StelloWasmEngine=false`): `0 Warning(s), 0 Error(s)`.
- `Stello.Engine.Tests`: `519/519` passed.
- `Stello.Net.Tests`: `116/116` passed.
- `Egaroucid.Engine.Tests`: `114 passed, 0 failed, 8 skipped` (`39.74 s`; skipped tests are the optional `TaggedV30_*` reference DLL tests).
- All 5 C++ presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`): `0` compiler warnings, `3/3` ctest tests passed (`stello_tests`, `stello_cli_version`, `stello_cli_microbench`).

## Phase 1: Bug Fixing (Gate P1 Complete)

### 1. Summary of Fixes & Regression Coverage

- **C++20 Engine (`Stello.Cpp`) — 6 confirmed bugs (`C1–C6`) + 6 defensive hardenings (`1C.1–1C.6`):**
  - `C1` (`src/search/ybwc.hpp`, `ybwc_impl.hpp`): merged `SplitPoint` `stop` and `cut` flags into a single atomic `uint8_t state` bitmask (`STOP_BIT = 1`, `CUT_BIT = 2`) updated via `fetch_or(..., relaxed)`.
  - `C2` (`src/search/engine_impl.hpp`): copied `end_mpc_stage`, `hash_get_height`, and `hash_put_height` into `solve_iteration` Lazy SMP helper searchers when `ybwc = false`.
  - `C3` (`src/search/engine_impl.hpp`): restored `best.mv` to `root_moves[0]` via `detail::move_to_front` before the exact pass when `keep_midgame_move` holds (`wld.score < 0` on heuristic root).
  - `C4` (`src/search/pool.hpp`, `engine_impl.hpp`): built `UniqueTask` and pushed into `jobs` before incrementing `tasks_->running` / decrementing `idle`, and constructed `StopHelpers` before helper spawn loops.
  - `C5` (`src/search/time.hpp`, `engine_impl.hpp`): clamped time budgets at `MAX_TIME_MS = INT64_MAX / 1024` and guarded signed arithmetic in `for_game`, `for_midgame`, `for_endgame`, `with_soft_percent`, and `iterate`.
  - `C6` & `1C.5` (`src/wasm/wasm_exports.cpp`, `protocol.cpp`, `stello_wasm.js`): rejected null+nonzero spans, rejected negative `TimeTicks` for `TimePerMove` (preserving `TimePerGame` overtime), made `g_main_pthread` atomic and checked `pthread_*` return values (propagating `0` to throw in JS), and freed previous engine state before reallocating on init/`hash_bits` changes.
  - `1C.1–1C.4`, `1C.6`: replaced `std::memset` on live atomics in `TranspositionTable::clear()`, switched `Kernel<true>::build_eval_base` / `sum_eval_after_move` to `_mm256_storeu_si256` / `_mm256_loadu_si256`, added pointer `Direct::sum_portable` overload eliminating `{weights, SIZE_MAX}` in `patterns_bmi2.cpp`, guarded `eval == nullptr && eval_length > 0` in `stello_engine_create`, and made `cpu::detect()` synchronize with `compare_exchange_strong`.
- **.NET, Desktop, Web & CLI (`Stello.Net`) — 13 confirmed bugs (`N1–N13`) + 8 defensive hardenings (`1C.7–1C.14`):**
  - `N1`, `N2`, `N4`, `N7`, `N8`, `1C.8`, `1C.9` (`MainViewModel.cs`, `LocalEngineHost.cs`): serialized state transitions via `_transitionGate` and looping `StopAsync()`, restricted `SetGameMode` via `CanChangeGame` + `IsLearning`, checked `cancel` before `moveNow` in `WaitForNaturalDelayAsync`, fixed clock ticks during `IsLearning` + `_clockGeneration` + per-player `_continuedOnTime` + immediate `0:00` notification before timeout prompt, guarded `_clockTimer` with `_disposed`/`_ticking` + null `SynchronizationContext` safety, stopped games before confirmation dialogs in `AddGameToBook()`, forwarded `HashBits`/`Threads` to `LearningLimits()`, and ran `learner.Minimax()` before saving a stopped learning run.
  - `N3`, `N4`, `N5`, `N13`, `1C.10` (`Stello.Web`, `engine-worker.js`, `engine-client.js`, `stello.js`): caught `JSException` in `LocalStorageBookStore.Load()` and `BrowserDialogService.Beep()`, guarded `AudioContext`, wrapped `BrowserGameFileService.SaveAsync()` `JSException` in `IOException`, cancelled immediately (with `signal(2)`) during the 300 ms MoveNow grace window, correlated `progress` reports by `searchId`, and validated `MoveNowResult` against legal moves.
  - `N6`, `N9–N12`, `1C.7`, `1C.11–1C.14` (`BookLearner.cs`, `BookMatch.cs`, `SearchEngine.cs`, `QualityCommands.cs`, `MatchCommands.cs`, `BenchCommands.cs`, `MpcCommand.cs`, `ReportCommand.cs`): validated all plies upfront with `OpeningBook.IsLegal` in `AddGame`, forwarded `MatchSide.Options` and clamped 95 % CIs to `[0, 1]`, checked `CreateSignals` / `SetYbwc` failure disposal in `SearchEngine`, applied per-side `Threads` & `HashBits` in `mid-quality`, fixed `MatchCommands` option owner errors and `BenchCommands` `ms > 0` guard, capped `mpc --to` at `55`, and filtered 64-disc terminal boards in `ReportCommand`.

### 2. Gate P1 Verification & C3 Parity

- **C++ presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`):** `0` compiler warnings, `3/3` ctests passed each (`52/52` `stello_tests`, `stello_cli_version`, `stello_cli_microbench`; zero ASan/UBSan findings).
- **`.NET` Release build (with Emscripten WASM single + threaded rebuilds):** `0 Warning(s), 0 Error(s)`.
- **`.NET` & WASM test suites (post-final-edit):**
  - `Stello.Engine.Tests`: `521 passed, 0 failed, 0 skipped` (`7 s`).
  - `Stello.Net.Tests`: `127 passed, 0 failed, 0 skipped` (`18 s`, including new `MainViewModelTests` & `WebHostTests`).
  - `Egaroucid.Engine.Tests`: `118 passed, 0 failed, 8 skipped` (`34 s`, optional `TaggedV30_*` reference DLL tests skipped).
  - WASM Node tests (`engine.test.mjs`, `engine-mt.test.mjs`): `20/20 passed, 0 failed` (`3.76 s`).
- **C3 parity check:**
  - `mid-bench --depth 12` vs `v5.0-ref/midgame-depth12.json`: `identical` across all `100` searches (`8,120,201` nodes in `0.651 s`, `12.47M` nps).
  - `solve-bench --from 40 --to 49` vs `v5.0-ref/ffo-40-49.json`: `identical` across all `10` searches (`1,070,469,669` nodes in `16.7 s`, `64.17M` nps, `All scores and moves are correct`).
  - No signature regeneration or extra Gate P1 match required.
- WASM Node tests (`node --test Stello.Cpp/tests/wasm/engine.test.mjs Stello.Cpp/tests/wasm/engine-mt.test.mjs`): `19/19` passed (`3.77 s`).

---

## Phase 2: Speed (No Change in Nodes — Gate P2 Complete)

### 1. Summary of Kept Optimizations (`2.1`–`2.6` & Extra Phase-2 Items `2.E1`–`2.E6`)

- **2.1 & `2.E1` / `2.E5` (Evaluation Overhaul — [patterns.hpp](../Stello.Cpp/src/eval/patterns.hpp), [patterns.cpp](../Stello.Cpp/src/eval/patterns.cpp), [patterns_bmi2.cpp](../Stello.Cpp/src/eval/patterns_bmi2.cpp), [eval_avx2_detail.hpp](../Stello.Cpp/src/search/eval_avx2_detail.hpp)):**
  - Replaced dual `(black, white)` 256 B arrays with single ternary `alignas(32) uint16_t features[64]` (`PatternState`, 128 B = `4 × ymm`) plus baked sub-table offsets, `all_1_state_` / `all_2_state_` sparse state init, even/odd-interleaved `int32_t` packed pair weights with 4-byte aligned `VPGATHERDD`, branchless BMI2 `PEXT` `sum_pext_pair`, unscaled `eval::raw_skip_threshold` leaf comparisons in `search_inc`, and removal of unused `min_mob`/`own_mobility_weight`/`opp_mobility_weight`.
  - Replaced dynamic bucket iteration in portable/WASM eval with compile-time `sum_flat_scalar<OddSlot>` and zero-STLF WASM `advance_lanes_wasm` + `sum_local_wasm<OddSlot>`.
- **`2.E2`, `2.E3`, `2.E4`, `2.E6` (Search & TT Micro-Architecture — [ordering.cpp](../Stello.Cpp/src/search/ordering.cpp), [table.hpp](../Stello.Cpp/src/search/table.hpp), [table.cpp](../Stello.Cpp/src/search/table.cpp), [endgame/search_impl.hpp](../Stello.Cpp/src/endgame/search_impl.hpp)):**
  - Compact 24 B `EndMove`/`LocalMove` with cached `uint16_t slot`, uninitialized stack move arrays, branchless child rank-sort in `ordering.cpp`, and trivially-copyable TT `Slot` (`uint64_t[8]` + `std::atomic_ref<uint64_t>`, removing redundant `std::construct_at` init loops).
- **2.2 (Compact 64 KB Stability & Bit Utils — [stable.hpp](../Stello.Cpp/src/endgame/stable.hpp), [stable.cpp](../Stello.Cpp/src/endgame/stable.cpp), [bits_avx2.hpp](../Stello.Cpp/src/endgame/bits_avx2.hpp)):**
  - Replaced 1 MB edge tables with single 64 KB `g_edge_table[65536]` (`EDGE_STABILITY`), SSE2 `_mm_movemask_epi8` A/H column extraction, vectorized AVX2 `full_lines_avx2` + `stable_of_avx2` using `_mm_testc_si128`, and 3-instruction AVX2 `transpose_avx2`.
- **2.3 (Bitboards, `__vectorcall`, Midgame Caching, WASM SIMD128 — [compiler.hpp](../Stello.Cpp/src/core/compiler.hpp), [bits.hpp](../Stello.Cpp/src/endgame/bits.hpp), [bits_avx2.hpp](../Stello.Cpp/src/endgame/bits_avx2.hpp), [engine_impl.hpp](../Stello.Cpp/src/search/engine_impl.hpp)):**
  - Merged ray tables into single-cache-line `alignas(64) LrMask LR_MASK[64]`, added `STELLO_VECTORCALL` on Win64 hot helpers, AVX2 `legal_and_potential_avx2`, midgame `search_inc_deep` flip caching (`look >= 4`) + `known_legal` pass-through, and WASM `<wasm_simd128.h>` `wasm_i8x16_shuffle` `legal_moves`.
- **2.4 (`SIMULLASTFLIP` & `last4..1` — [bits.hpp](../Stello.Cpp/src/endgame/bits.hpp), [bits_avx2.hpp](../Stello.Cpp/src/endgame/bits_avx2.hpp), [last_impl.hpp](../Stello.Cpp/src/endgame/last_impl.hpp)):**
  - Implemented branchless `simul_last_flip_portable` + 3-lookup BMI2 `simul_last_flip_bmi2`, updated `solve_last`, and refactored `last4..1` (`noexcept` `last4_fast..last1_fast`, deferred `LastStats` flushed at `last4` / public wrappers, partial-reduction `__m128i` `probe_flips_fast` + `!_mm_testz_si128`, exact fail-soft parity).
- **2.5 (`x86-64-v4` 256-Bit EVEX Dispatch, Zero `zmm` — [cpu.hpp](../Stello.Cpp/src/core/cpu.hpp), [cpu.cpp](../Stello.Cpp/src/core/cpu.cpp), [kernel.hpp](../Stello.Cpp/src/search/kernel.hpp), [bits_avx512.hpp](../Stello.Cpp/src/endgame/bits_avx512.hpp), [search_v4.cpp](../Stello.Cpp/src/search/search_v4.cpp)):**
  - Extended `CpuTier { Portable, V3, V4 }` with `--cpu auto|portable|v3|v4` (`STELLO_TIER_DISPATCH`), `flips_avx512cd`, `legal_moves_avx512`, `legal_and_potential_avx512`, `stable_of_avx512`, 1-op GFNI `horizontal_mirror_gfni`, and `-mprefer-vector-width=256` / `/arch:AVX512 /vlen=256` (`zmm = 0`).
- **2.6 (Non-Temporal TT Clear, Pre-ETC Prefetch, `/GS-`, Clang PGO — [search_v3.cpp](../Stello.Cpp/src/search/search_v3.cpp), [table.cpp](../Stello.Cpp/src/search/table.cpp), [CMakePresets.json](../Stello.Cpp/CMakePresets.json), [build_pgo.cmd](../Stello.Cpp/cmake/build_pgo.cmd)):**
  - Added AVX2 `_mm256_stream_si256` + `_mm_sfence()` TT clear (ASan-guarded), pre-ETC child prefetch loop in `end_deep`, `/GS-` in MSVC Release/Native presets, and reproducible `build_pgo.cmd` workflow.

### 2. Rejected & Deferred Experiments (Measured Decisions)

1. **16-bit `loc[64]` stack store + dynamic 11-bucket scalar eval loop:** Caused 18-cycle Zen 4 store-to-load forwarding stalls; replaced with 4-byte-aligned `VPGATHERDD` (and 32-bit `indices[]` in WASM).
2. **Explicit 11-pointer unrolled scalar eval sum:** Regressed `eval_sum` to `166.69` cyc (`d12` `0.649 s`); kept `VPGATHERDD` in `v3`/`v4` and `sum_flat_scalar<OddSlot>` in portable.
3. **`dual_legal_moves_avx2` (`18.16` cyc vs `2 × 9.07` cyc) & staged `min_mob` early branches:** `d14` slowed from `1.689 s` to `1.700 s`; kept sequential `legal_moves` and branchless evaluation.
4. **`2.E3` insertion sort + `switch` `single_square_score`:** Regressed by `~12 %` due to branch mispredictions vs branchless child rank-sort.
5. **`2.E4` runtime `concurrent_tt` branching:** Both inline and `STELLO_NOINLINE` variants regressed 1t slightly vs unconditional `std::atomic_ref<uint64_t>`.
6. **Incremental `FullLines` propagation (2.2):** FFO `#40–49` slowed (`15.92 s` vs `15.85 s`) because `stability_cut` fires infrequently under high empty/score thresholds.
7. **WASM 21-op `wasm_sum_bytes_diag` `flip_sse_acepck` & 3× `byteswap64` `legal_moves` (2.3):** Regressed WASM `ffo-40-44` by `~66 ms` (`2995.1 → 3061.3 ms`); also A/B tested 64-bit scalar `legal_moves` in WASM (`890.5 ms` `mid-d12` / `3011.9 ms` `ffo-40-44`), which lost to `wasm_i8x16_shuffle` SIMD `legal_moves` (`871.2 ms` / `2913.5 ms`).
8. **Removing `FLIP_NEIGHBOURS` guards on `last3` moves 2/3 (2.4):** Regressed FFO `#40–49` (`15.20 s` without guards vs `15.12 s` with).
9. **`LASTFLIP_HIGHCUT` (2.4):** Deferred to Phase 5 because lazy high-cutoff `last1_nws` alters fail-soft scores and pass node counts, breaking Gate P2 exact-node parity.
10. **256-bit `flips_avx512_gfni` (2.5):** Measured `6.36` cyc vs `4.08` cyc for `flips_avx512cd`; kept `flips_avx512cd` (and kept `horizontal_mirror_gfni` at `0.37` cyc).
11. **LUT-free 256-bit AVX-512CD `last1_fast` on `v4` (2.5):** Measured `solve_1 = 9.02` cyc (`ffo-40-49` `14.61 s`) vs 3-lookup BMI2 `simul_last_flip_bmi2` (`solve_1 = 6.95–7.09` cyc, `ffo-40-49` `14.04 s`) on Zen 4 7800X3D.
12. **Hardware `CRC32C` (`_mm_crc32_u64`) TT hash & unconditional entry TT prefetching (2.5/2.6):** `CRC32C` measured `1.58–1.69` cyc vs `0.93` cyc multiply-mix; entry TT prefetching regressed `d14` (`1.620 → 1.697 s`) and `ffo-40-49` (`14.04 → 14.07 s`) on the 96 MB L3 7800X3D.

### 3. `stello-cpp microbench` Final Per-Primitive Cycles (`clang-release`, `repeats=500,000`, `warmup=1,000`)

All shared primitive checksums matched identically across `--cpu portable`, `--cpu v3`, and `--cpu v4` (`hash_board` compares portable mul-mix vs SSE4.2 `crc32c`).

| Primitive | Phase 0 `v3` | Phase 2 `portable` | Phase 2 `v3` (`AVX2`) | Phase 2 `v4` (`AVX-512VL`) | `v4` vs P0 `v3` |
|---|---:|---:|---:|---:|---:|
| `flips` | `6.35` | `18.12` | `6.62` | **`4.08`** | **−35.7 %** |
| `last_flip_count` | `4.54` | `6.85` | `7.40` | `7.17` | +57.9 %* |
| `solve_1` | `7.99` | `13.22` | `7.23` | **`6.95`** | **−13.0 %** |
| `legal_moves` | `9.28` | `31.66` | `9.07` | **`8.07`** | **−13.0 %** |
| `potential_mobility` | `6.50` | `18.09` | `6.65` | **`5.80`** | **−10.8 %** |
| `legal_and_potential` | `15.86` | `41.51` | `11.45` | **`9.63`** | **−39.3 %** |
| `dual_legal_moves` | — | `28.80` | `18.16` | **`15.54`** | — |
| `stable_of` | `31.08` | `33.36` | `14.99` | **`13.69`** | **−56.0 %** |
| `transpose` | — | `5.06` | `2.34` | **`2.33`** | — |
| `horizontal_mirror` | — | `3.28` | `3.21` | **`0.37`** | **−88.5 %** (vs `v3`) |
| `hash_board` | — | **`0.93`** | `1.58` | `1.61` | — |
| `eval_state_of` | — | `510.78` | `40.55` | **`40.52`** | — |
| `eval_sum` | `162.28` | `140.17` | `120.71` | **`116.90`** | **−28.0 %** |
| `eval_after_move` | `162.96` | `206.41` | `145.80` | **`141.53`** | **−13.2 %** |
| `eval_advance` | `28.60` | `62.20` | **`6.58`** | `6.61` | **−76.9 %** |

\* `last_flip_count` now delegates to full `simul_last_flip` (`solve_1` uses that single lookup to replace both `last_flip_count` passes + score logic, cutting net `solve_1` cycles by `−13.0 %`).

### 4. `.NET` FFI Benchmarks vs Phase 0 (`v5.0-ref/`, `hash-bits=19`)

| Benchmark | Positions | Threads | Phase 2 Nodes | Phase 2 Time | Phase 2 NPS | Phase 0 Time | Speedup | Parity |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| `midgame-depth10` | 100 | 1 | 3,083,842 | **0.229 s** | 13.48M | 0.237 s | **+3.5 %** | `100/100 identical` |
| `midgame-depth12` | 100 | 1 | 8,120,201 | **0.587 s** | 13.84M | 0.631 s | **+7.6 %** | `100/100 identical` |
| `midgame-depth14` | 100 | 1 | 22,875,227 | **1.646 s** | 13.90M | 1.780 s | **+8.2 %** | `100/100 identical` |
| `midgame-depth16` | 100 | 1 | 60,566,022 | **4.333 s** | 13.98M | 4.549 s | **+5.0 %** | `100/100 identical` |
| `midgame-depth16-8t` | 100 | 8 | 83,751,776 | **1.420 s** | 59.00M | 1.526 s | **+7.5 %** | Non-det MT |
| `midgame-depth16-16t` | 100 | 16 | 102,427,394 | **1.363 s** | 75.16M | 1.426 s | **+4.6 %** | Non-det MT |
| `midgame-random-depth8` | 1,000 | 1 | 8,699,892 | **0.613 s** | 14.19M | 0.640 s | **+4.4 %** | `1000/1000 identical` |
| `midgame-handover-depth12` | 300 | 1 | 297,050,433 | **3.872 s** | 76.72M | 4.531 s | **+17.0 %** | `298/300` (`C3` fix*) |
| `ffo-40-49` | 10 | 1 | 1,070,469,669 | **14.1 s** | 76.04M | 16.664 s | **+18.4 %** | `10/10 identical` |
| `ffo-40-49-8t` | 10 | 8 | 1,159,505,480 | **2.591 s** | 447.56M | 3.114 s | **+20.2 %** | `10/10 exact` |
| `ffo-40-49-16t` | 10 | 16 | 1,319,777,355 | **2.172 s** | 607.70M | 2.629 s | **+21.0 %** | `10/10 exact` |
| `ffo-50-59-8t` | 10 | 8 | 38,002,855,330 | **90.2 s** | 421.16M | 105.095 s | **+16.5 %** | `10/10 exact` |
| `ffo-50-59-16t` | 10 | 16 | 40,280,874,692 | **70.8 s** | 568.78M | 93.144 s | **+31.5 %** | `10/10 exact` |

\* On `midgame-handover-depth12`, `298/300` match `v5.0-ref` identically; `r00100e19` (`2,165,217 → 798,646` nodes, `e6`, `-58`) and `r00196e18` (`8,124,978 → 2,910,248` nodes, `h2`, `-40`) keep identical move and score with fewer nodes due to Phase 1 bugfix `C3` (`keep_midgame_move` root restoration before the exact solve). `stello-v5` reproduces `303,631,734` while Phase 2 `portable`, `v3`, and `v4` all identically visit `297,050,433` nodes (`5.872 s` / `4.084 s` / `3.816 s`).

### 5. WebAssembly ST/MT Sizes & Node.js Benchmarks vs Phase 0 (`v24.19.0`, `hash-bits=19`)

- **Artifact sizes (raw / gzip / brotli):**
  - `engine/stello_wasm.js`: `10,543` / `2,757` / `2,459` B
  - `engine/stello_wasm_bg.wasm`: `9,164,312` / `8,982,297` / `8,960,826` B (`+5.9 KB` brotli vs Phase 0)
  - `engine-mt/stello_wasm.js`: `10,543` / `2,757` / `2,459` B
  - `engine-mt/stello_wasm_bg.wasm`: `9,194,463` / `8,993,998` / `8,974,354` B (`+10.0 KB` brotli vs Phase 0)

| Variant | `mid-d10` (`3,083,842` 1t) | `mid-d12` (`8,120,201` 1t) | `ffo-40-44` (`122,045,591` 1t) |
|---|---|---|---|
| `ST 1t` | **`329.4 / 331.6 ms`** (`9.36M` nps, **+14.2 %** vs `376.3 ms`) | **`871.2 / 875.6 ms`** (`9.32M` nps, **+13.9 %** vs `992.1 ms`) | **`2913.5 / 2919.2 ms`** (`41.89M` nps, **+2.8 %** vs `2995.1 ms`) |
| `MT 1t` | **`335.1 / 339.4 ms`** (`9.20M` nps, **+15.1 %** vs `385.8 ms`) | **`880.7 / 885.0 ms`** (`9.22M` nps, **+16.1 %** vs `1022.2 ms`) | **`2902.5 / 2908.5 ms`** (`42.05M` nps, **+3.5 %** vs `3003.5 ms`) |
| `MT 4t` | **`262.6 / 277.8 ms`** (`19.18M` nps, **+10.9 %** vs `291.1 ms`) | **`579.5 / 602.6 ms`** (`21.83M` nps, **+12.1 %** vs `649.4 ms`) | **`1072.2 / 1111.7 ms`** (`130.56M` nps, **+3.2 %** vs `1106.2 ms`) |

### 6. Compiler Flags (`/GS-`), Reproducible Clang PGO, & Zero-ZMM Audit

- **MSVC `/GS-` (`msvc-release`):** `mid-bench d14` `1.894 → 1.838 s` (`+3.0 %`), `solve-bench ffo-40-49` `15.00 → 14.81 s` (`+1.3 %`).
- **Clang instrumentation PGO ([build_pgo.cmd](../Stello.Cpp/cmake/build_pgo.cmd)):** `mid-bench d14` `1.644 → 1.623 s` (`+1.3 %`), `ffo-40-49` `13.99 s` (`#40` at `100.14M` nps).
- **`llvm-objdump -d` `zmm` audit:** `0` `zmm` instructions across `Stello.Cpp/build/windows-ffi/stello_engine.dll`, `clang-release/{stello-cpp.exe,stello_engine.dll}`, `msvc-release/{stello-cpp.exe,stello_engine.dll}`, and `clang-native/stello-cpp.exe`.

### 7. Phase 2 Verification Match (`100 ms`, `1t`, `starts-v6.txt`, `1,174` Games, `book=off`)

| Side B vs Side A (`stello-v5`) | Score (95 % CI) | W / D / L | Elo (95 % CI) | LOS | Discs/g | Avg Depth (A / B) | First Solve (A / B) | NPS (A / B) | Max Time (A / B) | Wall Time |
|---|---|---|---|---|---|---|---|---|---|---|
| `stello` (`Phase 2`) | **49.1 %** (`47.4–50.8 %`) | `+476 =201 −497` | **−6** (`−18 to +6`) | 15.0 % | `0.0 ± 0.2` | `17.29 / 17.36` | `20.4 / 20.6` | `19.23M / 21.10M` (**+9.7 %**) | `125 / 104 ms` | [6m 12s](benchmark-results/match_p2_v6_vs_v5_100ms_1t.md) |

### 8. Gate P2 Full Verification Record

- **C++ presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`):** `0` compiler warnings, `3/3` ctests passed each (all `105` parity signatures verified across `Portable`, `V3`, and `V4`, plus `bits_avx2_tests`, `bits_avx512_tests`, and ASan/UBSan clean).
- **`.NET` Release build (with FFI & WASM ST/MT rebuilds):** `0 Warning(s), 0 Error(s)`, `zmm = 0`.
- **`.NET` & WASM test suites (post-final-edit):**
  - `Stello.Engine.Tests`: `521 passed, 0 failed, 0 skipped` (`7 s`).
  - `Stello.Net.Tests`: `127 passed, 0 failed, 0 skipped` (`18 s`).
  - `Egaroucid.Engine.Tests`: `118 passed, 0 failed, 8 skipped` (`38 s`, optional `TaggedV30_*` reference DLL tests skipped).
  - WASM Node tests (`engine.test.mjs`, `engine-mt.test.mjs`): `20/20 passed, 0 failed` (`3.63 s`).

### 9. Phase 2 Addendum (Zero-Node-Change Speed Experiments)

Evaluated Priorities 1–4 from [Possible remaining Stello speed optimizations.md](Possible%20remaining%20Stello%20speed%20optimizations.md) one at a time in alternated `A, B, B, A, A, B` runs (`hash-bits=19`, `1` thread) against the Phase 2 baseline (`b0aeaf7`), preserving 100 % exact node, evaluation, move, score, depth, and score-kind parity.

- **Summary of experiments:**
  1. **Priority 1 — `7 → 6` empty reply mask handoff (`end_simple` → `end_fast`) — KEPT:**
     - Instrumentation on FFO `#40–49`: `41,685,722` of `44,859,387` `end_fast` calls arrived with a known reply mask (`92.92 %`), including `1,220,122` known passes (`0`).
     - Isolated `A, B, B, A, A, B` vs `b0aeaf7`: Native `ffo-40-49` (`v4`, `1,070,469,669` nodes) improved **`+0.83 %`** (mean `76.05M → 76.68M` NPS, won 3/3 paired runs); `midgame-handover-d12` remained flat (`3.904 → 3.909 s`); WASM ST `ffo-40-44` (`122,045,591` nodes) improved **`+0.63 %`** (`2905.4 → 2887.3 ms`, won 3/3 paired runs); WASM MT1 `ffo-40-44` improved **`+0.83 %`** (`2907.3 → 2883.3 ms`, won 3/3 paired runs).
  2. **Priority 2 — 16-bit packed eval sum (`VPADDW`) (`sum_gather_avx2_vpaddw<OddSlot>`) — KEPT (`v3`/`v4`):**
     - Added load-time `EvalWeights::packed_vpaddw_safe()` (`8 * min_w >= -32768 && 8 * max_w <= 32767`) keeping exact wide 32-bit fallback for extreme custom models (verified with a synthetic `−6000/+6000` `egaroucid16` test in `parity_incremental_eval`), clean `vpaddw` + single `vpslld`/`vpsrad` codegen (`zmm = 0`).
     - Isolated `A, B, B, A, A, B` vs Exp 1: `mid-d12 v4` **`+1.09 %`** (`0.5833 → 0.5770 s`, 3/3 pairs); `mid-d14 v4` **`+1.96 %`** (`1.6837 → 1.6513 s`, 3/3 pairs); `mid-d16 v4` **`+1.25 %`** (`4.3833 → 4.3290 s`, 3/3 pairs); `mid-d14 v3` **`+1.06 %`** (`1.7103 → 1.6923 s`, 3/3 pairs).
  3. **Priority 3 — Static score reuse (`search_inc_deep` → `try_probcut_inc`) — REVERTED (re-evaluate in Phase 4):**
     - Instrumentation (`mid-bench v4`): produced vs consumed counts were `393,444 / 138,324` (`35.16 %`) at `d12`, `1,098,376 / 306,578` (`27.91 %`) at `d14`, and `2,892,211 / 649,541` (`22.46 %`) at `d16` with `0` mismatches (`57/57` `stello_tests`).
     - Clean uninstrumented `A, B, B, A, A, B` vs Exp 2: flat/mixed natively (`mid-d12 v4` `+0.44 %`, `mid-d14 v4` `−0.36 %`, `mid-d16 v4` `+0.19 %`, `mid-d14 v3` `−0.23 %`) and flat on WASM ST (`mid-d10` `−0.5 %`, `mid-d12` `0.0 %`, `mid-d14` `+0.1 %`). Saving `~0.5–1.5 %` of total node evaluations did not overcome `score_by_sq[64]` stack traffic and the extra parameter; reverted cleanly and left for re-evaluation in Phase 4 Item 2 (`hash_move`-first).
  4. **Priority 4 — Sibling-delta pattern evaluation (`SiblingBaseline`, `slot_square_masks_`) — KEPT ON NATIVE `Portable` ONLY (`search_inc_deep`); REJECTED ON `v3`/`v4`, WASM SIMD128, & `look == 0`:**
     - `parity_incremental_eval` verifies `init_sibling_baseline` + `sum_weights_sibling_delta` across all `positions.txt` fixtures and all 6 models (`built_in`, `classic11`, `plus15`, `egaroucid16`, mobility, and `−6000/+6000`), checking `scratch_delta == 0` after every call.
     - **`4a` (`search_inc_deep`, `>= 3` non-hash children):** Native `portable` won 3/3 pairs on `mid-d12` (**`+1.40 %`**, `0.8450 → 0.8333 s`) and `mid-d14` (**`+2.34 %`**, `2.4080 → 2.3530 s`). Native `v4` regressed (`−2.4 %` `d12`, `−0.3 %` `d14`, `−2.3 %` `d16`) vs AVX2 `VPGATHERDD` + `VPADDW`, and WASM ST (`__wasm_simd128__`) regressed (`−0.6 %` `d10`, `−1.2 %` `d12`, `−1.7 %` `d14`, flat `ffo-40-44`) vs unrolled SIMD128 lane advance + flat gather.
     - **`4b` (lazy `look == 0` sibling-delta in `search_inc`, `list.size() >= 4`):** Regressed on WASM ST (`−11 % to −12 %`), native `v4` (`−6 % to −7 %`), native `v3` (`−5.5 %`), and trimmed `portable` (`+1.26 %` vs `4a`-only `+2.34 %` at `d14`) because early `index == 1..2` cutoffs do not amortize `init_sibling_baseline` and the stack frame; removed completely. `4a` stays strictly compiled via `if constexpr (Tier == cpu::CpuTier::Portable)` under `#ifndef __wasm_simd128__`.

- **Cumulative Final Verification (Post-Addendum vs `b0aeaf7` & `v5.0-ref/`):**
  - **Cumulative native `A, B, B, A, A, B` vs `b0aeaf7`:** `mid-d12 v4` `0.5823 → 0.5807 s` (`+0.29 %`, best `14.00M` nps); `mid-d14 v4` `1.6513 → 1.6387 s` (**`+0.77 %`**, best `14.05M` nps); `mid-d16 v4` `4.3767 → 4.3427 s` (**`+0.78 %`**, 3/3 pairs, best `14.00M` nps); `ffo-40-49 v4` `14.13 → 14.00 s` (**`+0.95 %`**, mean `75.79M → 76.39M` nps); `mid-d12/d14 portable` `0.8183 → 0.8220 s` / `2.3170 → 2.3150 s`.
  - **`.NET` FFI `--compare Plans\benchmark-results\v5.0-ref\*.json`:**
    - `midgame-depth10`: `100/100 identical`, `0.232 s` vs `0.237 s` (`+2.3 %`)
    - `midgame-depth12`: `100/100 identical`, `0.603 s` vs `0.631 s` (`+4.7 %`)
    - `midgame-depth14`: `100/100 identical`, `1.674 s` vs `1.780 s` (`+6.3 %`)
    - `midgame-depth16`: `100/100 identical`, `4.369 s` vs `4.549 s` (`+4.1 %`)
    - `midgame-random-depth8`: `1000/1000 identical`, `0.603 s` vs `0.640 s` (`+6.1 %`)
    - `midgame-handover-depth12`: `298/300 identical` (`r00100e19` & `r00196e18` reflect Phase 1 `C3`, identical move & score), `3.892 s` vs `4.531 s` (`+16.4 %`)
    - `ffo-40-49`: `10/10 identical`, `14.2 s` vs `16.7 s` (`+17.5 %`)
  - **Final WASM ST/MT sizes & `wasm_bench.mjs`:**
    - `engine/stello_wasm_bg.wasm`: `9,164,799` raw / `8,982,504` gzip / `8,960,860` B brotli
    - `engine-mt/stello_wasm_bg.wasm`: `9,194,950` raw / `8,994,205` gzip / `8,974,311` B brotli
    - `st`: `mid-d10` `326.0 / 328.8 ms` (`9.46M`), `mid-d12` `859.2 / 864.8 ms` (`9.45M`), `ffo-40-44` `2899.7 / 2909.4 ms` (`42.09M`)
    - `mt1`: `mid-d10` `332.1 / 335.5 ms` (`9.29M`), `mid-d12` `876.3 / 882.1 ms` (`9.27M`), `ffo-40-44` `2892.3 / 2897.0 ms` (`42.20M`)
    - `mt4`: `mid-d10` `267.8 / 286.5 ms` (`19.02M`), `mid-d12` `583.7 / 603.7 ms` (`21.52M`), `ffo-40-44` `1060.4 / 1072.7 ms` (`132.99M`)
    - WASM ST `A, B, B, A, A, B` vs `b0aeaf7`: `ffo-40-44` **`+0.58 %`** (`2907.9 → 2891.2 ms`); midgame flat (`323.0 → 326.5 ms` `d10`, `856.7 → 857.8 ms` `d12`, `2420.1 → 2438.0 ms` `d14`).
  - **Post-addendum Gate checks (all after final edit):**
    - All 5 C++ presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`): `0` compiler warnings, `3/3` ctests each.
    - `llvm-objdump -d` `zmm` count: `0` across `windows-ffi\stello_engine.dll`, `clang-release\{stello-cpp.exe, stello_engine.dll}`, `msvc-release\stello-cpp.exe`, and `clang-native\stello-cpp.exe`.
    - WASM Node tests (`engine.test.mjs`, `engine-mt.test.mjs`): `20/20 passed, 0 failed`.
    - `.NET` Release build (`0 Warning(s), 0 Error(s)`): `Stello.Engine.Tests` `521 passed, 0 failed`, `Stello.Net.Tests` `127 passed, 0 failed`, `Egaroucid.Engine.Tests` `118 passed, 0 failed, 8 skipped` (`TaggedV30_*`).

---

## Phase 3: Threads (Gate P3 Complete)

### 1. Stage 0 — Preserved Phase 2 Reference & Pre-Phase-3 Baselines

- **Preserved reference engine:** `scratch/p2_final_ref/stello_engine_p2.dll` (built from Phase 2 + addendum commit `5e9dc80`), SHA-256 `7C725B01127453D17B3AC2EC8FCA80A15FE51B2357C025B02CA511C9C4B78A81` (for `engine=stello-lib` Gate P3 matches).
- **Phase 2 (`5e9dc80`) MT baselines (`clang-release`, `hash-bits=19`, `v4`):**
  - `FFO #40–49`:
    - `1t`: `1,070,469,669` nodes in `14.16 s` (`75.62M` nps)
    - `8t` (x3): `1,155.8M / 1,218.3M / 1,171.1M` (mean `1,181.7M`, `+10.4 %`), `2.522 / 2.666 / 2.566 s` (**mean `2.585 s`**, `5.48×`, `457.2M` nps; `#48` = `287.4M` in `0.687 s`)
    - `16t` (x3): `1,302.4M / 1,291.5M / 1,323.9M` (mean `1,305.9M`, `+22.0 %`), `2.080 / 2.071 / 2.108 s` (**mean `2.086 s`**, `6.79×`, `625.9M` nps; `#48` = `315.2M` in `0.580 s`)
  - `FFO #50–59`:
    - `8t`: `39,053,379,656` nodes in **`92.37 s`** (`422.82M` nps)
    - `16t`: `42,414,159,067` nodes in **`74.40 s`** (`570.12M` nps)
  - `mid-d16` (`--passes 3`):
    - `8t` (x3): `1.403 / 1.427 / 1.417 s` (mean **`1.416 s`**, best `84.12M` nodes, `59.42M` nps)
    - `16t` (x3): `1.370 / 1.333 / 1.305 s` (mean **`1.336 s`**, best `101.65M` nodes, `76.07M` nps)

### 2. Stage 1 — Fixed the 4 Verified Threading Bottlenecks

1. **`may_split()` idle + move-count guard** across all 6 split sites (`search_root`, `search_inc`, `search_inc_deep`, `solve_root`, `end_window`, `end_deep` via `std::popcount(legal & ~done)`) eliminating heap `ScopeTasks` allocation when workers are busy or `< 2` moves remain.
2. **Incremental `PatternState` in `MidgameChild`:** passed `const eval::PatternState* parent_eval`, `uint64_t move_flips`, `uint64_t known_legal`, and `bool parent_is_even`, computing `advance_eval_state` lazily inside `child_null_window` / `child_window` instead of from-scratch `eval_state_of_node`.
3. **Removed 1,024-node shared `node_sink->fetch_add` contention:** flushed thread-local `nodes` to `node_sink` at `>= 65,536` nodes in `check_abort()` + exact flush in `Helpers::finish()`, and cache-line-aligned (`alignas(64)`) `nodes`/`evaluations` counters in `SearchEngine::search()`.
4. **Decoupled nested locks in `pool.hpp`:** moved `idle.fetch_add` and `scope->finish()` outside `queue_mutex`, and incremented `tasks_->running` prior to locking `queue_mutex` with RAII rollback.
- **Stage 1 measured deltas:** `FFO #40–49` `8t` improved `2.585 → 2.488 s` (`+3.9 %`, `1,159.0M` nodes, `465.7M` nps); `16t` improved `2.086 → 2.032 s` (`+2.7 %`, `1,280.4M` nodes, `630.2M` nps); `57/57` `clang-release` & `clang-sanitized` green.

### 3. Stage 2 — Dynamic `SplitNode`, Helpful Master, & Wide-Window Wave-Cut Fix

- Replaced per-wave `std::make_shared<ScopeTasks>` / `std::make_unique` allocations with stack-allocated `SplitNode` / `WaveSplitNode` (`SplitEntry`) and **Helpful Master** work-stealing in [pool.hpp](../Stello.Cpp/src/search/pool.hpp) and [ybwc_impl.hpp](../Stello.Cpp/src/search/ybwc_impl.hpp).
- **Diagnosis & resolution of initial `#48` node explosion (`287M → 741M`):**
  - Initial single-pass `SplitNode` did not cut siblings on wide-window (`beta - alpha > 1`, e.g. `#48` Exact `[0, +65]` / `24`-empty `end_window`) fail-highs (`probe > alpha && probe < beta`) and allowed parallel `child_window` re-searches on stale `alpha`.
  - Split `Kernel<Tier>::split()` cleanly into:
    - **Fast path (`beta - alpha == 1`, NWS):** zero-wave single-pass stack `SplitNode` calling `child_null_window`, cutting immediately on `score > alpha`.
    - **Wide-window path (`beta - alpha > 1`):** `WaveSplitNode` runs null-window probes, cuts siblings immediately on any `score > window_alpha`, sorts collected fail-highs descending, re-probes/re-searches sequentially on the master (freeing all workers to split inside the PV subtree), and launches any remaining moves in a tightened-`alpha` wave.
  - **Pool/lifetime fixes:** moved `active_helpers.fetch_sub` + `wait_cv.notify_all()` inside `wait_mutex` with an unconditional `wait_mutex` drain in `wait_split` whenever `ever_helped == true` (preventing stack `SplitNode` UAF); added `unclaimed > joining` with RAII `JoinGuard` to eliminate empty helper joins; restricted Helpful Master `may_split()` checks to ancestors (`has_waiting_ancestor()`); tie-broke equal-priority splits by lowest `active_helpers`; and enabled mid-loop splitting (`i >= 1`) across all split sites (`end_window`, `solve_root`, `search_root`, `search_inc`, `search_inc_deep`).

### 4. Stage 3 — Tuning & Investigations (`3a`–`3d`)

- **3a. Split criteria sweep (FFO `#40–49` & `#50–59` + `mid-d16`, `8t` & `16t`):**
  - `SPLIT_MAX_SLAVES`: swept `{2, 3, 4, 6, 8, 64}` (`end=14`, `todo=2`, uncapped root, ancestor waiting). `sl=4` won `16t` across both `#40–49` (`1.903 s` mean, `7.44×`, `+19.3 %` nodes vs `sl=6` `1.916 s` / `+22.6 %`) and `#50–59` (`65.0 s` vs `66.1 s`) while also beating `sl=6` on `mid-d16` (`1.434 / 1.482 s` vs `1.452 / 1.548 s` on 1-pass), and delivering `2.242 s` (`+9.5 %` nodes) @ `8t` `#40–49` and `82.3 s` @ `8t` `#50–59`.
  - `split_root_cap=on` regressed `#40–49`; `SPLIT_MIN_MOVES_TODO ∈ {2, 3, 4}` favored `2`; `YBWC_END_MIN_EMPTIES ∈ {13, 14, 15, 16}` confirmed **`14`**.
  - Locked `SPLIT_MAX_SLAVES = 4`, `SPLIT_MIN_MOVES_TODO = 2`, `YBWC_END_MIN_EMPTIES = 14` in [engine.hpp](../Stello.Cpp/src/search/engine.hpp) and removed all temporary `--split-*` CLI flags.
- **3b. Windows physical-core CPU affinity spike — REVERTED:**
  - Prototyped and cleanly tested interleaved `A/B/C` (`PIN=0` unpinned, `PIN=1` `SetThreadGroupAffinity` + staggered core base, `PIN=2` `SetThreadIdealProcessorEx`) at `8t`:
    - `FFO #40–49` (3 runs): `PIN=0` `2.377 s` (`516.3M` nps), `PIN=1` `2.436 s` (`529.1M` nps), `PIN=2` `2.372 s` (`512.7M` nps).
    - `mid-d16` (3 runs, 1-pass): `PIN=0` `1.461 s` (`64.4M`), `PIN=1` `1.485 s` (`65.3M`), `PIN=2` `1.408 s` (`65.1M`).
    - `FFO #50–59`: `PIN=0` `83.9 s` (`477.1M`), `PIN=1` `79.5 s` (`501.6M`, `+5.5 %`), `PIN=2` `82.6 s` (`477.8M`).
  - Neither `PIN=2` (`+0.2 %` `#40–49`, `+1.6 %` `#50–59`) nor `PIN=1` (`−2.4 %` `#40–49`, `−1.6 %` `mid-d16`) met the keep thresholds; reverted cleanly.
- **3c. Midgame split threshold & Stockfish-style Lazy SMP diversification — KEPT:**
  - `LAZY_MODE=-1` (pure YBWC, 0 lazy helpers) regressed `mid-d16` to `1.795 s` (`8t`) / `1.888 s` (`16t`) at `~40M` nps; `(workers + 2) / 4` clearly outperformed `1` and `workers / 2`.
  - Lowering `YBWC_MID_MIN_DEPTH` from `6` to `5` (`mid-d16` `p3` `1.301 → 1.268 s` @ `8t`, `1.285 → 1.209 s` @ `16t`) combined with Stockfish-style diversification (`DIV=3`: rotating `moves[1..]` by `(i + 1) % (count - 1)` per helper + staggered `+1/+2` skip-depth stride) beat `MIN_DEPTH=6` across `mid-d16` (`1.268 / 1.209 s` vs `1.292 / 1.229 s`, `-7 %` `16t` nodes `95.7M`) and `mid-d14` (`0.576 / 0.562 s` vs `0.582 / 0.573 s`).
- **3d. ABDADA spike — SKIPPED BY TRIGGER RULE:**
  - `16t FFO #40–49` reached `7.44×` scaling (`1.903 s` during Step 2, `7.23–7.34×` final) with `+19.3–22.4 %` nodes and `FFO #50–59` `16t` gained `+8.1–14.5 %`, avoiding any throwaway TT slot changes right before Phase 4's Unified TT rewrite.

### 5. Stage 4 — Final Phase 3 Benchmarks & Full Verification Matrix

- **Native final idle-CPU benchmarks (`clang-release`, `hash-bits=19`, `v4`):**

| Benchmark | Threads | `Stello 5.0` | `Phase 2` (`5e9dc80`) | **`Phase 3`** | Phase 3 vs Phase 2 | Phase 3 vs `v5.0` | Scaling vs `Phase 3 1t` |
|---|---:|---|---|---|---|---|---|
| `FFO #40–49` | 1t | `16.664 s` (`64.24M`) | `14.16 s` (`75.62M`) | **`13.94 s` (`76.81M`)** | `+1.6 %` (nodes `1,070.5M` exact) | `+19.5 %` | `1.00×` |
| `FFO #40–49` (3× med / mean) | 8t | `3.114 s` (`375.96M`) | `2.566 / 2.585 s` (`457.2M`) | **`2.284 / 2.437 s` (`512.0M` mean)** | **`+12.3 %` med / `+6.1 %` mean** | **`+36.3 %` med** | `6.10×` med / `5.72×` mean (`+10.1–16.3 %` nodes) |
| `FFO #40–49` (3× mean) | 16t | `2.629 s` (`478.91M`) | `2.086 s` (`625.9M`) | **`1.928 s` (`679.7M` mean)** | **`+8.2 %`** | **`+36.4 %`** | **`7.23×`** (`1,310.1M`, `+22.4 %` nodes) |
| `FFO #50–59` | 1t | `559.648 s` (`54.52M`) | — | **`468.97 s` (`65.06M`)** | — (`30,513.3M` exact) | `+19.3 %` | `1.00×` |
| `FFO #50–59` | 8t | `105.095 s` (`357.55M`) | `92.37 s` (`422.82M`) | **`79.29 s` (`480.43M`)** | **`+16.5 %`** | **`+32.5 %`** | **`5.91×`** (`38,093.7M`, `+24.8 %` nodes) |
| `FFO #50–59` | 16t | `93.144 s` (`461.14M`) | `74.40 s` (`570.12M`) | **`68.83 s` (`634.31M`)** | **`+8.1 %`** | **`+35.3 %`** | **`6.81×`** (`43,654.9M`, `+43.1 %` nodes) |
| `mid-d16` (`p3`) | 1t | `4.549 s` (`13.31M`) | `4.369 s` (`13.86M`) | **`4.332 s` (`13.98M`)** | `+0.9 %` (`60.57M` exact) | `+5.0 %` | `1.00×` |
| `mid-d16` (`p3`, 3× mean) | 8t | `1.526 s` (`54.95M`) | `1.416 s` (`59.42M`) | **`1.221 s` (`69.54M`)** | **`+16.0 %`** | **`+25.0 %`** | **`3.55×`** (`84.89M`, `+40.2 %` nodes) |
| `mid-d16` (`p3`, 3× mean) | 16t | `1.426 s` (`69.50M`) | `1.336 s` (`76.07M`) | **`1.216 s` (`83.61M`)** | **`+9.9 %`** | **`+17.3 %`** | **`3.56×`** (`101.60M`, `+67.8 %` nodes) |

- **Post-Phase 3 multithreaded scaling vs external engines (`Edax AVX` PGO `v4` & `Egaroucid` AVX-512, idle CPU, `Stello` `hash-bits=19`, external `hash-bits=21`):**

| Benchmark | Threads | `Stello` (Phase 3) Time (NPS) | `Stello` Scaling | `Edax AVX` Time (NPS) | `Edax AVX` Scaling | `Egaroucid` Time (NPS) | `Egaroucid` Scaling |
|---|---:|---|---|---|---|---|---|
| `FFO #40–49` | 1t | `13.94 s` (`76.81M`) | `1.00×` | `11.496 s` (`81.18M`) | `1.00×` | `9.470 s` (`54.13M`) | `1.00×` |
| `FFO #40–49` (3× med / mean) | 8t | `2.284 / 2.437 s` (`512.0M`) | **`6.10×`** med / `5.72×` mean (`+10.1–16.3 %` nodes) | `1.994 / 1.994 s` (`495.8M`) | `5.77×` (`+5.9 %` nodes) | `1.852 / 1.966 s` (`313.2M`) | `5.11×` med / `4.82×` mean (`+8.9–40.3 %` nodes) |
| `FFO #40–49` (3× mean) | 16t | `1.928 s` (`679.7M`) | `7.23×` (`+22.4 %` nodes) | `1.529 s` (`667.1M`) | **`7.52×`** (`+9.3 %` nodes) | `1.454 s` (`413.9M`) | `6.51×` (`+17.4 %` nodes) |
| `FFO #50–59` | 1t | `468.97 s` (`65.06M`) | `1.00×` | `253.005 s` (`93.43M`) | `1.00×` | `325.701 s` (`47.72M`) | `1.00×` |
| `FFO #50–59` | 8t | `79.29 s` (`480.43M`) | `5.91×` (`+24.8 %` nodes) | `38.487 s` (`637.50M`) | **`6.57×`** (`+3.8 %` nodes) | `50.333 s` (`315.09M`) | `6.47×` (`+2.0 %` nodes) |
| `FFO #50–59` | 16t | `68.83 s` (`634.31M`) | `6.81×` (`+43.1 %` nodes) | `26.97 s` (`948.02M`) | **`9.38×`** (`+8.2 %` nodes) | `42.52 s` (`449.98M`) | `7.66×` (`+23.1 %` nodes) |
| `mid-d16` (`p3`) | 1t | `4.332 s` (`13.98M`) | `1.00×` | `8.946 s` (`55.86M`)\* | `1.00×` | `5.784 s` (`14.32M`)\*\* | `1.00×` |
| `mid-d16` (`p3`, 3× mean) | 8t | `1.221 s` (`69.54M`) | **`3.55×`** (`+40.2 %` nodes) | `3.248 s` (`175.16M`)\* | `2.75×` (`+13.8 %` nodes) | `1.892 s` (`54.90M`)\*\* | `3.06×` (`+25.4 %` nodes) |
| `mid-d16` (`p3`, 3× mean) | 16t | `1.216 s` (`83.61M`) | **`3.56×`** (`+67.8 %` nodes) | `3.238 s` (`209.09M`)\* | `2.76×` (`+35.5 %` nodes) | `1.787 s` (`62.25M`)\*\* | `3.24×` (`+34.3 %` nodes) |

  - \* `Edax AVX` `mid-d16` (`sd 16`) follows `LEVEL[16]`, searching `e48/e42/e36/e30` at depth `16@73%` (`1.996 s` at `1t`) and `e26` at endgame `26@87%` (`6.950 s` at `1t`); `setboard` TT clearing across 100 positions adds ~`0.17 s` single-threaded overhead (excluding that gives `~2.86×` at `8t` / `~2.87×` at `16t`).
  - \*\* `Egaroucid` `mid-d16` uses `--depth 16 --mpc 74` (pure midgame depth `16` across all 100 positions including `e26`); with `--level 16` (`e26` solved at `26@98%`), `Egaroucid` scales `12.56 s` (`1t`) → `4.059 s` (`8t`, `3.09×`, `+22.9 %` nodes) → `3.706 s` (`16t`, `3.39×`, `+31.6 %` nodes).

- **WASM ST/MT final sizes & `wasm_bench.mjs` (`node v24.19.0`):**
  - `engine/stello_wasm_bg.wasm`: `9,177,061` raw / `8,984,952` gzip / `8,963,114` B brotli
  - `engine-mt/stello_wasm_bg.wasm`: `9,208,981` raw / `8,997,728` gzip / `8,974,001` B brotli
  - `st`: `mid-d10` `328.3 / 332.1 ms` (`9.39M`), `mid-d12` `862.4 / 871.2 ms` (`9.42M`), `ffo-40-44` `2997.9 / 3012.1 ms` (`40.71M`)
  - `mt1`: `mid-d10` `330.7 / 336.2 ms` (`9.33M`), `mid-d12` `873.1 / 882.8 ms` (`9.30M`), `ffo-40-44` `2902.4 / 2915.0 ms` (`42.05M`)
  - **`mt4`:** `mid-d10` **`180.1 / 188.0 ms`** (`22.72M`, **`+48.7 %` vs P2 `267.8 ms`**, **`+61.6 %` vs v5.0**); `mid-d12` **`424.1 / 447.1 ms`** (`25.69M`, **`+37.6 %` vs P2 `583.7 ms`**, **`+53.1 %` vs v5.0**); `ffo-40-44` **`933.0 / 949.8 ms`** (`151.17M`, **`+13.7 %` vs P2 `1060.4 ms`**)!
- **Verification matrix (all run after final edit):**
  - **All 5 C++ presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized` ASan+UBSan):** `0` warnings, `3/3` ctests passed each (including `58/58` `stello_tests` with new `phase3_helpful_master_transitions_and_rapid_stop_stress`).
  - **`windows-ffi` & `zmm` audit:** `0` warnings; `0` `zmm` instructions across `windows-ffi\stello_engine.dll`, `clang-release\{stello-cpp.exe, stello_engine.dll}`, `msvc-release\stello-cpp.exe`, `clang-native\stello-cpp.exe`.
  - **WASM Node tests (`engine.test.mjs`, `engine-mt.test.mjs`):** `20/20 passed, 0 failed` (`3.69 s`).
  - **`.NET` Release build & tests:** `0 Warning(s), 0 Error(s)`; `Stello.Engine.Tests` `521/521`, `Stello.Net.Tests` `127/127`, `Egaroucid.Engine.Tests` `118 passed, 0 failed, 8 skipped` (`TaggedV30_*`).
  - **`.NET` FFI `1t` `--compare Plans\benchmark-results\v5.0-ref\*.json`:** `100/100 identical` on `mid-d12` (`0.578` vs `0.631 s`, `+9.1 %`), `100/100` on `mid-d14` (`1.620` vs `1.780 s`, `+9.9 %`), `100/100` on `mid-d16` (`4.330` vs `4.549 s`, `+5.1 %`), `10/10` on `ffo-40-49` (`14.2` vs `16.7 s`, `+17.8 %`).
- **Post-review threading fixes & Gate P3 notes ([Thread feedback.md](Thread%20feedback.md)):**
  - **Code fixes (`Stello.Cpp`):**
    1. Forwarded `heartbeat_owner->stop` (`cancel`, `move_now`, `hard_limit_ms`) and `pending_exception` inside both `SplitNode`/`WaveSplitNode` `poll` loops and `root_split_point` propagation, plus added `poll()` immediately after helping each descendant split in `ThreadPool::wait_split`.
    2. Added lock-guarded `PendingException` propagation across worker/poll/wait paths with a stop-only `WaitOnExit` drain so throwing `ProgressCallback`s unwind cleanly to the caller instead of triggering `std::terminate`.
    3. Replaced split `live_best_move` / `live_best_score` atomics with a single 64-bit `live_best` atomic snapshot (`pack_live_best` / `unpack_live_best`).
    4. Replaced `AlignedCounter` struct with `alignas(64) std::atomic<uint64_t>` locals in `SearchEngine::search()`, eliminating MSVC `C4324`.
  - **Regression tests (`threads_tests.cpp`):** `phase3_nested_helpful_master_wait_forwards_cancel_and_move_now`, `phase3_throwing_progress_callback_propagates_from_splits`, `phase3_atomic_live_best_snapshot_has_no_torn_pairs`.
  - **Gate P3 benchmark/match distinctions:** Faster wall times across all suites vs Phase 2, while `16t` FFO node inflation rose slightly (`+22.0 → +22.4 %` on `#40–49`, `+39.0 → +43.1 %` on `#50–59`, while `8t` `#50–59` fell `+28.0 → +24.8 %`), `mid-d16` gained cleanly over Phase 2 at `8t`/`16t` with flat `8t → 16t` (`1.221 → 1.216 s`), and `starts-v6.txt` matches beat Phase 2 conclusively at `500 ms` (`53.2 %`, `99.9 %` LOS) and positively at `2,000 ms` (`51.1 %`, `90.4 %` LOS, CI `49.4–52.9 %`).

### 6. Gate P3 Verification Matches (`stello` Phase 3 vs `Phase2` `stello_engine_p2.dll`, `threads=8`, `--workers 1`, `hash-bits=19`, `book=off`)

| Side B vs Side A (`Phase2`) | Time / Move | Starts (Games) | Score (95 % CI) | W / D / L | Elo (95 % CI) | LOS | Discs/g | Avg Depth (A / B) | First Solve (A / B) | NPS (A / B) | Max Time (A / B) | Wall Time |
|---|---:|---:|---|---|---|---|---|---|---|---|---|---|
| `Phase3` (`Match 1`) | 500 ms | 587 (`1,174`) | **53.2 %** (`51.3–55.1 %`) | `+462 =325 −387` | **+22** (`+9 to +36`) | **99.9 %** | `+0.2 ± 0.1` | `23.97 / 24.32` | `25.7 / 26.0` | `166.48M / 207.46M` (**+24.6 %**) | `512 / 515 ms` | [3h 18m](benchmark-results/match_p3_v6_vs_p2_500ms_8t.md) |
| `Phase3` (`Match 2`) | 2,000 ms | 587 (`1,174`) | **51.1 %** (`49.4–52.9 %`) | `+425 =351 −398` | **+8** (`−4 to +20`) | **90.4 %** | `+0.1 ± 0.1` | `27.04 / 27.38` | `27.8 / 28.0` | `192.47M / 242.45M` (**+26.0 %**) | `2001 / 2001 ms` | [12h 02m](benchmark-results/match_p3_v6_vs_p2_2000ms_8t.md) |

---

## Phase 4: Search

### 1. Pre-Phase 4 Deep Pure-Midgame Baselines (`midgame-deep.txt`)

- **`midgame-deep.txt` (`24` positions: `8` each at `48/42/36` empties, `1` pass, idle CPU, `Stello` `clang-release` `v4` `hash-bits=19`, external `hash-bits=21`):**

| Depth | Threads | `Stello` (Phase 3) Time (Nodes, NPS) | `Stello` EBF / Scaling | `Edax AVX` Time (Nodes, NPS) | `Edax AVX` EBF / Scaling | `Egaroucid` (`--mpc 74`) Time (Nodes, NPS) | `Egaroucid` EBF / Scaling |
|---|---:|---|---|---|---|---|---|
| `d16` | 1t | `1.964 s` (`23.52M`, `11.98M`) | — | `0.768 s` (`24.20M`, `31.50M`) | — | `2.053 s` (`29.70M`, `14.46M`) | — |
| `d18` | 1t | `5.140 s` (`63.61M`, `12.37M`) | `EBF 1.645` | `1.207 s` (`39.22M`, `32.50M`) | `EBF 1.273` | `4.460 s` (`64.43M`, `14.45M`) | `EBF 1.473` |
| `d20` | 1t | `14.05 s` (`171.89M`, `12.23M`) | `EBF 1.644` | `2.583 s` (`87.64M`, `33.93M`) | `EBF 1.495` | `9.756 s` (`137.78M`, `14.12M`) | `EBF 1.462` |
| `d22` | 1t | `37.52 s` (`483.73M`, `12.89M`) | `EBF 1.678` | `4.848 s` (`166.82M`, `34.41M`) | `EBF 1.380` | `23.94 s` (`329.13M`, `13.78M`) | `EBF 1.546` |
| `d20` (3× med / mean) | 8t | `2.985 / 3.201 s` (`261.9M` mean, `81.81M`) | **`4.71×`** med / `4.39×` mean (`+52.4 %` nodes) | `1.069 / 1.073 s` (`100.4M` mean, `93.50M`) | `2.42×` (`+14.5 %` nodes) | `2.628 / 2.635 s` (`167.8M` mean, `63.68M`) | `3.71×` (`+21.8 %` nodes) |
| `d22` | 8t | `7.629 s` (`672.93M`, `88.21M`) | **`4.92×`** (`+39.1 %` nodes) | `1.810 s` (`225.87M`, `124.79M`) | `2.68×` (`+35.4 %` nodes) | `5.476 s` (`364.68M`, `66.60M`) | `4.37×` (`+10.8 %` nodes) |

- Keeping empties `≥ 36` guarantees all three engines stay 100 % in pure midgame through `d22` without endgame handover triggers. By `d22` (`1t`), `Stello`'s tree expands (`EBF 1.64–1.68` vs `Edax AVX` `1.27–1.50` and `Egaroucid` `1.46–1.55`), visiting `2.90×` more nodes than `Edax AVX` and `1.47×` more than `Egaroucid`, even though `Stello` matches `Egaroucid` at `d16` and outscales both engines on `8t` (`4.92×` at `d22`).
