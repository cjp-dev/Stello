# Updating the documentation for the Stello 2.0 release

The user's goals are in [Update documentation for Stello 2 release.md](../Update%20documentation%20for%20Stello%202%20release.md). This plan carries them out. It gets a Status section that is updated after each step.

## Goals (user, 2026-11-02)

1. Chapters 01–15 describe Stello 2.0, and only Stello 2.0, as precisely and thoroughly as today.
2. The documentation of Stello 1 moves to an archive, "Stello 1 documentation", which the introduction mentions.
3. The introduction tells the history:
   - Stello C++, the original;
   - Stello .NET 1.0, the C# port;
   - Stello .NET 2.0, the current version.
4. A series of chapters describes the way from Stello 1 to Stello 2.0, phase by phase. It starts with a chapter on the goal and the plan. Phase 5 gets its own new chapter.
5. The glossary and the references move to the end.

**Decisions (user, 2026-11-02):**
- The archive is a full copy of the Stello 1 documents.
- The final numbers come from a fresh set of matches.
- "2.x" is written "2.0" everywhere, in `Plans/` too.
- The work follows this plan, with one commit per part.

## The new structure

`docs/brain/README.md`, the introduction:
- the history: C++ → .NET 1.0, with a link to `Plans/Stello porting documentation.md` → .NET 2.0;
- the brain on one page (the move-choice diagram);
- the chapter table in three parts;
- a link to the archive.

### Part 1: How Stello 2.0 works

| # | Chapter | Work |
|---|---|---|
| 01 | Overview | Projects (`Stello.Engine`, `Stello.Engine.V1`, `Egaroucid.Engine`, `Stello.Trainer`, the tools), main types, the life of one move in 2.0 |
| 02 | Board and squares | Check; remove the Stello 1 remark |
| 03 | Rules and move generation | Check |
| 04 | Evaluation | **Rewrite** as the pattern evaluation: patterns, places and folding, stages, reading the indices, the file and how the search uses it (from chapter 21) |
| 05 | Move ordering | Check against `SearchEngine`; 2.0 only |
| 06 | Midgame search | 2.0 only: PVS, Multi-ProbCut and its fitted error model, ETC, the score scale |
| 07 | Transposition table | Check |
| 08 | Endgame solver | **Rewrite** as the new solver: when it runs, the layers, Egaroucid's last 13 squares, the half-turn trick, the search above 13 squares, solve mode, cancellation (from chapter 21) |
| 09 | Time control | Check |
| 10 | Opening book | 2.0's book (version 4, values in 1/100 disc). The version history moves to Part 2 |
| 11 | Book learning | Check, 2.0 only |
| 12 | Book tool | Check (`recalc` and its win/loss/draw rule, `compare`, `match`) |
| 13 | Game record | Check |
| 14 | App integration | Check (the evaluation is built into the assembly; the web book file) |
| 15 | GUI and settings | Check (scores in discs, About) |

### Part 2: From Stello 1 to Stello 2.0

| # | Chapter | Sources |
|---|---|---|
| 16 | The goal and the plan: why (the Egaroucid comparison), the user's order, the engines, the gates, the phases | the plan, today's 20 |
| 17 | Measuring the engine | today's 18 (Stello 1 baseline numbers kept as history) |
| 18 | The Egaroucid port | today's 19 |
| 19 | Stello 1 against Egaroucid | today's 20 |
| 20 | Phases 0–2: the frozen baseline, the training data, the trainer and the evaluation experiments (including the 16-bit trainer and the larger evaluation, which was not pursued) | 21, the findings |
| 21 | Phase 3: developing the endgame solver (what made it fast, what was dropped, the gate) | 21, the findings |
| 22 | Phase 4: the release in the apps and the book recalculated by Stello 2 | 21, 10, the findings |
| 23 | Phase 5: the search | the findings (mostly new text). Items 1–6, the book recalculated again, the time-odds matches, C++ against C# |
| 24 | Results and what comes next | the final matches (below); item 7, a Rust core |
| 25 | Glossary | today's 16 |
| 26 | References | today's 17 |

### The archive: `docs/brain/stello-1/`

- **Contents:** README and chapters 01–18 exactly as at `a261a6d`, the last commit before the 2.0 work, with their images in `stello-1/images/`.
- **A note at the top of each page:** "Stello 1 documentation (archive): this describes the engine before 2.0, kept in `Stello.Engine.V1`".
- **Links are repointed:**
  - for the code Stello 1 keeps (`SearchEngine.cs`, `Search/*`, `Evaluation/*`), to `Stello.Engine.V1`;
  - for the shared code (board, book, apps), to the current files;
  - between chapters, within the archive.

## Steps and commits (branch `docs/stello-2.0`)

1. This plan, and "2.x" → "2.0" in `Plans/` and the docs. Branch names and result file names (`feature/stello-v2.x`, `match_v2x-*`) stay as they are, because they are identifiers.
2. The archive.
3. Part 1, in two commits:
   - 04 and 08 rewritten;
   - the other chapters.
4. Part 2, with the renumbering:
   - 16, 20–24 new or rebuilt;
   - 17–19 moved;
   - 25–26 moved.
5. README and the final numbers, the links from `Plans/` updated, chapter 21 (today's) gone, its content now in 04, 08 and 20–22.
6. Merge into main and push. Only the site redeploys; check the deploy run.

## Rules

- **Code is the truth.** Every statement about how 2.0 works is checked against the code: names, constants, file paths and line references.
- **Every number comes from a result file** in `Plans/benchmark-results/` or from the findings, and is linked.
- **Same style as today:** "In short" first, then data structures, algorithm, worked examples, design notes, "Where in the code", tests, further reading. Diagrams use SVG or mermaid as today.
- **No duplicate explanations:** Part 1 says how 2.0 works, Part 2 how and why it got there. Part 1 links to Part 2 for the history.
- **Links are checked** by a small script: every relative link and anchor resolves.

## Verification

- The link check passes for `docs/brain/` and `Plans/`.
- `grep "Stello 1"` in chapters 01–15 finds only links to the archive or to Part 2.
- `grep "2\.x"` finds only identifiers.
- Every "Where in the code" file exists.
- Constants quoted in Part 1 match the code.

## Final measurements (started 2026-11-02)

- **Stello 2.0 against Stello 1,** each with its own book: 100 and 500 ms per move.
- **Stello 2.0 against Egaroucid:**
  - no books, 100 and 500 ms;
  - with both books, 500 ms.
- **FFO #40–49** solved exactly.

All matches are 1,180 games from `starts.txt`. The results are named `match_v2-final-*` and `v2-final-solve-bench-40-49.json`.

## Status

- Step 1: in progress.
