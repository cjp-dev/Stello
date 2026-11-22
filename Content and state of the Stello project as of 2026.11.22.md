# Content and State of the Stello Project (as of 2026-11-22)

> **Purpose:** Onboarding and context-kickstart document for AI chat sessions working on **Stello**.
> **Snapshot date:** 2026-11-22 · **Active branch:** `feature/stello-5.0` · **Completed in 5.0 cycle:** Phases 0, 1, 2, 3, 4, 5, 6, and Phase 7 (Final matches, FFO `#40–59` benchmarks, Version `5.0.0` bump, and 31-chapter documentation update; [Plans/making-stello-5-0-findings.md](Plans/making-stello-5-0-findings.md), [docs/29-stello-5-0-speed-threads-search-and-book.md](docs/29-stello-5-0-speed-threads-search-and-book.md)) · **Next milestone:** Merge `feature/stello-5.0` into `main` (deploy & live browser check), then begin Stello 6.0 ([Plans/making-stello-6-0-plan.md](Plans/making-stello-6-0-plan.md)).

---

## 1. Project Overview & Origins

**Stello** is an Othello (Reversi) engine and application suite by Claus Pedersen (`cjp-dev`), licensed under **GPL-3.0-only** ([LICENSE](LICENSE), [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)), with the goal of reaching world-class playing strength across both Windows desktop (WPF) and browser (Blazor WebAssembly, live at <https://othello.clauspedersen.net>).

The codebase builds on three key pillars:
1. **Original Stello C++ (1992–1998):** Located in [Stello C++/](Stello%20C++/) (Visual C++ 6 / MFC app with engine sources in `BRAIN/` and legacy book `OPENING`). Supplied the original midgame search frame, response table, time control, and opening book format.
2. **Two world-class reference engines** (kept locally in git-ignored root folders):
   - **[Egaroucid-main/](Egaroucid-main/)** (Takuto Yamana, GPL-3.0-or-later): Inspiration for Stello's pattern evaluation, endgame solver, Multi-ProbCut, and training data. An exact single-thread C# port lives in [Stello.Net/Egaroucid.Engine/](Stello.Net/Egaroucid.Engine/) with a C++ fixture generator in [Stello.Net/tools/Egaroucid.Reference/](Stello.Net/tools/Egaroucid.Reference/).
   - **[edax-reversi-master/](edax-reversi-master/)** (Richard Delorme, GPL-3.0): Inspiration for bitboard move generation and endgame search ideas, and source of the FFO #40–59 endgame benchmark positions (`problem/fforum-40-59.obf`).
3. **Detailed project documentation & plans:** How the engine works is documented in 31 chapters under [docs/](docs/README.md) (with [docs/01-overview.md](docs/01-overview.md) as the architectural entry point), and every improvement cycle has paired plan/findings documents in [Plans/](Plans/).

---

## 2. Version History & Engine Iterations

Stello has evolved across C++, .NET/C#, Rust, and modern C++20:

| Version | Date | Implementation & Status | Key Milestone |
|---|---|---|---|
| **Stello C++** | 1992–1998 | Original MFC / Visual C++ 6 ([Stello C++/](Stello%20C++/)) | Hand-crafted eval, alpha-beta, solver, self-learning book |
| **Stello 1.0** | Sep–Oct 2026 | C# / .NET 10, frozen in `Stello.Engine.V1` | 1-to-1 port of C++ brain with bitboards + WPF & Blazor WASM apps |
| **Stello 2.0** | Nov 2026 | C# / .NET 10, frozen in `Stello.Engine.V2` | Trained `plus15` pattern eval (`stello-eval.bin`), Egaroucid-style solver, PVS + MPC; +501 Elo vs V1 (500 ms), +62 Elo vs Egaroucid port (500 ms) |
| **Stello 3.0** | Nov 2026 | Rust workspace ([Stello.Rust/](Stello.Rust/)) | 1-to-1 Rust port + speed pass (2.8–3.1× midgame, 1.75× FFO) + multi-threading (Lazy SMP + YBWC, seqlock TT) + Rust WASM; +78 Elo (1t) / +121 Elo (8t) vs V2 |
| **Stello 3.1** | Nov 2026 | Rust `3.1.0`, **frozen/preserved** as `stello-v3` | Solved-abort/clock/worker fixes, Computer vs Computer, desktop parity |
| **Stello 4.0** | Nov 2026 | C++20 `4.0.0`, **frozen/preserved** as `stello-v4` (`Stello.Cpp/ref/stello_engine_v4.dll`) | 1-to-1 port of Rust 3.1 to C++20 (`stello_version() = 40000`, ABI v3); 100% 1-thread node parity with 3.1, zero external deps, faster FFI & Emscripten WASM |
| **Stello 5.0** | Nov 2026 | **Active C++20 `5.0.0` core** ([Stello.Cpp/](Stello.Cpp/), `stello_version() = 50000`, ABI v3) | `x86-64-v3` AVX2/BMI2 dispatch + incremental `PatternState` + WASM `-msimd128` (`+39%` midgame, `+37%` FFO 1t, `+98–100%` 1t WASM); YBWC root splits + brother cut + `(w+2)/4` Lazy SMP (`2.00×` faster 8t FFO #40–49 vs 4.0 at `3.18 s`); 3-stage selective endgame `mpc-end` (`+10 Elo` @ 500 ms, `+22 Elo` @ 2 s); Format v3 `egaroucid16 + mobility × 60` evaluation (`4.97` MAE / `6.74` RMSE, beating Egaroucid's `5.05 / 6.90`; `+33–34 Elo` combined); `6,000`-position `v5` book (`recalc`, `trim`, `calclib`, Lincke DOE `expand`; `55.0%` vs 4.0 book @ 500 ms, `-41%` binary size); **+58 to +84 Elo** vs 4.0, **+115 to +145 Elo** vs 2.0, **+211 / +109 / −1 Elo** vs Egaroucid across 100 / 500 / 2,000 ms |

---

## 3. Repository Structure & Architecture

### Top-Level Folders

| Path | Tracked in Git? | Role |
|---|---|---|
| [Stello.Cpp/](Stello.Cpp/) | Yes (`build/`, `out/` ignored) | **Active C++20 search engine** (`stello_core`, `stello_engine` FFI DLL, `stello_wasm` ST/MT, `stello-cpp` CLI, self-contained CTest & WASM Node test suite, `ref/stello_engine_v4.dll`) |
| [Stello.Net/](Stello.Net/) | Yes (`bin/`, `obj/`, generated `wwwroot/` assets ignored) | **.NET 10 solution** ([Stello.Net.slnx](Stello.Net/Stello.Net.slnx)): apps, C# engine wrapper, book/game/eval code, frozen baselines, tools, xUnit tests |
| [Stello.Rust/](Stello.Rust/) | Yes (`target/`, `pkg/` ignored) | **Frozen Stello 3.1 Rust workspace** (`stello-core`, `stello-ffi`, `stello-wasm`, `stello-cli`); kept only as `engine=stello-v3` reference |
| [Stello C++/](Stello%20C++/) | Yes | Original 1992–1998 MFC C++ program and legacy `OPENING` book |
| [docs/](docs/README.md) | Yes | 31-chapter technical & historical documentation + `docs/stello-1/` archive |
| [Plans/](Plans/) | Yes (`*.games.jsonl` ignored) | Design plans, findings logs ([making-stello-5-0-findings.md](Plans/making-stello-5-0-findings.md), [making-stello-6-0-plan.md](Plans/making-stello-6-0-plan.md)), and `benchmark-results/` JSON/MD reports (`v4.0-ref/`, `v3.0-ref/`) |
| [external/GameShell/](external/GameShell/) | Submodule | Shared UI shell for `Stello.Web` (clone with `--recurse-submodules`) |
| [Training data/](Training%20data/) | **No (gitignored)** | Raw Egaroucid training datasets (~8.1 GB), converted `stello-bin/` (~8.49 GB), and MPC CSV fits |
| [Egaroucid-main/](Egaroucid-main/) | **No (gitignored)** | Local Egaroucid source & `bin/resources/` (used by `Egaroucid.Engine.Tests` & `Egaroucid.Reference`) |
| [edax-reversi-master/](edax-reversi-master/) | **No (gitignored)** | Local Edax source & FFO problem files |

### Architecture: One Engine, Two Languages

`Stello.Engine` (C#) owns the board/rules/game record (`Board`, `Square`, `Move`, `Game`), opening book (`OpeningBook`, `BookTracker`, `BookLearner`, `BookRecalculator`, `BookTrimmer`, `BookCalclib`, `BookExpander`), and `ComputerPlayer`. Search calls cross a narrow **C ABI v3** (`StelloNative.cs` → `stello_engine.dll` on Windows/POSIX) or run in browser **Web Workers** (`wwwroot/engine` single-threaded and `wwwroot/engine-mt` Emscripten `-pthread -msimd128` with shared memory).

**`Stello.Cpp/src/` modules:**
- `core/` (`board`, `square`, `bits`, `cpu`, `version`, `compiler`): 16-byte bitboard `Board`, shift-and-mask moves/flips (`wasm_simd128.h` under Emscripten), runtime CPUID detection for `has_avx2_bmi2()` (`x86-64-v3`: AVX2 + BMI1 + BMI2 + POPCNT + LZCNT + fast `pext`, disabled by `--cpu portable`).
- `eval/` (`gzip`, `patterns`, `patterns_bmi2`, `symmetry`, `weights`): Zero-dependency RFC 1951/1952 DEFLATE/gzip decompressor; `PatternSet` (`classic11`, `plus15`, `egaroucid16`) with precomputed per-square feature deltas (`square_deltas`) and `PatternState` for incremental make/unmake updates (`AVX2` / `WASM SIMD128` / scalar); `EvalWeights` supporting **Format v2** (pattern-only) and **Format v3** (pattern + 96 delta-encoded legal-mobility weights per stage: 48 own + 48 opponent move-count slots).
- `search/` (`engine`, `engine_impl.hpp`, `kernel.hpp`, `kernel_impl.hpp`, `search_v3.cpp`, `search_portable.cpp`, `pool`, `ybwc`, `ybwc_impl.hpp`, `ordering`, `table.hpp`, `table.cpp`, `time`, `limits`, `options`):
  - Dual-TU `Kernel<true>` (`search_v3.cpp`, compiled with `-mavx2 -mbmi -mbmi2 -mpopcnt -mlzcnt -mno-avx512f` / `/arch:AVX2`, zero `zmm` registers) and `Kernel<false>` (`search_portable.cpp`, baseline `x86-64`), dispatched once at `Searcher::run`.
  - Iterative deepening PVS, Multi-ProbCut (`ProbCutErrors`), response table + incremental pattern/mobility move ordering, inlined 64-byte seqlocked 2-entry TT (`table.hpp`), scoped worker pool, Lazy SMP (`(workers + 2) / 4` midgame helpers, `0` solve helpers when YBWC active) + YBWC (including root splitting in `search_root`/`solve_root`, immediate local brother-cut interruption, and ~100 ms split-wait progress heartbeats).
  - 3-stage full-window `[-64, +64]` selective endgame (`mpc-end` stages `74% / 1.13` → `88% / 1.55` → `98% / 2.32`, `EndProbCutErrors`, `empties >= 16`) before exact `WLD → Exact`, plus `estimate_wld_ms` guard on `TimePerGame` solver budget escalation.
- `endgame/` (`search`, `search_impl.hpp`, `last`, `last_impl.hpp`, `bits`, `bits_avx2.hpp`, `stable`, `local`): Explicit 256-bit AVX2 `flips` and `legal_moves` (`bits_avx2.hpp`), WLD then exact solver, `last4`–`last1`, parity & `.bss` lock-free edge stability (`stable::of`), per-worker `LocalTable`.
- `ffi/` (`stello_ffi`) & `wasm/` (`protocol`, `threads`, `wasm_exports`, `stello_wasm.js`) & `cli/` (`stello-cpp`).

**`.NET 10` projects in [Stello.Net/Stello.Net.slnx](Stello.Net/Stello.Net.slnx) (13 projects):**
- **Apps & shared UI:** `Stello.App` (MVVM view models, settings, `IEngineHost`), `Stello.Net` (WPF Windows app, settings in `%AppData%\Stello`), `Stello.Web` (Blazor WASM app).
- **Engine & tests:** `Stello.Engine`, `Stello.Engine.Tests` (517 tests), `Stello.Net.Tests` (116 tests).
- **Frozen/reference engines (tools & tests only):** `Stello.Engine.V1`, `Stello.Engine.V2` (with its own isolated frozen Stello 2.0 `plus15 × 10` `stello-eval.bin`), `Egaroucid.Engine`, `Egaroucid.Engine.Tests` (100 passed / 8 optional skipped `TaggedV30_*`).
- **CLI tools:** `tools/Stello.BookTool`, `tools/Stello.Benchmark`, `tools/Stello.Trainer` (plus non-solution C++ harness `tools/Egaroucid.Reference`).

**Key data assets in [Stello.Net/Book/](Stello.Net/Book/):**
- `stello-eval.bin` (`8,892,085` bytes gzip / `38,434,770` bytes unpacked; Format v3 `egaroucid16 + mobility` × 60 stages, `320,289` weights/stage, held-out test split **MAE `4.97`, RMSE `6.74`** across 1,040,276 positions; embedded at C++ build time by [embed_eval.cmake](Stello.Cpp/cmake/embed_eval.cmake) for both desktop and WebAssembly).
- `opening-book.txt` (`370,387` B) & `opening-book.bin` (`114,169` B, `33,809` B brotli; `OpeningBook.EngineVersion = 5`; `6,000` canonical positions, `13,154` moves, `7,047` leaves all at `60s:v5`, built via `recalc` → `trim` → `calclib` → Lincke DOE `expand`, beating the 4.0 book **55.0%** at 500 ms). Copied into WPF `Data/OPENING` and Web `wwwroot/data/OPENING.bin`.
- `opening-book-stello1.txt` (frozen book for `stello-v1` matches).

---

## 4. Training Data & Evaluation (`Training data/` & `Stello.Trainer`)

The local git-ignored folder **[Training data/](Training%20data/)** (~16.59 GB total) holds the datasets downloaded from <https://www.egaroucid.nyanyan.dev/en/technology/train-data/> (redistribution forbidden by Egaroucid's terms) and Stello's pre-converted binary training files:

| Path | Contents | Details |
|---|---|---|
| `Egaroucid_Train_Data/0001_egaroucid_7_5_1_lv17/` | 26 files (`*.txt`) | 25.5M positions with final scores (Egaroucid 7.5.1 self-play at level 17) |
| `Egaroucid_Train_Data_v0002_0/` & `..._v0002_1/` | Folders `8`–`33` & `34`–`59` (100 files each, 5,200 files total) | 52M full game transcripts (`f5d6` format) from Egaroucid 7.8.0 vs Edax 4.5.5 at level 11, after 8–59 random opening moves |
| `stello-bin/` | 174 `.bin` files + `manifest.txt` (~8.49 GB) | Converted by `Stello.Trainer convert` (2026-10-31): **499.5M positions** (479.6M train, 10.0M validation, 10.0M test), capped at 10M/disc-count, split by game hash (2% val / 2% test). **17-byte records:** `own` (`u64`) + `opp` (`u64`) + `score` (`sbyte`) |
| `mpc-values.csv` & `mpc-end-values.csv` | CSV datasets | Deep-minus-shallow search error data generated by `Stello.Trainer mpc` / `mpc-end` for fitting Multi-ProbCut |

`Stello.Trainer` ([TrainerProgram.cs](Stello.Net/tools/Stello.Trainer/TrainerProgram.cs)) runs multi-threaded full-batch Adam in C# (`convert`, `train` with `--min-discs`/`--max-discs` and `--mobility on|off`, `report`, `size`, `mpc --model`, `mpc-end --model`) to produce `stello-eval.bin` and MPC models.

---

## 5. .NET Tools (`Stello.Net/tools/`)

- **[Stello.Benchmark](Stello.Net/tools/Stello.Benchmark/Program.cs):**
  - `match`: Two-game-per-start matches (`--a`, `--b`, `--starts`, `--max-starts`, `--workers`, `--name`, `--resume`). Supports `engine=stello` (C++ 5.0), `stello-v4` (`Stello.Cpp/ref/stello_engine_v4.dll`), `stello-v3` (Rust 3.1 DLL), `stello-v2`, `stello-v1`, `stello-lib` (`lib=...`, `sha256=...`), `egaroucid`, plus `opt=<key>=<value>` A/B switches, `threads=`, `hash-bits=`, `book=`, `eval=`. Writes `.games.jsonl` and reports into `Plans/benchmark-results/`.
  - `solve-bench`: FFO endgame benchmark (`--from`/`--to`, e.g., `#40–49` or `#50–59`), `--compare <ref.json>`.
  - `mid-bench`: Fixed-depth midgame benchmark (`--depth`), `--compare <ref.json>` for strict node/move/score parity checks.
  - `eval-compare`, `mid-quality`, `make-starts`, `make-positions`, `make-exact`, `parity-data`.
- **[Stello.BookTool](Stello.Net/tools/Stello.BookTool/Program.cs):**
  - `import`, `format`, `build`, `verify`, `stats`, `trim` (prune unreachable/bad branches and over-deep tails), `recalc` (re-search book leaves), `calclib` (search unbooked legal moves via `onlyMoves` and add within threshold), `expand` (Lincke Drop-Out Expansion with lazy deviations), `compare`, `match`.
- **[Stello.Trainer](Stello.Net/tools/Stello.Trainer/TrainerProgram.cs):**
  - `convert`, `train`, `report`, `size`, `mpc`, `mpc-end`.

---

## 6. Environment, Toolchain & Stello 5.0 Complete Results Summary

- **Primary machine:** Windows x64, **AMD Ryzen 7 7800X3D** (8 physical cores / 16 logical threads, Zen 4, 96 MB 3D V-Cache L3, AVX2 + BMI2 + fast 1-cycle `pext`).
- **Toolchain:** `.NET 10 SDK` (`10.0.401`) + `wasm-tools`, `CMake 3.31` + `Ninja 1.12`, `Clang/LLVM 23.1.5` (default for `.NET` FFI via `build_ffi.cmd`), `MSVC 19.44` (VS 2022 BuildTools), `Emscripten SDK` (`emsdk` via `build_wasm.cmd`/`build_wasm.sh`), `Node.js v24.19.0`.
- **Current verified test status (0 warnings everywhere):**
  - C++ (`ctest` across all 5 presets `clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`): **2/2 CTest targets (`48/48` tests)** passed, `stello-cpp 5.0.0 (50000)`, 0 ASan/UBSan findings.
  - `.NET` (`-c Release -p:StelloWasmThreads=required`): `Stello.Engine.Tests` **517/517**, `Egaroucid.Engine.Tests` **100 passed / 8 optional skipped**, `Stello.Net.Tests` **116/116**.
  - WASM (`node --test Stello.Cpp/tests/wasm/engine.test.mjs Stello.Cpp/tests/wasm/engine-mt.test.mjs`): **19/19** passed.
- **Summary of Stello 5.0 Phases 0–7 ([Plans/making-stello-5-0-findings.md](Plans/making-stello-5-0-findings.md), [docs/29-stello-5-0-speed-threads-search-and-book.md](docs/29-stello-5-0-speed-threads-search-and-book.md)):**
  - **Phase 0 (Setup & baselines):** Decoupled `Stello.Cpp/tests/` from `Stello.Rust/`, preserved `Stello.Cpp/ref/stello_engine_v4.dll` (`engine=stello-v4`), recorded `Plans/benchmark-results/v4.0-ref/`.
  - **Phase 1 + Pre-Phase-2 Disassembly Opts (Speed, 100% node-identical to 4.0):** `x86-64-v3` AVX2/BMI2 dispatch, incremental pattern indices (`PatternState`), inlined TT hot path (`table.hpp`), lock-free `.bss` `stable::of`, and WASM `-msimd128`:
    - Native `.NET` FFI `mid-bench d12`: **`+39.3%`** (`10.80M` → `15.05M` nps); `mid-bench d16`: **`+28.9%`** (`10.82M` → `13.94M` nps).
    - Native `.NET` FFI `solve-bench` FFO `#40–49` (1t): **`+37.1%`** (`23.6 s` → `17.2 s`, `64.65M` nps).
    - WASM (`-msimd128`): **`+98.5–100.1%`** ST midgame (`mid-d10` / `mid-d12`), **`+42.2%`** MT 4t midgame, **`+31.2%`** ST FFO `#40–44`.
  - **Phase 2 (Threads):** Root splitting in `search_root`/`solve_root`, local brother-cut interruption, ~100 ms split-wait heartbeats, `YBWC_END_MIN_EMPTIES = 14`, pure YBWC in `solve`, and `(workers + 2) / 4` Lazy SMP helpers (`2` @ 8t, `4` @ 15t/16t):
    - FFO `#40–49` (with `plus15`): `8t` **`3.542 s` (`4.88×` vs 1t, `399.98M` nps)**; `16t` **`3.042 s` (`5.69×` vs 1t, `502.70M` nps)**.
    - Gate P2 match (`8t` vs Phase 1 `8t`): **`51.1%` (`+8 Elo`) at 500 ms**; **`54.3%` (`+30 Elo`, LOS `99.9%`) at 2 s**.
  - **Phase 3 (Search — all 6 items rigorously tested & rejected at Gate P3, temporary switches removed):**
    - Tested refitted MPC error models & phase ramps (`8,340` games), 2nd TT move (`2,960` games), `soft-pct` 80/90/100% (`3,540` games), aspiration windows ±2/±4 discs (`2,360` games), `hash-bits=21` vs `19` at 2 s 8t (`600` games, where 64 MB `hash-bits=19` stayed inside the 7800X3D's 96 MB L3 and beat 256 MB by `+9.3%` NPS and `+0.39` plies), and root clog search (`1,180` games).
  - **Phase 4 (Endgame & Transition):**
    - *Item 1 (`end_local` 4-pattern table at 11–13 empties):* Cut FFO `#40–49` nodes by `−18.1%` (1t) / `−20.9%` (8t) and time by `−9.6%` / `−11.1%`, but shifted `FirstSolve` by only `+0.1` empties and scored `48.3%` (`−12 Elo`) at 500 ms; rejected and reverted.
    - *Item 2 (`mpc-end` selective endgame & solve handover — Accepted):* 3 full-window `[-64, +64]` selective endgame passes (`74% → 88% → 98%`, `empties >= 16`) before `WLD → Exact` plus `TimePerGame` `for_solver` guard won **`51.5%` (`+10 Elo`, LOS `97.1%`, `+0.5 ± 0.2` discs/g) at 500 ms** and **`53.2%` (`+22 Elo`, LOS `100.0%`, `+0.5 ± 0.1` discs/g) at 2,000 ms**.
  - **Phase 5 (Evaluation — Items 1 & 2 Accepted):**
    - *Item 1 (`egaroucid16 × 60`, `10M` pos/disc, `--smooth 0`):* Test MAE dropped `5.47 → 5.07` (RMSE `7.31 → 6.87`); won **`52.9%` (`+20 Elo`, LOS `99.7%`) at 500 ms** and **`52.0%` (`+14 Elo`, LOS `98.2%`) at 2,000 ms**.
    - *Item 2 (`egaroucid16 + mobility × 60`, Format v3):* Added 96 legal-mobility weights/stage (`48` own + `48` opponent); test MAE dropped to **`4.97` (RMSE `6.74`)**, beating Egaroucid (`5.05 / 6.90`); won **`51.8%` (`+13 Elo`, LOS `96.9%`) at 500 ms** and **`52.9%` (`+20 Elo`, LOS `98.4%`) at 2,000 ms** on top of Item 1 (`~+33–34 Elo` combined for Phase 5). Landed as unified `stello-eval.bin` (`8,892,085` B gzip; WASM `8.96 MB` brotli).
  - **Phase 6 (Opening Book — Items 1–4 & Gate P6 Accepted):**
    - *Item 1 (`recalc --time-s 60`):* Recalculated all `10,531` leaves to `v5` (average heuristic depth `25.18 → 29.61`, `+4.43` plies; `+281` net `Exact` solves; caught and repaired `87` corrupted pre-3.1 timed `X:60s` entries).
    - *Item 2 (`trim --hero-loss 100 --opp-loss unlimited --max-ply 30`):* Pruned unreachable 30-year-old Stello 1.0 branches and over-deep tails from `11,200 → 1,993` live positions (`−82.2%`), converting `373` severed `BackedUp` edges into `v5` leaves without changing any ancestor or root minimax values.
    - *Item 3 (`calclib --time-s 60 --threshold 100 --max-ply 30`):* Searched unbooked legal moves (`onlyMoves`) across all `1,967` candidate positions (`4h 12m`), adding `642` moves (`4,473 → 5,115` moves) and discovering better first moves in `450` positions (`22.6%`).
    - *Item 4 (`expand --time-s 60 --omega 50 --max-ply 28 --max-positions 6000`) & Gate P6:* Expanded book via Lincke DOE to **`6,000` positions / `13,154` moves / `7,047` `60s:v5` leaves** (`114,169` B binary = `33.8 KB` brotli, `-41%` vs 4.0). Beat 4.0 book **55.0%** (`53.4–56.5%`, `+0.7` discs/g, `1,272` games on `636` changed-move shared starts) and **51.8%** (`+0.3` discs/g, `568` games on all `284` ply-8 openings) at 500 ms.
  - **Phase 7 (Final Matches, FFO `#40–59`, Version `5.0.0` & Docs):**
    - **vs Stello 4.0 (`1,180` games each, no book):**
      - `1t`: `100 ms` **61.4% (`+81 Elo`)**, `500 ms` **58.3% (`+58 Elo`)**, `2,000 ms` **59.4% (`+66 Elo`)**.
      - `8t`: `100 ms` **60.5% (`+74 Elo`)**, `500 ms` **60.2% (`+72 Elo`)**, `2,000 ms` **61.9% (`+84 Elo`)**.
    - **vs Stello 3.1 (`1,180` games each, no book):**
      - `1t`: `100 ms` **61.9% (`+84 Elo`)**, `500 ms` **57.5% (`+53 Elo`)**, `2,000 ms` **60.3% (`+72 Elo`)**.
      - `8t`: `100 ms` **60.8% (`+76 Elo`)**, `500 ms` **61.1% (`+78 Elo`)**, `2,000 ms` **60.2% (`+72 Elo`)**.
    - **vs Stello 2.0 (`1,180` games each, no book, `1t`):** `100 ms` **69.7% (`+145 Elo`)**, `500 ms` **66.6% (`+120 Elo`)**, `2,000 ms` **66.0% (`+115 Elo`)**.
    - **vs Egaroucid C# port (`1,180` games each, `time-strict=on`, no book, `1t`):** `100 ms` **77.1% (`+211 Elo`)**, `500 ms` **65.2% (`+109 Elo`)**, `2,000 ms` **49.8% (`−1 Elo`, `+0.1 ± 0.2` discs/g — closing the `42.4%` / `−53 Elo` deficit from Stello 3.0/4.0)**.
    - **FFO `#40–49` & `#50–59` (`solve-bench`, `hash-bits=19`, 100% exact):**
      - `#40–49`: `1t` **`16.9 s` (`63.21M` nps)** vs `24.0 s` (4.0) / `25.9 s` (3.1); `8t` **`3.181 s` (`370.36M` nps, `2.00×` faster than 4.0)** vs `6.360 s` (4.0) / `7.029 s` (3.1).
      - `#50–59`: `1t` **`559.6 s` (`54.52M` nps)** vs `586.5 s` (4.0); `8t` **`110.8 s` (`351.51M` nps)** vs `144.6 s` (4.0) / `160.8 s` (3.1).

---

## 7. Build, Test & CLI Cheat Sheet

```powershell
# C++20 presets (from Stello.Cpp): clang-release | clang-native | msvc-release | msvc-native | clang-sanitized
cmake --preset clang-release
cmake --build --preset clang-release
ctest --preset clang-release --output-on-failure

# .NET full Release build & tests (builds native stello_engine.dll + WASM if emsdk active)
dotnet build Stello.Net/Stello.Net.slnx -c Release
dotnet build Stello.Net/Stello.Net.slnx -c Release -p:StelloWasmEngine=false   # skip Emscripten WASM
dotnet test Stello.Net/Stello.Net.slnx -c Release --no-build

# WASM Node tests (Stello.Cpp/tests/wasm/)
node --test Stello.Cpp/tests/wasm/engine.test.mjs Stello.Cpp/tests/wasm/engine-mt.test.mjs

# Benchmarks & matches (run from repo root or built bin dir)
dotnet run --project Stello.Net/tools/Stello.Benchmark -c Release -- mid-bench --depth 14 --passes 3
dotnet run --project Stello.Net/tools/Stello.Benchmark -c Release -- solve-bench --from 40 --to 49 --threads 8
dotnet run --project Stello.Net/tools/Stello.Benchmark -c Release -- match --a "time-ms=500" --b "engine=stello-v4,time-ms=500" --name test --resume

# Book tool & Trainer
dotnet run --project Stello.Net/tools/Stello.BookTool -c Release -- verify Stello.Net/Book/opening-book.txt Stello.Net/Book/opening-book.bin
dotnet run --project Stello.Net/tools/Stello.BookTool -c Release -- expand Stello.Net/Book/opening-book.txt --time-s 60 --omega 50 --max-ply 28 --max-positions 6000 --workers 8 --hash-bits 19
dotnet run --project Stello.Net/tools/Stello.Trainer -c Release -- report --model Stello.Net/Book/stello-eval.bin
```

---

## 8. Documentation ([docs/](docs/README.md)) & Plans ([Plans/](Plans/)) Index

- **`docs/` (31 chapters + `stello-1/` archive):**
  - **Part 1 (`01`–`15`):** Architecture (updated for Stello 5.0) — Overview (`01`), Board/Squares (`02`), Rules/Move Gen (`03`), Pattern + Mobility Evaluation (`04`), Move Ordering (`05`), Midgame Search & Threads (`06`), Seqlock TT (`07`), Endgame Solver (`08`), Time Control (`09`), Opening Book (`10`–`12`), Game Record (`13`), App Integration & WASM (`14`), GUI & Settings (`15`).
  - **Part 2 (`16`–`24`):** Stello 2.0 history — plan, benchmark methodology (`17`, updated with `stello-v4` & `mpc-end`), Egaroucid port, eval training, solver, search experiments, final V2 results.
  - **Part 3 (`25`–`27`):** Stello 3.0 & 3.1 — Rust rewrite, multi-threading (Lazy SMP + YBWC), fixes & desktop alignment.
  - **Part 4 (`28`):** Stello 4.0 — C++20 port, controlled Rust vs Clang vs MSVC benchmarks, FFI & Emscripten WASM.
  - **Part 5 (`29`):** Stello 5.0 — [29-stello-5-0-speed-threads-search-and-book.md](docs/29-stello-5-0-speed-threads-search-and-book.md).
  - **Reference (`30`–`31`):** [30-glossary.md](docs/30-glossary.md) & [31-references.md](docs/31-references.md).
- **`Plans/`:**
  - *Stello 5.0 plan & findings:* [making-stello-5-0-plan.md](Plans/making-stello-5-0-plan.md) & [making-stello-5-0-findings.md](Plans/making-stello-5-0-findings.md).
  - *Next cycle (Stello 6.0):* [making-stello-6-0-plan.md](Plans/making-stello-6-0-plan.md).
  - *Earlier engine cycles (plan + findings):* `Stello porting documentation.md` (V1), `egaroucid-port-*` + `egaroucid-comparison-report.md`, `improve-stello-strength-*` (V2), `making-stello-3-*` (3.0), `making-stello-3-1-*` (3.1), `Stello 4.0 C++ implementation.md` & `Stello 4.0 C++ findings.md` (4.0), `cpp-vs-csharp-speed.md`.
  - *UI, web, book & audits:* `Stello web architecture plan.md`, `Web architecture review.md`, `general-game-gui-specification.md`, `Opening book improvment.md`, `stello-2-docs-plan.md`, `stello-exhaustive-docs-and-code-audit-*`.
  - *Artifacts:* `Plans/benchmark-results/` (JSON baselines `v4.0-ref/`, `v3.0-ref/`, evaluation & match reports, including all 18 `match_p7_*.md` and 11 `p7_*_ffo*.json`) and `Plans/Stello 3.1 development snapshot status/`.

---

## 9. Working Rules & Engineering Gates

1. **Matches & long runs:** Start only on the user's explicit go, from a numbered list with time estimates. Check CPU idle state first, run nothing heavy concurrently, and preserve `.games.jsonl` (`--resume`).
2. **A/B switches (`SearchOptions` / `opt=`):** Every experimental engine change sits behind a temporary `SearchOptions` switch. Once its match gate passes or fails, **remove the switch immediately** (keep only the winning code).
   - Strength claims require **lower bound > 50%** in a 1,180-game match at 500 ms (`--workers 8` for 1t, `--workers 1` for 8t), confirmed at 2 s (600 games, `--max-starts 300`).
   - Kept node-changing optimizations require updating `stello-v3-signatures.txt` and C++/WASM parity fixtures so strict node-for-node tests remain enforced.
3. **Zero-warning gate:** All 5 C++ presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`), `.NET` tests, and WASM Node tests must pass with **0 compiler/MSBuild warnings**.
4. **Git & untracked assets:** Commit with identity `cjp-dev` + `Co-Authored-By` trailer; run `git fsck` after commits; fast-forward `main` only at release with user OK. **Never commit** `Training data/`, Egaroucid resources, `build/`, `out/`, `target/`, `wwwroot/engine*`, `scratch/`, or `*.games.jsonl`.

---

## 10. Current State & Next Actions

- **Stello 5.0 is complete and verified on `feature/stello-5.0`:**
  - All 18 final matches (`21,240` games) and 11 FFO `#40–59` `solve-bench` runs are recorded in [Plans/making-stello-5-0-findings.md](Plans/making-stello-5-0-findings.md) and [docs/29-stello-5-0-speed-threads-search-and-book.md](docs/29-stello-5-0-speed-threads-search-and-book.md).
  - Version `5.0.0` (`stello_version() = 50000`, `"Stello Version 5.0"`, `6,000` opening book positions) is built and verified across all 5 C++ CMake presets (`48/48` tests), `.NET` Release (`517/517` + `100/100` + `116/116`), and WASM (`19/19`).
  - Final release step for 5.0: Fast-forward `main` to `feature/stello-5.0`, push to origin, monitor the Azure Static Web Apps deploy, and run the live browser check at <https://othello.clauspedersen.net>.
- **Next major cycle:** [Plans/making-stello-6-0-plan.md](Plans/making-stello-6-0-plan.md).
