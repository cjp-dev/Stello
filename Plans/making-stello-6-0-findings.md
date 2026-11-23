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

*(Items 1–5 completed and verified; `v5.0-ref/` benchmarks and `starts-v6.txt` historical/external baseline matches pending user go.)*

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

### 6. Phase 0 Baseline Matches (`starts-v6.txt`, `1,174` Games Each, `book=off`, Side B = `Stello 5.0`)

*(Matches running sequentially; Part A `v5.0-ref/` JSONs + WASM baseline and external FFO `#40–49` / `#50–59` benchmarks pending.)*

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

### 7. Test Verification (Items 1–5)

- `.NET Release build` (`-p:StelloWasmEngine=false`): `0 Warning(s), 0 Error(s)`.
- `Stello.Engine.Tests`: `519/519` passed (`8 s`).
- `Stello.Net.Tests`: `116/116` passed (`17 s`).
- `Egaroucid.Engine.Tests`: `111 passed, 0 failed, 8 skipped` (`37 s`; skipped tests are the optional `TaggedV30_*` reference DLL tests).
- C++ `clang-release` (`ctest --preset clang-release`): `2/2` passed (`31.76 s`).
- WASM Node tests (`node --test Stello.Cpp/tests/wasm/engine.test.mjs Stello.Cpp/tests/wasm/engine-mt.test.mjs`): `19/19` passed (`3.80 s`).
