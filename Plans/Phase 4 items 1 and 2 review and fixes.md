# Phase 4 items 1 and 2: review and fixes

**Handoff for the implementing agent, 27 November 2026.** The review covers `88c59a0` through `4fe801f`: hash-first ordering (`2220202`), the leaf wipeout fix (`df53e55`), and the unified transposition table (`9cd7a6f`). **Fix the cached score/move mismatch first.** The first section preserves the review as delivered; the second gives reproduction steps and acceptance checks. These are recommendations, not changes already made.

## Review as delivered

**I would keep Item 1 and fix Item 2’s cached-move handling before signing it off.** I reviewed `88c59a0` through `4fe801f` and found four issues.

1. **Item 2 can return the right cached score with the wrong move.** The [dual-bound merge](../Stello.Cpp/src/search/table.cpp#L124) can retain a strong lower bound but replace its move with one from a weaker search. I reproduced this on `g01e48`, on one thread with ProbCut off: a full one-ply search found **c4 = −193**; a later narrow search found **h3 = −1131**. The next [exact table hit](../Stello.Cpp/src/search/engine_impl.hpp#L737) returned **h3 = −193**. Searching h3 alone confirmed **−1131**. This gives move ordering a false best move; I did not observe a wrong root score. Preserve the move that established the retained lower bound and add this sequence as a regression test.

2. **The .NET wrapper and settings still describe the old table.** At the default size, the [wrapper](../Stello.Net/Stello.Engine/SearchEngine.cs#L220) reports **64 MiB** of native hash memory to the garbage collector; the engine now allocates **32 MiB**. The [desktop and web settings labels](../Stello.Net/Stello.App/ViewModels/SettingsViewModel.cs#L15) advertise **1,048,576 entries / 64 MB** instead of **2,097,152 shared entries / 32 MiB**. Correct the accounting and labels while preserving existing users’ hash-bit setting.

3. **Item 1D’s non-wipeout pass test never evaluates a leaf.** The [test](../Stello.Cpp/tests/search_tests.cpp#L1486) takes the [single-move shortcut](../Stello.Cpp/src/search/engine_impl.hpp#L124): depth 0, zero nodes. Neither side can move after a1, so the position is also game over rather than a pass. I verified a small repair: add a white disc on **d1** and restrict the root to **a1**. White then has no reply, Black can play e1, and the depth-1 leaf is evaluated. The positive wipeout cases passed.

4. **The replacement policy prefers deep entries; it does not protect them absolutely.** Contrary to the [findings](making-stello-6-0-findings.md#L616), the [replacement code](../Stello.Cpp/src/search/table.cpp#L137) always admits a new key. I filled one bucket with four depth-28 exact-stage entries; a colliding depth-14 selective entry displaced one. This costs cache work, not score correctness. I would correct the claim rather than change the policy without measurements.

**Performance verdict:** Item 2 halved table memory, and its [one-thread d20→22 effective branching factor](making-stello-6-0-findings.md#L635) improved from **1.6786 to 1.6403**. It did **not** pass the [written strength gate](making-stello-6-0-plan.md#L288): **50.5% (48.9–52.1%)** at 500 ms and **50.2% (47.7–52.7%)** at 2 s. Keeping it for memory and search efficiency is defensible; a playing-strength gain remains unproven.

I independently built MSVC Release, ran the focused table, wipeout, concurrent-solve and signature tests, and verified that all **105** archived signatures retained their moves, scores, kinds and depths. No repository files changed and no new matches were run.

## Fixes and acceptance checks

### 1. Keep the move that supports the retained lower bound

Reproduce this with the real [g01e48 board](../Stello.Net/tools/Stello.Benchmark/Data/midgame-deep.txt#L4), Black to move, the built-in evaluator, a fresh engine at `hashBits=12`, one thread and `selective=false`. Use the same searcher and table for the first three calls. Pass `look=0`, `ply=1`, `previous_move=-1` to the interior search. Set `hash_put_height=1` throughout.

| Step | Hash read height | Window / operation | Result on the reviewed build |
|---|---:|---|---|
| 1 | 0 | Search `[-32665, 32665]` | Stores exact `[-193, -193]`, move c4. |
| 2 | 0 | Search `[-6401, -6400]` | Fails high on h3 at `-1131`. This is a valid but weaker lower bound. |
| 3 | 1 | Search `[-32665, 32665]` | One-node exact hit returns `-193` **with h3**. It must return c4. |
| 4 | — | Fixed depth 1, root restricted to h3 | h3 actually scores `-1131`. |

Steps 1–2 deliberately permit writes but not reads at that ply. This is not a simulated race: [ordinary iterative deepening sets the write height one ply beyond the read height](../Stello.Cpp/src/search/engine_impl.hpp#L494). If reads are enabled throughout the test, step 2 can cut immediately and hide the bug.

**Fix:** In the [same-level merge](../Stello.Cpp/src/search/table.cpp#L103), select the move according to which search supplied the **retained lower bound**, rather than simply taking the last fail-high move. Keep c4 when a weaker lower bound arrives; adopt a valid new move when it establishes a stronger lower bound or a fresh exact value. An upper-only update cannot establish the move for an existing lower bound. Where there is only an upper bound, a move can still be an ordering hint. Preserve the existing rules for contradictory bounds, depth/selectivity changes and score domains; this does not require a wider table entry.

**Accept when:** Step 3 returns `(-193, c4)` in one node. Extend the [dual-bound tests](../Stello.Cpp/tests/search_tests.cpp#L264) to exercise both directions: a weaker fail-high must **not** steal the move; a stronger one with a valid move **must** replace it. Keep the existing meeting-bounds, contradiction, and domain-isolation cases. Re-run the signature check; if any counts change, investigate rather than updating the baseline blindly.

### 2. Correct memory accounting and labels without migrating saved settings

The native layout is [four entries per 64-byte bucket](../Stello.Cpp/src/search/table.hpp#L160). For `b` hash bits there are `2^b` buckets: **bytes = `64 × 2^b`; shared entries = `4 × 2^b`**. The smallest compatible fix is to change the [managed bytes-per-slot constant and its comment](../Stello.Net/Stello.Engine/SearchEngine.cs#L30) to one 64-byte bucket, and correct the display text in the [shared settings choices](../Stello.Net/Stello.App/ViewModels/SettingsViewModel.cs#L10). The [web form](../Stello.Net/Stello.Web/Components/SettingsForm.razor#L31) uses the same choices.

Keep the stored setting values and [their conversion to hash bits](../Stello.Net/Stello.App/Models/GameSettings.cs#L37). They are existing preference identifiers, not the new displayed entry counts. Changing both at once would silently resize the table for existing players.

| Existing saved value | Hash bits (unchanged) | Shared entries to display | TT memory |
|---:|---:|---:|---:|
| 65,536 | 15 | 131,072 | 2 MiB |
| 262,144 | 17 | 524,288 | 8 MiB |
| 1,048,576 (default) | 19 | 2,097,152 | 32 MiB |
| 4,194,304 | 21 | 8,388,608 | 128 MiB |
| 16,777,216 | 23 | 33,554,432 | 512 MiB |

**Accept when:** A previously saved default still selects `hb=19`; the label reads 2,097,152 shared entries / 32 MiB; and the wrapper reports 33,554,432 bytes to the GC. Update the [settings choice test](../Stello.Net/Stello.Net.Tests/NewGuiFeaturesTests.cs#L189) and current [table](../docs/07-transposition-table.md), [app integration](../docs/14-app-integration.md) and [settings](../docs/15-gui-and-settings.md) documentation. Keep historical release measurements labeled as historical.

### 3. Make the negative wipeout test search a real pass

Change **only the negative case** in the [wipeout regression](../Stello.Cpp/tests/search_tests.cpp#L1403). Use Black **c1**, White **b1, d1, h8**, Black to move. Call the regular search at fixed depth 1 with `only_moves = a1`. After a1, assert that White still has discs, White has no legal reply, and Black has **e1**. This distinguishes a pass from both a wipeout and game over.

**Accept when:** The result is a1, `Heuristic`, depth 1, with at least one node and a score strictly inside the terminal-win band. The review probe observed one node and `-850`; do **not** pin that evaluator-dependent number. Keep the existing checks for the real b5 wipeout, color swap, restricted roots, and interior leaves.

### 4. Correct the replacement and strength claims; do not retune by default

Use this wording for the [Item 2 architecture finding](making-stello-6-0-findings.md#L616): **“For a new key in a full bucket, depth and selectivity help choose the resident to evict. They do not prevent a shallow incoming entry from replacing a deep one.”** A weaker update to the *same* key is a different case and [is refused](../Stello.Cpp/src/search/table.cpp#L96).

For the [PV-table experiment](making-stello-6-0-findings.md#L645) and [plan summary](making-stello-6-0-plan.md#L303), say **“The separate PV table saved no nodes on the tested one-thread suites,”** not “PV entries cannot be evicted.” There is no reason to reverse the experiment’s rejection. Likewise, keep the match results and state the decision explicitly: **retained for memory and search efficiency; the playing-strength gate was not met**. Both [500 ms](benchmark-results/match_p4_2_vs_p4_1_500ms_1t.md) and [2 s](benchmark-results/match_p4_2_vs_p4_1_2000ms_1t.md) intervals include 50%.
