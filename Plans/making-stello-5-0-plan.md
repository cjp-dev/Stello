# Stello 5.0: speed, threads, search, endgame, evaluation and opening book

This plan is in `Plans/making-stello-5-0-plan.md`. Its findings document, `Plans/making-stello-5-0-findings.md`, is created at phase 0 and updated after every phase. Branch: `feature/stello-5.0`, from `main` (Stello 4.0 release).

## Context
- **Where we are:** Stello 4.0 is complete (2026-11-09; [Stello 4.0 C++ findings.md](Stello%204.0%20C%2B%2B%20findings.md) and [docs chapter 28](../docs/28-stello-4-0-the-engine-in-cpp.md)). Its C++20 engine (`Stello.Cpp`, `stello_version() = 40000`, ABI 3) is node-for-node identical to Stello 3.1 (Rust) on 1 thread, faster on desktop (`+4.3–7.0 %` midgame, `+5.3–6.0 %` 1t FFO, `+4.9–7.2 %` 8t–16t FFO, `+7.6 %` nodes/s in 500 ms matches) and in WebAssembly (`+6.8–17.3 %` 1t, `+3.6–15.8 %` 4–8t, `20–25 KB` smaller brotli bundles), with zero external library dependencies.
- **What was postponed from Stello 3.2 ([making-stello-3-1-plan.md](making-stello-3-1-plan.md) Part 2):** the speed pass (AVX2 solver bitboards, incremental pattern indices, WebAssembly `simd128`), the parallel search improvements (root splitting, brother cut interruption, split heartbeats, 8c/16t tuning), the midgame search ideas, the endgame move-ordering table and selective endgame, the larger/richer evaluation, and the opening book recalculation and expansion.
- **How Stello 4.0 C++ reshapes this plan:**
  1. **C++20 `x86-64-v3` runtime dispatch & explicit AVX2:** In Stello 4.0 Phase 4/5, disassembly showed that the portable `clang-release` build (`-mcx16`) emits software SWAR `popcount` and `63 ^ bsr` outside `patterns_bmi2.cpp`, while `-march=native` (`clang-native`) unlocked hardware `popcnt`/`lzcnt`/`blsr` (`+7–9 %` 1t midgame, `+17.6 %` 16t FFO) but triggered Clang 23 AVX-512 `zmm` auto-vectorization in `endgame::bits::legal_moves()`. Compiling the hot search/solver paths with `-mavx2 -mbmi -mbmi2 -mpopcnt -mlzcnt -mno-avx512f` (plus explicit 256-bit `__m256i` `flips` and `legal_moves`) and runtime CPUID dispatch captures both the hardware bit-manipulation gain and the 256-bit SIMD bitboard gain without `zmm` downclocking while keeping the portable `x86-64` fallback.
  2. **Incremental pattern evaluation helps desktop, WebAssembly, and Phase 5:** Maintaining the 62 pattern feature indices incrementally during make/unmake replaces 62 per-node `pext` (or portable multiply-shift) extractions with small per-flipped-square ternary updates and a SIMD sum of 62 `int16_t` weights. This accelerates x86-64, removes WebAssembly's `sum_portable` bottleneck (where WASM lacks `pext`), and makes 16-shape `egaroucid16` (64 features vs `plus15`'s 62) nearly free in search speed for Phase 5.
  3. **16-thread SMT behavior on the Ryzen 7 7800X3D:** Stello 4.0 Phase 4 measured that on 8 physical Zen 4 cores / 16 SMT threads, running 8 Lazy SMP helpers at 16 threads increased `mid-bench d16` nodes by `+45 %` and made 16 threads slower in wall time than 8 threads (`3.18 s` vs `2.69 s`), whereas 16 threads accelerated `FFO #40–49` (`6.13 s` -> `5.31 s`). Phase 2 tunes the Lazy SMP helper cap and YBWC split thresholds specifically for this behavior.
  4. **Self-contained `Stello.Cpp`:** `Stello.Cpp` currently reads parity fixtures from `Stello.Rust/stello-core/tests/data` and runs WASM Node tests from `Stello.Rust/stello-wasm/tests/`. Phase 0 moves these into `Stello.Cpp/tests/` so `Stello.Cpp` is completely self-contained, leaving `Stello.Rust/` frozen at 3.1.

