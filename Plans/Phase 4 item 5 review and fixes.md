# Phase 4 Item 5 review: game-clock fixes and tests

**Handoff for the implementing agent, 28 November 2026.** Reviewed `8bc410d..79d5f0f`. **Keep `79d5f0f` as the control.** The combined `TimePerGame` policy won both matches. Fix the inconsistent deadline at the selective-endgame handover first. Test changes to *when* the engine grants extra time separately; the review did not establish that either proposed restriction plays better.

## 1. Fix the selective-endgame handover

**What is wrong.** [Ordinary midgame iterations](../Stello.Cpp/src/search/engine_impl.hpp#L496) take the budget by value. An extension updates both that copy and the [searcher's live hard deadline](../Stello.Cpp/src/search/engine.hpp#L385). On return, [selective stages 1, 2 and 4](../Stello.Cpp/src/search/engine_impl.hpp#L207) use the caller's **original soft** deadline to decide whether to start, but the **extended hard** deadline once running. The discrepancy is before WLD (win/loss/draw), not in the WLD predictor.

**Fix.** Keep the extension local to ordinary midgame. At selective-stage entry, before stage 1 can start, restore `s.hard_limit_ms = budget.hard_ms` from the same caller budget that supplies the soft deadline. Put this in the common path after single-thread or helper-assisted midgame returns. Do not restart the clock, undo the extension during ordinary midgame, change the existing soft `>` / hard `>=` comparisons, or enlarge the later WLD/Exact caps. Document which budget the selective stages use in the [time-control guide](../docs/09-time-control.md#L85).

**Regression:** use [g21e24](../Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt#L6), one thread, hash bits 16, `TimePerGame(10,000 ms)`, default soft percentage, fresh search state and the [existing manual clock](../Stello.Cpp/tests/search_tests.cpp#L1465). The original soft/hard deadlines are **583/875 ms**; on the reviewed build they have reached **874/1,312 ms** by ordinary depth 16.

| Clock schedule | Expected after fix |
|---|---|
| At completion of ordinary depth 16, set the clock to **584 ms**. | Stage 1 must not start: the original soft deadline has passed. This already works and guards against accidentally extending both deadlines. |
| Keep the clock at zero until the **first stage-1 progress report**, then hold it at **876 ms**. | Stage 1 starts but aborts at the next hard check. Return the legal depth-16 heuristic fallback; no stage 2, WLD or Exact. |

In the second arm the shipped build searched **891,075 more nodes** after the jump and finished stage 1 (heuristic depth 24). Restoring the original hard deadline in the probe reduced that to **33 nodes** and returned depth 16. Assert the stage and fallback, **not** those machine/build-specific node counts. Retain the existing positive low-clock WLD/Exact controls.

## 2. Test the two score-drop rules independently

**Shallow drops.** The [first-root-move check](../Stello.Cpp/src/search/engine_impl.hpp#L701) requires `look >= 7` (root depth 8). The [iteration-end check](../Stello.Cpp/src/search/engine_impl.hpp#L547) does not, although the [test description](../Stello.Cpp/tests/search_tests.cpp#L2251) implies it does. With one thread, hash bits 16, 60,000 ms/game and a manual clock, **86/100** [midgame roots](../Stello.Net/tools/Stello.Benchmark/Data/midgame.txt) had extended by depth 7; **50** had reached the cap. On the [300 exact-scored roots](../Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt), the counts were **279/300** and **198/300**. These are panel frequencies, not frequencies from timed games.

- **Candidate A:** apply `look >= 7` to the completed-iteration drop too. Leave the other triggers and multipliers unchanged.
- **Discriminating test:** on [g21e28](../Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt#L4), G6 stays best, but its score goes from **-306 at depth 1 to -593 at depth 2**. At the depth-2 completion callback set the clock to **2,699 ms**, one millisecond past the original soft deadline. With the guard the engine must stop at depth 2; the shipped rule extends and can begin depth 3. If the shallow policy is retained instead, test that behavior explicitly and correct the test description.

**The same drop can be charged twice.** On [g01e48](../Stello.Net/tools/Stello.Benchmark/Data/midgame.txt#L4) at 60,000 ms/game, C4 remains best from depth 7 to 8, but drops from **218 to 18** centidiscs. The hard deadline goes **1,450 -> 2,030 ms** when C4 is searched first, then **2,030 -> 2,175 ms** at iteration end. No second move change or score drop occurred.

- **Candidate B:** retain the immediate first-move extension so it can rescue the current iteration. Record that the score-drop extension was applied for this iteration; skip the end-of-iteration score-drop charge if already applied. Reset the flag before the next root iteration. Leave the separate best-move-change rule, overall cap and soft-percentage recalculation unchanged.
- **Discriminating test:** at the depth-8 completion callback the first extension has already happened, but the iteration-end check has not. Verify the live hard deadline is **2,030 ms**, then set the clock to **1,400 ms**. One extension gives a **1,353 ms** soft deadline and stops at depth 8; two give **1,450 ms** and admit depth 9. Use fresh state for each arm.

Compare A and B separately with an otherwise identical control before combining them. If a candidate does not improve the policy, keep the measured behavior and document it rather than changing the code solely to match a comment.

## 3. Keep early exit a heuristic; test what it contributes

The [early-exit verifier](../Stello.Cpp/src/search/engine_impl.hpp#L561) compares the incumbent's ordinary score with selective null-window searches of the alternatives **one ply shallower**. Passing means no alternative cleared that search's threshold; it does not prove a three-disc lead at the incumbent's depth or at game end.

Using the [existing depth-12 clock schedule](../Stello.Cpp/tests/search_tests.cpp#L2331) on 100 roots with 28 empties, **29 exited**. On [g74e28](../Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt#L163), the engine chose G2 (**-12** exact) over D1 (**-6**). This is **not** a reason to require a more expensive verifier: a same-depth scout agreed on all 29 exits, and ordinary search still chose G2 through depth 18.

**Tests and documentation.**

- **Accept:** replace the positive test's run-time search for any board that exits with a named fixture. Start with [g23e28](../Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt#L10): in the depth-12 verifier probe F2 scored **1,206** and no alternative cleared **906**. With the production clock schedule, assert that the stability and clock gates were reached, that alternatives were actually checked, and that depth 13 did not start.
- **Reject:** keep [g24e28](../Stello.Net/tools/Stello.Benchmark/Data/midgame-exact.txt#L13). The incumbent F2 was stable for 11 iterations, but D1 cleared the heuristic threshold (**-181** bound against **-182**), so depth 13 started. Test that rejection, not just the final depth or the fact that F2 and G4 tie under exact play.
- Keep `TimePerMove` as the no-early-exit control. Clarify the meaning of "verified" in the guide; do not make a regression test require G2 on g74e28.

## 4. Strength evidence and sign-off

These are independent recalculations from the archived **color-swapped opening pairs**, not new matches. The 30-second confirmation uses the first 300 of the same 587 openings.

| Game clock | Item 5 points | Paired 95% interval | Thinking time/game: Item 5 vs Item 4 |
|---|---:|---:|---:|
| [15 s, 587 pairs](benchmark-results/match_p4_5dtpg_vs_base_game15s.json) | 618.5/1,174 (52.68%) | 51.01-54.36% | 7.84 vs 6.83 s |
| [30 s, 300 pairs](benchmark-results/match_p4_5dtpg_vs_base_game30s.json) | 316.5/600 (52.75%) | 50.37-55.13% | 15.12 vs 13.19 s |

Both pass the strength gate for the **combined** policy. It spent about **15% more clock**; the results do not establish faster search, deeper completed ordinary iterations, or an independent benefit from early exit. The rejected [IIR and history variants](making-stello-6-0-findings.md#L809) did not establish a strength gain; no rerun is requested for them.

1. **For the deadline fix:** run `tpg_dynamic_time_and_verified_early_exit_tests`, `selective_endgame_progress_and_time_per_game_tests`, `time_budget_and_for_solver_tests`, `wld_admission_decision_tests`, `progress_and_time_limit_tests`, and `stello_v3_all_105_signatures_match_baseline`. All six passed on the reviewed MSVC Release build **before** the fix. The [native runner](../Stello.Cpp/tests/test_main.cpp#L31) accepts one filter at a time; check for a named `[PASS]`, not just exit code zero. No fixed-depth signature change is expected.
2. **Before promoting a playing-policy change:** match the handover fix alone against `79d5f0f`; use the same handover on both sides for later experiments. To isolate early exit, compare **extension only** with **extension plus early exit**. To isolate smart allocation, compare against a **simple fixed increase to the ordinary move budget**, calibrated on separate games to match actual mean spending. Give both sides the same game clock; keep solver caps, hash, book and thread settings identical. Screen at 15 s/game and confirm promising changes at 30 s/game with the same paired-opening method.
3. **Record clock overruns during new matches.** The [match runner](../Stello.Net/Stello.Engine/BookMatch.cs#L285) subtracts elapsed time but plays on and scores final discs even if a clock expires. The archived results do not establish that any game did or did not flag. Record each side's elapsed time and the first side to exceed its allowance per game; report the ordinary disc result and a flag-adjusted result separately. Do not retroactively label the original wins as time losses. Report actual time spent alongside points so a larger budget is not mistaken for better allocation.
