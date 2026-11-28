# Phase 4 Item 4: recursive ProbCut, endgame verification and WLD admission

**Handoff for the implementing agent, 28 November 2026.** Reviewed `0a02d7d` (Item 3 after its review fixes) to `baf118c` (accepted Item 4). **Keep the ETC parent-bound store and recursive ProbCut in ordinary midgame.** The combined build won both strength matches and its deep node reduction is reproducible. Two behaviors need separate decisions: should a selective endgame verifier itself be allowed to prune, and when should the game clock admit a win/loss/draw (WLD) proof? Use `baf118c` as the control for those decisions; change one policy at a time.

## 1. Test and isolate ProbCut inside selective endgame verification

**What changed.** The [endgame guard](../Stello.Cpp/src/endgame/search_impl.hpp#L542) raises the recursion level from 0 to 1 before calling the shallow **midgame** verifier. The [midgame gate](../Stello.Cpp/src/search/engine_impl.hpp#L743) now permits another statistical cutoff at level 1 (`level < 2`). The endgame routine still cannot call *itself* recursively, but the evidence it uses to justify a cutoff can now itself be pruned. The old boolean guard prevented that. The [current isolation test](../Stello.Cpp/tests/search_tests.cpp#L1945) compares endgame node totals with the outer cutoff enabled or disabled; it does not check the verifier's decision.

**Regression fixture:** [g27e24](../Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt#L24), Black to move, 24 empties, exact best score **+26 discs**. Call the production `try_end_probcut` at stage 4, window **[25, 26]**:

| Policy | Verifier result | Nodes in review run |
|---|---|---:|
| `baf118c`: inner midgame ProbCut allowed | Returns **25** (fail-low), incorrectly claiming the value is at most 25 | 4,392 |
| Same outer probe and thresholds, inner midgame ProbCut suppressed | **Declines to cut**; the caller must search on | 17,810 |

Use the [built-in engine's model and error tables](../Stello.Cpp/src/search/engine.cpp#L120), one thread, and fresh transposition table (TT) and response history for each arm. The diagnostic invoked the **outer verifier directly** at starting levels 0 and 1: its guard then placed the midgame search at 1 and 2 respectively. Setting the level to 1 on `end_window` instead would bypass the *outer* probe and test the wrong thing. Assert the optional cutoff result, not exact node counts. [g28e24](../Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt#L27) is a second stage-4 witness: false fail-low **5** against exact **+6**.

**How often did it matter?** On all 100 boards with 24 empties in the [exact-scored panel](../Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt), test stages 1, 2 and 4 with the two one-disc windows bracketing the exact value. Of **600 paired comparisons**, **61 cutoff decisions changed**: allowing the inner cut introduced **30 false cuts** and removed **2** that were false without it. These are deliberately difficult boundary probes, **not** a game-wide error rate. They concern *selective heuristic* results, not incorrect exact solves.

**Fix to trial and acceptance:**

1. Keep the outer endgame probe and both high/low verification branches. Suppress further midgame ProbCut throughout that verifier only; restore the previous level on every exit, including abort. Do **not** lower the global midgame recursion limit, disable endgame selectivity, or change coefficients in the same trial.
2. With isolation enabled, the first fixture must decline to cut; restoring the current mixed nesting must make that test fail. Keep the existing ordinary-midgame recursion checks and test that the caller's level is restored. Repeat the 600 paired probes, recording wrong high/low cutoffs, total cuts and nodes by stage. Some false cuts remain possible under either selective policy.
3. Measure the accuracy **and cost** of isolation on the fixed-time [move-quality panel](making-stello-6-0-plan.md#L287) at 200 and 1,000 ms; record disc loss, best-move and solved rates. Test a candidate intended for shipping against `baf118c` under the [paired strength gate](making-stello-6-0-plan.md#L288). If mixed nesting is retained, name and document it as intentional. The existing Item 4 wins include mixed nesting and cannot decide this comparison.

## 2. Calibrate game-clock WLD admission against actual proof work

**What changed.** The [predictor](../Stello.Cpp/src/search/time.hpp#L113) floors the effective last iteration at half of all ordinary-midgame nodes. The [admission caller](../Stello.Cpp/src/search/engine_impl.hpp#L221) passes `depth_done = empties - 8`. When the floor dominates and total nodes are at least 10,000, the node counts cancel: the estimate is approximately `24 * mid_elapsed_ms * 1.95^(empties - 24)`. This can prevent an expensive solve, but a position with an easy winning proof can look just as expensive. The [synthetic tests](../Stello.Cpp/tests/search_tests.cpp#L525) check the formula's arithmetic, not its predictive quality.

**Reproduction:** [g46e28](../Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt#L79), Black, 28 empties, **15,000 ms remaining per game**, one thread, hash bits 19. All three selective stages completed. From their retained TT and root move order:

| Item 4 estimate | Capped remaining allowance | Actual WLD proof alone | Item 3 formula on the same recorded inputs |
|---:|---:|---:|---:|
| 10,063 ms (skip) | 4,040 ms | **93 ms** | 852 ms (admit) |

The timed engine chose **g3**, the optimal move (+30), but returned **Heuristic, depth 28**, not a proven win. A trace also confirmed all three stages and no WLD start at 21,000 ms remaining. This is a **missed proof, not a wrong move**. The 93 ms excludes the already-paid stages and subsequent exact disc maximization; the 852 ms is not an old-binary timing. Neither `TimePerMove` strength match exercised this admission decision.

**Fix to trial and acceptance:**

1. On the 24/26/28-empty panel, log position, game-clock budget, ordinary-midgame elapsed time and nodes, last-iteration nodes, stage completion, remaining allowance and predicted WLD time. For both admitted **and rejected** cases, measure the normal `[-1, 1]` WLD pass from the retained state. Do not compare estimates with a cold `Solve` run. Bound the diagnostic; if the allowance expires, record **not proved within budget**, not a fabricated completion time.
2. Compare missed affordable proofs with attempted proofs that run out of time, including their time cost. Test a revised predictor **or** a short, hard-capped WLD attempt on otherwise rejected cases, separately. A failed trial must return the last valid result and must not silently proceed to full disc maximization. Validate beyond the positions used to select the policy.
3. Retain the [g01e30 overspending regression](../Stello.Cpp/tests/search_tests.cpp#L1390): 120,000 ms remaining per game must not consume 20 seconds on the move. Pair it with an affordable-win case based on `g46e28`, plus deterministic admission/skip and budget-exhaustion tests using recorded inputs or a controlled clock. The desired result is **more affordable proofs without buying them with expensive failed starts**. Report both sides. Do not turn the observed 93 ms into a portable CI threshold or simply revert the scale constant.

## 3. Cover the worker boundary and audit the accepted midgame policy

| Gap | What to test and how to tell it worked |
|---|---|
| **YBWC worker inheritance.** The [eight-thread smoke test](../Stello.Cpp/tests/search_tests.cpp#L1976) checks a legal move, depth and positive nodes at root d10. The deepest ordinary child is d9, whose verifier is at most five plies (`look=4`); [worker splitting](../Stello.Cpp/src/search/engine_impl.hpp#L1367) needs `look >= 5`. Lazy SMP helper searches have no helpers of their own. No threading defect was observed. | Exercise both [parent setup](../Stello.Cpp/src/search/ybwc_impl.hpp#L358) and [worker receipt](../Stello.Cpp/src/search/ybwc.cpp#L37). Use a deterministic split fixture with parent levels **1 and 2**, or a deeper instrumented run that proves the relevant split was actually taken. Check the executing worker sees exactly the parent's level. Dropping the copy or resetting it to zero must fail the test; merely increasing root depth or thread count is not evidence. |
| **Post-change midgame false-cut rate is unknown.** The [published 1/68 and 2/68](making-stello-6-0-findings.md#L724) are **pre-change** Stage 0 measurements, not measurements of recursive `4b-1a`. | Repeat the d14 `deep24` and `parity24` audits for `0a02d7d` and `baf118c` using the same sampling rule and a same-depth **nonselective midgame** reference. Give counts for sampled, accepted and falsely accepted probes, with the denominator for each rate; distinguish high/low and outer/inner level, and report score error. Keep these results separate from the exact-disc *endgame* panel in section 1. |

## 4. Correct the experiment record

- **Item 4C:** The [findings](making-stello-6-0-findings.md#L748) justify not shipping **two-ply iterative deepening plus aspiration** (S3: **186/400, 46.5%**). Aspiration alone on ordinary one-ply steps was inconclusive (S4: **198.5/400**, Elo interval **-24 to +19**). Neither isolates a timed strength loss from two-ply steps alone. The [plan](making-stello-6-0-plan.md#L314) calls `25.93 vs 25.80` completed mean depth, but [match accounting](../Stello.Net/Stello.Engine/BookMatch.cs#L22) includes partial iterations and selective terminal passes. Keep the rejection; label hard aborts and worse ordering a **hypothesis**. To claim causation, isolate the two-ply-only variant and record completed ordinary depth, attempted depth, hard aborts and partial-root work separately. No new match is needed just to correct the wording.
- **Experiment inventory:** The [plan](making-stello-6-0-plan.md#L312) calls `4a-4b` hash-move **after** ETC; the [findings](making-stello-6-0-findings.md#L746) say **before**. Check the experiment record and align them. The original Item 4B plan at `0a02d7d` also called for a dedicated depth-3 static ProbCut and a unified-TT error-model refit. Map each to its actual experiment/result or mark it deferred. “Static pre-filter adjustments” alone do not establish that either was tested.

## Verified baseline and sign-off

| Check | Result to preserve or compare against |
|---|---|
| Paired strength, Item 4 vs Item 3 | [500 ms/move](benchmark-results/match_p4_item4_vs_item3_500ms.json): **703.5/1,174 = 59.92%** (paired 95% CI **57.90-61.95%**). [2 s/move](benchmark-results/match_p4_item4_vs_item3_2000ms.json): **337/600 = 56.17%** (**53.38-58.95%**). Independently recomputed from color-swapped opening pairs; both pass for the combined build. |
| Deep pure midgame, 24 boards, one thread | Independently reproduced **41,756,742 nodes at d20**, **77,938,163 at d22**, effective branching factor **1.366**. [Archived d22](making-stello-6-0-findings.md#L737): **494.60M to 77.94M nodes**, **19.62 s to 3.38 s**. Fixed-depth throughput actually fell from about **25.21M to 23.03M nodes/s**: the gain is from a **smaller tree**, not cheaper nodes as in Item 3. Match-wide NPS and reported heuristic depth are different measurements. |
| Native tests and signatures | Independent MSVC Release build; six focused tests passed: ETC parent store, recursion/endgame isolation, WLD admission, TT lower-bound move preservation, all 105 signatures, and selective endgame/game-clock behavior. [Signature delta](making-stello-6-0-findings.md#L757) against Item 3: all 105 moves/kinds/depths unchanged; five exact FFO scores **and nodes** unchanged; all 100 selective d10 node counts changed (**-681,009** net), with four selective score changes. The full five-preset, managed and WASM results are the implementing agent's, not independently rerun. |

Leave the [ETC parent store](../Stello.Cpp/src/search/engine_impl.hpp#L1199) and the rejected physical-stability, aspiration and PV-extension variants alone. For sign-off, report the verifier policy chosen and its cost/accuracy tradeoff, WLD admissions versus actual proofs under **time per game**, and a nonzero-level worker test that would catch lost propagation. Rerun the affected native tests and signature regression against `baf118c`; investigate unexpected pure-midgame or exact differences instead of updating baselines to make the tests pass.
