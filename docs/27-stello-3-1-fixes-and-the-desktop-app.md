# 27 – Stello 3.1: fixes and the desktop app

[Back to the index](README.md)

## In short

Stello 3.1 is a fix release after the Rust engine and threads of 3.0 ([chapters 25](25-phases-0-4-the-engine-in-rust.md) and [26](26-phases-5-9-threads.md)). The work was done in phases 0–5 of [making-stello-3-1-plan.md](../Plans/making-stello-3-1-plan.md); the decisions, tests and measurements are in [making-stello-3-1-findings.md](../Plans/making-stello-3-1-findings.md).

- **Real bugs:** stopped solves no longer claim an unproved exact result, clocks and the endgame time budget are corrected, and browser workers and late progress reports are handled more reliably.
- **Search details:** the old C# engines remain frozen references, not a reason to keep a quirk. Four search fixes and consistent counting were kept; the other proposed changes were dropped or needed no fix.
- **The apps:** both can play Computer vs Computer. The Windows app now has the web app's layout, board choices, animation, sound and Help, with shared settings and view-model state.
- **The tools:** matches can be resumed, use a physical-core-aware worker default, warm both sides before play, and run the Egaroucid port with stricter timing. FFO #50–59 are available and were solved correctly.
- **In play:** four final comparisons against the actual 3.0 engine found no statistically significant difference; this is a correctness release, not evidence of a strength gain. Against the Egaroucid port, Stello scored 74.4 % at 100 ms and 67.4 % at 500 ms per move.

## Phase 1: real bugs

