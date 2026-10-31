# Findings from improving Stello's playing strength (Stello 2.0)

What the work on Stello 2.0 teaches us, written down as we go: the decisions, the measurements, what worked and what did not, and the ideas for later.

- **The plan:** [improve-stello-strength-plan.md](improve-stello-strength-plan.md). This file is updated after every phase.
- **Where it starts:** the comparison with the Egaroucid port ([egaroucid-comparison-report.md](egaroucid-comparison-report.md), [egaroucid-port-findings.md](egaroucid-port-findings.md)).
- **The engines:**

  | Name | Project | Match side / benchmark | What it is |
  |---|---|---|---|
  | Stello 1 | `Stello.Net/Stello.Engine.V1` | `engine=stello-v1`, `--engine stello-v1` | The engine before 2.0 (commit a261a6d), frozen as the baseline |
  | Stello 2 | `Stello.Net/Stello.Engine` | `engine=stello` (the default), `--engine stello` | The engine being improved; the apps use it |
  | Egaroucid | `Stello.Net/Egaroucid.Engine` | `engine=egaroucid`, `--engine egaroucid` | The C# port of Egaroucid 7.9, the reference |

## Status

| Phase | State | What it showed |
|---|---|---|
| 0 Set-up and the frozen baseline | done | Stello 1 searches node for node like the engine it was copied from; a match against it is even |
| 1 Training data and trainer | | |
| 2 Evaluation 2.0 | | |
| 3 Endgame solver | | |
| 4 Release 2.0 in the apps | | |
| 5 Search improvements (2.x) | | |

## Stello 1, the frozen baseline

- **What was copied:** `SearchEngine`, `Search/*` (move ordering, time control, transposition table), `Evaluation/*` (evaluator, edge tables) and `ComputerPlayer`, under the namespace `Stello.Engine.V1`. Board, moves, limits, results and the opening book are shared with Stello 2. Stello 1 reaches the internal `Bitboards` through `InternalsVisibleTo`.
- **Proof that it is the same engine:**
  - on all 100 `midgame.txt` positions at depth 8, and on FFO #40–44, it gives the same moves, scores, kinds, depths and node counts as `Stello.Engine` at a261a6d;
  - `StelloV1Tests` keeps checking 30 of those searches and FFO #40–41 against the recorded `Data/stello-v1-signatures.txt`, so the copy can't drift.
- **A/B switches:** the plan put `SearchOptions` (`opt=` on a match side) in phase 0. There is nothing to switch yet, so it comes with the first alternative, the new evaluation in phase 2.
- **Its book:** Stello 1 plays from `Book/opening-book.txt`. When phase 4 recalculates the book for Stello 2, the current book must be kept for Stello 1.

## Measurements

All on 1 thread, on the same Intel i7-12850HX laptop VM as the port's measurements.

| What | Stello 1 | Stello 2 | Egaroucid | Note |
|---|---:|---:|---:|---|
| `mid-bench`, depth 8, 100 positions | 4,650,177 nodes, 1.9 s | the same | — | phase 0: identical |
| FFO #40–44, exact | 329,535,633 nodes, 21.1 s | the same | 4.3 s | phase 0: identical |
| Match Stello 2 (= Stello 1 so far) against Stello 1, 100 ms per move, 1,180 games | — | 51.8 % (49.5–54.1), +13 Elo | — | phase 0: no significant difference; depth 10.36 / 10.35, 2.5M nodes/s both ([result](benchmark-results/match_v2-p0-v1-vs-current-100ms.md)) |

## Ideas for later

- (filled in as the work goes on)
