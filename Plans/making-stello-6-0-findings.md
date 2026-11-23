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

*(Items 1–5, `v5.0-ref/` benchmarks, WASM 5.0 ST/MT baseline, and external FFO `#40–59` benchmarks completed; `starts-v6.txt` baseline matches `#1`–`#26` completed and paused after Match `#26` pending user go for the remaining 4 `2,000 ms 8t` matches `#27`–`#30`.)*

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

*(Paused after Match `#26` per user request; 4 remaining `2,000 ms 8t` matches `#27`–`#30` pending user go.)*

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

### 7. Test Verification (Phase 0 Setup Complete, Matches Paused After #18)

- `.NET Release build` (`-p:StelloWasmEngine=false`): `0 Warning(s), 0 Error(s)`.
- `Stello.Engine.Tests`: `519/519` passed.
- `Stello.Net.Tests`: `116/116` passed.
- `Egaroucid.Engine.Tests`: `114 passed, 0 failed, 8 skipped` (`39.74 s`; skipped tests are the optional `TaggedV30_*` reference DLL tests).
- All 5 C++ presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`): `0` compiler warnings, `3/3` ctest tests passed (`stello_tests`, `stello_cli_version`, `stello_cli_microbench`).
- WASM Node tests (`node --test Stello.Cpp/tests/wasm/engine.test.mjs Stello.Cpp/tests/wasm/engine-mt.test.mjs`): `19/19` passed (`3.77 s`).
