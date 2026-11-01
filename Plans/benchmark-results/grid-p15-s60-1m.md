### Evaluation report: p15-s60-1m.bin (2026-11-01 20:57)

Model: pattern set plus15, 60 stages, 6,011,212 bytes. Test positions: 1,040,276 (the test split of the converted training data, not used in training). Target: the final disc difference for the side to move.
Stello 1's evaluation is divided by a fitted scale per group; Egaroucid's is in discs (and was trained on data that includes these games).

| Empty squares | Positions | Draw guess MAE | Model MAE | Model RMSE | Stello 1 MAE | Stello 1 RMSE | Egaroucid MAE | Egaroucid RMSE |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 56-60 | 4 | 1.50 | 4.02 | 4.19 | 1.53 | 2.24 | — | — |
| 51-55 | 40,272 | 12.69 | 6.08 | 8.38 | 8.08 | 12.31 | — | — |
| 46-50 | 100,000 | 14.96 | 6.27 | 8.48 | 7.91 | 10.43 | — | — |
| 41-45 | 100,000 | 17.08 | 6.36 | 8.47 | 8.34 | 10.97 | — | — |
| 36-40 | 100,000 | 18.45 | 6.19 | 8.18 | 9.50 | 12.27 | — | — |
| 31-35 | 100,000 | 19.54 | 5.95 | 7.82 | 10.22 | 13.06 | — | — |
| 26-30 | 100,000 | 20.21 | 5.53 | 7.22 | 10.50 | 13.32 | — | — |
| 21-25 | 100,000 | 20.57 | 5.05 | 6.60 | 10.20 | 12.93 | — | — |
| 16-20 | 100,000 | 20.73 | 4.99 | 6.43 | 9.86 | 12.45 | — | — |
| 11-15 | 100,000 | 20.65 | 5.03 | 6.47 | 9.29 | 11.73 | — | — |
| 6-10 | 100,000 | 20.46 | 4.79 | 6.17 | 8.75 | 10.97 | — | — |
| 1-5 | 100,000 | 19.80 | 3.05 | 4.25 | 8.00 | 10.01 | — | — |
| **All** | 1,040,276 | | **5.35** | **7.17** | **12.14** | **15.46** | **—** | **—** |

Model evaluations per second (from scratch, 1 thread): 3.50M.
