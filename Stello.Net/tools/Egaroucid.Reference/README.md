# Egaroucid reference

This folder builds the original C++ Egaroucid and uses it to write the test fixtures of the C# port in `Stello.Net/Egaroucid.Engine`. The port must reproduce the reference exactly: the same evaluations, hash codes, stable discs and perft counts, and the same move, value, depth and number of nodes in every search. See [Plans/egaroucid-port-plan.md](../../../Plans/egaroucid-port-plan.md), phase 1.

Egaroucid is © Takuto Yamana, GPL-3.0-or-later (<https://github.com/Nyanyan/Egaroucid>). Its source is not part of this repository. Download it into `<repo>/Egaroucid-main` (that folder is git-ignored), or pass its path to `build.sh`.

## Files

| File | What it does |
|---|---|
| `build.sh [Egaroucid-main]` | Builds `bin/egaroucid-generic` (Egaroucid for Console) and `bin/egaroucid-ref` (`reference.cpp`), and copies Egaroucid's resources to `bin/resources`. The binaries look for their resources there. |
| `reference.cpp` | A small driver that includes Egaroucid's engine unchanged and prints what the fixtures need |
| `make-fixtures.sh [out]` | Writes all fixtures to `Stello.Net/Egaroucid.Engine.Tests/Data/egaroucid` |

`bin/` is git-ignored. It needs g++ with C++20 (g++ 15 was used) and takes about 35 s to build.

## Build flags

- `-DHAS_NO_AVX2` builds the **Generic** code paths, which are the ones the C# port follows. The default SIMD build counts endgame nodes differently: its `last1_nws` adds the pass node only when the score is above alpha, and it returns lazy bounds.
- `-ffp-contract=off` stops the compiler from fusing multiply and add in the floating-point formula of the Multi-ProbCut table, so the table matches .NET's arithmetic.

## Driver commands

The driver reads positions one per line: `[id] <64 squares a1..h8> <side to move>`. `X` is black and `O` is white. Anything after `;` is ignored, so the benchmark files in `Stello.Benchmark/Data` can be read directly.

| Command | Output per position |
|---|---|
| `eval` | hash code, `mid_evaluate`, the move-ordering-end evaluation, the stable discs of both sides, the 64 feature indices |
| `perft <depth>` | the leaf count with a pass counted as a move, and without |
| `search <level> [log]` | `ai(board, level, no book, use_multi_thread = false)`: move, value, depth, probability, end search, nodes, clog nodes. With `log`, Egaroucid's own log of the search follows, as lines starting with `#`, without the times and speeds, which differ from run to run. |
| `book` | the book value of the position and of every legal move that is in the book (`resources/book.egbk3`) |
| `mpc` | a checksum and sample entries of the Multi-ProbCut error table (no input) |
| `random <n> <seed>` | generates positions by random play: n for each disc count 4–63, and n where the side to move must pass |
| `book-walk <n> <seed>` | generates n positions by random walks through the book |

For every position, `search` clears the transposition table and the endgame's local table first, so a result never depends on the position before it.

It calls `ai` with `use_multi_thread = false`. The console always passes `true`, even with `-t 1`, and that sends the search through the YBWC code paths. Those sort moves with `std::sort`, whose order of equal moves depends on the C++ library. The console's own results (`egaroucid-generic -t 1 -nobook -l 60 -solve <file>`) therefore have slightly different node counts. The C# port matches the driver now, and the threaded paths are compared in phase 6.

## Fixtures

The fixture positions come from fixed seeds, the book, `midgame.txt` and the FFO file. None are taken from Egaroucid's training data, which may not be redistributed. Running `make-fixtures.sh` again gives identical files.
