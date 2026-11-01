### Evaluation report: e16-s60-3m.bin (2026-11-01 20:19)

Model: pattern set egaroucid16, 60 stages, 8,320,960 bytes. Test positions: 1,040,276 (the test split of the converted training data, not used in training). Target: the final disc difference for the side to move.
Stello 1's evaluation is divided by a fitted scale per group; Egaroucid's is in discs (and was trained on data that includes these games).

| Empty squares | Positions | Draw guess MAE | Model MAE | Model RMSE | Stello 1 MAE | Stello 1 RMSE | Egaroucid MAE | Egaroucid RMSE |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 56-60 | 4 | 1.50 | 3.30 | 3.57 | 1.53 | 2.24 | — | — |
| 51-55 | 40,272 | 12.69 | 6.03 | 8.33 | 8.08 | 12.31 | — | — |
| 46-50 | 100,000 | 14.96 | 6.18 | 8.34 | 7.91 | 10.43 | — | — |
| 41-45 | 100,000 | 17.08 | 6.23 | 8.27 | 8.34 | 10.97 | — | — |
| 36-40 | 100,000 | 18.45 | 6.03 | 7.96 | 9.50 | 12.27 | — | — |
| 31-35 | 100,000 | 19.54 | 5.77 | 7.58 | 10.22 | 13.06 | — | — |
| 26-30 | 100,000 | 20.21 | 5.35 | 7.00 | 10.50 | 13.32 | — | — |
| 21-25 | 100,000 | 20.57 | 4.90 | 6.40 | 10.20 | 12.93 | — | — |
| 16-20 | 100,000 | 20.73 | 4.87 | 6.27 | 9.86 | 12.45 | — | — |
| 11-15 | 100,000 | 20.65 | 4.95 | 6.37 | 9.29 | 11.73 | — | — |
| 6-10 | 100,000 | 20.46 | 4.73 | 6.08 | 8.75 | 10.97 | — | — |
| 1-5 | 100,000 | 19.80 | 2.99 | 4.17 | 8.00 | 10.01 | — | — |
| **All** | 1,040,276 | | **5.23** | **7.01** | **12.14** | **15.46** | **—** | **—** |

Model evaluations per second (from scratch, 1 thread): 2.29M.