## Decisions (user, 2026-11-10)
- **Phases 0–7:** renumbered cleanly for Stello 5.0 (Phase 0 setup/baseline, Phase 1 speed, Phase 2 threads, Phase 3 search, Phase 4 endgame, Phase 5 evaluation, Phase 6 opening book, Phase 7 5.0 release).
- **Order of work:** speed comes first (Phase 1 changes no nodes, so the 4.0 node signatures validate every optimization), then threads (Phase 2), search (Phase 3), endgame (Phase 4), and evaluation (Phase 5). The opening book (Phase 6) comes last so it is recalculated with the final 5.0 engine.
- **Phase 1 speed architecture:** runtime dispatch between the portable `x86-64` path (`--cpu portable`) and an `x86-64-v3` (`AVX2 + BMI2 + POPCNT + LZCNT`, no AVX-512 `zmm`) fast path with explicit 256-bit AVX2 `flips` and `legal_moves`, incremental pattern indices through make/unmake, and Emscripten `-msimd128` in WebAssembly.
- **`Stello.Rust` stays frozen at 3.1:** all engine changes happen in `Stello.Cpp` (and `.NET` tools/apps where needed). Shared test fixtures and WASM Node tests move into `Stello.Cpp/tests/`.
- **Primary machine:** AMD Ryzen 7 7800X3D (8 physical cores / 16 logical processors, Windows x64).

