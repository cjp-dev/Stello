# Third-party notices

Stello is licensed under the GNU General Public License version 3 only (GPL-3.0-only); see [LICENSE](LICENSE). This file lists the work by others that Stello uses, ports or learns from.

## Egaroucid

- **Author:** Takuto Yamana
- **Links:** [website](https://www.egaroucid.nyanyan.dev/en/), [GitHub](https://github.com/Nyanyan/Egaroucid)
- **Licence:** GPL-3.0-or-later. Stello uses it under GPL version 3.

What Stello takes from it:
- **The engine port.** `Stello.Net/Egaroucid.Engine` is a C# port of the Egaroucid engine: board, evaluation, search, endgame search, time management and opening book lookup. The port is in progress; see [Plans/egaroucid-port-plan.md](Plans/egaroucid-port-plan.md). Each ported file names its C++ original and carries its licence header.
- **Stello 2's endgame solver.** `Stello.Net/Stello.Engine/Endgame/` adapts Egaroucid's endgame search of the last 13 empty squares, its stable-disc calculation and its move-ordering weights, through the port. Each file names its C++ original.
- **Resource files.** The port reads Egaroucid's evaluation weights, hash tables and opening book from a local copy of Egaroucid. These files are not included in this repository.
- **Ideas and methods.** Some come from its source code and its technology pages.

## Egaroucid training data

- **Author:** Takuto Yamana
- **Link:** [training data](https://www.egaroucid.nyanyan.dev/en/technology/train-data/)

Stello 2's evaluation, `Stello.Net/Book/stello-eval.bin`, was trained on this data by `Stello.Net/tools/Stello.Trainer`. The file holds only the trained pattern weights, not the data. The engine comparison tools also use the data to measure how accurate an evaluation is. The data's terms forbid redistribution, so it is not part of this repository. As the terms ask: *Stello used Egaroucid's self-play data for training and testing its Othello AI.*

## Edax

- **Author:** Richard Delorme
- **Link:** [GitHub](https://github.com/abulmo/edax-reversi)
- **Licence:** GPL-3.0

Stello uses ideas from Edax's bitboard move generation and endgame search (described in [docs/brain](docs/brain/README.md), chapters 03 and 08). The FFO test positions in `Stello.Net/tools/Stello.Benchmark/Data/ffo-40-59.txt` were taken from Edax's `problem/fforum-40-59.obf`.

## FFO endgame test positions

The test positions #40–#59 come from the Fédération Française d'Othello (FForum). They are published at [radagast.se/othello/ffotest.html](http://radagast.se/othello/ffotest.html).
