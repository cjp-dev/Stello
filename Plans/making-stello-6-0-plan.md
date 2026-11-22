# Stello 6.0: bug fixing, speed, threads, search, endgame, evaluation and opening book

This plan is in `Plans/making-stello-6-0-plan.md`. Its findings document, `Plans/making-stello-6-0-findings.md`, is created at Phase 0 and updated after every phase. Branch: `feature/stello-6.0`, from `main` (Stello 5.0 release, tagged `v5.0`).

## Context
- **Where we are (at Stello 5.0 release):** Stello 5.0 ([making-stello-5-0-plan.md](making-stello-5-0-plan.md) and [making-stello-5-0-findings.md](making-stello-5-0-findings.md)) brought `x86-64-v3` AVX2/BMI2 runtime dispatch, incremental pattern indices, WASM `-msimd128` (`+39 %` native midgame, `+37 %` 1t FFO, `+94–100 %` WASM midgame), YBWC root splitting + brother cut + `(w + 2) / 4` Lazy SMP (`4.88×` 8t / `5.69×` 16t FFO, `+30 Elo` at 2 s 8t), 3-stage selective endgame `mpc-end` (`+10 Elo` at 500 ms, `+22 Elo` at 2 s), Format v3 `egaroucid16 + mobility` × 60 evaluation (`4.97` MAE / `6.74` RMSE — surpassing Egaroucid's `5.05 / 6.90`, `~+33–34 Elo`), and the trimmed, `calclib`-enriched, DOE-expanded `v5` opening book.
- **Measured gaps still open on the Ryzen 7 7800X3D (8 cores / 16 threads, Zen 4, 96 MB L3) + NVIDIA GeForce RTX 4080:**
  1. **Endgame node gap (`2.21×` vs Egaroucid 7.8.2):** On FFO `#40–49` (1t), Stello 5.0 is already **`+25–34 %` faster per node** (`64.65M` vs `51.63M` nps @ 1t; `399.98M` vs `299.29M` @ 8t; `502.70M` vs `397.09M` @ 16t), yet searches **`1,115.7M` nodes** vs Egaroucid's **`505.4M` nodes** (`17.3 s` vs `9.79 s`). Line-by-line comparison against `Egaroucid-main` and `edax-reversi-master` decomposed this into 7 concrete causes: a split/cold `endgame_table` with skipped ID/MPC pre-warming in `Solve` mode, missing shared TT + move ordering at `11–13` empties, single-bound 2-way TT thrashing (plus a selective-endgame `store_depth = 1/2/4` eviction flaw), unordered from-scratch `ordering_value`, wide-window WLD+half-range passes instead of narrow even-parity aspiration, incomplete ETC/ESC, and `lone_empties` A1–H8 scanning at `≤ 6` empties.
  2. **Multi-thread overhead & node inflation (`+27–38 %` vs `+6–17 %`):** On FFO `#40–49`, Stello scales `4.88×` @ 8t and `5.69×` @ 16t vs Egaroucid's `5.27×` @ 8t and `7.07×` @ 16t. Audit of `Stello.Cpp/src/search/` found four concrete bottlenecks: `s.may_split()` never checks `pool->idle() > 0` (heap-allocating `ScopeTasks` + 2 mutexes + 1 condvar and building child arrays even when all workers are busy, and recomputing `state_of` from scratch on local fallback), `flush_nodes()` executes ~490k cross-core atomic `fetch_add` RMWs/s on one shared cache line, static wave barriers abort all brothers on null-window fail-highs without a Helpful Master, and `ScopeTasks::running_mutex` nests inside `PoolShared::queue_mutex`.
  3. **Untapped speed & Zen 4 hardware (`x86-64-v4` 256-bit AVX-512 & cache density):** Zen 4 executes 256-bit `ymm` AVX-512VL/CD/DQ/VNNI at full clock speed with zero `zmm` downclocking; `stable.cpp` currently allocates **1 MB** of scalar tables (16× larger than Edax's 64 KB edge table); `PatternState` is **256 B (`8 × ymm`)** instead of a single ternary **128 B (`4 × ymm`)**; and midgame `search_inc` computes `flips()` up to 3× per move while calling `order_by_evaluation` *before* trying `hash_move`.
  4. **Evaluation ceiling & RTX 4080 GPU training:** Stello 5.0's linear `egaroucid16 + mobility` × 60 already beats Egaroucid's linear pattern model. Pushing meaningfully below `4.97` MAE (and/or shrinking WebAssembly below `8.96 MB` brotli) motivates exploring Egaroucid's post-training integer-rounding hill-climb, fresh Stello 5.0/6.0 self-play + WTHOR data, and **RTX 4080 GPU-trained NNUE / non-linear pattern-hybrid evaluators** (with zero-dependency C++20 SIMD inference, including Zen 4 `vpdpbusd` VNNI, and an optional desktop/web model split).
  5. **Verified bugs across the full repository:** An exhaustive pre-plan audit of `Stello.Cpp` and `Stello.Net` (including `Stello.Engine`, `Stello.App`, `Stello.Net` WPF, `Stello.Web` Blazor WASM + JS, and CLI tools) uncovered **19 remaining confirmed bugs** (6 C++, 13 .NET/Web/CLI) and **14 defensive/latent issues** that belong in an initial Bug Fixing phase before any new optimizations begin *(2 additional `BookTrimmer`/`BookCalclib`/`BookExpander` bugs found in the same audit were already fixed ahead of Stello 5.0 Phase 6.4 in commits `2fbd82d` and `fb325e2`)*.
  6. **Legacy `starts.txt` inefficiency & parent–child overlap (`61.5 %` prefix overlap, `15.8 %` lopsided starts):** Auditing `Stello.Benchmark/Data/starts.txt` (590 positions at plies 10–12 selected before Stello 2.0 using Stello 1.0's hand-crafted evaluation `|v1| <= 40`) against Stello 5.0's 60-second book values and all 18 Phase 7 matches (`21,240` games) uncovered three structural issues:
     - **Lopsided Stello 1 evaluations (`15.8 %` have `|v5| > 4.00` discs, up to `±9.00` discs):** Across the six 2,000 ms Phase 7 matches, pairs with `|v5| <= 1.00` disc were **`62.0 %` decisive** and `1.01–2.00` discs were **`54.7 %` decisive**, whereas `4.01–8.00` discs (`90` starts) dropped to **`23.7 %` decisive** (`40 %` produced zero decisive pairs across all six 2 s matches) and `> 8.00` discs (`3` starts) were **`0.0 %` decisive** (automatic `1–1` color sweeps wasting ~16 % of match time).
     - **`61.5 %` parent/grandparent prefix overlap (`363 / 590` starts):** Because `make-starts` took all qualifying nodes across plies 10, 11, and 12 simultaneously, `354 / 590` starts have their immediate 1-ply parent also in `starts.txt`, and `221 / 590` (`37.5 %`) transition directly into another start in `starts.txt` via Stello 5.0's #1 best move — causing **`21–27 %` of those 221 parent–child pairs (`94–118` games per 1t match) to replay the exact same game to the exact same final disc score**.
     - **Out-of-book lines (`only 172 / 590` in the `v5` book):** `418 / 590` (`70.8 %`) were pruned by `trim --hero-loss 100` because at least one side had already made a `> 1.00`-disc mistake before ply 10–12.
- **External engines, codebases & literature surveyed for Stello 6.0:**
  - **Local reference engines:** `Egaroucid-main` (Takuto Yamana, v7.9.0, GPL-3.0-or-later) and `edax-reversi-master` (Richard Delorme, v4.7, GPL-3.0) + Toshihiko Okuhara's `edax-reversi-AVX` (4.5.x AVX2/AVX-512/BMI2 & thread-local TT optimizations).
  - **Chess & modern alpha-beta / NNUE:** `official-stockfish/stockfish` (Correction History, Internal Iterative Reductions [IIR], CutNode-aware LMR & Continuation History, dynamic best-move-instability time management, Lazy SMP diversification, dual small/big NNUE, SCReLU, `vpdpbusd` VNNI, PGO).
  - **Other strong Othello engines & research:** **Othello Sensei** (`borassi/othello-sensei`, Michele Borassi — pattern features & uncertainty-guided derivative/best-first search), **Neural Reversi** (`natsutteatsuiyone/neural-reversi`, Rust + PyTorch NNUE alpha-beta), **Othello-Zero** (`HaccerKat/Othello-Zero`, distilled compact `int16` NNUE), **Saio** (Benedetto Romano — library/book construction), Takuto Yamana (*"Compressing the Evaluation Function With Small-Scale Deep Learning on Othello"*, IEEE ToG), Michael Buro (Multi-ProbCut & GLEM), Thomas Lincke (Drop-Out Expansion), and Hiroki Takizawa (*"Othello is Solved"*, arXiv:2310.19387 — parallel alpha-beta vs `df-pn`).
- **Verdict on phase exhaustion:** Every phase (Speed, Threads, Search, Endgame, Evaluation, Opening Book) still has concrete, high-leverage ideas backed by source evidence; **no phase is exhausted yet**.

## Decisions (user, 2026-11-19)
- **Phases 0–8:** Phase 0 (setup, 5.0 baseline & native external harnesses), Phase 1 (**Bug Fixing** — full codebase audit), Phase 2 (speed, 100 % node-identical), Phase 3 (threads), Phase 4 (search, starting with Unified TT), Phase 5 (the endgame), Phase 6 (evaluation), Phase 7 (the opening book), Phase 8 (the Stello 6.0 release).
- **Bug-fixing scope:** Full repository audit (`Stello.Cpp`, `Stello.Engine`, Opening Book pipeline, `Stello.App`, WPF `Stello.Net`, Blazor/JS `Stello.Web`, and CLI tools `Stello.Benchmark`, `Stello.BookTool`, `Stello.Trainer`).
- **Hardware & WebAssembly constraints:** Explore optional 256-bit `x86-64-v4` (AVX-512VL/CD/DQ/VNNI, zero `zmm` registers) runtime dispatch alongside `x86-64-v3`, portable `x86-64`, and WASM `simd128`; allow a desktop/web evaluation split if a larger or neural model wins on desktop.
- **Evaluation & training ambition:** Explore both richer linear/pattern refinements and GPU-trained NNUE / non-linear hybrids using an optional Python/PyTorch/CUDA training pipeline on the **RTX 4080** alongside `Stello.Trainer`, while keeping runtime inference in `Stello.Cpp` **100 % zero external dependencies**, plus generating fresh Stello 5.0/6.0 self-play / WTHOR training data.
- **Direct external opponent harnesses:** Add native C Edax (`engine=edax`) and native C++ Egaroucid (`engine=egaroucid-cpp`) match/benchmark support in `Stello.Benchmark` alongside `stello-v5`/`stello-v4`/`stello-v3`/`stello-v2`/`egaroucid`.
- **Upgraded 6.0 match start suite (`starts-v6.txt`):** Keep legacy `Data/starts.txt` (590 starts) untouched for historical 2.0–5.0 reproducibility, and generate a prefix-free, `v5`-balanced suite `Data/starts-v6.txt` from the 6,000-position `v5` master book (`--plies 10-14 --max-value 220 --prefix-free` → `587` starts / `1,174` games, or `--max-value 200` → `528` starts / `1,056` games) with 0 % ancestor–descendant prefix overlap and all positions within `±2.00–2.20` discs by 60-second Stello 5.0 search.
- **Feature scope:** Stay focused on playing strength and bug fixes, plus closely related high-value book/engine items (including varied book play from [Opening book improvment.md](Opening%20book%20improvment.md) Phase 4).
- **Phase order:** Speed stays strictly node-identical first (Phase 2), then pure threading overhead fixes & Helpful Master (Phase 3), then Unified TT + midgame search (Phase 4), then Endgame closing the 2.21× FFO gap + re-tuning Phase 3 split thresholds once TT is warm (Phase 5), then Evaluation (Phase 6), then Opening Book (Phase 7), then Release (Phase 8).
- **Primary machine:** AMD Ryzen 7 7800X3D (8 physical cores / 16 logical threads, Zen 4, 96 MB L3, AVX-512VL/CD/DQ/VNNI) + NVIDIA GeForce RTX 4080 (Windows x64).

## Working rules (as for 4.0 and 5.0)
- **Matches and long runs:** start only on the user's go, from a numbered list with time estimates. Check CPU idle state first, run nothing heavy at the same time, and keep the per-game lines in `.games.jsonl` (`--resume` supported).
- **A/B switches:** each experimental engine change in Phases 3–6 sits behind a temporary `SearchOptions` switch (`Stello.Cpp/src/search/options.hpp`, FFI setter if needed or `opt=<key>=<value>` in `Stello.Benchmark`). As soon as a change's match gate passes or fails, its switch is removed immediately: a kept change stays as the only code, and a rejected change is reverted cleanly.
  - A correctness fix (Phase 1) is kept unless measurement shows it makes the engine play worse ("not worse" — upper bound of the 95 % interval > 50 %).
  - A change that claims to make the engine stronger must show a gain (lower bound > 50 % at 500 ms across all `starts-v6.txt` pairs, confirmed at 2 s on 600 games / `--max-starts 300`).
  - When a kept change alters node counts, `Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt` (or the 6.0 signature file) and C++/WASM parity fixtures are regenerated with a note in their header and the findings, keeping strict node-for-node regression tests enforced.
- **Gates:**
  - For a change that alters play: a 500 ms match on `starts-v6.txt` (`587` pairs = `1,174` games at `--max-value 220`, or `528` pairs = `1,056` games at `--max-value 200`) against the previous default (`--workers 8` at 1 thread on the 7800X3D, `--workers 1` at 8 threads), confirmed at 2 s (`600` games, `--max-starts 300`), plus periodic progress matches against native C++ Egaroucid (`egaroucid-cpp`) and native C Edax (`edax`).
  - For a change that only alters speed (Phase 2): `mid-bench` and `solve-bench --compare` against the Phase 1 baseline, with 100 % identical moves, scores, depths and node counts across `--cpu portable`, `--cpu v3`, and `--cpu auto` (`v4`).
  - All tests pass with 0 compiler/MSBuild warnings: C++ presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`), `Stello.Engine.Tests`, `Egaroucid.Engine.Tests`, `Stello.Net.Tests`, and the WASM Node tests (`19/19`).
- **Git:**
  - identity `cjp-dev`, with the `Co-Authored-By` trailer;
  - `git fsck` after each commit;
  - `main` is fast-forwarded only at the release, with the user's OK.
- **Never commit:** training data, Egaroucid/Edax binaries or resources, GPU checkpoints, `build/`, `out/`, `target/`, `wwwroot/engine*`, `scratch/`, or `*.games.jsonl`.
- **Findings:** `Plans/making-stello-6-0-findings.md` created at Phase 0 and updated after every phase.

---

## Phase 0: set-up, the 5.0 baseline, and native external match harnesses
1. **Branch, tag, and preserved 5.0 reference (`stello-v5`):**
   - ensure `main` is at the Stello 5.0 release and tagged `v5.0`;
   - create branch `feature/stello-6.0` and `Plans/making-stello-6-0-findings.md`;
   - preserve the release 5.0 `stello_engine.dll` (`stello_version() = 50000`, ABI 3) as `Stello.Cpp/ref/stello_engine_v5.dll` (git-ignored) and add `engine=stello-v5` in `Stello.Benchmark` (alongside `stello-v4`, `stello-v3`, `stello-v2`, `stello-v1`, `stello-lib`, `egaroucid`);
   - record the 5.0 reference JSONs on the 7800X3D (`ffo-40-49.json`, `ffo-50-59.json` [8t], `endgame-random-14-24.json`, `midgame-depth10/12/14/16.json`, `midgame-random-depth8.json`, `midgame-handover-depth12.json`, plus 8t/16t and WASM ST/MT baselines) in `Plans/benchmark-results/v5.0-ref/`.
2. **Deterministic `IDisposable` match-player cleanup (`Stello.Benchmark` / `Stello.Engine`):**
   - wrap `EngineMatch.Work()` (`Stello.Net/Stello.Engine/BookMatch.cs:162-211`) and `BenchSearcher.NativeReference` (`Stello.Net/tools/Stello.Benchmark/BenchSearcher.cs:192`) in `try/finally` disposal so native library handles and external subprocess players shut down cleanly on completion or `Ctrl+C`.
3. **Direct native C Edax harness (`engine=edax`):**
   - build local `edax-reversi-master` (`x86-64-v3` / `avx512`) into a git-ignored tool path and add a persistent per-worker subprocess `IMatchPlayer` in `Stello.Benchmark` speaking unmodified **`edax -xboard -q -book-usage off -ponder off -h <bits> -n <threads>`**;
   - **Why `-xboard`:** Unlike `-edax` and `-nboard` (where `options_bound()` at `edax-reversi-master/src/options.c:286` clamps `options.time >= 1000 ms`) and `-gtp` (where `gtp.c:281-283` ignores move-time `time_left` and returns no score/depth/nodes), `-xboard` accepts `st <sec>` down to `0.1` s (bypassing the 1 s floor), supports `TimePerGame` (`level` + `time`/`otim`) and `FixedDepth` (`sd`), preserves TT across moves via `usermove` after `new`/`force`/`setboard`, and emits full `post` observer stats (`depth score*100 time_cs nodes [@sel%] pv`).
4. **Direct native C++ Egaroucid harness (`engine=egaroucid-cpp`):**
   - **Why a thin runner:** Stock `Egaroucid_for_Console` has no millisecond `TimePerMove` mode (`command.hpp:220-237` only supports `TimePerGame` in integer seconds and fixed `level`), `gtp_command.hpp` ignores `-time` and does not clear TT on `clear_board`, and `egaroucid_c_api.cpp:263` ignores `time_limit_ms` — even though `Egaroucid-main/src/engine/ai.hpp:297,670` already implements millisecond `iterative_deepening_search_time_limit`;
   - create a standalone ~60-line C++ driver in `Stello.Net/tools/Egaroucid.Runner/` (next to `Egaroucid.Reference/`, zero edits to `Egaroucid-main/`) that `#include`s `Egaroucid-main/src/engine/engine_all.hpp` and speaks a minimal 4-command bitboard stdin/stdout protocol (`init <threads> <hashBits> <resourceDir>`, `clear`, `go <ownHex> <oppHex> movetime <ms> | gametime <ms> | level <N> | solve`, `quit`), returning `bestmove <sq> score <v> depth <d> prob <p> end <0|1> nodes <n> ms <t>`;
   - add a persistent per-worker subprocess `IMatchPlayer` (`engine=egaroucid-cpp`) and `solve-bench`/`mid-bench` support in `Stello.Benchmark`.
5. **Prefix-free, `v5`-balanced match start suite (`starts-v6.txt` & `make-starts --prefix-free`):**
   - **Why upgrade:** Legacy `Stello.Net/tools/Stello.Benchmark/Data/starts.txt` (590 positions, plies 10–12) was filtered by Stello 1.0's evaluation (`|v1| <= 40`). Under Stello 5.0's 60-second searches, `93 / 590` (`15.8 %`) are unbalanced by `> ±4.00` discs (up to `±9.00` discs, producing `76–100 %` automatic `1–1` color sweeps at 2 s), `363 / 590` (`61.5 %`) have their 1-ply parent or 2-ply grandparent also in `starts.txt` (`221` linked directly by Stello 5.0's #1 best move, causing `21–27 %` duplicate games in 1t matches), and `418 / 590` were pruned from the `v5` opening book.
   - **How to build `starts-v6.txt` (`MatchCommands.MakeStarts` in `Stello.Net/tools/Stello.Benchmark/MatchCommands.cs`):**
     1. Add `--prefix-free` to `Stello.Benchmark make-starts`: after selecting all canonical book lines from `Stello.Net/Book/opening-book.txt` within `--plies <from-to>` whose 60-second `v5` best-move score satisfies `|value| <= --max-value` (in `1/100` disc units), remove any candidate whose key is a move-prefix (ancestor) of another candidate in the set (keeping the deepest qualifying node along each branch so **0 %** of starts lie on the same ancestor–descendant line).
     2. Generate `Stello.Net/tools/Stello.Benchmark/Data/starts-v6.txt` from the 6,000-position `v5` master book using `--plies 10-14 --max-value 220 --prefix-free` (**`587` starts / `1,174` games**, matching the ~590-pair scale of `starts.txt`; or `--max-value 200` for **`528` starts / `1,056` games** within `±2.00` discs), shuffled deterministically with `new Random(1)` so `--max-starts 300` (`600` games) mixes plies 10–14 and opening families evenly.
     3. Keep `Data/starts.txt` untouched for `BenchmarkTests.cs` (`StartFile_HoldsDistinctLegalLines`, which asserts plies 10–12) and historical 2.0–5.0 cross-checks; copy `Data/starts-v6.txt` to the benchmark output directory (`Stello.Benchmark.csproj`), use `starts-v6.txt` for all Stello 6.0 gate and final matches, and add a unit test in `BenchmarkTests.cs` verifying that every position in `starts-v6.txt` is legal, canonical-unique, within `±maxValue`, in `opening-book.txt`, and strictly prefix-free.
6. **Initial `starts-v6.txt` historical & external baselines (all `587` starts = `1,174` games per match, `book=off`, on user's go):**
   - To give a clean view of Stello's development across each distinct algorithmic generation on `starts-v6.txt` (plus the real native external targets for 6.0), record full `1,174`-game baseline matches (`all 587 starts`, no `--max-starts` truncation; multi-threaded `8t` runs only for engines that support multi-threading):
     - **Historical Stello lineage (`Stello 5.0` vs earlier distinct generations on `starts-v6.txt`):**
       - vs **`Stello 4.0` (`engine=stello-v4`, C++20):** `1t` (`--workers 8`) and `8t` (`--workers 1`) *(represents the 3.0/3.1/4.0 `plus15 × 10` search-and-eval generation at C++20 speed, making separate `stello-v3` matches redundant)*
       - vs **`Stello 2.0` (`engine=stello-v2`, C#):** `1t` only (`--workers 8`; single-threaded C# pattern/solver engine)
       - vs **`Stello 1.0` (`engine=stello-v1`, frozen C# port of the original 1992 C++ engine, `V1MatchPlayer`):** `1t` only (`--workers 8`; single-threaded engine)
     - **External reference engines (`Stello 5.0` vs native C/C++ external engines on `starts-v6.txt`):**
       - vs **native C++ Egaroucid (`engine=egaroucid-cpp`):** `1t` (`--workers 8`) and `8t` (`--workers 1`) *(replaces the single-threaded C# `egaroucid` port with the real native C++ engine)*
       - vs **native C Edax (`engine=edax`):** `1t` (`--workers 8`) and `8t` (`--workers 1`)
     - Plus same-machine FFO `#40–49` and `#50–59` `solve-bench` timings (`1t` and `8t`) against `stello-v4`, `egaroucid-cpp`, and `edax`.

**Gate P0:** `engine=stello-v5`, `engine=edax`, `engine=egaroucid-cpp`, and `starts-v6.txt` (`make-starts --prefix-free`) pass unit tests and smoke matches with clean subprocess teardown; `v5.0-ref/` JSONs and the full `starts-v6.txt` historical (`v4`, `v2`, `v1`) and native external (`egaroucid-cpp`, `edax`) baseline matches are recorded; all existing test suites pass with 0 warnings.

---

## Phase 1: bug fixing (full codebase audit)

All bugs below were verified directly in source during the pre-6.0 audit. Each confirmed bug gets a targeted fix and a regression test before Phase 2 starts. *(The `BookTrimmer`/`BookCalclib`/`BookExpander` proven-loss boundary and DOE lifecycle bugs found in the same audit were already fixed and unit-tested during Stello 5.0 Phase 6 in commits `2fbd82d` and `fb325e2`.)*

### 1A. C++20 Engine Confirmed Bugs (`Stello.Cpp`)
1. **[C1 — High] YBWC brother-cutoff memory-ordering race (`point.cut` vs `point.stop`):**
   - **Location:** `Stello.Cpp/src/search/ybwc.hpp:21`, `Stello.Cpp/src/search/ybwc_impl.hpp:197-198, 207-208, 233, 255`.
   - **Cause:** Workers write `point.cut.store(true, relaxed)` then `point.stop.store(true, relaxed)` while the parent polls `point->stop.load(relaxed)` and checks `point.cut.load(relaxed)` on abort. Without a happens-before edge, compiler or weak-memory reordering can let the parent observe `stop == true` before `cut == true`, misclassifying a normal YBWC brother cutoff as an external `Abort::Stop` and aborting the entire search prematurely.
   - **Fix:** Merge `stop` and `cut` into a single `std::atomic<uint8_t> state` bitmask (`STOP = 1`, `CUT = 2`) updated atomically via `fetch_or(STOP | CUT, relaxed)` (or pair `store(release)` on `stop` with `load(acquire)` in `SplitPoint::stopped()`). Add a multi-threaded stress test.
2. **[C2 — Medium] Lazy SMP endgame helpers (`ybwc = false`) drop selective endgame state:**
   - **Location:** `Stello.Cpp/src/search/engine_impl.hpp:208-214, 304`.
   - **Cause:** `solve_iteration` creates Lazy SMP helper searchers without copying `s.end_mpc_stage`, `s.hash_get_height`, or `s.hash_put_height`. During selective endgame stages (`1/2/4`), helpers run unpruned exact `[-64, 64]` searches instead of assisting `mpc-end`.
   - **Fix:** Capture and copy `end_mpc_stage`, `hash_get_height`, and `hash_put_height` onto each helper before calling `solve_root`.
3. **[C3 — Medium] Lost-position `keep_midgame_move` defeated in Exact solve:**
   - **Location:** `Stello.Cpp/src/search/engine_impl.hpp:31-40, 249-266, 1127-1144`.
   - **Cause:** On a proven loss (`wld.score < 0`), WLD fail-low comparisons promote an unverified losing move (`mv_bad`) to `root_moves[0]`. Although `iterate()` sets `keep_midgame_move = true` to preserve `mv_mid`, Exact `solve_iteration` searches `root_moves[0]` (`mv_bad`) first and `apply_solve_best` immediately overwrites `best.mv` with `mv_bad` when `i == 0` completes — causing an aborted Exact solve to play `mv_bad` and widening Exact's initial window.
   - **Fix:** When `keep_midgame_move` is true, move `best.mv` back to `root_moves[0]` via `detail::move_to_front` before launching Exact `solve_iteration`. (Verify "not worse" at 500 ms if node counts change.)
4. **[C4 — Medium] Permanent deadlock in `Scope::try_spawn` on allocation failure (`std::bad_alloc`):**
   - **Location:** `Stello.Cpp/src/search/pool.hpp:200-220, 306-312`, `Stello.Cpp/src/search/engine_impl.hpp:342, 436`.
   - **Cause:** `tasks_->running` is incremented before throwing `UniqueTask` / `deque::push_back` allocations; an exception leaves `running > 0` forever and wedges `WaitOnDrop::~WaitOnDrop()`. Also, `StopHelpers` in `solve_iteration` and `lazy_smp_midgame` is constructed *after* the spawn loop, leaving spawned helpers running on mid-loop exception.
   - **Fix:** Construct `UniqueTask` and push to `jobs` before incrementing `running` / decrementing `idle` (rolling back on failure), and construct `StopHelpers` before the spawn loops.
5. **[C5 — Medium] Signed `int64_t` overflow UB on extreme `time_ms`:**
   - **Location:** `Stello.Cpp/src/search/time.hpp:27, 47-48, 54, 67`, `Stello.Cpp/src/search/engine_impl.hpp:230`.
   - **Cause:** Large `time_ms` or `INT64_MIN` passed through `SearchLimits::from_parts` / FFI overflows signed arithmetic (`* 2 / 3`, `3 * move_time * (64 - slow_empties) / 256`, `time_ms - elapsed_ms`, `hard_ms * 4`), triggering UBSan and corrupted negative/wrapped limits.
   - **Fix:** Clamp `time_ms` ceiling (e.g., `INT64_MAX / 1024`), guard `limits.time_ms() <= elapsed_ms` before subtracting in `for_endgame`, and guard `budget.hard_ms * 4`.
6. **[C6 — Low] WASM bridge input validation & worker preparation edge cases:**
   - **Location:** `Stello.Cpp/src/wasm/wasm_exports.cpp:170, 195, 268, 275-285`, `Stello.Cpp/src/wasm/protocol.cpp:355-356`.
   - **Cause:** (a) `req_ptr == nullptr && req_len > 0` constructs a `string_view` over `""` of length `req_len` (buffer over-read), and `has_eval != 0 && eval_ptr == nullptr && eval_len > 0` is unguarded; (b) `stello_wasm_worker_prepare` ignores `pthread_create` / `pthread_getattr_np` failure and briefly aliases `g_main_pthread` if booted concurrently with main-thread init; (c) negative `TimeTicks` in `[-9999, -1]` truncate to `0` ms and pass `TimePerMove`.
   - **Fix:** Reject null-pointer + non-zero-length spans, check `pthread_create` / `pthread_getattr_np` return values (propagating `0` to throw in `stello_wasm.js`), and reject `time_ticks < 0` for `TimePerMove`.

### 1B. .NET Engine, Opening Book, Apps, Web & CLI Confirmed Bugs (`Stello.Net`)
1. **[N1 — High] `MainViewModel.StopAsync()` re-entrancy allows concurrent native `SearchEngine.Search()` calls:**
   - **Location:** `Stello.Net/Stello.App/ViewModels/MainViewModel.cs:651-657, 742-824`.
   - **Cause:** Two different commands (or `HandleTimeoutAsync`) awaiting `StopAsync()` while search `T1` is running both resume sequentially once `T1` ends and launch overlapping `RunAsync()` loops (`T2` and `T3`), invoking native `SearchEngine.Search` concurrently on the same `StelloEngine*` (UB/crash) or clobbering `WebEngineHost._progress`.
   - **Fix:** Loop `while (!Idle.IsCompleted) { _cancel?.Cancel(); await Idle; }` in `StopAsync()` and serialize all game-state-transitioning async actions with a single re-entrancy gate (`SemaphoreSlim(1, 1)`).
2. **[N2 — High] `MainViewModel.SetGameModeCommand` stays enabled during `IsLearning`:**
   - **Location:** `Stello.Net/Stello.App/ViewModels/MainViewModel.cs:95-101, 265-292`, `Stello.Net/Stello.Net/MainWindow.xaml:81-88`.
   - **Cause:** Missing `CanExecute = nameof(CanChangeGame)` allows starting a game (reading `OpeningBook._positions`) while `BookLearner` mutates `OpeningBook` on a background thread.
   - **Fix:** Add `CanExecute = nameof(CanChangeGame)`, `[NotifyCanExecuteChangedFor(nameof(SetGameModeCommand))]`, and an `if (IsLearning) return;` guard.
3. **[N3 — High] `LocalStorageBookStore.Load()` unhandled `JSException` crashes Blazor WebAssembly startup:**
   - **Location:** `Stello.Net/Stello.Web/Services/LocalStorageBookStore.cs:14-31`, `StartupBook.cs:15-28`, `Program.cs:52`.
   - **Cause:** `localStorage.getItem` throws `JSException` (`SecurityError`) in strict privacy / blocked-storage browsers and is not caught (unlike `LocalStorageSettingsStore.Load`), aborting startup before `host.RunAsync()`.
   - **Fix:** Catch `JSException` in `LocalStorageBookStore.Load()` and return `null` so startup falls back to `data/OPENING.bin`.
4. **[N4 — High] `WebEngineHost` ignores `cancellationToken` during 300 ms MoveNow grace window & `WaitForNaturalDelayAsync` checks `moveNow` before `cancel`:**
   - **Location:** `Stello.Net/Stello.Web/Services/WebEngineHost.cs:98-145`, `Stello.Net/Stello.App/ViewModels/MainViewModel.cs:776-807, 828-845`.
   - **Cause:** Cancelling immediately after triggering Move Now fails to abort `ChooseMoveAsync` and bypasses the cancel check in `WaitForNaturalDelayAsync`, playing a cancelled move on `_game`.
   - **Fix:** Pass `cancellationToken` into the 300 ms wait (signalling `2` on cancel), call `cancellationToken.ThrowIfCancellationRequested()` before returning in `WebEngineHost` (restarting the worker if needed), and check `cancel.ThrowIfCancellationRequested()` first in `WaitForNaturalDelayAsync`.
5. **[N5 — High] Uncorrelated `progress` messages in `Stello.Web` + unvalidated move in `EngineProtocol.MoveNowResult`:**
   - **Location:** `Stello.Net/Stello.Web/wwwroot/js/engine-worker.js:24-26`, `engine-client.js:47-50`, `WebEngineHost.cs:102-106`, `EngineProtocol.cs:94-107`.
   - **Cause:** Late `progress` messages from Search #1 lack a request `id` and can overwrite Search #2's `last` `SearchInfo`; if Move Now falls back to `MoveNowResult`, it returns `last.BestMove` without checking `(legal & sq.Bit) != 0`, throwing `InvalidOperationException` in `_game.Play`.
   - **Fix:** Tag `progress` messages with search request `id`, ignore stale IDs in `engine-client.js` / `WebEngineHost`, and validate move legality in `EngineProtocol.MoveNowResult`.
6. **[N6 — Medium] `BookLearner.AddGame` accepts `Move.Pass` on game-over boards (enabling book corruption & 2-cycle `StackOverflowException`):**
   - **Location:** `Stello.Net/Stello.Engine/BookLearner.cs:61-87, 249-261`.
   - **Cause:** `BookLearner.Apply` omits `&& board.HasLegalMove(player.Opponent())` when validating `Move.Pass` and mutates `_book` before validating later moves.
   - **Fix:** Validate all plies upfront using `OpeningBook.IsLegal(board, player, move)` before mutating `_book`.
7. **[N7 — Medium] `MainViewModel` clock & timeout bugs:**
   - **Location:** `Stello.Net/Stello.App/ViewModels/MainViewModel.cs:424-443, 501-523, 756, 787-795, 968-976, 990-1063`.
   - **Cause:** `TickClock()` runs during `IsLearning`; an in-flight search overwrites clocks reset via `EditSettingsCommand`; `_hasHumanContinuedOnTime` wipes restored clocks after `Undo` and wipes the human's new color after `SwitchSides`; early `return` on timeout leaves `0:01` displayed during the prompt.
   - **Fix:** Guard `IsLearning` in `TickClock()`, add a `_clockGeneration` counter checked before writing `timeLeft` back, track continued-on-time per `Player` (cleared in `RestoreClock()` when restored time > 0), and raise clock property notifications before calling `HandleTimeoutAsync`.
8. **[N8 — Medium] `LocalEngineHost.LearnAsync` saves an un-minimaxed, unsorted `OpeningBook` when stopped (and ignores `HashBits`/`Threads`):**
   - **Location:** `Stello.Net/Stello.App/Services/LocalEngineHost.cs:37-79`, `MainViewModel.cs:641-643`.
   - **Fix:** Call `learner.Minimax()` before `bookStore.Save(book)` when stopped, and pass `Settings.HashBits` / `Settings.ThreadsFor(...)` into `LearningLimits()` and `CreateLearner`.
9. **[N9 — Medium] `Stello.Benchmark` `mid-quality` silently ignores `threads=`:**
   - **Location:** `Stello.Net/tools/Stello.Benchmark/QualityCommands.cs:150-183`.
   - **Fix:** Apply `sides[e].Limits.Threads` and `sides[e].HashBits` to each side's `SearchLimits` inside the worker loop.
10. **[N10 — Medium] `Stello.Trainer` `mpc --to 56` crashes with `FixedDepth(0)`:**
    - **Location:** `Stello.Net/tools/Stello.Trainer/MpcCommand.cs:25-26, 61-73`.
    - **Fix:** Cap `--from` / `--to` at `55`.
11. **[N11 — Medium] `Stello.Trainer` `report` puts 0-empty (64-disc) positions into `"1-5"` bucket (`-1 / 5 == 0`):**
    - **Location:** `Stello.Net/tools/Stello.Trainer/ReportCommand.cs:39-40, 73-74`.
    - **Fix:** Filter `samples[i].Discs < 64` before grouping.
12. **[N12 — Low] `MatchSide.NewPlayer()` silently drops `MatchSide.Options`:**
    - **Location:** `Stello.Net/Stello.Engine/BookMatch.cs:111-121`.
    - **Fix:** Pass `Options` to `new StelloMatchPlayer(HashBits, Book, Options)`.
13. **[N13 — Low] `BrowserDialogService.Beep()` unguarded `AudioContext` throws `JSException`:**
    - **Location:** `Stello.Net/Stello.Web/Services/BrowserDialogService.cs:48`, `Stello.Net/Stello.Web/wwwroot/js/stello.js:13-22`.
    - **Fix:** Guard `AudioContext` in `stello.js` and catch `JSException` in `BrowserDialogService.Beep()`.

### 1C. Defensive & Latent Hardening (14 items)
- **C++ (`Stello.Cpp`):**
  1. Replace `std::memset` on live `std::atomic<uint64_t>` slots during generation-wrap clear (`src/search/table.cpp:32`) with atomic zero stores.
  2. Wrap `std::array<uint16_t, MAX_PATTERN_FEATURES>` in an `alignas(32)` struct (or use `loadu`/`storeu`) in `src/search/search_v3.cpp:125-162` so aligned `vmovdqa` cannot rely on manual caller `alignas(32)`.
  3. Replace invalid `{weights, SIZE_MAX}` span in non-x86-64 `Direct::sum_pext` fallback (`src/eval/patterns_bmi2.cpp:29`) with pointer or real span length.
  4. Return `STELLO_INVALID_ARGUMENT` when `eval == nullptr && eval_length > 0` in `stello_engine_create` (`src/ffi/stello_ffi.cpp:88`).
  5. Reset the old engine before allocating a new one on WASM `hash_bits` resize (`src/wasm/wasm_exports.cpp:180-181, 206-208`) to avoid transient 2× TT OOM inside WASM's 2 GiB cap.
  6. Make `set_portable_only` race-free against first-time `detect()` (`src/core/cpu.cpp:133-159`).
- **.NET / Web / CLI (`Stello.Net`):**
  7. Check `StelloNative.CreateSignals().IsInvalid` and eagerly dispose `engine` on `SetYbwc` failure (`Stello.Engine/SearchEngine.cs:206-214`).
  8. Add `_disposed` guard and null-`SynchronizationContext` safety to `_clockTimer` callbacks (`Stello.App/ViewModels/MainViewModel.cs:222, 978-988`).
  9. Stop active games before awaiting confirmation dialogs in `AddGameToBook()` (`Stello.App/ViewModels/MainViewModel.cs:532-564`).
  10. Wrap `JSException` in `IOException` inside `BrowserGameFileService.SaveAsync()` (`Stello.Web/Services/BrowserGameFileService.cs:43`).
  11. Clamp 95 % score confidence intervals to `[0, 1]` (`Stello.Engine/BookMatch.cs:70`, `tools/Stello.Benchmark/MatchReport.cs:78-79, 107`).
  12. Fix backwards error text (`"is only for engine=egaroucid"` → `"engine=stello"`) in `tools/Stello.Benchmark/MatchCommands.cs:329-342`.
  13. Guard `ms > 0` before dividing `oldMs / ms` in `tools/Stello.Benchmark/BenchCommands.cs:256-259`.
  14. Deterministic `IDisposable` cleanup in `EngineMatch.Work()` and `BenchSearcher.NativeReference` (landed in Phase 0).
- *(Note on selective-endgame TT eviction flaw: `endgame/search_impl.hpp:180,299,341,464` sets `store_depth = s.end_mpc_stage` (`1/2/4`), letting `empties = 14` evict `empties = 28` in `slot[0]`. This is resolved structurally by composite `level = (depth << 4) | mpc_stage` in Phase 4's Unified TT.)*

**Gate P1:** Regression tests added for all 19 confirmed bugs; all 5 C++ presets (`clang-release`, `clang-native`, `msvc-release`, `msvc-native`, `clang-sanitized`), `.NET` test suites, and WASM Node tests pass with 0 warnings; if C3 alters search parity, a 500 ms match confirms "not worse" and signatures are refreshed.

---

## Phase 2: speed (no change in nodes)
Every step in Phase 2 must remain **100 % node-, move-, and score-identical** to Phase 1 on 1 thread across all parity suites. Each step is benchmarked independently and kept only if it speeds up `mid-bench`, `solve-bench`, and/or WASM.

1. **Single ternary `PatternState` (`uint16_t features[64]`, 128 B = `4 × ymm`)** *(inspired by Edax `eval.c:844-878`)*:
   - **Why:** `Stello.Cpp/src/eval/patterns.hpp:133-134` currently maintains two 64-element arrays (`black[64]` + `white[64]` = 256 B = `8 × ymm`), forcing `advance_eval_state` (`search_v3.cpp:74-113`) to update 8 AVX2 registers per move and `sum_eval` (`search_v3.cpp:21-51`) to compute `2 * opp + own`, widen `uint16 → uint32`, and write a 256-byte `indices[64]` stack array.
   - **What:** Replace `(black, white)` with a single ternary `alignas(32) uint16_t features[64]` (`4 × ymm`, `8 × v128` in WASM) — halving incremental update instructions and YBWC copy size — and benchmark zero-extra-RAM color mirroring (`max_idx - idx` in 4 `vpsubw` ops for White-to-move) against Edax's dual color-flipped stage tables with baked offsets.
2. **Compact 64 KB AVX2 stability engine** *(Edax `board.c:75,1108-1148` + Egaroucid `stability.hpp:164-277`)*:
   - **Why:** `Stello.Cpp/src/endgame/stable.cpp:21-22` allocates **1,048,576 bytes (1 MB)** for two `uint64_t[65536]` edge tables (thrashing L1/L2) and compiles *outside* the AVX2 TU (`CMakeLists.txt:54, 89`) using scalar `full_diagonals` and scalar 4-direction propagation.
   - **What:** Replace the 1 MB tables with Edax's single **64 KB** `uint8_t EDGE_STABILITY[256 * 256]` table (16× smaller), extract columns via SSE2/AVX2 `_mm_movemask_epi8`, vectorize diagonal full-lines and the 4-direction interior stability loop inside `Kernel<true>`, and expose fast `edge_stability` / `corner_stability` helpers for Phases 4–5.
3. **Midgame redundant-work elimination & fused `get_moves_and_potential`** *(Edax `board.c:955-978` + Egaroucid `midsearch.hpp:227`)*:
   - **Why:** In `search_inc` (`Stello.Cpp/src/search/engine_impl.hpp:718, 770, 871`), `flips()` is computed up to **3× per move** (ETC, `order_by_evaluation`, and main search loop), and `order_by_evaluation` computes `replies_bb = legal_moves(child_own, child_other)` (`line 874`) and discards it, forcing every child `search_inc` (`line 651`) to recompute `legal_moves`.
   - **What:** Store `flips` and `replies_bb` in a small stack move list (passing `replies_bb` down into child `search_inc`), fuse AVX2 `legal_moves` + `potential_mobility` (`get_moves_and_potential` reusing the first-step shift vectors), and upgrade scalar `core/bits.hpp:40-66` from 6-step sequential shifts to 1-stage parallel prefix for portable/WASM speed.
4. **Endgame `last1..last4` micro-optimizations** *(Edax `count_last_flip_bmi2.c` + Egaroucid `endsearch_nws_last_simd.hpp`)*:
   - pre-double `LAST_FLIP_COUNTS` (`2 × flips`) and drop the redundant `own &= ~(1ULL << square)` (`endgame/last_impl.hpp:79`);
   - fix `solve_last` (`endgame/search_impl.hpp:1016-1031`) to use `last_flip_count` instead of full `flips() + final_score()`;
   - benchmark BMI2 `_pext_u64` `last_flip_count` (3 `pextq` + 1 row shift on Zen 4's 1-cycle `pext`), AVX2 multiply-free lazy high/low-cutoff `last1_nws` (preserving exact Phase 1 node counts; dropping redundant pass node-count moves to Phase 5), packed `__m128i OP` in `last4..last1`, and moving abort checks out of `last4..last1` up to `end_fast`/`end_local`.
5. **Optional `x86-64-v4` 256-bit AVX-512VL/CD/DQ runtime dispatch (`search_v4.cpp`, zero `zmm` registers!)** *(Edax `flip_avx512cd.c` + Egaroucid `flip_avx512.hpp`)*:
   - **Why:** Zen 4 (Ryzen 7 7800X3D) and Intel Ice Lake+ execute 256-bit EVEX AVX-512 instructions with zero frequency penalty; `vplzcntq` (`AVX512CD+VL`) replaces the 3-step lower-ray shadow fill and `vpternlogq` compresses upper-ray flip logic.
   - **What:** Extend `stello::cpu` (`has_avx512_v4()`, `--cpu portable|v3|v4|auto`), add `Stello.Cpp/src/search/search_v4.cpp` compiled with `-mavx512f -mavx512vl -mavx512cd -mavx512dq -mavx512bw -mprefer-vector-width=256`, implement `flip_avx512cd`, LUT-free `count_last_flip_avx512cd`, 128-bit AVX-512DQ+VL stability, and hardware `CRC32C` hashing (`_mm_crc32_u64`, also benchmarked in `v3`/SSE4.2); verify `zmm = 0` via `llvm-objdump -d`.
6. **Clang PGO (Profile-Guided Optimization) & targeted TT prefetching** *(Stockfish / Edax-AVX)*:
   - benchmark Clang instrumentation PGO (`-fprofile-instr-generate` / `-fprofile-instr-use`) trained on representative midgame + FFO workloads for `stello_engine.dll` and `stello-cpp.exe`;
   - add targeted TT prefetching (`_mm_prefetch`) before probes in midgame and endgame.

**Gate P2:**
- 100 % node-, move-, and score-identical to Phase 1 on all parity suites (`--cpu portable`, `--cpu v3`, and `--cpu auto`/`v4`);
- measurably faster in `mid-bench`, `solve-bench`, and Node WASM (`19/19` tests green, 0 compiler warnings, `zmm = 0` in `v4`).

---

## Phase 3: threads
Directly targets closing the multi-thread scaling & node-inflation gap against Egaroucid (`5.27×` @ 8t / `7.07×` @ 16t with `+6–17 %` extra nodes vs Stello 5.0's `4.88×` / `5.69×` with `+27–38 %` extra nodes).

1. **Fix the 4 verified threading bottlenecks (zero algorithmic risk):**
   - **Guard splits with `pool->idle() > 0` before entering `Kernel::split()`:** Currently `s.may_split()` (`engine.hpp:271`) ignores `idle()`, so every node with `empties > 14` or `look >= 6` eagerly constructs child arrays, heap-allocates `std::make_shared<ScopeTasks>` (2 `std::mutex` + 1 `std::condition_variable`), zero-initializes 64 atomics, and locks `running_mutex` twice even when all 15 workers are busy;
   - **Pass incremental `PatternState` (128 B) in `MidgameChild`:** Eliminate the from-scratch `state_of()` call in `child_null_window` / `child_window` (`ybwc_impl.hpp:57, 74`);
   - **Remove shared `node_sink->fetch_add` every 1,024 nodes (`engine.cpp:481`):** Keep `s.nodes` strictly thread-local during search (~490k cross-core atomic RMWs/s eliminated at 502M NPS) and accumulate only on task finish / ~100 ms heartbeat;
   - **Decouple nested locks in `pool.hpp:171-174`:** Release `queue_mutex` before calling `scope->finish()` (`running_mutex`) so 16-thread completions never serialize both locks together.
2. **Dynamic `SplitNode` + Helpful Master work-stealing** *(Edax `ybwc.c` + Egaroucid `ybwc.hpp`)*:
   - replace per-wave `make_shared<ScopeTasks>` / `make_unique` / static wave barriers with a stack/pool-allocated `SplitNode` where helpers dynamically pull remaining moves, observe live `alpha`, re-search PV fail-highs inline, and only abort siblings on true `score >= beta` cutoffs (instead of aborting all brothers and launching a new wave on every null-window fail-high);
   - implement **Helpful Master**: instead of sleeping in `cv.wait_for(1ms)` (`pool.hpp:99`) while waiting for child tasks, let the waiting parent execute queued sibling/descendant tasks using pre-allocated per-thread `ThreadState` (zero `malloc` during help).
3. **Split criteria, Stockfish-style Lazy SMP diversification, & ABDADA spike:**
   - tune `SPLIT_MAX_SLAVES` (e.g., `3–4`, Edax `settings.h:186`) and `SPLIT_MIN_MOVES_TODO` (e.g., `2–4`) at 8t and 16t;
   - test Stockfish-style **Lazy SMP helper diversification** (staggered skip-depth schedules, perturbed sibling ordering, or slightly varied ProbCut `t` on helper threads) vs current identical-depth Lazy SMP in midgame;
   - investigate **ABDADA** (lock-free TT `evaluating` counter/flag to avoid duplicate sibling work without split locks) for 16t SMT endgame solving if YBWC still trails Egaroucid's `7.07×` at 16t;
   - *(Follow-up scheduled in Phase 5: re-tune `YBWC_END_MIN_EMPTIES` (`14` vs `15`/`16`) and desktop `Threads (Automatic)` once Phase 4's Unified TT and Phase 5's pre-warming reduce Cut-node splitting.)*

**Gate P3:**
- FFO `#40–49` and `#50–59` are faster with lower node inflation at 8t and 16t, and `mid-bench d16` scales cleanly at 8t/16t;
- 8-thread matches against Phase 2 (`--workers 1`) pass at 500 ms and 2 s;
- `clang-sanitized` (ASan + UBSan) and 8t/16t stress tests pass with 0 findings.

---

## Phase 4: search (Unified TT, Midgame Search & Dynamic Time Management)
Each item sits behind a temporary `SearchOptions` / `opt=` switch, is profiled on `mid-bench`, and must pass a 1,180-game 500 ms match (confirmed at 2 s, 600 games). **Unified TT comes first** because it unlocks both Phase 4 search improvements and Phase 5 endgame breakthroughs.

1. **Unified cache-dense Transposition Table + dedicated `pv_table`** *(Egaroucid `transposition_table.hpp` + Edax `hash.c` + Stockfish TT)*:
   - **Why:** Stello 5.0 splits RAM 50/50 into `midgame_table_` and `endgame_table_` (each 1M entries at `hash-bits=19`, 32 B/entry effective), stores only ONE bound + ONE move, unconditionally evicts `slot[1]`, silently drops concurrent reads on seqlock contention, and tags selective endgame writes with `store_depth = 1/2/4` (allowing `empties = 14` to evict `empties = 28`).
   - **What:**
     - merge `midgame_table_` and `endgame_table_` into **one unified 3-way or 4-way 24-byte-entry table** (~2.7M entries in the same 64 MB L3 budget on the 7800X3D — doubling effective endgame capacity and warming endgame directly from midgame/ID);
     - store **dual `[lower, upper]` bounds** (tightening bounds across repeated NWS, aspiration, and selective-to-exact passes), **composite `level = (depth << 4) | mpc_stage`** (structurally fixing selective-endgame eviction), **two moves `move[0..1]`** (re-evaluated cleanly inside the dual-bound unified table), and **`cost = log2(nodes)` + generation/age replacement + LRU refresh on read**;
     - use **lock-free pre-check before locking** on both reads (`board == key`) and writes (`existing_level <= level`) with brief per-entry spin on hit (eliminating dropped parallel TT hits without hurting L3 throughput);
     - add a dedicated small **`pv_table`** (`hash_size >> 4`, Edax `search.h:70`) protecting PV / near-root nodes from flooding, and stop writing midgame TT at `look == 0` (depth-1 leaf thrashing).
2. **Try `hash_move` before `order_by_evaluation`, dedicated NWS `eval1`/`eval2`, & node-type-aware shallow ordering** *(Egaroucid + Edax)*:
   - in `search_inc` (`engine_impl.hpp:732`), search `hash_move` (and `move[1]`) **before** calling `order_by_evaluation` (skipping sibling ordering on the ~80–90 % of Cut-nodes where `hash_move` fails high; alters counted ordering nodes while keeping the tree identical);
   - split PV (`nega_scout`) and NWS (`search_nws`), adding specialized `eval1_nws` / `eval2_nws` (static cell priority, no TT writes, no O(N²) response sorting);
   - test Edax/Egaroucid **node-type-aware shallow midgame ordering** at deep nodes (`depth ≥ 12–16`: PV `depth >> 3`, NWS `depth >> 4`, `PV/ALL/CUT` depth offsets, wipeout priority, `W_HASH` child-TT bonus, cheap 64 KB `edge_stability` / `corner_stability`, and omitting `potential_mobility` in NWS).
3. **Midgame ETC refinement, non-PV recursive ProbCut, & `+2`-ply ID / aspiration / PV extension** *(Edax + Egaroucid)*:
   - tune `MIDGAME_ETC_MIN_LOOK` (`look ≥ 4` vs Egaroucid's `depth ≥ 16`), store parent TT cutoff on ETC hit (Edax `search.c:1143-1152`), and raise `alpha` / skip fail-low children in ETC (Egaroucid `transposition_cutoff.hpp:169-182`);
   - test **non-PV-only ProbCut**, **recursive `probcut_level < 2`**, permissive `eval_0` pre-filter (Edax `midgame.c:190-218`), depth-3 static ProbCut (Egaroucid), and refitted cubic/per-pair MPC error models on the unified TT;
   - re-test **`+2`-ply midgame ID steps + narrow root aspiration** (now supported by dual `[lower, upper]` TT bounds + parity-consistent steps, addressing why 5.0 Phase 3 rejected aspiration on a single-bound TT) and **PV extension** into exact endgame (Edax `midgame.c:591`).
4. **Stockfish-inspired modern alpha-beta techniques (adapted & vetted for Othello):**
   - **Dynamic time management** *(Stockfish `timeman.cpp` + Edax `search.c:669-677` + Egaroucid `ai.hpp:359-412`)*: track root `bestMoveChanges` instability across iterations, extend `TimePerGame` time on fail-low / score drop, and add Egaroucid's root early-exit check when the best move stays stable and all alternatives fail low by `≥ 4–5` discs (far more adaptive than 5.0's rejected flat `soft-pct` sweep);
   - **Internal Iterative Reductions (IIR)** *(Stockfish `search.cpp`)*: reduce depth by 1 on Cut/PV nodes (`depth ≥ 6–8`) lacking a TT move (cheaper than IID, which Stello 2.0 rejected);
   - **Correction History** *(Stockfish 17–19)*: maintain a small per-bucket `(side, corner/edge/mobility hash)` table tracking `search_score - static_eval` residuals to correct systematic local horizon/positional bias in leaf evaluations and ProbCut pre-checks;
   - **Conservative CutNode LMR + Continuation/Countermove History** *(Stockfish)*: test a gentle `R = 1` reduction on late non-corner/low-eval moves at deep `CutNode`s (`depth ≥ 8`, `moveCount ≥ 6–8`) with full-depth re-search on fail-high — explicitly noting that Othello's large disc-flip volatility makes aggressive chess LMR dangerous, so only a strictly verified win is kept;
   - **Root move cost sorting** *(Edax `root.c:429`)*: order non-PV root moves by subtree node count from the previous iteration;
   - *Explicitly excluded after game-theory review:* **Null-Move Pruning** (passing/zugzwang is often advantageous in Othello, violating the null-move observation assumption) and **aggressive chess LMP / static futility pruning** (single moves flip many discs and swing mobility sharply; Multi-ProbCut already provides Othello's calibrated statistical pruning).

**Gate P4 (per item):** 1,180-game 500 ms match lower bound > 50 %, confirmed at 2 s (600 games). Remove each temporary `opt=` switch immediately upon acceptance or revert cleanly on rejection.

---

## Phase 5: the endgame (closing the 2.21× FFO node gap)
Systematically targets all 7 diagnosed causes of the `1,115.7M` vs `505.4M` FFO `#40–49` node gap against Egaroucid while improving timed-match endgame transition strength.

1. **Warm Unified-TT ID + selective MPC pre-search + narrow even-parity aspiration in `Solve` mode (Causes #1 & #5 — biggest FFO win):**
   - **Why:** In `TimeControlMode::Solve` (`engine_impl.hpp:182-267`), Stello runs only 4 plies into a separate `midgame_table`, skips all selective MPC stages, enters WLD `[-1, 1]` with a 100 % cold `endgame_table`, and then re-searches the entire `[0, 65]` or `[-65, 0]` half-range without aspiration.
   - **What:** Run unified-TT ID (`MPC 74 %`) up to `empties - 10`, then ascending selective endgame passes (`88 % → [93 % → 98 %]`), then direct exact solve clamped by root `stability_bound` with **even-parity narrow aspiration `[est - 2, est + 2]`** (Edax `root.c:478-585` / Egaroucid `ai.hpp:254-270`). Add even-alpha rounding (`alpha += (alpha & 1)`) in exact NWS and skip TT lookup/store on forced 1-move endgame nodes (Edax `endgame.c:532, 658`).
2. **Shared TT down to 11 empties + zero-bundle `11–13` empty move ordering (Causes #2 & #3):**
   - test extending the shared unified TT down to `11–13` empties (Edax `NWS_endgame`, replacing the thrashing 1,024-entry `LocalTable` at `11–13` while keeping `LocalTable` for `7–10`);
   - benchmark two zero-extra-file `11–13` empty move-ordering upgrades head-to-head on FFO `#40–59` AND 500 ms / 2 s timed matches:
     - **(A) Zero-file incremental 4-pattern `end_local` ordering** *(Egaroucid's `-18–21 %` node reducer without Stello 5.0's rejected overhead)*: carry 4 corner/edge patterns (`1 × ymm` = 16 `uint16_t`s) incrementally down from `end_deep` and index **already-embedded `egaroucid16` stage 51–53 weights** (0 extra bytes in desktop/WASM, 0 from-scratch `state_of_local`);
     - **(B) Zero-eval `movelist_evaluate_fast`** *(Edax `move.c:300-350`)*: wipeout + `hash_move[0..1]` + `SQUARE_VALUE` + parity + fused `get_moves_and_potential` + `get_corner_stability` + `get_weighted_mobility`.
3. **Upgrade `ordering_value` & try `hash_move` before ordering in `end_window` (Cause #4):**
   - try `hash_move` (and `move[1]`) before calling `ordering_value` in `end_window` (`endgame/search_impl.hpp:235`);
   - replace unordered LS1B from-scratch `ordering_value` (`search_impl.hpp:566-611`) with incremental `PatternState` depth-0 (`14–15` empties NWS), static-cell-priority `eval1` (depth 1, `16+` empties NWS), and TT-backed `MPC 74 %` search (depth `≥ 2`, `16+` empties PV), plus `W_HASH` child-TT bonus and `edge_stability`.
4. **ESC, complete ETC, 4-empty stability cutoff, pre-sorted empty list + true 4-bit quadrant parity, & lazy `last1` cleanup (Causes #6 & #7):**
   - add **Enhanced Stability Cutoff (ESC)** in endgame ETC (`alpha ≤ -NWS_STABILITY_THRESHOLD[empties]`, Edax `search.c:1131-1140`), store parent TT cutoff on ETC hit, raise `alpha` & mark fail-low children `done` in `end_window` ETC, and ensure ETC only probes depths actually stored in the shared TT;
   - add **4-empty stability cutoff** (`search_SC_NWS_4`, Edax `endgame.c:328`), **pre-sorted corner-first doubly-linked empty list + incremental 4-bit `parity`** (`empty.h`, `endgame.c:217-474`) for `≤ 6` empties (fixing `lone_empties` missing 3/5/7-empty odd quadrants and A1–H8 bit order), and drop redundant `last1_nws` pass check/node count when `score ≤ alpha` (Egaroucid `endsearch_nws_last_simd.hpp:138-155`).
5. **Re-tune Phase 3 parallel endgame thresholds:**
   - re-measure `YBWC_END_MIN_EMPTIES` (`14` vs `15`/`16`) and desktop `Threads (Automatic)` now that warm unified TT cuts Cut-node splitting sharply.
6. **Game-theory research spike — Othello Sensei Derivative / Best-First Search (`borassi/othello-sensei`):**
   - Hiroki Takizawa (*"Othello is Solved"*, 2023) established that parallel alpha-beta outperforms `df-pn` (proof-number search) for broad Othello solving, so Stello's core solver stays alpha-beta. As an isolated spike, evaluate whether Borassi's uncertainty-guided **derivative / best-first search** improves multi-second deep analysis accuracy or hard WLD proofs compared to alpha-beta, keeping it only if measurements justify its complexity.

**Gate P5:** FFO `#40–59` 100 % exact with dramatic node reduction (targeting `< 650M` 1t nodes on FFO `#40–49` and beating native Egaroucid 7.8/7.9 wall time at 1t/8t/16t using Stello's NPS edge), plus 500 ms and 2 s timed matches pass as in Phase 4.

---

## Phase 6: evaluation (RTX 4080 GPU NNUE / non-linear hybrids, pattern refinements & new data)
Stello 5.0's `egaroucid16 + mobility` × 60 (Format v3) already beats Egaroucid's linear pattern accuracy (`4.97 / 6.74` vs `5.05 / 6.90`). Leveraging the local **NVIDIA GeForce RTX 4080** (optional Python/PyTorch/CUDA trainer alongside `Stello.Trainer`) while keeping `Stello.Cpp` runtime inference **100 % zero external dependencies** (and allowing a desktop/web model split if warranted):

1. **Track A — Squeezing linear pattern evaluation & fresh data (low risk, CPU or CUDA):**
   - implement Egaroucid's **post-training integer-rounding coordinate hill-climb** (`eval_optimizer_cuda.cu:655-736`), **residual prediction clipping** (`[-64, +64]`), and **appearance-capped per-feature Adam LR** (`alpha / min(50, count)`) to remove `--units 16` quantization loss;
   - test richer 10-cell pattern shapes (inspired by Egaroucid `7.7_beta` and **Othello Sensei**'s outermost-edge / `5×2` / `3×3` / diagonal features) and direct corner/edge stability & parity features;
   - generate **fresh Stello 5.0/6.0 self-play & deep-relabeled training data** (16–20 ply midgame + exact endgames, plus WTHOR / GGS tournament games and book-heavy early stages) to reduce Egaroucid-lv11 distribution bias and build an independent held-out test split.
2. **Track B — GPU-trained NNUE & non-linear pattern hybrids (high upside, RTX 4080 + SIMD C++20 inference):**
   - inspired by **`natsutteatsuiyone/neural-reversi`**, **`HaccerKat/Othello-Zero`**, **Stockfish SFNNv9** (`SCReLU` / `CReLU`, `int8`/`int16` quantization, Zen 4 **AVX-512 VNNI `vpdpbusd`** & AVX2 `vpmaddubsw`), and **Takuto Yamana's IEEE ToG paper** (*"Compressing the Evaluation Function With Small-Scale Deep Learning on Othello"*):
     - **Candidate 1 (Desktop strength) — Pattern-Embedding NNUE:** Sparse pattern indices → incrementally updated `int16` accumulator (`H = 64–128`) → `SCReLU` → hidden `L2[16–32]` → phase-bucketed output, capturing global cross-pattern non-linearities (e.g., corner-vs-tempo/mobility synergy) that additive pattern tables cannot express (targeting held-out **MAE `< 4.3–4.5`**);
     - **Candidate 2 (WebAssembly / compact) — Compact Bitboard/Square-Input NNUE:** Incremental accumulator updated directly on placed + flipped squares (`Othello-Zero` / `neural-reversi` style), targeting a **`1–3 MB` brotli** WASM bundle (down from `8.96 MB`!) at equal or better accuracy;
     - **Candidate 3 (Speed + accuracy) — Stockfish-style Dual Evaluator (Fast Pattern / Small Net + Big NNUE):** Use fast linear pattern eval (or small NNUE) for move ordering and lazy far-from-window cutoffs, routing only tight-window leaves (`|eval - beta| < threshold`) to the Big NNUE.
3. **MPC / MPC-end refit & desktop/web packaging decision:**
   - refit `ProbCutErrors` and `EndProbCutErrors` for any winning evaluator;
   - measure WASM brotli download size, `init` latency, linear RAM, and Node/browser Elo to choose between a unified NNUE/pattern file or an explicit desktop (`stello-eval-desktop.bin`) vs web (`stello-eval-web.bin`) split.

**Gate P6:**
- lower held-out MAE and RMSE on both the existing test split and the fresh Stello/WTHOR held-out split;
- 500 ms and 2 s matches against Phase 5 pass (lower bound > 50 %);
- desktop and web first-visit transfer size, init time, and runtime memory are measured and within budget.

---

## Phase 7: the opening book
1. **Varied opening play & play-time guards ([Opening book improvment.md](Opening%20book%20improvment.md) Phase 4 + Edax/Egaroucid):**
   - add a configurable book variety/randomness setting (`0` = always best; `> 0` = uniform or Egaroucid-style softmax temperature sampling among moves within a disc margin of `moves[0]`, strictly never leaving a proven win for an unproven move or stepping into a proven loss when an unproven/drawn/winning move exists);
   - add the **child-transposition fallback** (`TryGetMove` plays a legal move leading into an existing book position when the current position is not in the book);
   - test Egaroucid's optional **unbooked-move NWS verification** (`ai.hpp:612-658`) at long time controls (`≥ 2 s`) so a shallow/stale book entry cannot lose to an unbooked refutation.
2. **`book_feed_hash` TT pre-seeding** *(Edax `book.c:1142-1171`)*:
   - when leaving the book (or analyzing near book lines), pre-seed the unified TT / `pv_table` with reachable book positions so midgame searches transposing back into book lines get free cutoffs and move ordering.
3. **`Stello.BookTool` format & construction upgrades:**
   - store an optional per-node **`bestDeviation` (`Leaf`)** (Egaroucid `book.hpp:84-131` / Edax `book.c:104-119`) so `calclib`/`expand` avoid redundant `onlyMoves` searches across runs and `trim` demotes severed children into parent leaves;
   - add **Umigame's Number `(b, w)`** and **A–F accuracy grades** (Egaroucid `umigame.hpp`, `book_accuracy.hpp`) to `Stello.BookTool stats`;
   - add **hybrid 1t → multi-thread tail scheduling** (Egaroucid `book_enlarge.hpp`) in `recalc`/`calclib`/`expand` so the final `2 × workers` slow positions run multi-threaded instead of leaving cores idle;
   - add **`book fill`** (Edax `book_fill` transposition-bridge discovery) and evaluate error-interval uncertainty priority (`book_enhance`, Edax `book.c:2199-2228`) alongside Lincke DOE.
4. **Build the `v6` master opening book:**
   - bump `OpeningBook.EngineVersion` from `5` to `6`, run `recalc`, `trim`, `calclib`, and DOE `expand` (with optional full-width early plies for human play) using the final 6.0 engine;
   - rebuild `Stello.Net/Book/opening-book.bin` (syncing `Stello.Web/wwwroot/data/OPENING.bin`), run `Stello.BookTool verify` (0 warnings/errors), and confirm web `localStorage` budget.

**Gate P7:** with books enabled, Stello 6.0 with the `v6` book beats Stello 6.0 with the `v5` book at 500 ms; varied-play setting passes unit tests and does not regress strength at margin `0`; `Stello.BookTool verify` passes with 0 warnings/errors; web binary book size and `localStorage` footprint are verified.

---

## Phase 8: the Stello 6.0 release
- **Final matches on all `587` starts (`1,174` games per match, `starts-v6.txt`, `book=off`, each run one at a time on the user's go):**
  - **Historical Stello lineage (`100 ms`, `500 ms`, and `2,000 ms`; `8t` only where supported):**
    - Stello 6.0 vs **Stello 5.0 (`engine=stello-v5`):** `1t` and `8t`
    - Stello 6.0 vs **Stello 4.0 (`engine=stello-v4`):** `1t` and `8t`
    - Stello 6.0 vs **Stello 2.0 (`engine=stello-v2`):** `1t` only
    - Stello 6.0 vs **Stello 1.0 (`engine=stello-v1`, original 1992 C++ logic):** `1t` only
  - **External reference engines (`100 ms`, `500 ms`, and `2,000 ms`; `1t` and `8t`):**
    - Stello 6.0 vs **native C++ Egaroucid (`engine=egaroucid-cpp`):** `1t` and `8t`
    - Stello 6.0 vs **native C Edax (`engine=edax`):** `1t` and `8t`
  - **Endgame `solve-bench`:** FFO `#40–49` and `#50–59` (`1t` and `8t`) head-to-head against Stello 5.0, Stello 4.0, native C++ Egaroucid, and native C Edax.
- **Version 6.0:**
  - bump `Stello.Cpp/CMakeLists.txt` and `Stello.Cpp/src/core/version.hpp` to `6.0.0` (`stello_version() = 60000`), `AboutInfo.cs` to `"Stello Version 6.0"`, and update version/parity tests.
- **Documentation:**
  - write `docs/30-stello-6-0-bugs-speed-threads-search-eval-and-book.md` (following Stello 5.0's `docs/29-stello-5-0-speed-threads-search-and-book.md`) covering Phases 1–7 and the final matches;
  - renumber Glossary & References chapters and update affected Part 1 chapters (`01`–`15`) and [docs/README.md](../docs/README.md), verifying 0 broken links or snippet errors.
- **The release:**
  - fast-forward `main` with the user's OK, tag `v6.0`, monitor the Azure Static Web Apps deploy, and run the live browser check.

---

## Verification summary
| Phase | Check |
|---|---|
| 0 | `engine=stello-v5`, `engine=edax` (`-xboard`), `engine=egaroucid-cpp` (`Egaroucid.Runner`), and prefix-free `starts-v6.txt` (`587` starts) pass unit tests & smoke matches with clean `IDisposable` teardown; `v5.0-ref/` JSONs plus full `1,174`-game `starts-v6.txt` historical (`v4`, `v2`, `v1`) and native external (`egaroucid-cpp`, `edax`) baselines recorded |
| 1 | Regression tests for all 19 confirmed bugs (6 C++, 13 .NET/Web/CLI) + 14 defensive items; all 5 C++ presets (incl. `clang-sanitized`), `.NET` tests, and WASM Node tests pass with 0 warnings |
| 2 | 100 % node-identical to Phase 1 on all parity sets (`--cpu portable`, `--cpu v3`, `--cpu auto`/`v4`, `zmm = 0`); faster in `.NET` FFI (`mid-bench`, `solve-bench`) and WASM |
| 3 | Faster 8t/16t FFO & `mid-bench d16` with lower node inflation; 8t matches beat Phase 2 at 500 ms & 2 s; ASan+UBSan and multi-thread stress tests clean |
| 4–6 | Per item: 500 ms (`1,174`g on `starts-v6.txt`) lower bound > 50 %, confirmed at 2 s (`600`g); FFO `#40–59` 100 % exact & targeting `< 650M` 1t nodes on `#40–49`; lower MAE/RMSE on both test splits; desktop & WASM bundle/RAM within budget |
| 7 | `v6` book beats `v5` book at 500 ms; varied-play tests pass; `Stello.BookTool verify` 0 warnings/errors; web book & `localStorage` size verified |
| 8 | Full `1,174`-game final matches on `starts-v6.txt` vs Stello 5.0 (`1t`/`8t`), Stello 4.0 (`1t`/`8t`), Stello 2.0 (`1t`), Stello 1.0 (`1t`), native C++ Egaroucid (`1t`/`8t`), and native C Edax (`1t`/`8t`); Chapter 30 & Part 1 docs updated; 0 broken links; live site check |

**Tools:**
- `Stello.Benchmark` `mid-bench` / `solve-bench --compare`, `match` (`--resume`, `opt=`, `threads=`, `engine=stello-v5|stello-v4|stello-v3|stello-v2|stello-v1|egaroucid|egaroucid-cpp|edax`);
- `ctest --preset clang-release|clang-native|msvc-release|msvc-native|clang-sanitized --output-on-failure`;
- `dotnet test Stello.Net/Stello.Net.slnx -c Release` (`Stello.Engine.Tests`, `Egaroucid.Engine.Tests`, `Stello.Net.Tests`);
- `node --test Stello.Cpp/tests/wasm/engine.test.mjs Stello.Cpp/tests/wasm/engine-mt.test.mjs`;
- `Stello.BookTool verify|stats|recalc|trim|calclib|expand|match` and `Stello.Trainer train|report|mpc|mpc-end` (+ optional RTX 4080 PyTorch/CUDA trainer).