## Working rules (as for 3.1 and 4.0)
- **Matches and long runs:** start only on the user's go, from a numbered list with time estimates. Check CPU idle state first, run nothing heavy at the same time, and keep the per-game lines in `.games.jsonl` (`--resume` supported).
- **A/B switches:** each engine change in Phases 2–5 sits behind a `SearchOptions` switch (`Stello.Cpp/src/search/options.hpp`, FFI setter if needed or `opt=<key>=<value>` in `Stello.Benchmark`). As soon as a change's match is approved, its switch is removed again: a kept change stays as the only code, and a rejected change is removed with the old code kept.
  - A correctness fix is kept unless measurement shows it makes the engine play worse ("not worse" — upper bound of the 95 % interval > 50 %).
  - A change that claims to make the engine stronger must show a gain (lower bound > 50 % at 500 ms, confirmed at 2 s).
  - When a kept change alters node counts, `Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt` (or the 5.0 signature file) is regenerated with a note in its header and the findings, and the signature tests (C++, C#, Node) stay strict node-for-node.
- **Gates:**
  - For a change that alters play: a 500 ms match of 1,180 games against the previous default (`--workers 8` at 1 thread on the 8-core 7800X3D, `--workers 1` at 8 threads), confirmed at 2 s (600 games, `--max-starts 300`).
  - For a change that only alters speed (Phase 1): `mid-bench` and `solve-bench --compare` against `v4.0-ref`, with 100 % identical moves, scores, depths and node counts.
  - All tests pass with 0 compiler/MSBuild warnings: C++ presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`), `Stello.Engine.Tests`, `Egaroucid.Engine.Tests`, `Stello.Net.Tests`, and the WASM Node tests (`15/15`).
- **Git:**
  - identity `cjp-dev`, with the Co-Authored-By trailer;
  - `git fsck` after each commit;
  - `main` is fast-forwarded only at the release, with the user's OK.
- **Never commit:** training data, Egaroucid resources or build binaries (`build/`, `out/`, `target/`, `wwwroot/engine*`, `*.games.jsonl`).
- **Findings:** `Plans/making-stello-5-0-findings.md` updated after every phase.

---

## Phase 0: set-up, self-contained C++ tests, and the 4.0 baseline
1. **Branch, tag and documents:**
   - ensure `main` is at the Stello 4.0 release and tagged `v4.0`;
   - create branch `feature/stello-5.0`, `Plans/making-stello-5-0-plan.md`, and `Plans/making-stello-5-0-findings.md`.
2. **Self-contained `Stello.Cpp` test fixtures and WASM Node tests:**
   - copy the parity fixture files (`positions.txt`, `patterns.txt`, `perft.txt`) into `Stello.Cpp/tests/data/` and update `Stello.Cpp/CMakeLists.txt` (`STELLO_PARITY_DATA_DIR`) and `Stello.Benchmark` (`ParityCommands.cs`) to point to `Stello.Cpp/tests/data` by default (keeping the existing files in `Stello.Rust/` so `cargo test` in the frozen 3.1 workspace still works);
   - copy `engine.test.mjs` and `engine-mt.test.mjs` into `Stello.Cpp/tests/wasm/` (updating their relative paths to `Stello.Web/wwwroot/engine*` and `stello-v3-signatures.txt`), and update docs/scripts to run `node --test Stello.Cpp/tests/wasm/engine.test.mjs Stello.Cpp/tests/wasm/engine-mt.test.mjs`.
3. **Preserved Stello 4.0 native reference (`stello-v4`):**
   - copy the release `stello_engine.dll` (`40000`, ABI 3) to `Stello.Cpp/ref/stello_engine_v4.dll` (git-ignored) and add `engine=stello-v4` in `Stello.Benchmark` (alongside `stello-v3`, `stello-v2`, `stello-v1`, `stello-lib`, `egaroucid`) so any phase can benchmark or play matches directly against unmodified Stello 4.0.
4. **The 4.0 baseline JSONs:**
   - record the 1-thread reference JSONs on the Ryzen 7 7800X3D (`ffo-40-49.json`, `midgame-depth10.json`, `midgame-depth12.json`, `midgame-depth14.json`, `midgame-depth16.json`, `midgame-random-depth8.json`, `midgame-handover-depth12.json`, `endgame-random-14-24.json`) in `Plans/benchmark-results/v4.0-ref/`.

**Gate P0:** all 5 C++ presets pass `43/43` tests using `Stello.Cpp/tests/data`; `node --test Stello.Cpp/tests/wasm/*.mjs` passes `15/15`; `Stello.Benchmark` supports `engine=stello-v4` and the `v4.0-ref/` JSONs are recorded.

---

## Phase 1: speed (no change in nodes)
Every step in Phase 1 must be **100 % node-identical** to Stello 4.0 on 1 thread across all parity suites. Each step is measured independently and kept only if it speeds up `mid-bench` and/or `solve-bench`.

1. **Runtime `x86-64-v3` (`AVX2 + BMI2 + POPCNT + LZCNT`) dispatch & explicit 256-bit AVX2 `flips` and `legal_moves`:**
   - **Why:** In Stello 4.0, baseline `clang-release` (`-mcx16`) emits software SWAR `popcount` and `63 ^ bsr` in the search and solver (`popcnt = 0`, `lzcnt = 0` outside `patterns_bmi2.cpp`), costing up to `8–15 %` in midgame and `16t` FFO. Meanwhile, full `-march=native` allowed Clang to emit 512-bit `zmm` instructions in `legal_moves()`.
   - **What:**
     - extend `stello::cpu` (`src/core/cpu.hpp`, `src/core/cpu.cpp`) to detect `has_avx2_bmi2()` (`AVX2 + BMI1 + BMI2 + POPCNT + LZCNT` and `fast_pext()`, disabled by `--cpu portable`);
     - implement explicit 256-bit AVX2 `flips` and `legal_moves` (`__m256i` intrinsics in `src/endgame/bits_avx2.hpp`, modeled on Egaroucid's AVX2 bitboard operations, with zero `zmm` registers);
     - compile the hot solver and midgame routines in a dedicated `x86-64-v3` translation unit (`-mavx2 -mbmi -mbmi2 -mpopcnt -mlzcnt -mno-avx512f` on Clang/GCC, `/arch:AVX2` on MSVC) alongside the portable `x86-64` translation unit, and dispatch once at search entry (`Searcher::run`);
     - verify with `llvm-objdump -d` that the `x86-64-v3` path uses hardware `popcnt`, `lzcnt`, `tzcnt`, `blsr`, and `ymm` instructions with `0` `zmm` instructions, and that the portable path contains `0` AVX2/BMI2 instructions.
2. **Incremental pattern indices through make/unmake in the midgame search:**
   - **Why:** Static evaluation (`evaluate_leaf` and `order_by_evaluation`) accounts for ~58 % of midgame time. Even with BMI2 `pext`, extracting 62 features from scratch at every node performs 124 `pextq` instructions and 124 two-level table lookups before reading the weights, and in WebAssembly (`sum_portable`) it is over 2× slower.
   - **What:**
     - precompute a per-square feature-delta table in `PatternSet` (`src/eval/patterns.hpp`, `src/eval/patterns.cpp`): for each square `0..63`, the list of `(feature_index, power_of_3_offset)` entries affected when a disc is placed or flipped;
     - maintain the active `PatternSet` feature indices across moves in the midgame search (`Board` / `Searcher`), or update from parent to child when evaluating/ordering, and sum the 62 `int16_t` stage weights using SIMD (`_mm256_add_epi16` / `_mm256_madd_epi16` on AVX2, `wasm_i16x8` on WASM SIMD, scalar on portable);
     - keep `EvalWeights::evaluate(own, opponent)` for standalone calls and tests, and verify in `parity_tests.cpp` that incremental evaluation matches `evaluate(own, opponent)` across all 6,214 `positions.txt` positions and 30,000+ moves for `classic11`, `plus15`, and `egaroucid16`.
3. **WebAssembly `-msimd128` and WASM abort-mode check:**
   - **What:**
     - add `-msimd128` to the Emscripten flags in `Stello.Cpp/CMakeLists.txt` (both single-threaded `engine` and multi-threaded `engine-mt`);
     - add 128-bit `wasm_simd128.h` fast paths for `legal_moves`, `flips`, and the 62-feature weight sum;
     - re-measure `STELLO_ABORT_EXCEPTIONS=OFF` vs `ON` in WASM with `-msimd128` and incremental evaluation in place (in 4.0 Phase 6, `OFF` was `+5 %` faster in midgame while `ON` was `+2 %` faster in FFO), choosing the faster setting for WASM.

**Gate P1:**
- 100 % node-identical to Stello 4.0 on all 105 `stello-v3-signatures.txt` checks and all `v4.0-ref` parity sets (`mid-bench` d10/d12/d14/d16, random d8, handover d12, `solve-bench` FFO #40–49, 500 random endgames), both in default mode and with `--cpu portable`;
- measurably faster in `Stello.Benchmark` (`mid-bench` and `solve-bench`) and in Node/Chromium WASM (`15/15` Node tests green).

---

## Phase 2: threads
1. **Root splitting** in `search_root` and `solve_root`. On the Ryzen 7 7800X3D with 8 threads, FFO #40–49 is `3.67×` faster than 1 thread today (`6.13 s` vs `22.49 s`), against Egaroucid's `4.15×`.
2. **Interrupting the splitting thread's own brother** when a task fails high: the local search observes `split_point->cut` (at `count_node()` or child boundaries), stops searching the superseded brother immediately, and unwinds/returns the cut cleanly without reporting a search abort.
3. **Progress heartbeats while a thread waits at a split,** from the polling callback of `scope_polling` (`src/search/pool.hpp`, `src/search/ybwc.cpp`), so the UI analysis panel never stalls during a long root or deep split wait.
4. **Tuning on the Ryzen 7 7800X3D (8 physical cores, 16 logical threads):**
   - tune `YBWC_MID_MIN_DEPTH` (currently `6`, where tasks average ~415 nodes) and `YBWC_END_MIN_EMPTIES` (currently `16`);
   - tune the Lazy SMP helper vs YBWC worker ratio at 8 and 16 threads (in Stello 4.0 Phase 4, 16 threads ran 8 Lazy SMP helpers, inflating `mid-bench d16` nodes by `+45 %` and making 16 threads slower in wall time than 8 threads; capping Lazy SMP helpers at physical cores or fewer at 16t should let extra SMT threads serve purely as low-overhead YBWC workers);
   - re-measure Lazy SMP helpers in `solve` once root splitting is active;
   - update the desktop `Threads (Automatic)` default in `GameSettings` / `SettingsViewModel` if physical cores (8) or another count beats `ProcessorCount - 1` (15) on the 7800X3D.

**Gate P2:**
- FFO #40–49 is faster with 8 and 16 threads than in Phase 1, 16 threads is not slower than 8 threads in `mid-bench d16`, and all solves are 100 % exact;
- 8 threads against Phase 1 with 8 threads (`--workers 1`) at 500 ms is not worse, and is stronger at 2 s;
- `clang-sanitized` (ASan + UBSan) and the 8-thread seqlock/pool stress tests pass with 0 findings.

---

## Phase 3: search
Each item is implemented behind a temporary `SearchOptions` / `opt=` switch, benchmarked on `mid-bench`, and tested in a 1,180-game 500 ms match (confirmed at 2 s, 600 games, if it passes).

1. **Multi-ProbCut (`src/search/engine.cpp`, `Stello.Trainer` `mpc`):**
   - refit the error model with the Phase 1 engine (without single-move rows), or fit one table per `(shallow, deep)` depth pair;
   - test `t` varying by game phase (disc count) and retune `t` (`mpc=`).
2. **The second-best move in the transposition table, alone (`src/search/table.hpp`, `src/search/ordering.hpp`):**
   - previously only tried bundled with 2-bound retention; store/order the second move from the 2-entry slot when the first entry's move does not cut off.
3. **The soft time limit (`src/search/time.hpp`, `soft-pct=`):**
   - re-tune the midgame soft time percentage (`soft-pct=`, default `67 %`) at 5.0's deeper search depths (~20+ plies).
4. **Aspiration windows or MTD(f) (`src/search/engine.cpp`):**
   - use the previous iteration's root score with a narrow aspiration window (widening on fail-low/fail-high) instead of `[-INFINITY_SCORE, +INFINITY_SCORE]` at depths $\ge 4$.
5. **Transposition table size at 2 s with 8 threads:**
   - measure `hash-bits=19` (64 MB) against `hash-bits=21` (256 MB) at 2 s per move with 8 threads (`--workers 1`), where 8 threads search ~175M nodes per move.
6. **The clog search:**
   - early forced game-end (wipeout/clog) check at the root with a strict depth or time cap.

**Gate P3 (per item):** a 500 ms match of 1,180 games against the previous default, lower bound > 50 % (or not worse for pure simplifications), confirmed at 2 s (600 games). Approved changes have their temporary switch removed immediately; rejected changes are reverted cleanly.

---

## Phase 4: the endgame
1. **A small move-ordering table for 11–13 empty squares (`end_local` in `src/endgame/search.cpp`, `Stello.Trainer`):**
   - like Egaroucid's 4-pattern table updated incrementally move by move, trained by `Stello.Trainer` and embedded alongside the main evaluation;
   - replaces or augments the mobility-only ordering at `11–13` empties where full 62-feature evaluation is too heavy.
2. **Start the solve from an estimated solve time, and selective endgame (`mpc-end`) at longer time controls:**
   - replace or refine the fixed `empties - 8` midgame-to-solve handover using depth/branching progress and `mpc-end` selectivity at longer time controls (2 s per move, 5 min per game).

**Gate P4:** FFO #40–59 all exact and faster (or not slower), and 500 ms / 2 s matches pass as in Phase 3.

---

## Phase 5: evaluation
1. **A larger evaluation (`egaroucid16` × 60 stages, `Stello.Trainer`):**
   - with Phase 1's incremental pattern indices, `egaroucid16` (16 shapes, 64 features) updates almost the same number of features per flipped disc as `plus15` (15 shapes, 62 features), eliminating the `−17 %` nps penalty that held `egaroucid16` back in Stello 2.0;
   - train `egaroucid16` × 60 stages (up to 10 M positions per disc count on the 7800X3D) and fit its own Multi-ProbCut table;
   - test in 500 ms and 2 s matches against `plus15` × 10 stages;
   - decide based on size and browser measurements whether the web app keeps the ~1 MB model (a desktop/web split of the embedded weights file) or uses a compressed intermediate model.
2. **Extra features (mobility, potential mobility, parity) and training improvements at the same size:**
   - investigate adding direct mobility / potential mobility / parity features in `Stello.Trainer` and `EvalWeights` to close the remaining `5.47` -> `5.05` MAE gap to Egaroucid.

**Gate P5:**
- lower MAE and RMSE on the trainer test split;
- 500 ms and 2 s matches against 5.0-so-far pass (lower bound > 50 %);
- web first-visit transfer size and runtime memory are measured and stay within budget.

---

## Phase 6: the opening book
1. **Recalculate the master book with the final 5.0 engine (`Stello.BookTool recalc`):**
   - bump `OpeningBook.EngineVersion` from `4` to `5` (`Stello.Engine/OpeningBook.cs`);
   - update `BookRecalculator.NeedsSearch` so that the 2,988 timed `X` (`Exact` with a time limit, `X:60s`) entries from `EngineVersion <= 4` — which may have come from an aborted solve before the 3.1 Phase 1.1 fix — are treated as needing re-search, while the 1,091 untimed `X` entries (full solves) are kept;
   - run `recalc` at 1 thread per worker (`--workers 8` on the 7800X3D) on the user's go.
2. **Search the moves the book lacks (`calclib` mode in `Stello.BookTool`):**
   - for each book position, search the legal moves not yet in the book using `onlyMoves` (`BookSearch`) and add moves within a threshold of the best book move.
3. **Drop-Out Expansion ([Opening book improvment.md](Opening%20book%20improvment.md) Phase 3):**
   - decide with the user on the target book size (and whether web book storage moves from `localStorage` ~5 MB to `IndexedDB`), the effort per position, and the DOE depth.

**Gate P6:** with books enabled, 5.0 with the new book beats 5.0 with the 4.0 book at 500 ms, `Stello.BookTool verify` passes with 0 warnings/errors, and the web binary book size is measured.

---

## Phase 7: the Stello 5.0 release
- **Final matches** (each run one at a time on the user's go):
  - Stello 5.0 vs Stello 4.0 (`engine=stello-v4`), Stello 3.1 (`engine=stello-v3`), Stello 2.0 (`engine=stello-v2`), and the Egaroucid port (`engine=egaroucid,time-strict=on`), at 100 ms, 500 ms, and 2 s, with 1 and 8 threads, plus `solve-bench` on FFO #40–49 and #50–59.
- **Version 5.0:**
  - bump `Stello.Cpp/CMakeLists.txt` and `version.hpp` to `5.0.0` (`stello_version() = 50000`), `AboutInfo.cs` to `"Stello Version 5.0"`, and update version tests.
- **Documentation:**
  - write `docs/29-stello-5-0-speed-threads-search-and-book.md` covering Phases 1–6 and the final matches;
  - renumber `docs/29-glossary.md` -> `docs/30-glossary.md` and `docs/30-references.md` -> `docs/31-references.md`;
  - update Part 1 chapters (`01`, `05`, `06`, `07`, `08`, `09`, `12`, `14`, `15`, `17`) for any kept search/eval/thread/book changes, and verify 0 broken links or snippet errors.
- **The release:**
  - fast-forward `main` with the user's OK, monitor the Azure Static Web Apps deploy, and run the live browser check.

---

## Verification summary
| Phase | Check |
|---|---|
| 0 | Self-contained `Stello.Cpp/tests/` data & WASM tests green; `engine=stello-v4` works; `v4.0-ref/` JSONs recorded |
| 1 | 100 % node-identical to 4.0 on all parity sets (`auto` and `--cpu portable`); faster in `.NET` FFI and WASM |
| 2 | FFO faster with 8 and 16 threads; 16t not slower than 8t in midgame; 8-thread matches at 500 ms & 2 s; ASan+UBSan clean |
| 3–5 | Per item: 500 ms match lower bound > 50 %, confirmed at 2 s; evaluation test split MAE/RMSE; web transfer & memory size |
| 6 | New book beats 4.0 book at 500 ms; `BookTool verify` clean; web book size checked |
| 7 | Final matches against 4.0, 3.1, 2.0 and Egaroucid; Chapter 29 & Part 1 docs; 0 broken links; live site check |

**Tools:**
- `Stello.Benchmark` `mid-bench`/`solve-bench --compare`, `match` (`--resume`, `opt=`, `threads=`, `engine=stello-v4|stello-v3|stello-v2|egaroucid`);
- `ctest --preset clang-release|clang-native|msvc-release|msvc-native|clang-sanitized --output-on-failure`;
- `dotnet test -c Release` on `Stello.Engine.Tests`, `Egaroucid.Engine.Tests`, and `Stello.Net.Tests`;
- `node --test Stello.Cpp/tests/wasm/engine.test.mjs Stello.Cpp/tests/wasm/engine-mt.test.mjs`.