| Problem | Fix |
|---|---|
| An interrupted solve could report a bound as an exact score | A stopped exact pass keeps the completed win/loss/draw result. During an unfinished win/loss/draw pass, an unproved draw or loss is not reported as a solved result; a winning move can still prove a win ([item 1](../Plans/making-stello-3-1-findings.md#item-1-an-aborted-solve-was-reported-as-exact-fixed-2026-11-04)). |
| The natural delay was charged to the computer's game clock | The time charged is captured when the engine returns, before the 250 ms display delay; clock ticks pause during that delay ([item 2](../Plans/making-stello-3-1-findings.md#item-2-natural-delay-charged-to-the-computer-clock-fixed-2026-11-05)). |
| Back and Forward did not restore both clocks reliably | Both clocks are saved for each ply and restored for the destination position. A loaded game has full clocks because its file has no clock history ([item 3](../Plans/making-stello-3-1-findings.md#item-3-clock-restore-after-back-and-forward-fixed-2026-11-05)). |
| The endgame soft deadline omitted time already spent searching | The deadline is offset from the whole search's start, and a custom percentage applies to the remaining endgame budget. Matches at one and five minutes per game met the agreed not-worse gate ([item 4](../Plans/making-stello-3-1-findings.md#item-4-endgame-soft-limit-ignores-elapsed-search-time-fixed-2026-11-05-both-matches-complete)). |
| A failed browser pool worker was not retried; late progress could replace a final result | The worker retries a failed start once, reports and uses the threads that actually started, and prevents a failed pool worker from restarting the whole engine. A per-search gate drops progress after completion in both apps ([item 5](../Plans/making-stello-3-1-findings.md#item-5-web-worker-robustness-and-late-progress-reports-fixed-2026-11-06)). |
| A few documentation statements were wrong | Corrected the perft figure, the Stello 1 search description, a menu prompt and audit figures ([item 6](../Plans/making-stello-3-1-findings.md#item-6-wrong-statements-in-the-docs-fixed-2026-11-06)). Two stale score-display tests were corrected in phase 3; the app's display was already right. |

The stopped-solve bug also matters to the opening book: 2,988 entries marked exact came from time-limited searches and might include bounds. That does not prove any particular entry wrong. They are marked for the later book recalculation; the 1,091 exact entries without a time limit are not affected ([findings](../Plans/making-stello-3-1-findings.md#item-1-an-aborted-solve-was-reported-as-exact-fixed-2026-11-04)).

## Phase 2: the parity quirks

Stello 3.0's Rust port deliberately matched the frozen C# engine node for node. In 3.1 each suspected quirk was investigated on its merits: a correctness fix could be kept if the match did not show worse play under the agreed gate, while a strength-only change needed evidence of a gain.

| Item | Verdict and reason |
|---|---|
| [1. Upper bound equal to alpha](../Plans/making-stello-3-1-findings.md#item-1-an-upper-bound-equal-to-alpha-2026-11-06) | **Kept:** equality also proves a cutoff. The old strict comparison merely searched more. |
| [2. Upper-bound hash replies in the response table](../Plans/making-stello-3-1-findings.md#item-2-upper-bound-hash-cutoffs-and-response-killers-investigated-2026-11-06) | **Unchanged:** a harmless move-ordering heuristic; disabling it gave no reliable gain. |
| [3. Hash-table aging and tighter bounds](../Plans/making-stello-3-1-findings.md#item-3-aging-transposition-table-entries-2026-11-06-3a-kept-3b-dropped) | **Aging kept (3a):** entries from positions with more empty squares than the current root are stale. **Tighter-bound retention dropped (3b):** no demonstrated gain. |
| [4. Endgame ETC move and split timing](../Plans/making-stello-3-1-findings.md#item-4-endgame-deep-node-stored-moves-and-split-timing) | **Kept:** retain the move that supplied the best cached fail-low score, and count a searched hash move when deciding whether to split. |
| [5. Already-decided endgame moves in the sort](../Plans/making-stello-3-1-findings.md#item-5-local-table-decided-moves-stay-in-the-sort) | **Kept:** remove them from the active sort, so they cannot disturb the ordering of moves still to search. |
| [6. Response ordering](../Plans/making-stello-3-1-findings.md#item-6-response-ordering) | **Unchanged:** swapping the hash move affects ties, and a last remaining move needs no sort. |
| [7. Full windows at shallow root depths](../Plans/making-stello-3-1-findings.md#item-7-full-windows-for-shallow-root-moves) | **Unchanged:** depths 1–3 use plain alpha-beta; trying PVS at every depth showed no consistent gain. |
| [8. Multi-ProbCut and the hash table](../Plans/making-stello-3-1-findings.md#item-8-multi-probcut-and-the-transposition-table) | **Unchanged:** shallow searches use the normal table. Suppressing their root stores searched 4.7 % more nodes at depth 10 and 6.2 % more at depth 12. |
| [9. Node and evaluation counts](../Plans/making-stello-3-1-findings.md#item-9-consistent-node-and-evaluation-counts) | **Kept:** counted nodes and passes share the abort-check path, and terminal-score evaluations count consistently. Search results stayed the same in the measured positions. |

The strict current-engine signature file was regenerated where a kept fix changed nodes; the C#, Rust and Node checks still compare nodes as well as moves and scores. The frozen C# 2.0 signatures remain separate ([item 1 review](../Plans/making-stello-3-1-findings.md#item-1-an-upper-bound-equal-to-alpha-2026-11-06)).

### Gate P2: the combined search fixes

The combined candidate played against a temporary switch that restored the old search choices. Both sides included the phase 1 fixes and the counting change; this was not yet a comparison with an untouched 3.0 build. One thread per engine, 12 workers, both colors per start ([Gate P2](../Plans/making-stello-3-1-findings.md#gate-p2-all-kept-fixes-against-30-2026-11-07)):

| Time per move | Games | 3.1 score | 95 % score interval |
|---|---:|---:|---:|
| 500 ms | 1,180 | 603.5 points, 51.1 % | 49.5–52.8 % |
| 2 s | 600 | 299 points, 49.8 % | 47.9–51.7 % |

Both upper bounds exceed 50 %, the agreed not-worse criterion, so the fixes were kept. Both intervals also include 50 %: neither result shows a significant gain or proves non-inferiority. The temporary switch was removed.

## Phase 3: the apps

Both front ends share `MainViewModel`, settings, appearance and sound preferences. The desktop and web controls use the same transposition-table choices; each app persists its settings in its own store ([shared foundation](../Plans/making-stello-3-1-findings.md#step-2-shared-app-foundation-2026-11-07)).

**Computer vs Computer** makes both sides computer players, uses the current settings and alternates moves until the game ends. Move Now advances the current move; New Game, Undo and Switch Sides interrupt the current search safely. Switch Sides changes to a game with the human playing White ([step 3](../Plans/making-stello-3-1-findings.md#step-3-computer-vs-computer-2026-11-07)).

The WPF app gained the web layout's player cards, clocks, move history, analysis and quick actions, along with green and wood boards, 3D pieces, flip animation, synthesized sound and Help. It keeps its desktop book-learning commands. Appearance and sound survive a restart, and Windows' client-area animation preference is respected. The user checked the desktop app on Windows at [Gate P3](../Plans/making-stello-3-1-findings.md#gate-p3-2026-11-08); [chapter 15](15-gui-and-settings.md) describes the controls.

The [Stello 1 archive](stello-1/README.md) stays frozen and linked to GitHub. It is not rendered in the shared GameShell viewer; GitHub already renders its formulas and diagrams ([archive decision](../Plans/making-stello-3-1-findings.md#step-6-the-stello-1-archive-2026-11-08)).

## Phase 4: tools and verification

| Change | What it does |
|---|---|
| [Match save and resume](../Plans/making-stello-3-1-findings.md#step-3-match-save-and-resume-2026-11-08) | Each finished game is flushed to a `.games.jsonl` journal. `--resume` checks the sides, limits, seeds and starts, then plays only missing games and regenerates the reports. The journal is git-ignored; the Markdown and JSON reports are kept. |
| [Match workers](../Plans/making-stello-3-1-findings.md#step-2-default-match-workers-2026-11-08) | The default is physical cores divided by the larger side's threads, rounded down with a minimum of one. `--workers` overrides it; if core detection fails the tool asks for an explicit value. |
| [Egaroucid time](../Plans/making-stello-3-1-findings.md#step-4-egaroucid-port-time-2026-11-08) | `time-strict=on` is the match-side default. It charges book checks, limits the clog search to a quarter of the remaining budget, and can return a legal move without an unbounded fallback. Direct port calls remain non-strict by default for the C++ parity checks. |
| [FFO #50–59](../Plans/making-stello-3-1-findings.md#step-5-ffo-range-and-sample-2026-11-08) | Both benchmark tools accept the inclusive range. One Rust run solved all ten with the expected moves and scores, in 690.4 s on this machine; #55 alone took 332.6 s. No C++ timing comparison was found. |
| [Warm-up](../Plans/making-stello-3-1-findings.md#step-6-side-a-cold-start-timing-2026-11-08) | Before a worker's first measured game, both players make an unscored, no-book search; their state is reset before play. This moves the warm-up's first-use work outside the move timings. A small check was consistent with a smaller side-A timing bias, but was not a controlled comparison. |

[Chapter 17](17-measuring-the-engine.md) documents the benchmark commands and match reports.

## Phase 5: final matches and version

### Against the actual Stello 3.0

A separately built native library from tag `v3.0` was checked against the saved 3.0 search results before play. The 3.1 and 3.0 sides used hash bits 19, no book, and both colors for each start. The one-thread matches used 12 workers; the eight-thread match used two ([setup and verification](../Plans/making-stello-3-1-findings.md#the-30-comparison-separate-native-build-2026-11-08)).

| Match | Games | 3.1 points / score | 3.1 score 95 % interval | Duration |
|---|---:|---:|---:|---:|
| [100 ms, 1 thread](../Plans/benchmark-results/match_p5-v30-100ms.md) | 1,180 | 586.5 / 49.7 % | 47.8–51.6 % | 4m 34s |
| [500 ms, 1 thread](../Plans/benchmark-results/match_p5-v30-500ms.md) | 1,180 | 586.5 / 49.7 % | 48.2–51.2 % | 21m 12s |
| [2 s, 1 thread](../Plans/benchmark-results/match_p5-v30-2s.md) | 600 | 309 / 51.5 % | 49.5–53.5 % | 40m 51s |
| [500 ms, 8 threads](../Plans/benchmark-results/match_p5-v30-8t-500ms.md) | 1,180 | 595 / 50.4 % | 48.4–52.5 % | 1h 59m |

Every interval includes 50 %, and every upper bound exceeds it: the results pass the agreed not-worse rule but do not establish a strength gain. The 500 ms and 2 s comparisons use the actual tag rather than Gate P2's partial switch. The starts overlap across runs, so the results should not be pooled as independent samples. All 4,140 game records were reconciled with the reports ([findings](../Plans/making-stello-3-1-findings.md#final-matches-1-4-actual-30-reference-2026-11-08)).

### Against the Egaroucid port

Both matches used `time-strict=on`, one thread per engine, 12 workers and 590 starts with both colors, without books. Hash bits were 19 for Stello and 21 for the port.

| Time per move | Stello points / score | Stello W–D–L | Port's longest move | Duration |
|---|---:|---:|---:|---:|
| [100 ms](../Plans/making-stello-3-1-findings.md#final-match-5-egaroucid-strict-time-at-100-ms-2026-11-08) | 878 / 1,180, 74.4 % | 819–118–243 | 124 ms | 4m 57s |
| [500 ms](../Plans/making-stello-3-1-findings.md#final-match-6-egaroucid-strict-time-at-500-ms-2026-11-08) | 795.5 / 1,180, 67.4 % | 719–153–308 | 553 ms | 22m 47s |

Stello was stronger in these comparisons. Strict timing bounds the port's search work, but does not make the wall-clock deadline absolute: the longest measured 500 ms move was 53 ms over.

### Version and checks

The Rust workspace is 3.1.0, `stello_version()` reports 30100, and the native ABI remains 3. The shared About box says Stello Version 3.1. The Release build had no warnings or errors; 87 Rust, 497 engine, 115 app and 15 Node tests passed. The Egaroucid/benchmark suite passed 97 tests, with eight optional tests of the separately built 3.0 library skipped in that run ([findings](../Plans/making-stello-3-1-findings.md#step-2-version-31-2026-11-08)).

The release-readiness review and deployment follow this documentation. Firefox and Safari checks are a manual checklist for the user after deployment, not claimed as passed here ([platform checks](../Plans/making-stello-3-1-findings.md#step-7-platform-checks-2026-11-08)).

## Further reading

- [The 3.1 plan](../Plans/making-stello-3-1-plan.md) and [findings](../Plans/making-stello-3-1-findings.md): the full decisions, checks, measurements and match reports.
- [Chapter 06](06-midgame-search.md), [chapter 07](07-transposition-table.md), [chapter 08](08-endgame-solver.md) and [chapter 09](09-time-control.md): the current search, tables, solver and time control.
- [Chapter 14](14-app-integration.md), [chapter 15](15-gui-and-settings.md) and [chapter 17](17-measuring-the-engine.md): the apps and the benchmark tools.

---

Previous: [26 Phases 5–9: threads](26-phases-5-9-threads.md) · Next: [28 Glossary](28-glossary.md)
