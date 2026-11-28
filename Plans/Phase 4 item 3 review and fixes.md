# Phase 4 item 3: shallow-search review and fixes

**Handoff for the implementing agent, 28 November 2026.** Reviewed `bb8bded` (Item 2 after its review fixes) to `2961a05` (Item 3). **Keep the dedicated shallow null-window search (`3A-1`, variant `m4`).** I found no production search-correctness regression, and the two archived matches pass the written strength gate. The work below is test and documentation cleanup, not a request to change move ordering, retune the clock, or repeat the matches. The [Items 1 and 2 handoff](Phase%204%20items%201%20and%202%20review%20and%20fixes.md) is historical; its fixes are already in the control commit.

## 1. Make the native shallow-search test exercise the search, not just the table

**Finding.** In [the parity test](../Stello.Cpp/tests/search_tests.cpp#L1660), the full-window reference search writes an exact entry into the same transposition table (TT) the scouts then read. I reproduced this on `g01e48` with `hash_h=4`: at **each** of `look=0,1,2`, the five scouts together visited **five nodes and evaluated zero leaves**. The `hash_h=0` cases do exercise the kernels. The [shared assertion](../Stello.Cpp/tests/search_tests.cpp#L1651), however, only checks which side of `alpha` the score lands on. It would accept an impossibly large fail-high score or a legal move that cannot produce that score.

**Fix.** Extend the existing test rather than importing a large random test suite:

1. Use one thread and `selective=false`. Obtain the full-window value `v` at the same `look` with TT reads and writes disabled. Start each scout with an empty table or explicitly [seed the single entry](../Stello.Cpp/src/search/table.hpp#L287) it is meant to encounter. Keep an exact-hit case as a *separate* one-node test.
2. Cover a cold table, a compatible **one-sided bound that cannot cut**, and a legal **move hint whose score cannot be used**. For the latter, use insufficient depth at `look=1,2`, or insufficient selectivity at `look=0`. On the multi-move fixture, verify that the non-cutting cases actually descend (`nodes > 1` and leaf evaluations with no child entries).
3. For a scout with window `[alpha, alpha+1]` and returned score `g`, assert **`alpha < g <= v`** on a fail-high and **`v <= g <= alpha`** on a fail-low. On a fail-high with legal moves, independently score the returned move at the same horizon and check its value is **at least `g`**, not necessarily equal to `v`. A scout can legitimately stop at the first adequate move. Retain the pass, double-pass, forced-extension and wipeout cases.

**Concrete regression fixture.** On [g01e48](../Stello.Net/tools/Stello.Benchmark/Data/midgame-deep.txt#L4), Black to move, `look=0`, `ply=1`, the [existing TT regression](../Stello.Cpp/tests/search_tests.cpp#L362) establishes **c4 = -193**, **h3 = -1131**. Seed a same-depth upper bound of `-173` with h3 as its hint, then scout `[-194,-193]`. The upper bound cannot cut and h3 cannot establish a fail-high. The search must go beyond the hint, return `-193`, and identify a move supporting it. Do not pin a tie-break if more than one move supports the bound.

**Accept when:** returning the seeded non-cutting score, treating the hint as a cutoff, or returning a fail-high with an unsupported move would make a test fail. No production change or signature refresh is expected.

## 2. Test the time-per-game WLD admission decision deterministically

**Finding.** The [WLD estimator](../Stello.Cpp/src/search/engine_impl.hpp#L90) changed its endgame-to-midgame speed multiplier from `3.5` to `2.0`. For fixed inputs this raises the estimate by 75% before integer rounding. Its [call site](../Stello.Cpp/src/search/engine_impl.hpp#L246) decides whether to *start* the win/loss/draw solve after all selective stages finish in `TimePerGame`. Neither strength match exercised it: both used `TimePerMove`. The [existing wall-clock test](../Stello.Cpp/tests/search_tests.cpp#L1376) passed, but that does not prove which decision it exercised. In my `g01e30` trace all three stages completed; the old and new formulas estimated **31.1 s** and **54.4 s** against a **28.75 s** cap. Both would have skipped WLD on that run. This does not invalidate the original agent's observation on its machine.

**Fix.** Keep the wall-clock smoke test and add deterministic **admit** and **skip** cases against the *production decision*. Use the existing [manual-clock testing pattern](../Stello.Cpp/tests/search_tests.cpp#L1236), or factor the admission decision into a small internal helper used by the real call site. Do not duplicate the formula in the test, expose a public option, or run a long solve just to observe admission.

| Synthetic inputs to the production estimator | Capped remaining budget | Old decision | Expected current decision |
|---|---:|---|---|
| 24 empties, depth done 16, last iteration 100,000 nodes, total 1,000,000 nodes in 100 ms | 200 ms | Admit (137 ms estimate) | **Skip** (240 ms estimate) |
| Same inputs | 300 ms | Admit | **Admit** |

**Accept when:** restoring `3.5` makes the first test fail, and unconditionally skipping WLD makes the second fail. Describe `2.0` as a calibration of admission, **not** a new hard deadline or a strength gain demonstrated by the matches. No evidence here calls for another multiplier change.

## 3. Restore meaningful custom-model ProbCut coverage

**Finding.** The built-in-model portion of [the managed FFI test](../Stello.Net/Stello.Engine.Tests/NativeEngineTests.cs#L57) really compares 100%, 150% and pruning off. The custom-model portion starts from [WrittenModel](../Stello.Net/Stello.Engine.Tests/NativeEngineTests.cs#L319), which sets `Selective=false`, sets the percentage to 150%, and compares exact solves. That portion passes even if the native engine ignores the supplied percentage.

**Fix.** Keep the built-in check. For the custom model, use identical written weights and fresh one-thread engines on the same deterministic **midgame, fixed-depth** fixture. Explicitly enable selectivity for 100% and 150%; keep a pruning-off control. Choose a fixture where ignoring the percentage gives an observable failure (e.g. the two enabled settings would produce identical node counts). Check legal moves, heuristic kind and requested depth. Do not assume node counts are monotonic on *every* board. Keep nonselective comparisons with the frozen C# engine separate; selective move parity with it is not the contract.

**Accept when:** ignoring the percentage for custom weights, or disabling pruning for all three searches, makes the custom-model test fail. Do not weaken the existing built-in test or change the search to satisfy the frozen reference.

## 4. Correct the findings, plan summary and current engine guide

| Where | Correction and acceptance check |
|---|---|
| [Stage 0 leaf-work counts](making-stello-6-0-findings.md#L672) | `41.5M + 10.1M + 4.6M = 56.2M`, **53.2%** of `105.7M`, not 97.1%. This does **not** prove 53.2% is the true share: calls, leaf evaluations and inclusive subtree work are different counters. Recover the definitions, numerator and denominator from the original diagnostics if available. Otherwise remove the unsupported percentage. The reader should be able to reproduce any percentage left in the text from quantities with the same units. |
| [3C decision in the findings](making-stello-6-0-findings.md#L673) and [plan](making-stello-6-0-plan.md#L310) | **98.50-99.46%** of sampled selective re-searches returned `g = alpha+1`. That leaves only one score unit of potential narrowing for those calls. It does not measure the cost of the rare larger overshoots. Report how much re-search work those consumed, in nodes or time, **if measured**; otherwise say **"deferred on frequency evidence; the time share was not established."** No new `[g,beta]` implementation is needed. |
| [Match interpretation](making-stello-6-0-findings.md#L709) | Keep the strength result. At 2 s, **23.20 to 24.20** is a one-ply increase in the **mean reported heuristic depth**, not proof of an extra fully completed ordinary iteration. [Match accounting](../Stello.Net/Stello.Engine/BookMatch.cs#L22) includes partial root iterations and selective terminal passes. If making a completed-depth claim later, record the [ordinary iteration-completion callback](../Stello.Cpp/src/search/engine_impl.hpp#L510) separately; do not infer it from the archived average. |
| [Move ordering](../docs/05-move-ordering.md#L9) and [midgame search](../docs/06-midgame-search.md#L134) | Distinguish one-unit-window scouts at `look=0,1,2` from the generic shallow search. These scouts use a legal TT hint first, then five static square groups, not response-table sorting or per-child response updates. `eval1_nws` may read the TT but does not write; `eval2_nws` keeps eligible writes. The generic shallow path still exists for wider windows and at `look=3`. Update the diagram and prose together; leave historical descriptions alone. |

## Why depth 22 got faster despite searching more nodes

From the [archived one-thread timings](making-stello-6-0-findings.md#L690), on the same 24 boards at the same requested depth:

| | Item 2 | Item 3 |
|---|---:|---:|
| Nodes | 451.73M | 494.60M |
| Time | 33.084 s | 19.530 s |
| Nodes/second | 13.65M | 25.33M |
| Average time per counted node | 73.24 ns | 39.49 ns |

The old path built a move list, consulted reply history when available and ranked the remaining moves before trying the first. Its [rank sort](../Stello.Cpp/src/search/ordering.cpp#L151) compares each key to all the others: 90 comparisons for one sort of ten moves. A null-window scout only needs **one** move that beats its threshold. The [new path](../Stello.Cpp/src/search/engine_impl.hpp#L1011) tries the hash move immediately and otherwise takes moves directly from five bit masks. Both versions can stop at the first cutoff; **only the new path avoids paying to sort the unsearched moves first**.

Sorting and history bookkeeping cost time but add no search nodes. Cheaper ordering sometimes tries more moves before finding the cutoff. Here **9.49% more nodes** at **46.08% less time per counted node** produced **85.47% higher NPS** and **40.97% less time**. This is a whole-search average, not evidence that the evaluation function itself got 85% faster. [Restoring one-ply TT writes](making-stello-6-0-findings.md#L697) cost another 0.821 s in the d22 comparison; removing eligible two-ply writes also lost. The experiments do not isolate an exact percentage attributable to sorting alone. An important [pruning-off control](making-stello-6-0-findings.md#L683) returned identical results on all 24 boards with only **1.12% more nodes** and **46.99% less time** at d12.

## Verification and sign-off

- **Independently checked:** MSVC Release built; the native shallow-kernel, TT move-preservation, wipeout, 105-signature and time-per-game tests passed. A separate exhaustive minimax probe checked **14,210** shallow scout results on **92 positions** across **two CPU tiers**, including passes, forced moves, cold tables and seeded bounds. It found no invalid bound or unsupported fail-high move. The probe was a review tool, not a permanent test dependency.
- **Match arithmetic:** recomputing from the archived color-swapped opening pairs gave **628.5/1,174 = 53.53%** (paired 95% CI **51.48-55.59%**) at [500 ms](benchmark-results/match_p4_item3_vs_item2_500ms.json) and **324.5/600 = 54.08%** (**51.09-57.08%**) at [2 s](benchmark-results/match_p4_item3_vs_item2_2000ms.json). Both clear the [written gate](making-stello-6-0-plan.md#L288). No match or deep timing run was repeated for this review.
- **What not to reopen:** keeping `m4` is supported. The deeper static-ordering, shortlist and stale-hash variants did not show a consistent win. Reverting coarser score resolution was consistent with the gate: the unaligned `q100` variant's interval **48.79-52.91%** did not prove a gain; it did not prove the variant weaker either. See the [experiment results](making-stello-6-0-findings.md#L697).
- **After cleanup:** run the affected native and managed tests plus the existing 105-signature regression. Against Item 2, the [accepted Item 3 fixture](../Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt#L13) retained all 105 moves/kinds/depths and all five exact scores; three selective midgame scores changed. All 100 midgame node counts changed (+653,331 net), as did four of five exact counts (+91 net). **Against Item 3, this cleanup should change none of them.** If it does, investigate the behavioral change rather than regenerating the fixture. Report which tests and documentation corrections were completed and which diagnostic numbers, if any, could not be recovered.
