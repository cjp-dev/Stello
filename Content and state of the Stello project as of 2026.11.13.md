# Content and State of the Stello Project (as of 2026-11-13)

> **Purpose:** Onboarding and context-kickstart document for future AI chat sessions working on **Stello**.
> **Snapshot date:** 2026-11-13 · **Current release:** Stello 4.0 (`v4.0`, `stello_version() = 40000`, ABI v3) · **Next milestone:** Start [Plans/making-stello-5-0-plan.md](Plans/making-stello-5-0-plan.md) (Phase 0, nothing executed yet).

---

## 1. Project Overview & Origins

**Stello** is an Othello (Reversi) engine and application suite by Claus Pedersen (`cjp-dev`), licensed under **GPL-3.0-only** ([LICENSE](LICENSE), [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)), with the goal of reaching world-class playing strength across both Windows desktop (WPF) and browser (Blazor WebAssembly, live at <https://othello.clauspedersen.net>).

The codebase builds on three key pillars:
1. **Original Stello C++ (1992–1998):** Located in [Stello C++/](Stello%20C++/) (Visual C++ 6 / MFC app with engine sources in `BRAIN/` and legacy book `OPENING`). Supplied the original midgame search frame, response table, time control, and opening book format.
2. **Two world-class reference engines** (kept locally in git-ignored root folders):
   - **[Egaroucid-main/](Egaroucid-main/)** (Takuto Yamana, GPL-3.0-or-later): Inspiration for Stello's pattern evaluation, endgame solver, Multi-ProbCut, and training data. An exact single-thread C# port lives in [Stello.Net/Egaroucid.Engine/](Stello.Net/Egaroucid.Engine/) with a C++ fixture generator in [Stello.Net/tools/Egaroucid.Reference/](Stello.Net/tools/Egaroucid.Reference/).
   - **[edax-reversi-master/](edax-reversi-master/)** (Richard Delorme, GPL-3.0): Inspiration for bitboard move generation and endgame search ideas, and source of the FFO #40–59 endgame benchmark positions (`problem/fforum-40-59.obf`).
3. **Detailed project documentation & plans:** How the engine works is documented in 30 chapters under [docs/](docs/README.md) (with [docs/01-overview.md](docs/01-overview.md) as the architectural entry point), and every improvement cycle has paired plan/findings documents in [Plans/](Plans/).

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
| **Stello 4.0** | Nov 2026 | **Current production core** in C++20 ([Stello.Cpp/](Stello.Cpp/)) | 1-to-1 port of Rust 3.1 to C++20 (`stello_version() = 40000`, ABI v3); 100% 1-thread node parity with 3.1, zero external deps, faster FFI (`+4.3–5.3%` 1t, `+3.0–3.8%` 8t) & Emscripten WASM (`+6.8–17.3%` 1t, `-20–25 KB` brotli) |
| **Stello 5.0** | Upcoming | Planned in [Plans/making-stello-5-0-plan.md](Plans/making-stello-5-0-plan.md) | Speed (AVX2/BMI2 dispatch, incremental pattern eval, WASM `simd128`), thread tuning, search, endgame, `egaroucid16`×60 eval, book recalc/DOE |

---

## 3. Repository Structure & Architecture

### Top-Level Folders

| Path | Tracked in Git? | Role |
|---|---|---|
| [Stello.Cpp/](Stello.Cpp/) | Yes (`build/`, `out/` ignored) | **Active C++20 search engine** (`stello_core`, `stello_engine` FFI DLL, `stello_wasm` ST/MT, `stello-cpp` CLI, CTest suite) |
| [Stello.Net/](Stello.Net/) | Yes (`bin/`, `obj/`, generated `wwwroot/` assets ignored) | **.NET 10 solution** ([Stello.Net.slnx](Stello.Net/Stello.Net.slnx)): apps, C# engine wrapper, book/game/eval code, frozen baselines, tools, xUnit tests |
| [Stello.Rust/](Stello.Rust/) | Yes (`target/`, `pkg/` ignored) | **Frozen Stello 3.1 Rust workspace** (`stello-core`, `stello-ffi`, `stello-wasm`, `stello-cli`); kept as `engine=stello-v3` reference |
| [Stello C++/](Stello%20C++/) | Yes | Original 1992–1998 MFC C++ program and legacy `OPENING` book |
| [docs/](docs/README.md) | Yes | 30-chapter technical & historical documentation + `docs/stello-1/` archive |
| [Plans/](Plans/) | Yes (`*.games.jsonl` ignored) | Design plans, findings logs, and `benchmark-results/` JSON/MD reports |
| [external/GameShell/](external/GameShell/) | Submodule | Shared UI shell for `Stello.Web` (clone with `--recurse-submodules`) |
| [Training data/](Training%20data/) | **No (gitignored)** | Raw Egaroucid training datasets (~8.1 GB), converted `stello-bin/` (~8.49 GB), and MPC CSV fits |
| [Egaroucid-main/](Egaroucid-main/) | **No (gitignored)** | Local Egaroucid source & `bin/resources/` (used by `Egaroucid.Engine.Tests` & `Egaroucid.Reference`) |
| [edax-reversi-master/](edax-reversi-master/) | **No (gitignored)** | Local Edax source & FFO problem files |

### Architecture: One Engine, Two Languages

`Stello.Engine` (C#) owns the board/rules/game record (`Board`, `Square`, `Move`, `Game`), opening book (`OpeningBook`, `BookTracker`, `BookLearner`, `BookRecalculator`), and `ComputerPlayer`. Search calls cross a narrow **C ABI v3** (`StelloNative.cs` → `stello_engine.dll` on Windows/POSIX) or run in browser **Web Workers** (`wwwroot/engine` single-threaded and `wwwroot/engine-mt` Emscripten `-pthread` with shared memory).

**`Stello.Cpp/src/` modules** (1-to-1 with `Stello.Rust/stello-core/src`):
- `core/` (`board`, `square`, `bits`, `cpu`, `version`): 16-byte bitboard `Board`, shift-and-mask moves/flips, runtime CPUID BMI2 detection.
- `eval/` (`gzip`, `patterns`, `patterns_bmi2`, `symmetry`, `weights`): Zero-dependency RFC 1951/1952 DEFLATE/gzip decompressor, `plus15` patterns (15 shapes, 62 places, 10 stages, 233,676 folded weights/stage), BMI2 `pext` isolated in `patterns_bmi2.cpp`.
- `search/` (`engine`, `pool`, `ybwc`, `ordering`, `table`, `time`, `limits`, `options`): Iterative deepening PVS, Multi-ProbCut, response table + eval move ordering, 64-byte seqlocked 2-entry TT (midgame + endgame), scoped worker pool, Lazy SMP + YBWC, `STELLO_ABORT_EXCEPTIONS=ON` (`throw Abort`).
- `endgame/` (`search`, `last`, `bits`, `stable`, `local`): WLD then exact solver at ≤7 depth-to-end plies, `last4`–`last1`, parity & edge stability, per-worker `LocalTable`.
- `ffi/` (`stello_ffi`) & `wasm/` (`protocol`, `threads`, `wasm_exports`, `stello_wasm.js`) & `cli/` (`stello-cpp`).

**`.NET 10` projects in [Stello.Net/Stello.Net.slnx](Stello.Net/Stello.Net.slnx) (13 projects):**
- **Apps & shared UI:** `Stello.App` (MVVM view models, settings, `IEngineHost`), `Stello.Net` (WPF Windows app, settings in `%AppData%\Stello`), `Stello.Web` (Blazor WASM app).
- **Engine & tests:** `Stello.Engine`, `Stello.Engine.Tests` (497 tests), `Stello.Net.Tests` (116 tests).
- **Frozen/reference engines (tools & tests only):** `Stello.Engine.V1`, `Stello.Engine.V2` (with its own embedded eval), `Egaroucid.Engine`, `Egaroucid.Engine.Tests` (97 tests + 8 optional skipped).
- **CLI tools:** `tools/Stello.BookTool`, `tools/Stello.Benchmark`, `tools/Stello.Trainer` (plus non-solution C++ harness `tools/Egaroucid.Reference`).

**Key data assets in [Stello.Net/Book/](Stello.Net/Book/):**
- `stello-eval.bin` (990,568 bytes compressed → 4,673,605 bytes inflated; embedded at C++ build time by [embed_eval.cmake](Stello.Cpp/cmake/embed_eval.cmake)).
- `opening-book.txt` & `opening-book.bin` (11,200 positions, 22,878 moves, 193,210 bytes, `OpeningBook.EngineVersion = 4`, leaves searched 60 s). Copied into WPF `Data/OPENING` and Web `wwwroot/data/OPENING.bin`.
- `opening-book-stello1.txt` (frozen book for `stello-v1` matches).

---

## 4. Training Data & Evaluation (`Traning data/` & `Stello.Trainer`)

The local git-ignored folder **[Training data/](Training%20data/)** (~16.59 GB total) holds the datasets downloaded from <https://www.egaroucid.nyanyan.dev/en/technology/train-data/> (redistribution forbidden by Egaroucid's terms) and Stello's pre-converted binary training files:

| Path | Contents | Details |
|---|---|---|
| `Egaroucid_Train_Data/0001_egaroucid_7_5_1_lv17/` | 26 files (`*.txt`) | 25.5M positions with final scores (Egaroucid 7.5.1 self-play at level 17) |
| `Egaroucid_Train_Data_v0002_0/` & `..._v0002_1/` | Folders `8`–`33` & `34`–`59` (100 files each, 5,200 files total) | 52M full game transcripts (`f5d6` format) from Egaroucid 7.8.0 vs Edax 4.5.5 at level 11, after 8–59 random opening moves |
| `stello-bin/` | 174 `.bin` files + `manifest.txt` (~8.49 GB) | Converted by `Stello.Trainer convert` (2026-10-31): **499.5M positions** (479.6M train, 10.0M validation, 10.0M test), capped at 10M/disc-count, split by game hash (2% val / 2% test). **17-byte records:** `own` (`u64`) + `opp` (`u64`) + `score` (`sbyte`) |
| `mpc-values.csv` (798 KB) & `mpc-end-values.csv` (107 KB) | CSV datasets | Deep-minus-shallow search error data generated by `Stello.Trainer mpc` / `mpc-end` for fitting Multi-ProbCut |

`Stello.Trainer` ([TrainerProgram.cs](Stello.Net/tools/Stello.Trainer/TrainerProgram.cs)) runs multi-threaded full-batch Adam in C# (`convert`, `train`, `report`, `size`, `mpc`, `mpc-end`) to produce `stello-eval.bin` and MPC models.

---

## 5. .NET Tools (`Stello.Net/tools/`)

- **[Stello.Benchmark](Stello.Net/tools/Stello.Benchmark/Program.cs):**
  - `match`: Two-game-per-start matches (`--a`, `--b`, `--starts`, `--max-starts`, `--workers`, `--name`, `--resume`). Supports `engine=stello` (C++ 4.0), `stello-v3` (Rust 3.1 DLL), `stello-v2`, `stello-v1`, `stello-lib` (`lib=...`, `sha256=...`), `egaroucid`, plus `opt=<key>=<value>` A/B switches, `threads=`, `hash-bits=`, `book=`, `eval=`. Writes `.games.jsonl` and reports into `Plans/benchmark-results/`.
  - `solve-bench`: FFO endgame benchmark (`--from`/`--to`, e.g., `#40–49` or `#50–59`), `--compare <ref.json>`.
  - `mid-bench`: Fixed-depth midgame benchmark (`--depth`), `--compare <ref.json>` for strict node/move/score parity checks.
  - `eval-compare`, `mid-quality`, `make-starts`, `make-positions`, `make-exact`, `parity-data`.
- **[Stello.BookTool](Stello.Net/tools/Stello.BookTool/Program.cs):**
  - `import`, `format`, `build`, `verify`, `stats`, `recalc` (re-search book leaves), `compare`, `match`.
- **[Stello.Trainer](Stello.Net/tools/Stello.Trainer/TrainerProgram.cs):**
  - `convert`, `train`, `report`, `size`, `mpc`, `mpc-end`.

---

## 6. Environment, Toolchain & Stello 4.0 Baselines

- **Primary machine:** Windows x64, **AMD Ryzen 7 7800X3D** (8 physical cores / 16 logical threads, Zen 4, AVX2 + BMI2 + fast 1-cycle `pext`).
- **Toolchain:** `.NET 10 SDK` (`10.0.401`) + `wasm-tools`, `CMake 3.31` + `Ninja 1.12`, `Clang/LLVM 23.1.5` (default for `.NET` FFI via `build_ffi.cmd`), `MSVC 19.44` (VS 2022 BuildTools), `Emscripten SDK` (`emsdk` 7.0.0 in CI via `build_wasm.cmd`/`build_wasm.sh`), `Node.js v24.19.0`.
- **Stello 4.0 verified test status (Gate P7, 0 warnings everywhere):**
  - C++ (`ctest` across 5 presets + exceptions `OFF`): **43/43** passed, `stello-cpp 4.0.0 (40000)`.
  - `.NET` (`-c Release`): `Stello.Engine.Tests` **497/497**, `Egaroucid.Engine.Tests` **97 passed / 8 optional skipped**, `Stello.Net.Tests` **116/116**.
  - WASM (`node --test`): **15/15** passed.
- **Stello 4.0 key parity & speed numbers (Ryzen 7 7800X3D, `--hash-bits 19` = 64 MB):**
  - `perft(8)` = `390,216`; `midgame.txt` d10 = `3,716,389` nodes, d12 = `10,579,515` nodes; FFO #40–44 = `93,697,164` nodes; FFO #40–49 = `1,115,666,701` nodes (all 1t runs node-identical to Rust 3.1).
  - `.NET` FFI 1t `mid-bench --depth 14`: **2.79 s** (`10.70M nps`, +4.3% vs Rust 3.1).
  - `.NET` FFI FFO #40–49: 1t **22.70 s** (`49.15M nps`, +5.3% vs Rust 3.1); 8t **6.14 s** (`260.1M nps`, +3.0% wall / +3.8% nps vs Rust 3.1); 16t **5.31 s** (`stello-cpp` `clang-release`).
  - 1,180-game 500 ms match (`cpp-4.0` vs `rust-3.1`): **591–589** (`-1` Elo, statistically equal strength; C++ +7.6% higher average `16.58M vs 15.41M nps`).
  - WASM sizes: ST `1,174,429 B` (`1,048,704 B` br), MT `1,203,076 B` (`1,058,234 B` br); +6.8–17.3% (1t) and +3.6–15.8% (4–8t) faster than Rust 3.1 WASM.

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

# WASM Node tests (currently in Stello.Rust/stello-wasm/tests/; Phase 0 of 5.0 moves them to Stello.Cpp/tests/wasm/)
node --test Stello.Rust/stello-wasm/tests/engine.test.mjs Stello.Rust/stello-wasm/tests/engine-mt.test.mjs

# Benchmarks & matches (run from repo root or built bin dir)
dotnet run --project Stello.Net/tools/Stello.Benchmark -c Release -- mid-bench --depth 14 --passes 3
dotnet run --project Stello.Net/tools/Stello.Benchmark -c Release -- solve-bench --from 40 --to 49 --threads 8
dotnet run --project Stello.Net/tools/Stello.Benchmark -c Release -- match --a "time-ms=500" --b "engine=stello-v3,time-ms=500" --name test --resume

# Book tool & Trainer
dotnet run --project Stello.Net/tools/Stello.BookTool -c Release -- verify Stello.Net/Book/opening-book.txt Stello.Net/Book/opening-book.bin
dotnet run --project Stello.Net/tools/Stello.Trainer -c Release -- report --model Stello.Net/Book/stello-eval.bin
```

---

## 8. Documentation ([docs/](docs/README.md)) & Plans ([Plans/](Plans/)) Index

- **`docs/` (30 chapters + `stello-1/` archive):**
  - **Part 1 (`01`–`15`):** Current Stello 4.0 architecture — Overview (`01`), Board/Squares (`02`), Rules/Move Gen (`03`), Pattern Evaluation (`04`), Move Ordering (`05`), Midgame Search & Threads (`06`), Seqlock TT (`07`), Endgame Solver (`08`), Time Control (`09`), Opening Book (`10`–`12`), Game Record (`13`), App Integration & WASM (`14`), GUI & Settings (`15`).
  - **Part 2 (`16`–`24`):** Stello 2.0 history — plan, benchmark methodology, Egaroucid port, eval training, solver, search experiments, final V2 results.
  - **Part 3 (`25`–`27`):** Stello 3.0 & 3.1 — Rust rewrite, multi-threading (Lazy SMP + YBWC), fixes & desktop alignment.
  - **Part 4 (`28`):** Stello 4.0 — C++20 port, controlled Rust vs Clang vs MSVC benchmarks, FFI & Emscripten WASM.
  - **Reference (`29`–`30`):** Glossary & References.
- **`Plans/`:**
  - *Current plan:* [making-stello-5-0-plan.md](Plans/making-stello-5-0-plan.md).
  - *Completed engine cycles (plan + findings):* `Stello porting documentation.md` (V1), `egaroucid-port-*` + `egaroucid-comparison-report.md`, `improve-stello-strength-*` (V2), `making-stello-3-*` (3.0), `making-stello-3-1-*` (3.1), `Stello 4.0 C++ implementation.md` & `Stello 4.0 C++ findings.md` (4.0), `cpp-vs-csharp-speed.md`.
  - *UI, web, book & audits:* `Stello web architecture plan.md`, `Web architecture review.md`, `general-game-gui-specification.md`, `Opening book improvment.md`, `stello-2-docs-plan.md`, `stello-exhaustive-docs-and-code-audit-*`.
  - *Artifacts:* `Plans/benchmark-results/` (JSON baselines `v3.0-ref/`, match reports) and `Plans/Stello 3.1 development snapshot status/`.

---

## 9. Working Rules & Engineering Gates

1. **Matches & long runs:** Start only on the user's explicit go, from a numbered list with time estimates. Check CPU idle state first, run nothing heavy concurrently, and preserve `.games.jsonl` (`--resume`).
2. **A/B switches (`SearchOptions` / `opt=`):** Every engine change in Phases 2–5 sits behind a temporary `SearchOptions` switch. Once its match gate passes or fails, **remove the switch immediately** (keep only the winning code).
   - Strength claims require **lower bound > 50%** in a 1,180-game match at 500 ms (`--workers 8` for 1t, `--workers 1` for 8t), confirmed at 2 s (600 games, `--max-starts 300`).
   - Pure correctness fixes require "not worse" (upper bound of 95% CI > 50%).
   - Speed-only changes (Phase 1) require 100% identical moves, scores, depths, and nodes vs `v4.0-ref`.
   - Kept node-changing optimizations require regenerating `stello-v3-signatures.txt` (or 5.0 signature file) so strict node-for-node tests remain enforced.
3. **Zero-warning gate:** All 5 C++ presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`), `.NET` tests, and WASM Node tests must pass with **0 compiler/MSBuild warnings**.
4. **Git & untracked assets:** Commit with identity `cjp-dev` + `Co-Authored-By` trailer; run `git fsck` after commits; fast-forward `main` only at release with user OK. **Never commit** `Traning data/`, Egaroucid resources, `build/`, `out/`, `target/`, `wwwroot/engine*`, or `*.games.jsonl`.

---

## 10. Current State & Next Actions (Stello 5.0)

- **Git state today:** `HEAD` is on `main` at commit `242b6eb` (*"Webassembly stack fix and Endgame progress fix"*, 2026-11-10), one commit ahead of tag `v4.0` (`788def0`). `242b6eb` added [Plans/making-stello-5-0-plan.md](Plans/making-stello-5-0-plan.md) and two post-4.0 fixes; **`feature/stello-5.0` does not exist yet and zero Stello 5.0 tasks have been started.**
- **Roadmap in [Plans/making-stello-5-0-plan.md](Plans/making-stello-5-0-plan.md):**
  - **Phase 0 (Setup & baseline):** Create `feature/stello-5.0` & `Plans/making-stello-5-0-findings.md`; move parity fixtures (`positions.txt`, `patterns.txt`, `perft.txt`) and WASM Node tests out of frozen `Stello.Rust/` into `Stello.Cpp/tests/` so `Stello.Cpp` is self-contained; preserve `Stello.Cpp/ref/stello_engine_v4.dll` (`engine=stello-v4`); record `Plans/benchmark-results/v4.0-ref/`.
  - **Phase 1 (Speed, 0 node changes):** Runtime `x86-64-v3` (`AVX2 + BMI2 + POPCNT + LZCNT`, `-mno-avx512f`) dispatch with explicit 256-bit AVX2 `flips`/`legal_moves`; incremental pattern feature indices across make/unmake + SIMD weight sum; Emscripten `-msimd128` + WASM abort-mode re-check.
  - **Phase 2 (Threads):** Root splitting in `search_root`/`solve_root`, immediate brother cut interruption, split wait progress heartbeats, 7800X3D tuning of Lazy SMP helper cap vs YBWC workers at 8t/16t.
  - **Phase 3 (Search):** Refit Multi-ProbCut (per depth pair / game phase), 2nd TT move alone, soft time % tuning, aspiration windows, `hash-bits` 19 vs 21 at 2 s 8t, clog search.
  - **Phase 4 (Endgame):** `end_local` move-ordering table for 11–13 empties; time-estimated solver handover + `mpc-end` at longer time controls.
  - **Phase 5 (Evaluation):** Train `egaroucid16` × 60 stages (enabled by Phase 1 incremental indices!) + test extra mobility/parity features; evaluate desktop/web size trade-off.
  - **Phase 6 (Opening book):** Bump `OpeningBook.EngineVersion` to 5, recalculate leaves (including 2,988 timed `X:60s` entries), `calclib` missing moves, Drop-Out Expansion.
  - **Phase 7 (Release):** Final matches (vs `stello-v4`, `stello-v3`, `stello-v2`, `egaroucid`), version bump to `5.0.0` (`50000`), write `docs/29-stello-5-0-speed-threads-search-and-book.md` & update Part 1 chapters, fast-forward `main` & verify Azure Static Web Apps deploy.
