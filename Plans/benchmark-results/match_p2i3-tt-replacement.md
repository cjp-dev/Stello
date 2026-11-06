### Match: `tt-replacement` (B) against `default` (A), 1180 games

| | A | B |
|---|---|---|
| Settings | Stello, 500 ms/move, hash bits 19, no book | Stello, 500 ms/move, hash bits 19, no book, TT replacement improved |
| Points | 601 | **579** (49.1 %) |
| Wins / draws / losses | +531 =140 −509 | +509 =140 −531 |
| Midgame depth (average) | 18.20 | 18.21 |
| First solve (empty squares, average) | 22.8 | 22.8 |
| Nodes per second | 8.48M | 8.45M |
| Thinking time per game | 6.4 s | 6.4 s |
| Time per searched move (average / longest) | 273 / 642 ms | 274 / 507 ms |

B scored **49.1 %** (95 % interval 47.2 % to 50.9 %), Elo **-6** (-19 to +6), likelihood of superiority 16.0 %; discs per game -0.1 ± 0.2. Wall time 21m 22s.
No significant difference.

This match measured the initial TT-aging implementation. A later review follow-up corrected collision retention and made endgame searches use the retained second bound; it was not included in this match.