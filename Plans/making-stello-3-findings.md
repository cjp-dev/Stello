# Findings from making Stello 3.0

What the work on Stello 3.0 teaches us, written down as we go: the decisions, the measurements, what worked and what did not, and the ideas for later.

- **The plan:** [making-stello-3-plan.md](making-stello-3-plan.md). This file is updated after every phase.
- **Where it starts:** Stello 2.0 as released on 2 November 2026 ([docs chapter 24](../docs/24-results-and-what-comes-next.md)), and the C++/C# speed comparison ([cpp-vs-csharp-speed.md](cpp-vs-csharp-speed.md)).

## Status

| Phase | State | What it showed |
|---|---|---|
| 0 Set-up and the frozen Stello 2.0 | not started | |
| 1 The core in Rust | not started | |
| 2 Desktop and tools on Rust | not started | |
| 3 The web app on Rust | not started | |
| 4 Release (3.0 preview) | not started | |
| 5 Threads and a shared hash table | not started | |
| 6 Lazy SMP | not started | |
| 7 YBWC | not started | |
| 8 Threads in the browser | not started | |
| 9 Threads setting and the 3.0 release | not started | |

## Baseline (Stello 2.0, C#, 1 thread)

| What | Value | Source |
|---|---|---|
| FFO #40–49, exact | 46.8 s, 1.10 G nodes | [v2-final-solve-bench-40-49.json](benchmark-results/v2-final-solve-bench-40-49.json) |
| Midgame speed in matches, 500 ms | about 4.2–4.5 M nodes/s | [docs chapter 24](../docs/24-results-and-what-comes-next.md) |
| Browser, 22 empty squares | solved in 0.49 s (live check) | [improve-stello-strength-findings.md](improve-stello-strength-findings.md) |
| Book recalculation, 60 s per position, 8 workers | about 14 h (Ryzen 7 7800X3D) | [docs chapter 23](../docs/23-phase-5-the-search.md) |
