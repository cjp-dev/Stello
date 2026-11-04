# Findings from making Stello 3.1 and 3.2

What the work on Stello 3.1 and 3.2 teaches us, written down as we go: the decisions, the measurements, what worked and what did not, and the ideas for later.

- **The plan:** [making-stello-3-1-plan.md](making-stello-3-1-plan.md). This file is updated after every phase.
- **Where it starts:** Stello 3.0 as released on 4 November 2026 (main baa44e5, git tag `v3.0`). Its findings are in [making-stello-3-findings.md](making-stello-3-findings.md), and the docs are [chapter 25](../docs/25-phases-0-4-the-engine-in-rust.md) and [chapter 26](../docs/26-phases-5-9-threads.md).

## Status

| Phase | State | What it showed |
|---|---|---|
| **Part 1: Stello 3.1** | | |
| 0 Set-up and the new baseline | done (2026-11-04) | 3.0 one-thread JSON references, 105-signature baseline test, and Rust-engine `opt=` match switches; Stello tests pass |
| 1 Real bugs | not started | |
| 2 The parity quirks | not started | |
| 3 The apps | not started | |
| 4 Tools and verification gaps | not started | |
| 5 The 3.1 release | not started | |
| **Part 2: Stello 3.2** | | |
| 6 Speed | not started | |
| 7 Threads | not started | |
| 8 Search | not started | |
| 9 The endgame | not started | |
| 10 Evaluation | not started | |
| 11 The opening book | not started | |
| 12 The 3.2 release | not started | |

## Phase 0: set-up and the new baseline (done 2026-11-04)
- **Branch and documents:**
  - the branch `feature/stello-3.1` starts from main baa44e5 (the 3.0 release);
  - the plan is [making-stello-3-1-plan.md](making-stello-3-1-plan.md) (a5b66e4), and this file holds the findings;
  - the git tag `v3.0` marks baa44e5.
- **The one-thread baseline JSONs** are in [benchmark-results/v3.0-ref](benchmark-results/v3.0-ref/). All runs used the Rust engine through `Stello.Benchmark`, 24 logical processors available, and one search thread:

  | Run | Searches | Nodes | Recorded time |
  |---|---:|---:|---:|
  | [FFO #40–49 exact](benchmark-results/v3.0-ref/ffo-40-49.json) | 10 | 1,099,769,823 | 27.88 s |
  | [Midgame depth 10](benchmark-results/v3.0-ref/midgame-depth10.json) | 100 | 3,719,468 | 0.387 s |
  | [Midgame depth 12](benchmark-results/v3.0-ref/midgame-depth12.json) | 100 | 10,582,535 | 1.081 s |
  | [1,000 random midgames, depth 8](benchmark-results/v3.0-ref/midgame-random-depth8.json) | 1,000 | 9,624,573 | 0.923 s |
  | [Handover, depth 12](benchmark-results/v3.0-ref/midgame-handover-depth12.json) | 300 | 320,339,863 | 7.372 s |
  | [Handover, depth 14](benchmark-results/v3.0-ref/midgame-handover-depth14.json) | 300 | 1,842,840,550 | 38.7 s |
  | [Random endgames, 14–24 empties](benchmark-results/v3.0-ref/endgame-random-14-24.json) | 500 | 27,720,283,198 | 567.0 s |

  Midgame JSONs use three passes and retain the fastest time per position; exact solves use one pass. These timings are machine-specific; the references are the per-position search results.
- **The baseline test:** [StelloV3BaselineTests.cs](../Stello.Net/Stello.Engine.Tests/StelloV3BaselineTests.cs) pins 100 midgame and 5 FFO signatures in `Stello.Net/Stello.Engine.Tests/Data/stello-v3-signatures.txt`. It is independent of the frozen Stello 2.0 signatures. The existing 2.0 test now pins only the frozen 2.0 engine.
- **A/B match switches:** `MatchCommands` supports repeatable `opt=<key>=<value>` for `selective`, `soft-pct`, `mpc` and `ybwc`; existing direct named switches remain supported. Invalid, duplicate and non-Stello switches are rejected.
- **Checks:** all 488 `Stello.Engine.Tests` pass, including the 105 Stello 3.0 and 105 frozen Stello 2.0 signatures; all 15 focused match-parser tests pass. The full Egaroucid test run had 40 passes and two timing-bound failures (700 ms against a 500 ms ceiling; 352 ms against a 200 ms limit). All FFO scores/moves matched the position fixture; the 500 random-endgame results are recorded for later comparison.
- **Review in the sandbox (Linux):**
  - **the Windows references are node for node identical to the 2.0 references** recorded on Linux in the 3.0 work: FFO #40–49, depths 10 and 12, the 1,000 random positions, handover at depth 12 and the 500 random endgames. This also shows that the MSVC build of the Rust engine searches exactly like the Linux build. Handover at depth 14 is new and has no 2.0 reference;
  - the 3.0 signatures equal the 2.0 ones, with only the header comment different;
  - the solution builds with 0 warnings; 488 engine tests and all 42 Egaroucid tests pass. The two timing failures on Windows did not happen here, so they were load on that machine;
  - a smoke match with `opt=selective=off,opt=ybwc=off` plays and shows both switches in the header.
