# 29 – Stello 5.0: speed, threads, search and book

[Back to the index](README.md)

## In short

With Stello 4.0's C++20 core in place ([chapter 28](28-stello-4-0-the-engine-in-cpp.md)), Stello 5.0 targets **playing strength** across six engineering phases on Windows x64 (**AMD Ryzen 7 7800X3D**, 8 cores / 16 threads), governed by a frozen `Stello 4.0` native reference ([`stello_engine_v4.dll`](../Plans/making-stello-5-0-findings.md#2-preserved-40-reference-engine-stello-v4), `engine=stello-v4`) and logged step by step in [making-stello-5-0-plan.md](../Plans/making-stello-5-0-plan.md) and [making-stello-5-0-findings.md](../Plans/making-stello-5-0-findings.md):

- **Phase 1 — Speed without changing nodes:** Runtime `x86-64-v3` AVX2+BMI2 kernel specialization (`flips_avx2`, `legal_moves_avx2`), incremental `PatternState` indices, Emscripten `-msimd128`, cross-TU `TranspositionTable` inlining, and lock-free `.bss` `stable::of` tables deliver **+36–39 %** midgame throughput and **+35–37 %** FFO #40–49 throughput natively, and **+94–100 %** midgame speed in WebAssembly.
- **Phase 2 — Threads:** Root splitting (`search_root`, `solve_root`), sibling brother cuts (`split_point`), split-wait heartbeats, lowering `YBWC_END_MIN_EMPTIES` from `16` to `14`, scaling `lazy_smp_helpers = (workers + 2) / 4`, and pure YBWC in exact solving lift FFO #40–49 8-thread scaling from `3.67×` to **4.88×** (`3.54 s` at P2; **+30 Elo** at 2 s/move vs Phase 1).
- **Phase 3 — Midgame search experiments (kept vs rejected):** Multi-ProbCut error-model refits (`powergap`/`pair`), TT second-best move (`mv2`), higher soft time limits (`80–100 %`), 4× larger TT (`hash-bits=21` spilling 7800X3D's 96 MB L3 V-Cache), and root clog search all failed to clear Gate P3 and were left out.
- **Phase 4 — Endgame and transition:** `end_local` pattern move ordering was rejected (`48.3 %` at 500 ms), whereas timed selective endgame (`try_end_probcut`, `74 -> 88 -> 98 %`) plus `TimePerGame` `estimate_wld_ms` clock protection passed Gate P4 (**+22 Elo** at 2 s/move) while keeping `Solve` 100 % exact.
- **Phase 5 — Evaluation (`egaroucid16 + mobility × 60`):** `60` single-disc stages (`--smooth 0` so odd/even parities never mix) + `96` legal-mobility weights per stage (`STELLOEV` v3) cut held-out error from `5.47` MAE / `7.31` RMSE to **4.97 MAE / 6.74 RMSE**, gaining **+20 Elo** (`egaroucid16`) and another **+13–20 Elo** (`+mobility`).
- **Phase 6 — Opening book (`6,000` positions):** `recalc` (raised `EngineVersion` to `5`, **+4.43 plies** mean depth), `trim` (`11,200 -> 1,993`), `calclib` (`+642` moves), and Lincke drop-out expansion (`expand`) built a **6,000-position** (`13,154`-move) book beating the 4.0 book **55.0 %** (`+0.7` discs/g) at 500 ms while shrinking `opening-book.bin` from `193 KB` to `114 KB` (`33.8 KB` brotli).
- **Phase 7 — Final matches & FFO solves:** Across **18 full `1,180`-game matches** (`21,240` games), Stello 5.0 scores **+58–84 Elo** over Stello 4.0 (`100–2,000 ms`, `1t` & `8t`), **+53–84 Elo** over Stello 3.1.0, **+115–145 Elo** over Stello 2.0, and **+211 Elo** (`100 ms`) / **+109 Elo** (`500 ms`) / tied **−1 Elo** (`2,000 ms`) against the Egaroucid C# port.

---

## Phase 1: speed without changing nodes

Because Stello's baseline DLL and WebAssembly targets run on plain x86-64 / WASM32 without assuming AVX2 at load time, Stello 5.0 introduces two speed tiers before changing a single node:

1. **Runtime `x86-64-v3` dispatch & AVX2 bitboards (`Kernel<true>` vs `Kernel<false>`):** `stello::cpu` (`cpu.cpp`) checks `has_avx2_bmi2()` (AVX2 + BMI1 + BMI2 + POPCNT + LZCNT) once at startup unless overridden by `--cpu portable`. [`search_v3.cpp`](../Stello.Cpp/src/search/search_v3.cpp) explicitly instantiates `Kernel<true>` (`-mavx2 -mbmi -mbmi2 -mpopcnt -mlzcnt -mno-avx512f` on Clang; `/arch:AVX2` on MSVC) with 256-bit `flips_avx2` and `legal_moves_avx2` (`endgame/bits_avx2.hpp`), isolating baseline TUs via `kernel.hpp` so LTO keeps baseline COMDAT copies out of `Kernel<true>`. Midgame `d14` speed rose **+10.6 %**, `FFO #40–49` **+34.7 %** (`23.55 s -> 17.5 s`), and `endgame-random-14-24` **+35.2 %** (`489.2 s -> 361.7 s`) with **100 % identical nodes**.
2. **Incremental pattern indices (`PatternState`):** Rather than calling `Direct::sum_pext` from scratch on every child, midgame search carries `alignas(32) PatternState` (`black[64]`, `white[64]` in [patterns.hpp](../Stello.Cpp/src/eval/patterns.hpp#L113)), updating only squares touched by the move via precomputed sparse deltas and scoring leaves (`look == 0`) with 256-bit AVX2 vectors plus 4-way unrolled scalar L1 weight loads (beating microcoded `_mm256_i32gather_epi32` on every depth). Native `mid-bench d10..d16` improved **+35.1–36.8 %** over 4.0 at zero node change; `--cpu portable` also jumped from `7.32M` to `11.62M` nps at `d12`.
3. **WebAssembly `-msimd128`:** Enabling `-msimd128` (`CMakeLists.txt`) with 128-bit `<wasm_simd128.h>` `PatternState` vectors (`patterns.cpp`) and 2-lane upper-ray `flips` (`bits.hpp`) while keeping `STELLO_ABORT_EXCEPTIONS=ON` doubled single-threaded WASM midgame speed in Node `v24.19.0` (**+100.1 %** at `mid-d10`: `788.1 -> 393.8 ms`; **+98.5 %** at `mid-d12`: `2,203.9 -> 1,110.4 ms`). A 2-lane SIMD `rbit` `legal_moves` benchmarked slower than Kogge-Stone and was reverted.
4. **Cross-TU TT inlining & lock-free `stable::of`:** Moving `TranspositionTable::get_at_root` / `store_at_root` / `prefetch` inline into [table.hpp](../Stello.Cpp/src/search/table.hpp) allowed `Kernel<true>` to use hardware `popcntq` instead of cross-TU SWAR calls, and replacing `std::vector` static locals in `stable.cpp` with `.bss` arrays + `std::atomic<uint8_t>` eliminated Windows `%gs:0x58` TLS/SEH prologues — adding another `+4.0 %` midgame speed (`0.731 -> 0.703 s` at `d12`).

---

## Phase 2: threads

Stello 4.0's 8-thread scaling (`3.67×` on `FFO #40–49`) trailed Egaroucid C++ (`5.27×`). Phase 2 addressed every bottleneck verified against `stello_engine_p1.dll`:

- **Root splitting & brother cuts (`engine_impl.hpp`, `ybwc_impl.hpp`):** Enabled YBWC splits after move 1 at both `search_root` (`look >= 6`) and `solve_root` (`empties > 14`). Attached the active `split_point` while sequential brothers run locally so any helper fail-high aborts local siblings immediately; guarded `#if STELLO_ABORT_EXCEPTIONS` unwind propagation outside `catch (...)` (preventing Win64 Clang ASan bare-`throw;` crashes), and extracted `maybe_report_heartbeat()` into `scope_polling`.
- **Tuned permanent thresholds:** Lowered `YBWC_END_MIN_EMPTIES` (`16 -> 14`, cutting `endgame-random-14-24` `-9.4 %` at `8t` and `-10.3 %` at `16t`), confirmed pure YBWC (`0` Lazy SMP solve helpers beat `1–3` on `FFO #40–49` at `8t`), and scaled midgame helpers as `(workers + 2) / 4` (`2` at `8t`, avoiding saturation of YBWC workers, `+23` Elo vs `4` helpers at `500 ms`; `4` at `15–16t`, where flat `2` regressed `d16` wall time).

At Gate P2 (`1,180`-game `2 s/move` match confirmed on `600` games), Phase 2 beat Phase 1 at `8` threads by **54.3 %** (`51.7–57.0 %`, **+30 Elo**, `LOS 99.9 %`). On FFO `#40–49`, 8-thread solve time dropped to **3.542 s** (**4.88×** speedup, `399.98M` nps — **+34 %** higher raw node rate than Egaroucid `7.8.2`'s `299.29M` nps at `8t`).

---

## Phase 3: midgame search experiments (kept vs rejected)

Five midgame ideas were implemented behind clean `opt=` toggles and tested against **Gate P3** (`1,180`-game `500 ms/move` `lower CI > 50 %`):

| Experiment | Tested configuration | Games | Score (95 % CI) | Elo / Outcome | Verdict |
|---|---|---:|---:|---|---|
| MPC error-model refit | `pair` / `powergap` fits (`t = 0.85..1.10`, early/late tilt) on `11,639` clean rows | 8,340 | `51.6 %` (49.7–53.5 % @ 500 ms); `50.1 %` @ 2 s | `−0.20` plies @ 2 s | **Rejected** |
| Second TT best move (`mv2`) | Previous `mv` packed into 7 unused `Slot` bits (`mid` & `both`) | 2,960 | `51.4 %` (49.5–53.2 % @ 500 ms); `48.7 %` @ 2 s | `−0.7 %` NPS @ 2 s | **Rejected** |
| Higher soft time limit | `soft-pct=80`, `90`, `100` vs `66.7 %` (`500 ms/move`) | 3,540 | `50.0 %` / `51.2 %` (49.5–52.9 %) / `50.0 %` | `+0.11–0.15` plies, no gain | **Rejected** |
| Aspiration windows | `±100..±500` (`1/100` disc) starting at `depth >= 4` | 2,360 | `50.3 %` (48.7–52.0 % @ `±400`, `48.5–52.2 %` @ `±200`) | Odd-even fail-low re-searches | **Rejected** |
| 4× TT (`hash-bits=21`) | `256 MB` vs `64 MB` (`hash-bits=19`) @ `2,000 ms`, `8t` | 600 | `49.0 %` (46.6–51.4 %) | `−9.3 %` NPS (`spilled 96 MB L3`) | **Rejected** |
| Root clog search | `clog=4..5` plies pre-search (`1/8` hard budget cap) | 1,180 | `49.2 %` (47.7–50.6 %) | `−0.02` plies depth tax | **Rejected** |

All temporary search switches were reverted cleanly. The `"Training data"` directory typo fix in `Stello.Trainer` / `Stello.Benchmark`, per-pair `mpc --fit` reporting, and generic `opt=` plumbing were kept.

---

## Phase 4: endgame and transition

1. **`end_local` pattern move ordering (Rejected):** Trained a compact 4-pattern single-stage model (`end4`, `52–54` discs, `5.61` MAE) for `11–13` empty squares. Although it cut `FFO #40–49` nodes by `−18.1 %` (`1t`) / `−20.9 %` (`8t`), scratch `LocalPatternState` construction slowed NPS by `−9 %`, saving only `9.6 %` FFO time (`~+0.1` empties) and losing **`48.3 %`** (`46.8–49.8 %`, `−12` Elo) in `1,180` games at `500 ms`. Reverted cleanly (keeping `--min-discs` / `--max-discs` trainer filters).
2. **Selective endgame `try_end_probcut` & `estimate_wld_ms` handover (Accepted):** Fitted $\sigma_{\text{end}}$ via `Stello.Trainer mpc-end` on solved `38–50`-disc positions and added `try_end_probcut` (`stages 1..6`, stored at `depth = stage` in `endgame_table` so exact `depth = empties >= 7` proofs are never corrupted). After midgame iterations reach `empties - 8` (`empties >= 16`, `TimePerMove` / `TimePerGame` only — `Solve` and `FixedDepth` stay **100 % exact**), `iterate()` runs 3 selective stages (`74 -> 88 -> 98 %`) within the midgame soft budget, cutting `mid-quality` disc loss at 24–28 empties in half and scoring **`51.5 %`** (`+10` Elo, `LOS 97.1 %`) at `500 ms` and **`53.2 %`** (`51.5–54.8 %`, **+22 Elo**, `LOS 100.0 %`) at `2,000 ms`. In `TimePerGame`, `estimate_wld_ms` guards `for_solver` (`rest / 2`) so unsolvable midgame transitions do not waste half the game clock.

---

## Phase 5: evaluation

- **Item 1 (`egaroucid16 × 60`, `--smooth 0`):** Training `60` single-disc stages showed that cross-disc smoothing mixes opposite Othello parities (`5.359` MAE with `--smooth 0` vs `5.635` with `--smooth 1` on `35` discs). Training `egaroucid16` across all `60` stages (`10M` rows/stage, `--min-discs 12 --smooth 0 --units 16 --min-appear 10`) cut test MAE/RMSE to **5.07 / 6.87**, beating `plus15 × 10` (`5.47 / 7.31`) by **52.9 %** (`+20` Elo, `LOS 99.7 %`) at `500 ms` and **52.0 %** (`+14` Elo, `LOS 98.2 %`) at `2,000 ms`.
- **Item 2 (`+96` legal-mobility weights, `STELLOEV` v3):** Extended `EvalWeights` to format version `3` (`48` own + `48` opponent legal-move weights per stage, `320,289` weights/stage, **8,892,085 bytes** gzip / **38,434,770 bytes** unpacked), further lowering test error to **4.97 MAE / 6.74 RMSE** (beating Egaroucid's `5.05 / 6.90`) at just `+6.4 KB` gzip (`+9 KB` WASM brotli) and beating Item 1 by **51.8 %** (`+13` Elo, `LOS 96.9 %`) at `500 ms` and **52.9 %** (`+20` Elo, `LOS 98.4 %`) at `2,000 ms`. Midgame and endgame MPC error formulas (`make_probcut_errors`, `make_end_probcut_errors`) were refitted for both `(egaroucid16)` and `(egaroucid16 + mobility)`.

---

## Phase 6: the opening book

Stello 5.0 rebuilt the master book with `Stello.BookTool` in four sequential passes (`EngineVersion = 5`):

1. **`recalc` (`--time-s 60`):** Updated all `10,531` non-legacy leaves (keeping `1,091` untimed `Exact` entries, re-searching timed pre-3.1 `Exact` leaves), lifting mean leaf midgame depth from `25.18` to **`29.61` plies (`+4.43` plies)** and flipping `946` best replies (moving `d3` root from `c5 (-87)` to `c5 (+100)`). Standalone re-ranking scored `49.9 %` (`48.7–51.0 %`) at `500 ms`.
2. **`trim` (`--hero-loss 100 --opp-loss unlimited --max-ply 30`):** Pruned unreachable legacy self-play tails (`11,200 -> 1,993` positions, `22,878 -> 4,473` moves), preserving every remaining ancestor value cleanly ([BookTrimmer.cs](../Stello.Net/Stello.Engine/BookTrimmer.cs)).
3. **`calclib` (`--time-s 60 --threshold 100 --max-ply 30`):** Probed unbooked legal moves (`onlyMoves`), adding **`642` strong moves** (`4,473 -> 5,115`, `22.6 %` of positions changing best move; [BookCalclib.cs](../Stello.Net/Stello.Engine/BookCalclib.cs)).
4. **`expand` (`--time-s 60 --omega 50 --max-ply 28 --max-positions 6000`):** Used Lincke drop-out expansion ($\omega \cdot \text{ply} + \text{cumulative\_loss}$, [BookExpander.cs](../Stello.Net/Stello.Engine/BookExpander.cs)) to expand the book to **6,000 positions / 13,154 moves** (`7,047` leaves all at `60s:v5`, `0` passes, `114,169` B binary = **41 % smaller** than 4.0's `193,210` B, `33,809` B brotli), raising ply 1–16 position counts `2.2×` (`ply 1–10`) and `5.0×` (`ply 11–16`), with `d3 c5 (+81)` and a 23-ply Tiger main line.

At **Gate P6** (`500 ms`, `8` workers, `5.0` playing with `v5` vs `v4` book), the Stello 5.0 book won decisively: **`55.0 %`** (`53.4–56.5 %`, **`+0.7` discs/game**, `138` pairs won vs `66` lost) on all `636` changed-move shared starts (`1,272` games) and **`51.8 %`** (`49.7–54.0 %`, `+0.3` discs/g) across all `284` ply-8 book openings (`568` games).

---

## Phase 7: final matches and FFO solve-bench

### 1. All 18 final matches (`21,240` games, no book, `hash-bits=19` Stello / `21` Egaroucid)

Each row plays all `590` balanced start positions from `starts.txt` with swapped colours (`1,180` games; Side B = **Stello 5.0**; `1t` matches run `--workers 8`, `8t` matches `--workers 1`):

| # | Opponent | Time | Threads | Score (95 % CI) | W / D / L | Elo (95 % CI) | LOS | Discs/g | Avg Depth (A / B) | NPS (A / B) | Result |
|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 1 | `Stello 4.0` | 100 ms | 1t | **61.4 %** (59.2–63.7 %) | `+643 =164 −373` | **+81** (+65..+98) | 100.0 % | +2.0 | 16.29 / 17.45 | 15.0M / 19.8M | [6m 40s](../Plans/benchmark-results/match_p7_v5_vs_v4_100ms_1t.md) |
| 2 | `Stello 4.0` | 500 ms | 1t | **58.3 %** (56.2–60.3 %) | `+601 =173 −406` | **+58** (+43..+73) | 100.0 % | +1.3 | 19.19 / 20.57 | 16.3M / 21.2M | [30m 38s](../Plans/benchmark-results/match_p7_v5_vs_v4_500ms_1t.md) |
| 3 | `Stello 4.0` | 2,000 ms | 1t | **59.4 %** (57.4–61.3 %) | `+582 =237 −361` | **+66** (+52..+80) | 100.0 % | +1.3 | 21.65 / 23.31 | 17.6M / 22.7M | [1h 53m](../Plans/benchmark-results/match_p7_v5_vs_v4_2000ms_1t.md) |
| 4 | `Stello 4.0` | 100 ms | 8t | **60.5 %** (58.3–62.6 %) | `+626 =175 −379` | **+74** (+58..+89) | 100.0 % | +1.8 | 18.58 / 20.15 | 100.2M / 111.2M | [49m 49s](../Plans/benchmark-results/match_p7_v5_vs_v4_100ms_8t.md) |
| 5 | `Stello 4.0` | 500 ms | 8t | **60.2 %** (58.2–62.1 %) | `+609 =202 −369` | **+72** (+58..+86) | 100.0 % | +1.3 | 21.76 / 23.74 | 123.3M / 139.4M | [3h 45m](../Plans/benchmark-results/match_p7_v5_vs_v4_500ms_8t.md) |
| 6 | `Stello 4.0` | 2,000 ms | 8t | **61.9 %** (60.1–63.6 %) | `+615 =230 −335` | **+84** (+71..+97) | 100.0 % | +1.4 | 24.45 / 26.84 | 141.4M / 158.8M | [13h 50m](../Plans/benchmark-results/match_p7_v5_vs_v4_2000ms_8t.md) |
| 7 | `Stello 3.1.0` | 100 ms | 1t | **61.9 %** (59.6–64.2 %) | `+648 =165 −367` | **+84** (+67..+102) | 100.0 % | +2.1 | 16.22 / 17.38 | 14.1M / 19.3M | [6m 42s](../Plans/benchmark-results/match_p7_v5_vs_v3_100ms_1t.md) |
| 8 | `Stello 3.1.0` | 500 ms | 1t | **57.5 %** (55.4–59.6 %) | `+586 =185 −409` | **+53** (+38..+68) | 100.0 % | +1.3 | 19.09 / 20.45 | 14.7M / 20.0M | [30m 42s](../Plans/benchmark-results/match_p7_v5_vs_v3_500ms_1t.md) |
| 9 | `Stello 3.1.0` | 2,000 ms | 1t | **60.3 %** (58.3–62.2 %) | `+576 =270 −334` | **+72** (+58..+87) | 100.0 % | +1.3 | 21.48 / 23.13 | 15.8M / 21.2M | [1h 54m](../Plans/benchmark-results/match_p7_v5_vs_v3_2000ms_1t.md) |
| 10 | `Stello 3.1.0` | 100 ms | 8t | **60.8 %** (58.6–62.9 %) | `+633 =168 −379` | **+76** (+61..+92) | 100.0 % | +1.7 | 18.60 / 20.25 | 96.6M / 116.0M | [49m 53s](../Plans/benchmark-results/match_p7_v5_vs_v3_100ms_8t.md) |
| 11 | `Stello 3.1.0` | 500 ms | 8t | **61.1 %** (59.1–63.0 %) | `+607 =227 −346` | **+78** (+64..+93) | 100.0 % | +1.5 | 21.69 / 23.74 | 115.2M / 138.1M | [3h 45m](../Plans/benchmark-results/match_p7_v5_vs_v3_500ms_8t.md) |
| 12 | `Stello 3.1.0` | 2,000 ms | 8t | **60.2 %** (58.5–62.0 %) | `+589 =243 −348` | **+72** (+59..+85) | 100.0 % | +1.1 | 24.42 / 26.89 | 132.2M / 160.5M | [13h 51m](../Plans/benchmark-results/match_p7_v5_vs_v3_2000ms_8t.md) |
| 13 | `Stello 2.0` | 100 ms | 1t | **69.7 %** (67.6–71.9 %) | `+748 =150 −282` | **+145** (+128..+163) | 100.0 % | +3.3 | 14.35 / 17.32 | 6.4M / 19.2M | [6m 46s](../Plans/benchmark-results/match_p7_v5_vs_v2_100ms_1t.md) |
| 14 | `Stello 2.0` | 500 ms | 1t | **66.6 %** (64.6–68.6 %) | `+705 =162 −313` | **+120** (+104..+136) | 100.0 % | +2.6 | 17.31 / 20.51 | 7.0M / 20.4M | [31m 07s](../Plans/benchmark-results/match_p7_v5_vs_v2_500ms_1t.md) |
| 15 | `Stello 2.0` | 2,000 ms | 1t | **66.0 %** (64.1–67.9 %) | `+665 =228 −287` | **+115** (+101..+130) | 100.0 % | +2.0 | 19.84 / 23.25 | 7.6M / 22.0M | [1h 55m](../Plans/benchmark-results/match_p7_v5_vs_v2_2000ms_1t.md) |
| 16 | `Egaroucid` | 100 ms | 1t | **77.1 %** (75.1–79.1 %) | `+854 =111 −215` | **+211** (+192..+231) | 100.0 % | +4.8 | 10.87 / 17.37 | 6.5M / 19.3M | [7m 10s](../Plans/benchmark-results/match_p7_v5_vs_egaroucid_100ms_1t.md) |
| 17 | `Egaroucid` | 500 ms | 1t | **65.2 %** (63.1–67.3 %) | `+680 =179 −321` | **+109** (+93..+126) | 100.0 % | +2.2 | 15.92 / 20.57 | 7.6M / 20.6M | [32m 19s](../Plans/benchmark-results/match_p7_v5_vs_egaroucid_500ms_1t.md) |
| 18 | `Egaroucid` | 2,000 ms | 1t | **49.8 %** (47.9–51.8 %) | `+466 =244 −470` | **−1** (−15..+12) | 43.2 % | +0.1 | 24.11 / 23.22 | 9.0M / 21.3M | [1h 58m](../Plans/benchmark-results/match_p7_v5_vs_egaroucid_2000ms_1t.md) |

### 2. FFO `#40–49` & `#50–59` `solve-bench` comparisons (`hash-bits=19`, 1 pass, all exact)

| Suite | Engine | Threads | Total Nodes | Total Time | Nodes/s | File |
|---|---|---:|---:|---:|---:|---|
| `FFO #40–49` | **`Stello 5.0`** | 1 | **1,070,469,669** | **16.9 s** | **63.21M** | [p7_v5_ffo40-49_1t.json](../Plans/benchmark-results/p7_v5_ffo40-49_1t.json) |
| `FFO #40–49` | `Stello 4.0` (`stello-v4`) | 1 | 1,115,666,701 | 24.0 s | 46.48M | [p7_v4_ffo40-49_1t.json](../Plans/benchmark-results/p7_v4_ffo40-49_1t.json) |
| `FFO #40–49` | `Stello 3.1.0` (`stello-v3`) | 1 | 1,115,666,701 | 25.9 s | 43.06M | [p7_v3_ffo40-49_1t.json](../Plans/benchmark-results/p7_v3_ffo40-49_1t.json) |
| `FFO #40–49` | **`Stello 5.0`** | 8 | **1,177,976,301** | **3.181 s** | **370.36M** | [p7_v5_ffo40-49_8t.json](../Plans/benchmark-results/p7_v5_ffo40-49_8t.json) |
| `FFO #40–49` | `Stello 4.0` (`stello-v4`) | 8 | 1,610,845,768 | 6.360 s | 253.27M | [p7_v4_ffo40-49_8t.json](../Plans/benchmark-results/p7_v4_ffo40-49_8t.json) |
| `FFO #40–49` | `Stello 3.1.0` (`stello-v3`) | 8 | 1,616,143,675 | 7.029 s | 229.93M | [p7_v3_ffo40-49_8t.json](../Plans/benchmark-results/p7_v3_ffo40-49_8t.json) |
| `FFO #50–59` | **`Stello 5.0`** | 1 | 30,513,267,013 | **559.6 s** | **54.52M** | [p7_v5_ffo50-59_1t.json](../Plans/benchmark-results/p7_v5_ffo50-59_1t.json) |
| `FFO #50–59` | `Stello 4.0` (`stello-v4`) | 1 | 25,419,468,677 | 586.5 s | 43.34M | [p7_v4_ffo50-59_1t.json](../Plans/benchmark-results/p7_v4_ffo50-59_1t.json) |
| `FFO #50–59` | **`Stello 5.0`** | 8 | 38,930,020,066 | **110.8 s** | **351.51M** | [p7_v5_ffo50-59_8t.json](../Plans/benchmark-results/p7_v5_ffo50-59_8t.json) |
| `FFO #50–59` | `Stello 4.0` (`stello-v4`) | 8 | 38,911,015,999 | 144.6 s | 269.15M | [p7_v4_ffo50-59_8t.json](../Plans/benchmark-results/p7_v4_ffo50-59_8t.json) |
| `FFO #50–59` | `Stello 3.1.0` (`stello-v3`) | 8 | 39,155,644,861 | 160.8 s | 243.48M | [p7_v3_ffo50-59_8t.json](../Plans/benchmark-results/p7_v3_ffo50-59_8t.json) |

---

## Version, artifacts and verification

- **Version 5.0.0:** [`Stello.Cpp/CMakeLists.txt`](../Stello.Cpp/CMakeLists.txt) and [`version.hpp`](../Stello.Cpp/src/core/version.hpp) report `5.0.0` (`50000`, ABI version `3`); [`AboutInfo.cs`](../Stello.Net/Stello.App/Models/AboutInfo.cs) reports **Stello Version 5.0** and `"knows 6,000 opening positions"`; [`SettingsWindow.xaml`](../Stello.Net/Stello.Net/Views/SettingsWindow.xaml) and [`SettingsForm.razor`](../Stello.Net/Stello.Web/Components/SettingsForm.razor) display **6,000 canonical positions**.
- **Built 5.0.0 binary artifacts:**
  - `stello_engine.dll`: `9,418,752` B
  - `stello-eval.bin`: `8,892,085` B gzip / `38,434,770` B unpacked (`STELLOEV` v3)
  - `opening-book.txt` / `.bin`: `370,387` B / `114,169` B (`33,809` B brotli)
  - `engine/stello_wasm_bg.wasm`: `9,136,543` B raw / `8,954,923` B brotli (`stello_wasm.js`: `10,423` B)
  - `engine-mt/stello_wasm_bg.wasm`: `9,167,007` B raw / `8,964,314` B brotli (`stello_wasm.js`: `10,423` B)
- **Verification (0 compiler/MSBuild warnings across all targets):**
  - **C++ (`ctest` across all 5 presets):** `clang-release`, `clang-native`, `msvc-release`, `msvc-native`, and `clang-sanitized` (0 ASan/UBSan findings) each passed `2/2` CTest checks (`stello_tests` **`48/48`**, `stello_cli_version`), with `stello-cpp 5.0.0 (50000)`.
  - **.NET Release (`Stello.Net.slnx -c Release -p:StelloWasmThreads=required`):** `0 Warning(s), 0 Error(s)`; sequential `--no-build` tests passed `Stello.Engine.Tests` (**`517/517`**), `Egaroucid.Engine.Tests` (**`100` passed, `8` skipped** for optional tagged 3.0; `StelloV4` passed), and `Stello.Net.Tests` (**`116/116`**).
  - **WebAssembly (`node --test`):** **`19/19`** tests passing across `engine.test.mjs` and `engine-mt.test.mjs`.

## Where in the code

| File | Role |
|---|---|
| [`patterns.hpp`](../Stello.Cpp/src/eval/patterns.hpp), [`patterns.cpp`](../Stello.Cpp/src/eval/patterns.cpp), [`weights.hpp`](../Stello.Cpp/src/eval/weights.hpp), [`weights.cpp`](../Stello.Cpp/src/eval/weights.cpp) | `PatternState`, WASM `-msimd128` incremental sum, `STELLOEV` v3 (`has_mobility()`, 96 weights/stage) |
| [`search/kernel.hpp`](../Stello.Cpp/src/search/kernel.hpp), [`search_v3.cpp`](../Stello.Cpp/src/search/search_v3.cpp), [`search_portable.cpp`](../Stello.Cpp/src/search/search_portable.cpp) | Runtime `x86-64-v3` AVX2+BMI2 `Kernel<true>` vs portable `Kernel<false>` |
| [`endgame/bits_avx2.hpp`](../Stello.Cpp/src/endgame/bits_avx2.hpp), [`stable.hpp`](../Stello.Cpp/src/endgame/stable.hpp), [`stable.cpp`](../Stello.Cpp/src/endgame/stable.cpp), [`search/table.hpp`](../Stello.Cpp/src/search/table.hpp) | 256-bit `flips_avx2` / `legal_moves_avx2`, lock-free `.bss` `stable::of`, inlined hardware-`popcnt` TT |
| [`engine_impl.hpp`](../Stello.Cpp/src/search/engine_impl.hpp), [`ybwc_impl.hpp`](../Stello.Cpp/src/search/ybwc_impl.hpp), [`endgame/search_impl.hpp`](../Stello.Cpp/src/endgame/search_impl.hpp) | Root YBWC splits, sibling brother cuts, `lazy_smp_helpers`, `YBWC_END_MIN_EMPTIES = 14`, `try_end_probcut`, `estimate_wld_ms` |
| [`EvalWeights.cs`](../Stello.Net/Stello.Engine/Evaluation/EvalWeights.cs), [`Stello.Trainer`](../Stello.Net/tools/Stello.Trainer) | C# `STELLOEV` v3, `--smooth 0`, `--mobility on`, `--min-discs` / `--max-discs` |
| [`BookTrimmer.cs`](../Stello.Net/Stello.Engine/BookTrimmer.cs), [`BookCalclib.cs`](../Stello.Net/Stello.Engine/BookCalclib.cs), [`BookExpander.cs`](../Stello.Net/Stello.Engine/BookExpander.cs), [`OpeningBook.cs`](../Stello.Net/Stello.Engine/OpeningBook.cs) | `EngineVersion = 5`, `trim`, `calclib` and Lincke DOE `expand` pipeline |

## Tests

- **C++ (`ctest` across all 5 presets):** `parity_tests.cpp` (`parity_incremental_eval` across 6,214 positions), `bits_avx2_tests.cpp`, `threads_tests.cpp` (`phase2_root_split_brother_cut_and_split_heartbeats`), `search_tests.cpp`, `ffi_tests.cpp` (`50000u`), `test_main.cpp` (`5.0.0`, `8,892,085`-byte embedded eval).
- **.NET (`dotnet test -c Release`):** `BookTrimmerTests.cs`, `BookCalclibTests.cs`, `BookExpanderTests.cs`, `BookRecalculatorTests.cs`, `OpeningBookTests.cs` (`c5 -> 81`, 23-ply Tiger), `NativeEngineTests.cs` (`50000u`), `PatternTests.cs`, `StelloLibraryMatchTests.cs`, and `ViewModelTests.cs` (`"Stello Version 5.0"`).
- **WebAssembly (`node --test`):** `engine.test.mjs` (`/^5\.\d+\.\d+$/`, strict `5.0` signatures) and `engine-mt.test.mjs` (`4`-thread FFO `#40–44` and cancellation).

## Further reading

- [making-stello-5-0-plan.md](../Plans/making-stello-5-0-plan.md): the Phases 0–7 plan and acceptance gates.
- [making-stello-5-0-findings.md](../Plans/making-stello-5-0-findings.md): full experimental logs, rejected items, DOE expansion metrics, all 18 match links and FFO `#40–59` tables.
- [Chapter 28](28-stello-4-0-the-engine-in-cpp.md): the C++20 baseline that Stello 5.0 builds on.
- [References](31-references.md#29-stello-50-speed-threads-search-and-book): Lincke drop-out expansion, Multi-ProbCut, YBWC, and SIMD references.

---

Previous: [28 Stello 4.0: the engine in C++](28-stello-4-0-the-engine-in-cpp.md) · Next: [30 Glossary](30-glossary.md)
