# Phase 4 Item 4 follow-up: solver clock and regression tests

**Handoff for the implementing agent, 28 November 2026.** This reviews `baf118c..29e47a6`, the implementation of the [first Item 4 handoff](Phase%204%20item%204%20review%20and%20fixes.md). Use **`29e47a6` as the control** for this cleanup. Keep the stage-4 win/loss/draw (WLD) predictor and the decision to allow inner midgame ProbCut during selective endgame verification. One production clock calculation needs fixing. The other three findings concern tests, not observed search defects.

## 1. Apply the soft percentage to the capped solver allowance

**Finding.** The [solver transition](../Stello.Cpp/src/search/engine_impl.hpp#L256) computes the soft deadline from the uncapped allowance, then reduces the hard deadline and sets `soft = min(old_soft, new_hard)`. Taking the minimum does not preserve the configured percentage. The hard limit is still enforced; the error is **starting an exact disc-score search when the engine should return its completed WLD proof**.

**Reproduction:** [g21e24](../Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt#L6), Black, 24 empties, one thread, hash bits 16, `TimePerGame(10,000 ms)`, `soft_time_percent = 10`. Use the [existing manual clock and progress callback](../Stello.Cpp/tests/search_tests.cpp#L1445). Hold the clock at zero through the selective stages; on the first WLD report set it to **400 ms** and leave it there.

| At entry to the solver | Hard allowance | Soft allowance at 10% |
|---|---:|---:|
| Before capping (half the remaining game clock) | 5,000 ms | 500 ms |
| After the WLD cap (`4 * 875 ms` midgame hard limit) | 3,500 ms | **350 ms** |

At 400 ms WLD can finish, but no new pass should start. `29e47a6` instead returned **Exact -4**.

**Fix.** Anchor the solver budget at the elapsed time when the selective stages finish. Cap the hard **span first**, then derive the soft deadline as `solver_start + soft_share(capped_span)`. Preserve the existing default two-thirds calculation and integer rounding; use the [percentage helper](../Stello.Cpp/src/search/time.hpp#L42) for explicit overrides. Do not apply a percentage to an absolute timestamp or restart the clock after WLD. Check the corrected deadline **before** starting Exact. Keep the WLD/Exact hard caps, the [existing `>` soft-limit comparison](../Stello.Cpp/src/search/engine.cpp#L403), and the other time modes unchanged. Update the [time-control guide](../docs/09-time-control.md#L91) to distinguish stage-4 WLD admission, the soft decision to start another pass, and the Exact hard cap.

**Accept when:** the 400 ms case reports WLD, returns a legal move with `WinLossDraw` and a negative score, and emits **no Exact report**. As a positive control, run the same position with the clock advancing to **100 ms** instead: Exact should still be allowed and return **-4**. Include a nonzero solver-start offset in the budget tests so the percentage cannot accidentally be taken over time already spent.

## 2. Make the split-worker test independent of scheduling

**Finding.** The [YBWC propagation test](../Stello.Cpp/tests/threads_tests.cpp#L1272) gives the owner and helper six small jobs and retries up to six times. On one logical CPU the owner can legitimately finish all the work before the helper runs. The test passed normally and failed **four out of four** affinity-limited runs with `YBWC helper did not claim split work within 6 attempts`. That is not evidence that a worker received the wrong recursion level.

**Reproduce on Windows** from the directory containing the native test executable (PowerShell):

```powershell
& $env:ComSpec /d /c 'start "" /b /wait /affinity 1 ".\stello_tests.exe" phase4_ybwc_split_propagates_nonzero_probcut_level'
```

**Fix and acceptance.** Test the full transfer: parent level **1 or 2**, [narrow split setup](../Stello.Cpp/src/search/ybwc_impl.hpp#L361) or [wide split setup](../Stello.Cpp/src/search/ybwc_impl.hpp#L676), then [worker receipt](../Stello.Cpp/src/search/ybwc.cpp#L37). Coordinate a real helper claim before the owner can drain the work, or construct the setup through the production path and invoke worker receipt deterministically. A hand-built `TaskSetup` alone bypasses the producer. Do not add sleeps, retries or a single-CPU skip. Both normal and affinity-limited runs must pass; temporarily zeroing the level in **either** producer or in the receiver must make the corresponding test fail.

## 3. Separate the affordable-win assertion from processor speed

**Finding.** With the [g46e28 regression settings](../Stello.Cpp/tests/search_tests.cpp#L1425) (15,000 ms remaining **per game**, one thread, hash bits 19), native V3 proved the win. Forced portable Release twice returned **g3, Heuristic, depth 28**. Both chose the optimal move on [g46e28](../Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt#L79). A slower processor can legitimately use up its stage budget; scaling the clock only under ASan does not make the test portable.

**Fix and acceptance.** Separate three questions:

- **Does the predictor admit this case?** Keep the [recorded-input test](../Stello.Cpp/tests/search_tests.cpp#L534): stage 4 took 361 ms at score +30, giving a 1,444 ms estimate. Admit with 4,040 ms available; decline with 1,000 ms.
- **Does the engine actually enter WLD and prove the win?** Keep the board and use the manual clock on all builds. Allow the selective stages to finish without real time pressure; observe a positive WLD report and a proven winning result. The clock can advance on that report to avoid spending test time on full disc maximization. Assert a winning move, not an exact margin of +30 from a WLD search. Run under native and forced portable dispatch, restoring the tier afterward. An unconditional skip of WLD must fail this test.
- **How fast is it in play?** Keep the real 15-second run as a benchmark, reporting CPU tier, result kind and elapsed time. Remove the ASan-only scaled clock if unused. Retain the separate [g01e30 overspending regression](../Stello.Cpp/tests/search_tests.cpp#L1415).

## 4. Pin mixed ProbCut with a correct cutoff that distinguishes the policies

**Finding.** Disabling inner midgame ProbCut for the [existing endgame fixture](../Stello.Cpp/tests/search_tests.cpp#L2077) still satisfied both node-count assertions. They cover outer eligibility and exact-stage behavior, not the [intentional inner verifier policy](../Stello.Cpp/src/endgame/search_impl.hpp#L542).

**Fix.** Use [g29e24](../Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt#L30), Black, 24 empties, exact **+18 discs**. Call the production `try_end_probcut` **directly** at stage **2**, window **[18, 19]**. The review used the portable kernel, built-in engine coefficients, one thread, hash bits 19, hash read/write heights 64, and fresh TT and response state for each arm.

| Level before the outer verifier | Level in the midgame verifier | Expected result |
|---|---|---|
| 0 (shipped policy) | 1: one inner ProbCut allowed | Returns **18**, a correct fail-low |
| 1 (isolation control) | 2: inner ProbCut suppressed | **No cutoff** |

Assert the optional result and that each starting level is restored. Do not set level 1 on `end_window`: that bypasses the **outer** probe. Do not pin node counts or require one of the known false cuts. **Accept when:** suppressing mixed nesting in production breaks the first arm, while the ordinary-midgame recursion and exact-stage checks still pass.

## Evidence and sign-off

- **What to keep:** The [held-out calibration](making-stello-6-0-findings.md#L777) reports **150/155** affordable WLD proofs captured versus **147/155**, with the same timeout cost. This supports the WLD predictor, not the separate [8x rule for starting Exact](../Stello.Cpp/src/search/time.hpp#L144). If validating that rule, measure exact-pass completions, missed affordable exact scores and timed-out work from the retained **post-WLD** state. No retuning is requested on the basis of the WLD numbers alone.
- **How to describe mixed nesting:** Isolation used **12.1x** as many nodes on the targeted 600-probe verifier panel, not across whole games. Its [400-game match](making-stello-6-0-findings.md#L768) scored **193/400**, paired 95% CI **46.1-50.4%**. Keep the incumbent; do not describe the point estimate of -12 Elo as a proven strength loss. No repeat match is needed for test cleanup.
- **Verification:** Before these fixes, an independent MSVC Release build and five focused regressions passed under normal scheduling. After the changes rerun `wld_admission_decision_tests`, `selective_endgame_progress_and_time_per_game_tests`, `phase4_ybwc_split_propagates_nonzero_probcut_level`, `probcut_recursion_guard_limit_and_endgame_isolation`, and `stello_v3_all_105_signatures_match_baseline`, plus the affected budget tests. Verify the runner actually selected each test. Report the two manual-clock outcomes, the single-CPU and portable results, and the two verifier decisions. No fixed-depth signature change is expected.
