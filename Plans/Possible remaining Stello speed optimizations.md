# Possible remaining Stello speed optimizations

**2026-11-26 | C++ engine at `36f1242` (Phase 2 engine: `b0aeaf7`).** I found four experiments for which I could not find a Stello A/B result. **Try the 7-to-6-empty reply handoff first:** the caller already has exactly the bitboard the callee needs. The best native evaluator lead is to widen the gathered weights once instead of eight times. Neither is a measured search speedup.

| Priority | Experiment | First workload | What would make it lose? |
|---|---|---|---|
| 1 | Keep the reply mask across the 7-to-6-empty handoff | FFO #40-49, 1 thread; then WASM ST | Few visited children reach move generation with a known mask |
| 2 | Add packed evaluation weights before selecting the ply | Native `eval_sum`, `eval_after_move`; then midgame depths 12/14 | Gather latency hides the arithmetic saved |
| 3 | Give ProbCut the static child score just computed for move ordering | Midgame depths 12/14 with MPC on | Too few scores consumed to pay for carrying them |
| 4 | Share unchanged pattern weights between siblings | WASM ST deep ordering first; native second | Indexing and scratch traffic cost more than the avoided table reads |

### 1. Keep the reply mask across the 7-to-6-empty handoff

At seven empties, [`end_simple` computes each eligible child's legal moves](../Stello.Cpp/src/endgame/search_impl.hpp#L819) to score mobility. Both the immediate low-mobility recursion and the later sorted recursion pass `mv.replies`. At six empties [`end_simple` forwards to `end_fast` without that argument](../Stello.Cpp/src/endgame/search_impl.hpp#L765); after its usual node count and stability check, [`end_fast` generates the same mask again](../Stello.Cpp/src/endgame/search_impl.hpp#L889).

Pass the mask through that one boundary. Use it **where the existing generation occurs**, not before the stability check. Zero is a known pass, not “unknown.” The single-child path can have `LEGAL_UNDEFINED`; retain generation there, on a side swap, and for ordinary recursive calls without a mask. Keep the lone-empty priority and bit iteration unmodified. Unlike the planned Phase 5 shallow solver, this changes neither which move is tried first nor how a pass is scored.

**Evidence:** enumerating only the seven-empty roots in [the low-empty corpus](../Stello.Net/tools/Stello.Benchmark/Data/endgame-random-low.txt) gave 96 parents and 479 legal children; nine children had **zero** replies. These are useful test cases, not 479 guaranteed savings: table hits, stability and cutoffs can intervene. Count *known-mask arrivals that actually reach generation* at six empties before interpreting FFO timings.

### 2. Add packed weights first; select and widen once

The 64-feature native path in [`sum_gather_avx2`](../Stello.Cpp/src/search/eval_avx2_detail.hpp#L15) gathers eight vectors of eight packed `(even, odd)` weight pairs. Today it selects and sign-extends the desired half of **each** gather before adding in 32 bits. Keep the same aligned gathers and indices, but sum the signed halfwords independently (`VPADDW`), select the requested half of the eight final pairs, then widen and horizontally sum in **32 bits**. Ordinary dword addition is *not* equivalent: low-half carries can corrupt the odd slot.

**Why this is exact for the shipped model:** I decoded the [Format v3 model](../Stello.Net/Book/stello-eval.bin) using the [loader's stage-delta format](../Stello.Cpp/src/eval/weights.cpp#L306). Across 60 stages and 320,193 representative pattern weights per stage, the extremes were **-704 and +1,000**. Symmetry expansion and odd-slot permutation copy weights; they do not enlarge them. Each half-lane adds just eight terms: even the conservative range **[-5,632, +8,000]** fits `int16`. The *whole* 64-term sum need not fit. For arbitrary loaded weights, determine eligibility once at load (eight times each extreme must fit), or retain today's wide path. Do not clip weights or change the file format.

This is a reduction experiment, **not** another scalar-versus-gather or 2-byte-stride experiment. [Egaroucid's `calc_pattern`](../Egaroucid-main/src/engine/evaluate_simd.hpp#L498) also delays extraction using a range proof, but its offset weights and gather layout are different. [Edax AVX's `accumlate_eval`](../edax-reversi-AVX-master/src/midgame.c#L36) illustrates the interleaved layout closer to Stello's. Check both ply parities in generated code, retain the zero-ZMM gate, and include an extreme custom model that must take the fallback.

### 3. Reuse the ordering score in the child's ProbCut pre-check

For each non-hash child, [`search_inc_deep`](../Stello.Cpp/src/search/engine_impl.hpp#L900) computes a clamped, **child-perspective** static score including mobility. It stores a composite ordering key, flips and replies, but discards the score. If that child gets past its pass/TT checks and reaches [the MPC pre-check](../Stello.Cpp/src/search/engine_impl.hpp#L1168), [`eval_state_score`](../Stello.Cpp/src/search/kernel.hpp#L146) sums the same pattern weights and generates both sides' mobility again. Parent ordering starts at `look >= 4`; an ordinary child then has `look >= 3`, above the MPC minimum of 2.

Keep the original clamped score by **move square**, not by rank; it fits `int16` and cannot be reconstructed exactly from the rounded ordering key. Consume it only for that same board, side and disc stage, including a same-child PVS re-search. Fall back for the unscored hash move, pass, and callers without an ordering score. Leave the MPC thresholds, shallow search, `count_node()`/abort poll and `evaluations` increment exactly where they are: only substitute the arithmetic. This is distinct from the shipped *reply-mask* reuse and from Phase 4's proposed search-order changes.

Before changing the recursive interface, count scores **produced vs. consumed** in an instrumented run. Include all scored children in the cost, even siblings never searched; only count a saving when a visited child actually reaches the MPC pre-check. Then time an uninstrumented build. A low consumption rate is reason to stop, even though each actual hit avoids one full evaluation.

### 4. Share unchanged pattern weights between siblings

Phase 2 updates the **indices** incrementally, but still reads 64 weights for every scored child. Siblings use the same child-stage table and parity. Use the unchanged parent indices with that **child's** stage and perspective as a common baseline (not the parent's evaluation). Cache the 64 baseline contributions and their sum. For a move, only placements touching the played or flipped squares can change: replace each affected baseline contribution with its new value **once**. [`PatternSet` already records square-to-feature membership](../Stello.Cpp/src/eval/patterns.cpp#L670). Mobility remains child-specific and must still be added normally.

I enumerated every legal move at each corpus root, intersecting its changed squares with the 64 distinct [pattern placements](../Stello.Cpp/src/eval/patterns.cpp#L701). The last column counts **model-table entries**, paying 64 once per parent; it excludes local scratch loads and is **not** a timing estimate.

| Corpus | Parents / legal children | Affected placements per child (mean / median) | Dense reads vs. baseline + affected |
|---|---:|---:|---:|
| [Fixed midgame](../Stello.Net/tools/Stello.Benchmark/Data/midgame.txt) | 100 / 1,025 | 19.53 / 19 | 65,600 vs. 26,421 |
| [Random midgame](../Stello.Net/tools/Stello.Benchmark/Data/midgame-random.txt) | 1,000 / 9,466 | 17.42 / 17 | 605,824 vs. 228,876 |

As a separate arithmetic check, I reconstructed the model tables and compared the dense child pattern sum, the packed-halfword sum from experiment 2, and the sibling-delta sum. **All 10,491 matched**, including 4,638 even-disc and 5,853 odd-disc children. This was an offline check of the pattern component, not a modified engine or a search parity test.

Start with **deep ordering**, where Stello already evaluates all non-hash siblings; don't pay the setup at a leaf that cuts on the first move. Test WASM separately: scalar indexed loads may benefit while eight aligned native gathers still win. Measure actual scored siblings and compare against experiment 2's winner, not against the old scalar evaluator.

### What I would not pursue first

- A cheap stability pre-check is mathematically valid but looks marginal: `stable_opponent <= popcount(opponent)`, so if `64 - 2*popcount(opponent) > alpha`, the existing [null-window stability test](../Stello.Cpp/src/endgame/search_impl.hpp#L29) cannot cut. At fixed test alphas 16, 24 and 32 it would avoid only **39/439, 23/711 and 2/809** threshold-eligible calls respectively on the 5-13-empty corpus roots. These are artificial windows, not a search profile. Leave this alone unless real FFO counters show a different distribution. Do not return the cheap bound as a fail-soft score.
- Do not repeat GFNI flips, LUT-free V4 `last1`, incremental full lines, `min_mob` early-outs, unconditional TT prefetch, insertion sort, or runtime single-thread TT branching. The [Phase 2 findings](making-stello-6-0-findings.md#2-rejected--deferred-experiments-measured-decisions) record those outcomes. Hash-move-first, last-four parity reordering, unified TT and threading are already assigned to later phases and are not substitutes for zero-node speedups.

### Decision gate and audit scope

Use the unchanged Phase 2 binary as A, one candidate at a time as B, identical release flags, model and `hash-bits=19`. On an idle machine, alternate A/B and B/A and repeat the relevant whole-search workload; keep a change only if its wall-time gain exceeds run-to-run variation without a repeatable regression elsewhere. Use primitive cycles to explain a result, not to accept one. For WASM, warm the runtime and compare ST and MT-at-1-thread; multi-worker counts are not deterministic.

Require the same position IDs, move, score, score kind, depth and **exact node count per position** on one thread, across portable/V3/V4 and WASM as applicable. Use the [105-signature test](../Stello.Cpp/tests/search_tests.cpp#L647) and the [incremental evaluator parity fixtures](../Stello.Cpp/tests/parity_tests.cpp#L580). The [.NET `--compare` implementation](../Stello.Net/tools/Stello.Benchmark/BenchCommands.cs#L225) compares only overlapping IDs and only move, score and nodes; also verify complete coverage, depth and kind in the saved results. Do not regenerate signatures. No matches or timing runs were needed for this report.

Novelty here means **no matching Stello experiment found in the recorded work**, not that no one has tried the technique. I compared the [project snapshot](../Content%20and%20state%20of%20the%20Stello%20project%20as%20of%202026.11.26.md), [6.0 plan](making-stello-6-0-plan.md), its findings, earlier plans (especially the [5.0 incremental-eval and gather trials](making-stello-5-0-findings.md)), and current call sites. Reference points included the local Edax, Edax AVX and Egaroucid sources, [Yamana's technical explanation](https://www.egaroucid.nyanyan.dev/en/technology/explanation/), and Okuhara's [bitboard](http://www.amy.hi-ho.ne.jp/okuhara/bitboard.htm) and [Edax optimization](http://www.amy.hi-ho.ne.jp/okuhara/edaxopt.htm) notes. No external engine's timing is presented as a Stello gain.
