# Content and State of the Stello Project (as of 2026-11-28)

> **Purpose:** Onboarding and context-kickstart document for AI chat sessions working on **Stello**.
> **Snapshot date:** 2026-11-28 · **Active branch:** `feature/stello-6.0` at `52de969` (branched from `main` at `caffe06`, after `v5.0` release at `473d0dc`) · **Completed in 6.0 cycle:** Phases 0, 1, 2 (+ addendum), 3, and 4 (`3cdcc91`, `9f8c214`, `b0aeaf7`, `5e9dc80`, `b2b0452`/`88c59a0`, `2220202`..`52de969`; [Plans/making-stello-6-0-findings.md](Plans/making-stello-6-0-findings.md)) · **Next milestone:** **Phase 5 (Endgame)** of [Plans/making-stello-6-0-plan.md](Plans/making-stello-6-0-plan.md) (plus optional `TimePerGame` policy/ablation matches from [Plans/Phase 4 item 5 review and fixes.md](Plans/Phase%204%20item%205%20review%20and%20fixes.md)).

---

## 1. Project Overview & Origins

**Stello** is an Othello (Reversi) engine and application suite by Claus Pedersen (`cjp-dev`), licensed under **GPL-3.0-only** ([LICENSE](LICENSE), [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)), with the goal of reaching world-class playing strength across both Windows desktop (WPF) and browser (Blazor WebAssembly, live at <https://othello.clauspedersen.net>).

The codebase builds on three key pillars:
1. **Original Stello C++ (1992–1998):** Located in [Stello C++/](Stello%20C++/) (Visual C++ 6 / MFC app with engine sources in `BRAIN/` and legacy book `OPENING`). Supplied the original midgame search frame, response table, time control, and opening book format.
2. **Three world-class reference engines** (kept locally in git-ignored root folders):
   - **[Egaroucid-main/](Egaroucid-main/)** (Takuto Yamana, GPL-3.0-or-later): Inspiration for Stello's pattern evaluation, endgame solver, Multi-ProbCut, and training data. An exact single-thread C# port lives in [Stello.Net/Egaroucid.Engine/](Stello.Net/Egaroucid.Engine/), alongside C++ fixture/match harnesses in [Stello.Net/tools/Egaroucid.Reference/](Stello.Net/tools/Egaroucid.Reference/) and [Stello.Net/tools/Egaroucid.Runner/](Stello.Net/tools/Egaroucid.Runner/) (`engine=egaroucid-cpp`).
   - **[edax-reversi-master/](edax-reversi-master/)** (Richard Delorme, GPL-3.0): Upstream C Edax reference (`engine=edax` via `-xboard`) and source of the FFO #40–59 endgame benchmark positions (`problem/fforum-40-59.obf`).
   - **[edax-reversi-AVX-master/](edax-reversi-AVX-master/)** (Toshihiko Okuhara's AVX2 / AVX-512VL / BMI2 Edax v4.5.7 fork, GPL-3.0): Direct native reference (`engine=edax-avx`) and primary inspiration for Phase 2 bitboard, ternary eval, 64 KB stability, `SIMULLASTFLIP`, and zero-ZMM 256-bit EVEX (`v4`) optimizations.
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
| **Stello 5.0** | Nov 2026 | Released (`v5.0` on `main`), **frozen/preserved** as `stello-v5` (`Stello.Cpp/ref/stello_engine_v5.dll`) | `x86-64-v3` AVX2/BMI2 dispatch + incremental `PatternState` + WASM `-msimd128`; YBWC root splits + brother cut + `(w+2)/4` Lazy SMP; 3-stage selective endgame `mpc-end`; Format v3 `egaroucid16 + mobility × 60` evaluation (`4.97` MAE / `6.74` RMSE); `6,000`-position `v5` book; **+58 to +84 Elo** vs 4.0, **+115 to +145 Elo** vs 2.0 |
| **Stello 6.0** | Nov 2026 | **Active C++20 core on `feature/stello-6.0`** (`stello_version() = 50000`, ABI v3 until Phase 8 bump) | **Phases 0–4 complete (`52de969`):** `edax`/`edax-avx`/`egaroucid-cpp` harnesses & `starts-v6.txt`; 19 bugs + 14 defensive hardenings; zero-node speed pass + addendum (`5e9dc80`, `v4` 256-bit EVEX `zmm = 0`, `VPADDW`, `7→6` reply mask, Clang PGO, WASM); Phase 3 dynamic `SplitNode`/`WaveSplitNode` + Helpful Master + diversified Lazy SMP (**+22** Elo @ `500 ms` `8t`); Phase 4 `hash_move`-first + leaf wipeout check, unified `32 MiB` lockless 4-way dual-bound Hyatt-XOR TT, `eval1_nws`/`eval2_nws` + `look == 2` static ordering (**+25/+28** Elo), ETC parent TT store + recursive midgame ProbCut `PROBCUT_MAX_LEVEL = 2` (`−82.75%` `deep24 d22 1t`, **+70/+43** Elo), and `TimePerGame` dynamic extension + heuristic early exit (**+19** Elo @ `15 s` / `30 s`) + handover/clock-overrun review fixes |

---

## 3. Repository Structure & Architecture

### Top-Level Folders

| Path | Tracked in Git? | Role |
|---|---|---|
| [Stello.Cpp/](Stello.Cpp/) | Yes (`build/`, `out/`, `*.prof*` ignored) | **Active C++20 search engine** (`stello_core`, `stello_engine` FFI DLL, `stello_wasm` ST/MT, `stello-cpp` CLI w/ `microbench`, `cmake/build_pgo.cmd`, CTest & WASM Node test suite, `ref/stello_engine_v4.dll`, `ref/stello_engine_v5.dll`) |
| [Stello.Net/](Stello.Net/) | Yes (`bin/`, `obj/`, generated `wwwroot/engine*` ignored) | **.NET 10 solution** ([Stello.Net.slnx](Stello.Net/Stello.Net.slnx)): apps, C# engine wrapper, book/game/eval code, frozen baselines, tools, xUnit tests |
| [Stello.Rust/](Stello.Rust/) | Yes (`target/`, `pkg/` ignored) | **Frozen Stello 3.1 Rust workspace** (`stello-core`, `stello-ffi`, `stello-wasm`, `stello-cli`); kept only as `engine=stello-v3` reference |
| [Stello C++/](Stello%20C++/) | Yes | Original 1992–1998 MFC C++ program and legacy `OPENING` book |
| [docs/](docs/README.md) | Yes | 31-chapter technical & historical documentation + `docs/stello-1/` archive |
| [Plans/](Plans/) | Yes (`*.games.jsonl` ignored) | Design plans, findings logs ([making-stello-5-0-findings.md](Plans/making-stello-5-0-findings.md), [making-stello-6-0-plan.md](Plans/making-stello-6-0-plan.md), [making-stello-6-0-findings.md](Plans/making-stello-6-0-findings.md)), and `benchmark-results/` JSON/MD reports (`v5.0-ref/`, `v4.0-ref/`, `v3.0-ref/`) |
| [external/GameShell/](external/GameShell/) | Submodule | Shared UI shell for `Stello.Web` (clone with `--recurse-submodules`) |
| [Training data/](Training%20data/) | **No (gitignored)** | Raw Egaroucid training datasets (~8.1 GB), converted `stello-bin/` (~8.49 GB), and MPC CSV fits |
| [Egaroucid-main/](Egaroucid-main/) | **No (gitignored)** | Local Egaroucid source & `bin/resources/` (used by `Egaroucid.Engine.Tests`, `Egaroucid.Reference`, & `Egaroucid.Runner`) |
| [edax-reversi-master/](edax-reversi-master/) | **No (gitignored)** | Local C Edax source (`wEdax-native.exe`) & FFO problem files |
| [edax-reversi-AVX-master/](edax-reversi-AVX-master/) | **No (gitignored)** | Okuhara's AVX Edax v4.5.7 fork (`wEdax-avx-native.exe`) |

### Architecture: One Engine, Two Languages

`Stello.Engine` (C#) owns the board/rules/game record (`Board`, `Square`, `Move`, `Game`), opening book (`OpeningBook`, `BookTracker`, `BookLearner`, `BookRecalculator`, `BookTrimmer`, `BookCalclib`, `BookExpander`), and `ComputerPlayer`. Search calls cross a narrow **C ABI v3** (`StelloNative.cs` → `stello_engine.dll` on Windows/POSIX) or run in browser **Web Workers** (`wwwroot/engine` single-threaded and `wwwroot/engine-mt` Emscripten `-pthread -msimd128` with shared memory).

**`Stello.Cpp/src/` modules:**
- `core/` (`board`, `square`, `bits`, `cpu`, `version`, `compiler`): 16-byte bitboard `Board`, parallel prefix `bits.hpp`, runtime CPUID dispatch (`CpuTier { Portable, V3, V4 }` via `has_sse42()`, `has_avx2_bmi2()`, `has_avx512_v4()`, `has_gfni()`, overridable via `--cpu auto|portable|v3|v4`), `STELLO_VECTORCALL`, and `STELLO_TU_HAS_AVX512_V4`.
- `eval/` (`gzip`, `patterns`, `patterns_bmi2`, `symmetry`, `weights`): Zero-dependency RFC 1951/1952 DEFLATE/gzip decompressor; `PatternSet` (`classic11`, `plus15`, `egaroucid16`) using a single ternary `alignas(32) uint16_t features[64]` `PatternState` (128 B = `4 × ymm`) with baked sub-table offsets, sparse `all_1_state_`/`all_2_state_` init, even/odd-interleaved `int32_t` packed pair weights (`sum_gather_avx2_vpaddw` `VPADDW` + `VPGATHERDD` on x64 AVX2/V4 guarded by `packed_vpaddw_safe()`, `sum_flat_scalar<OddSlot>` + `SiblingBaseline` on native `Portable`, `advance_lanes_wasm` + `sum_local_wasm<OddSlot>` on WASM), BMI2 `PEXT` `sum_pext_pair`, `eval::raw_skip_threshold` unscaled leaf comparisons, and Format v2 / v3 (`EvalWeights` with 96 mobility weights per stage).
- `search/` (`engine`, `engine_impl.hpp`, `eval_avx2_detail.hpp`, `kernel.hpp`, `kernel_impl.hpp`, `search_portable.cpp`, `search_v3.cpp`, `search_v4.cpp`, `microbench.hpp`, `microbench_impl.hpp`, `pool`, `ybwc`, `ybwc_impl.hpp`, `ordering`, `table.hpp`, `table.cpp`, `time`, `limits`, `options`):
  - Tri-tier `Kernel<cpu::CpuTier>` (`Portable`, `V3` AVX2+BMI2+`-mno-avx512f`, `V4` 256-bit EVEX AVX-512VL/CD/DQ/BW + GFNI with `-mprefer-vector-width=256` / `/arch:AVX512 /vlen=256` and `zmm = 0`), dispatched once via `STELLO_TIER_DISPATCH`.
  - Iterative deepening PVS, recursive Multi-ProbCut (`ProbCutErrors`, `PROBCUT_MAX_LEVEL = 2`), `hash_move`-first search before sibling ordering in `search_inc_deep` + `1/2`-move bypass, leaf wipeout detection (`WIN_SCORE + 64`), `eval1_nws`/`eval2_nws` + `look == 2` static 5-mask priority ordering, ETC parent TT lower-bound store (`cost = 0`), branchless child rank-sort (`ordering.cpp`), flip caching + `known_legal` pass-through, unified `32.0 MiB` (`hb=19`, `2,097,152`-entry) lockless 4-way 64 B Hyatt-XOR dual-bound TT (`std::atomic_ref<uint64_t>`) with AVX2 non-temporal clear, dynamic stack `SplitNode`/`WaveSplitNode` + Helpful Master + diversified Lazy SMP (`SPLIT_MAX_SLAVES = 4`, `YBWC_MID_MIN_DEPTH = 5`), 3-stage full-window `[-64, +64]` selective endgame (`mpc-end`) before `for_solver_capped` WLD/Exact admission, and `TpgMoveState` `TimePerGame` instability/drop extension + once-per-move heuristic early exit (restoring original `s.hard_limit_ms` at selective-endgame handover).
  - Built-in `__rdtsc()` `microbench` suite covering 17 primitives across `portable`, `v3`, and `v4`.
- `endgame/` (`search`, `search_impl.hpp`, `last`, `last_impl.hpp`, `bits`, `bits_avx2.hpp`, `bits_avx512.hpp`, `stable`, `local`): Single-cache-line `LR_MASK[64]`, AVX2 `flips_avx2_v128_smartclr`, `legal_and_potential_avx2`, 256-bit EVEX `flips_avx512cd`, `legal_moves_avx512`, `legal_and_potential_avx512`, GFNI `horizontal_mirror_gfni`, WASM `UPPER_RAYS_128` `flips` + `wasm_i8x16_shuffle` `legal_moves`, compact 64 KB `g_edge_table[65536]` + AVX2/AVX-512VL `stable::of` & `transpose`, 3-lookup BMI2 `simul_last_flip_bmi2` + `noexcept` `last4_fast..last1_fast` with `7→6` empty reply-mask handoff & partial `__m128i` flip probes, pre-ETC child prefetch loop (`search_impl.hpp`), compact 24 B `EndMove`/`LocalMove`, and per-worker `LocalTable`.
- `ffi/` (`stello_ffi`) & `wasm/` (`protocol`, `threads`, `wasm_exports`, `stello_wasm.js`) & `cli/` (`stello-cpp`).

**`.NET 10` projects in [Stello.Net/Stello.Net.slnx](Stello.Net/Stello.Net.slnx) (13 projects):**
- **Apps & shared UI:** `Stello.App` (MVVM view models, serialized `_transitionGate`, clock/dialog hardening), `Stello.Net` (WPF Windows app, settings in `%AppData%\Stello`), `Stello.Web` (Blazor WASM app w/ Worker cancellation & move validation hardening).
- **Engine & tests:** `Stello.Engine`, `Stello.Engine.Tests` (**522** tests), `Stello.Net.Tests` (**127** tests, incl. `MainViewModelTests` & `WebHostTests`).
- **Frozen/reference engines (tools & tests only):** `Stello.Engine.V1`, `Stello.Engine.V2`, `Egaroucid.Engine`, `Egaroucid.Engine.Tests` (**120** passed / **8** optional skipped `TaggedV30_*`).
- **CLI tools:** `tools/Stello.BookTool`, `tools/Stello.Benchmark`, `tools/Stello.Trainer` (plus non-solution C++ harnesses `tools/Egaroucid.Reference` and `tools/Egaroucid.Runner`).

**Key data assets in [Stello.Net/Book/](Stello.Net/Book/):**
- `stello-eval.bin` (`8,892,085` B gzip / `38,434,770` B unpacked; Format v3 `egaroucid16 + mobility` × 60 stages, held-out **MAE `4.97`, RMSE `6.74`**, embedded at C++ build time by [embed_eval.cmake](Stello.Cpp/cmake/embed_eval.cmake)).
- `opening-book.txt` (`370,387` B) & `opening-book.bin` (`114,169` B, `33,809` B brotli; `OpeningBook.EngineVersion = 5`; `6,000` canonical positions, `13,154` moves, `7,047` leaves all at `60s:v5`). Copied into WPF `Data/OPENING` and Web `wwwroot/data/OPENING.bin`.
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

`Stello.Trainer` ([TrainerProgram.cs](Stello.Net/tools/Stello.Trainer/TrainerProgram.cs)) runs multi-threaded full-batch Adam in C# (`convert`, `train`, `report` [filtering 64-disc terminal boards], `size`, `mpc` [capped at `--to 55`], `mpc-end --model`) to produce `stello-eval.bin` and MPC models.

---

## 5. .NET Tools (`Stello.Net/tools/`)

- **[Stello.Benchmark](Stello.Net/tools/Stello.Benchmark/Program.cs):**
  - `match`: Two-game-per-start matches (`--a`, `--b`, `--starts` defaulting to `Data/starts-v6.txt` with `587` prefix-free starts / `1,174` games, `--max-starts`, `--workers`, `--name`, `--resume`). Supports `engine=stello` (active C++), `stello-v5`, `stello-v4`, `stello-v3`, `stello-v2`, `stello-v1`, `stello-lib` (`lib=...`, `sha256=...`), `egaroucid`, `egaroucid-cpp`, `edax`, `edax-avx`, plus `opt=<key>=<value>` A/B switches, `threads=`, `hash-bits=`, `book=`, `eval=`. Tracks `ElapsedA`, `ElapsedB`, and `FirstOverrun` (`TimePerGame` clock overruns) and reports separate paired flag-adjusted scores/Elo/LOS alongside ordinary disc results in `.md`/`.json` reports and `.games.jsonl` (`Plans/benchmark-results/`, git-ignored).
  - `solve-bench`: FFO endgame benchmark (`--from`/`--to`, `--compare <ref.json>`, supports external C/C++ engines).
  - `mid-bench`: Fixed-depth midgame benchmark (`--depth`, e.g., `Data/midgame.txt` `100` roots and `Data/midgame-deep.txt` `24` pure-midgame `48/42/36`-empty roots), `--compare <ref.json>` for strict node/move/score parity checks.
  - `eval-compare`, `mid-quality` (per-side `threads=` and `hash-bits=`), `make-starts` (`--prefix-free`), `make-positions`, `make-exact`, `parity-data`.
- **[Stello.BookTool](Stello.Net/tools/Stello.BookTool/Program.cs):**
  - `import`, `format`, `build`, `verify`, `stats`, `trim`, `recalc`, `calclib`, `expand`, `compare`, `match` (`AddGame` validates full move sequences upfront).
- **[Stello.Trainer](Stello.Net/tools/Stello.Trainer/TrainerProgram.cs):**
  - `convert`, `train`, `report`, `size`, `mpc`, `mpc-end`.

---

## 6. Environment, Toolchain & Stello 5.0 / 6.0 Results Summary

- **Primary machine:** Windows x64, **AMD Ryzen 7 7800X3D** (8 physical cores / 16 logical threads, Zen 4, 96 MB 3D V-Cache L3, AVX2 + BMI2 + fast 1-cycle `pext` + AVX-512VL/CD/DQ/BW/VNNI + GFNI) + **NVIDIA GeForce RTX 4080**.
- **Toolchain:** `.NET 10 SDK` (`10.0.401`) + `wasm-tools`, `CMake 3.31` + `Ninja 1.12`, `Clang/LLVM 23.1.5` (default for `.NET` FFI via `build_ffi.cmd`), `MSVC 19.44` (VS 2022 BuildTools), `Emscripten SDK` (`emsdk` via `build_wasm.cmd`/`build_wasm.sh`), `Node.js v24.19.0`.
- **Current verified test status (`0` warnings everywhere, `zmm = 0` across all native binaries):**
  - C++ (`ctest` across all 5 presets `clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`): **3/3 CTest targets (`71/71` `stello_tests`, `stello_cli_version`, `stello_cli_microbench`)** passed, `105` parity signatures verified across `Portable`, `V3`, and `V4`, 0 ASan/UBSan findings.
  - `.NET` (`-c Release`): `Stello.Engine.Tests` **522/522**, `Stello.Net.Tests` **127/127**, `Egaroucid.Engine.Tests` **120 passed / 8 optional skipped (`TaggedV30_*`)**.
  - WASM (`node --test Stello.Cpp/tests/wasm/engine.test.mjs Stello.Cpp/tests/wasm/engine-mt.test.mjs`): **20/20** passed.

### Stello 5.0 Summary (Shipped on `main` / `v5.0` — [Plans/making-stello-5-0-findings.md](Plans/making-stello-5-0-findings.md))
- Delivered AVX2/BMI2 `v3` dispatch + WASM `-msimd128`, YBWC root splits + Lazy SMP `(w+2)/4`, 3-stage selective endgame `mpc-end` (`+10` / `+22` Elo at 500 ms / 2 s), Format v3 `egaroucid16 + mobility × 60` (`4.97` MAE / `6.74` RMSE), and the `6,000`-position `v5` book (`55.0%` vs 4.0 book). Beat Stello 4.0 by **+58 to +84 Elo**, Stello 2.0 by **+115 to +145 Elo**, and Egaroucid C# port by **+211 / +109 / −1 Elo** (100 / 500 / 2,000 ms).

### Stello 6.0 Progress (`feature/stello-6.0`, Phases 0–4 Complete — [Plans/making-stello-6-0-findings.md](Plans/making-stello-6-0-findings.md))
- **Phase 0 (`3cdcc91` — Harnesses, `microbench`, & `starts-v6.txt` Baselines):**
  - Preserved `stello-v5`, added `engine=edax` / `edax-avx` / `egaroucid-cpp` harnesses (`hash-bits=21` for external engines vs `19` for Stello), added prefix-free `Data/starts-v6.txt` (`587` starts / `1,174` games), added `stello-cpp microbench`, and recorded `Plans/benchmark-results/v5.0-ref/`.
  - **External FFO `#40–49` & `#50–59` baselines (`hash-bits=21` vs Stello 5.0 `19`):** `egaroucid-cpp` solved `#40–49` in **`9.470 s` (`512.6M` nodes)** 1t / `2.380 s` (`729.0M` nodes) 8t and `#50–59` in `325.7 s` (`15.54B` nodes) 1t / `50.33 s` (`15.86B` nodes) 8t; `edax-avx` solved `#40–49` in `11.496 s` (`81.18M` nps) 1t / **`2.034 s` (`485.57M` nps)** 8t and `#50–59` in **`253.0 s` (`93.43M` nps)** 1t / **`38.49 s` (`637.50M` nps)** 8t; Stello 5.0 baseline was `16.664 s` 1t / `3.114 s` 8t (`#40–49`) and `559.6 s` 1t / `105.1 s` 8t (`#50–59`).
  - **All 30 `starts-v6.txt` baseline matches (`35,220` games vs Stello 5.0, 100 / 500 / 2,000 ms):** Stello 5.0 vs `v4` `+65` to `+93` Elo; vs `v2` `+111` to `+168` Elo; vs `v1` `+621` to `+696` Elo; vs `egaroucid-cpp` `+175 / +35 / −55` Elo (1t) and `−14 / −36 / −13` Elo (8t); vs `edax` `−81 / −87 / −89` Elo (1t) and `−89 / −86 / −63` Elo (8t); vs `edax-avx` `−103 / −91 / −93` Elo (1t) and `−94 / −91 / −58` Elo (8t).
- **Phase 1 (`9f8c214` — Bug Fixing):**
  - Fixed all **19 confirmed bugs** (`C1–C6` in C++, `N1–N13` in .NET/Web/CLI) plus **14 defensive hardenings** (`1C.1–1C.14`), with full unit/WASM/CTest regression coverage (`C3` root move preservation also cut losing-handover nodes in `midgame-handover-depth12` from `303.63M` to `297.05M` with identical moves/scores).
- **Phase 2 (`b0aeaf7` & addendum `5e9dc80` — Zero-Node-Change Speed Pass):**
  - **`microbench` (`v4` vs Phase 0 `v3`, cycles):** `flips` **`4.08`** (`−35.7%`), `solve_1` **`6.95`** (`−13.0%`), `legal_moves` **`8.07`** (`−13.0%`), `legal_and_potential` **`9.63`** (`−39.3%`), `stable_of` **`13.69`** (`−56.0%`), `horizontal_mirror` **`0.37`** (`−88.5%` vs `v3`), `eval_sum` **`116.90`** (`−28.0%`), `eval_after_move` **`141.53`** (`−13.2%`), `eval_advance` **`6.61`** (`−76.9%`). Portable `eval_sum` improved from `167.40 → 140.17` cyc (`−16.3%`).
  - **`.NET` FFI vs `v5.0-ref/` (`hash-bits=19`):** `mid-bench` `d10` `0.229 s` (`+3.5%`), `d12` `0.587 s` (`+7.6%`), `d14` `1.646 s` (`+8.2%`), `d16` `4.333 s` (`+5.0%`), `d16-8t` `1.420 s` (`+7.5%`), `d16-16t` `1.363 s` (`+4.6%`), `handover-d12` `3.872 s` (`+17.0%`); `solve-bench` `ffo-40-49` `1t` **`14.1 s` / `76.04M` nps (`+18.4%`, `10/10` identical)**, `8t` **`2.591 s` / `447.56M` nps (`+20.2%`)**, `16t` **`2.172 s` / `607.70M` nps (`+21.0%`)**; `ffo-50-59` `8t` **`90.2 s` (`+16.5%`)**, `16t` **`70.8 s` / `568.78M` nps (`+31.5%`)**.
  - **WASM Node.js:** `ST 1t` `mid-d10` **`329.4 ms` (`+14.2%`)**, `mid-d12` **`871.2 ms` (`+13.9%`)**, `ffo-40-44` **`2913.5 ms` (`+2.8%`)**; `MT 4t` `mid-d10` **`262.6 ms` (`+10.9%`)**, `mid-d12` **`579.5 ms` (`+12.1%`)**, `ffo-40-44` **`1072.2 ms` (`+3.2%`)**.
  - **`100 ms` `1t` verification match vs `stello-v5` (`1,174` games):** `49.1%` (`47.4–50.8%`, `0.0 ± 0.2` discs/g), **`21.10M` vs `19.23M` nps (`+9.7%`)**, avg depth `17.36` vs `17.29`, first solve `20.6` vs `20.4`, max move time `104` vs `125 ms`.
  - **Addendum (`5e9dc80`):** Kept `7→6` reply-mask handoff (`+0.83%` FFO `#40–49`), `sum_gather_avx2_vpaddw` `VPADDW` (`+1.09–1.96%` `v4` midgame), and `Portable`-only sibling-delta evaluation (`+1.40–2.34%`) with exact 1t node parity.
- **Phase 3 (`b2b0452` & review `88c59a0` — Threads):**
  - Fixed all 4 threading bottlenecks (`idle()` split guard, incremental `PatternState` in `MidgameChild`, thread-local node counting, decoupled pool locks), replaced wave allocations with dynamic stack `SplitNode`/`WaveSplitNode` + **Helpful Master** work-stealing, locked `SPLIT_MAX_SLAVES = 4` & `YBWC_MID_MIN_DEPTH = 5` + diversified Lazy SMP, and landed post-review safety fixes (nested stop forwarding, `PendingException`, single 64-bit atomic `live_best`).
  - **Benchmarks (`clang-release`, `v4`, `hb=19`):** `FFO #40–49` `8t` `2.284 s` med (`+12.3%` vs P2, `+36.3%` vs `v5.0`) / `16t` `1.928 s` (`7.23×` vs P3 `1t` `13.94 s`); `FFO #50–59` `8t` `79.29 s` (`+16.5%`) / `16t` `68.83 s` (`+8.1%`); `mid-d16` `8t` `1.221 s` (`+16.0%`); WASM `MT4` `mid-d10` `180.1 ms` (`+48.7%` vs P2), `mid-d12` `424.1 ms` (`+37.6%`), `ffo-40-44` `933.0 ms` (`+13.7%`).
  - **Gate P3 `8t` matches vs Phase 2 (`1,174` games each):** `500 ms` **`53.2%` / `+22` Elo (`+9..+36`, LOS `99.9%`)**; `2,000 ms` `51.1%` / `+8` Elo (`−4..+20`, LOS `90.4%`).
- **Phase 4 (`2220202`..`52de969` — Search):**
  - **Item 1 (`2220202`, `df53e55`):** `hash_move`-first search before `eval_and_sort` + `1/2`-move bypass (`−2.0–4.8%` `1t`, `−2.7–3.0%` nodes) and immediate leaf wipeout recognition (`WIN_SCORE + 64`).
  - **Item 2 (`9cd7a6f`, `bb8bded`):** Merged midgame+endgame TTs into one **`32.0 MiB`** (`hb=19`, `2,097,152`-entry) lockless 4-way dual-bound Hyatt-XOR TT (`deep24 d20/d22 8t` `−6.4% / −1.6%` ms, `mid-quality 200 ms` disc loss `0.54 → 0.47`; `500 ms` `+4` / `2 s` `+1` Elo within CI).
  - **Item 3 (`2961a05`, `0a02d7d`):** Dedicated `eval1_nws`/`eval2_nws` + `look == 2` NWS static 5-mask priority ordering (`−32.9–41.0%` `1t` / `−38.2–40.0%` `8t`, `deep24` EBF `1.639 → 1.584`, Gate P4 **`53.5%` / `+25` Elo [`+10..+39`]** @ `500 ms`, **`54.1%` / `+28` Elo [`+8..+50`]** @ `2 s`); coarser resolution (`3B`) and fail-soft re-search (`3C`) rejected.
  - **Item 4 (`baf118c`, `29e47a6`, `8bc410d`):** Midgame ETC parent TT lower-bound store (`cost = 0`) + recursive midgame ProbCut (`PROBCUT_MAX_LEVEL = 2`, cutting `deep24 d22 1t` by **`−82.75%` ms / `−84.24%` N**, `8t` `−73.68%` ms, `d20→d22` EBF `1.585 → 1.366`, Gate P4 **`59.9%` / `+70` Elo [`+55..+85`]** @ `500 ms`, **`56.2%` / `+43` Elo [`+24..+63`]** @ `2 s`) + post-stage-4 `TimePerGame` WLD/Exact admission & `for_solver_capped`; `+2`-ply ID / aspiration / PV extension (`4C`) rejected.
  - **Item 5 (`79d5f0f` & review fixes `52de969`):** Kept **`5D-TPG`** (`TimePerGame` dynamic instability/score-drop extension + once-per-move one-ply-shallower heuristic NWS early exit): **`618.5/1,174` (`52.7%`, `+19` Elo [`+7..+30`], LOS `99.9%`)** @ `15 s/game` (`7.84` vs `6.83 s/g`) and **`316.5/600` (`52.7%`, `+19` Elo [`+3..+36`], LOS `98.8%`)** @ `30 s/game` (`15.12` vs `13.19 s/g`); rejected `5A`, `5B-1/2/3a/3b/4`, `5C`, `5D-TPM`, and `5E`. Commit `52de969` restored `s.hard_limit_ms = budget.hard_ms` on selective-endgame handover, pinned named `g21e24`/`g21e28`/`g01e48`/`g23e28`/`g24e28` regression tests, and added `TimePerGame` clock-overrun & flag-adjusted tracking in the `.NET` match runner.

---

## 7. Build, Test & CLI Cheat Sheet

```powershell
# C++20 presets (from Stello.Cpp): clang-release | clang-native | msvc-release | msvc-native | clang-sanitized
cmake --preset clang-release
cmake --build --preset clang-release
ctest --preset clang-release --output-on-failure

# Microbench & Clang PGO build
.\Stello.Cpp\build\clang-release\stello-cpp.exe microbench --cpu v4   # auto | portable | v3 | v4
cmd /c Stello.Cpp\cmake\build_pgo.cmd

# .NET full Release build & tests (builds native stello_engine.dll + WASM if emsdk active)
dotnet build Stello.Net/Stello.Net.slnx -c Release
dotnet build Stello.Net/Stello.Net.slnx -c Release -p:StelloWasmEngine=false   # skip Emscripten WASM
dotnet test Stello.Net/Stello.Net.slnx -c Release --no-build

# WASM Node tests (Stello.Cpp/tests/wasm/)
node --test Stello.Cpp/tests/wasm/engine.test.mjs Stello.Cpp/tests/wasm/engine-mt.test.mjs

# Benchmarks & matches (default --starts is Data/starts-v6.txt)
dotnet run --project Stello.Net/tools/Stello.Benchmark -c Release -- mid-bench --depth 14 --passes 3
dotnet run --project Stello.Net/tools/Stello.Benchmark -c Release -- solve-bench --from 40 --to 49 --threads 8
dotnet run --project Stello.Net/tools/Stello.Benchmark -c Release -- match --a "engine=stello-v5,time-ms=500" --b "time-ms=500" --name test --resume

# Book tool & Trainer
dotnet run --project Stello.Net/tools/Stello.BookTool -c Release -- verify Stello.Net/Book/opening-book.txt Stello.Net/Book/opening-book.bin
dotnet run --project Stello.Net/tools/Stello.Trainer -c Release -- report --model Stello.Net/Book/stello-eval.bin
```

---

## 8. Documentation ([docs/](docs/README.md)) & Plans ([Plans/](Plans/)) Index

- **`docs/` (31 chapters + `stello-1/` archive):**
  - **Part 1 (`01`–`15`):** Architecture (5.0 baseline, with `05`, `06`, `07`, `09`, `14`, and `15` updated through Phase 4; remaining chapters & `30` scheduled for final Stello 6.0 refresh in Phase 8) — Overview (`01`), Board/Squares (`02`), Rules/Move Gen (`03`), Pattern + Mobility Evaluation (`04`), Move Ordering (`05`), Midgame Search & Threads (`06`), Dual-Bound Unified TT (`07`), Endgame Solver (`08`), Time Control (`09`), Opening Book (`10`–`12`), Game Record (`13`), App Integration & WASM (`14`), GUI & Settings (`15`).
  - **Part 2 (`16`–`24`):** Stello 2.0 history.
  - **Part 3 (`25`–`27`):** Stello 3.0 & 3.1 (Rust & desktop app).
  - **Part 4 (`28`):** Stello 4.0 (C++20 port).
  - **Part 5 (`29`):** Stello 5.0 ([29-stello-5-0-speed-threads-search-and-book.md](docs/29-stello-5-0-speed-threads-search-and-book.md)).
  - **Reference (`30`–`31`):** [30-glossary.md](docs/30-glossary.md) & [31-references.md](docs/31-references.md).
- **`Plans/`:**
  - *Active cycle (Stello 6.0):* [making-stello-6-0-plan.md](Plans/making-stello-6-0-plan.md), [making-stello-6-0-findings.md](Plans/making-stello-6-0-findings.md), and Phase 4 review handoffs (`Phase 4 items 1 and 2 review and fixes.md`, `Phase 4 item 3 review and fixes.md`, `Phase 4 item 4 *review and fixes.md`, [Phase 4 item 5 review and fixes.md](Plans/Phase%204%20item%205%20review%20and%20fixes.md)).
  - *Stello 5.0 & earlier cycles:* [making-stello-5-0-plan.md](Plans/making-stello-5-0-plan.md), [making-stello-5-0-findings.md](Plans/making-stello-5-0-findings.md), `Stello porting documentation.md` (V1), `egaroucid-port-*`, `improve-stello-strength-*` (V2), `making-stello-3-*` (3.0/3.1), `Stello 4.0 C++ *` (4.0).
  - *Artifacts (`Plans/benchmark-results/`):* JSON baselines (`v5.0-ref/`, `v4.0-ref/`, `v3.0-ref/`), Phase 0 external FFO JSONs (`p0_*`), all 30 Phase 0 baseline matches (`match_p0_*`), Phase 2 (`match_p2_*`), Phase 3 (`match_p3_*`), and Phase 4 (`match_p4_*`) verification match reports.

---

## 9. Working Rules & Engineering Gates

1. **Matches & long runs:** Start only on the user's explicit go, from a numbered list with time estimates; announce actual start time, estimated duration, estimated end time, and remaining match time before starting, and summarize each result upon completion. Check CPU idle state first, run nothing heavy concurrently, and preserve `.games.jsonl` (`--resume`) when continuing.
2. **A/B switches (`SearchOptions` / `opt=`):** Every experimental search change sits behind a temporary `SearchOptions` switch and is removed immediately upon acceptance or reverted on rejection.
   - Strength claims require **lower bound > 50%** in a **`1,174`-game match on `starts-v6.txt`** (`587` prefix-free starts, default when `--starts` is omitted) at 500 ms (`--workers 8` for 1t, `--workers 1` for 8t), confirmed at 2 s (600 games, `--max-starts 300`).
   - Any node-changing optimization requires updating `stello-v3-signatures.txt` and C++/WASM parity fixtures across `Portable`, `V3`, and `V4`.
3. **Zero-warning & zero-ZMM gate:** All 5 C++ presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`), `.NET` tests, and WASM Node tests must pass with **0 compiler/MSBuild warnings** and **0 `zmm` instructions** in native binaries (`llvm-objdump -d`).
4. **Git & untracked assets:** Commit with author `cjp-dev <cjp230364@gmail.com>` + `Co-Authored-By: Antigravity <noreply@google.com>`; run `git fsck --no-dangling` after commits; fast-forward `main` only at release with user OK. **Never commit** `Training data/`, Egaroucid/Edax folders, `build/`, `out/`, `target/`, `wwwroot/engine*`, `scratch/`, `*.profraw`, `*.profdata`, or `*.games.jsonl`.

---

## 10. Current State & Next Actions

- **Stello 5.0 (`v5.0`)** is live on `main` and preserved as `stello-v5` (`Stello.Cpp/ref/stello_engine_v5.dll`).
- **Stello 6.0 Phases 0–4 are complete and verified on `feature/stello-6.0`** (`52de969`):
  - Full Phase 0 reference harnesses & `starts-v6.txt` baselines are recorded (`522/522` + `127/127` + `120/120` .NET tests, `71/71` × 5 C++ presets, `20/20` WASM Node tests, `0` warnings, `zmm = 0`).
  - Phase 2 (+ addendum `5e9dc80`), Phase 3 (`+22` Elo @ `500 ms` `8t`, `7.23×` 16t FFO `#40–49`), and Phase 4 (`hash_move`-first + wipeout check, unified `32 MiB` lockless dual-bound TT, `eval1_nws`/`eval2_nws` + `look == 2` static ordering **`+25/+28`** Elo, ETC parent store + recursive ProbCut **`+70/+43`** Elo & `−82.75%` `deep24 d22 1t`, `5D-TPG` **`+19`** Elo @ `15 s` / `30 s`, plus handover & clock-overrun review fixes) are landed and tested.
- **Next active milestone:** **Phase 5 (Endgame — closing the 2.21× FFO node gap)** in [Plans/making-stello-6-0-plan.md](Plans/making-stello-6-0-plan.md) (optionally preceded by the Section 2 / Section 4.2 `TimePerGame` policy/ablation matches proposed in [Plans/Phase 4 item 5 review and fixes.md](Plans/Phase%204%20item%205%20review%20and%20fixes.md)) — warm Unified-TT ID + selective endgame pre-search in `Solve` mode, shared TT down to `11–13` empties + L1D-resident `24 KB` `LocalTable`, stability bounds & exact `LASTFLIP_HIGHCUT`, end move-ordering upgrades — followed by **Phase 6 (Evaluation / RTX 4080 NNUE)**, **Phase 7 (`v6` book)**, and **Phase 8 (Release `v6.0`)**.
