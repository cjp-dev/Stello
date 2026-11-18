### Evaluation report: e16-mob-s60-10m.bin (2026-11-17 21:09)

Model: pattern set egaroucid16, 60 stages, 8,892,085 bytes. Test positions: 1,040,276 (the test split of the converted training data, not used in training). Target: the final disc difference for the side to move.
Stello 1's evaluation is divided by a fitted scale per group; Egaroucid's is in discs (and was trained on data that includes these games).

| Empty squares | Positions | Draw guess MAE | Model MAE | Model RMSE | Stello 1 MAE | Stello 1 RMSE | Egaroucid MAE | Egaroucid RMSE |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 56-60 | 4 | 1.50 | 3.91 | 4.62 | 1.53 | 2.24 | — | — |
| 51-55 | 40,272 | 12.69 | 5.85 | 8.03 | 8.08 | 12.31 | — | — |
| 46-50 | 100,000 | 14.96 | 5.99 | 8.13 | 7.91 | 10.43 | — | — |
| 41-45 | 100,000 | 17.08 | 6.00 | 8.03 | 8.34 | 10.97 | — | — |
| 36-40 | 100,000 | 18.45 | 5.80 | 7.70 | 9.50 | 12.27 | — | — |
| 31-35 | 100,000 | 19.54 | 5.54 | 7.33 | 10.22 | 13.06 | — | — |
| 26-30 | 100,000 | 20.21 | 5.10 | 6.72 | 10.50 | 13.32 | — | — |
| 21-25 | 100,000 | 20.57 | 4.63 | 6.10 | 10.20 | 12.93 | — | — |
| 16-20 | 100,000 | 20.73 | 4.61 | 5.97 | 9.86 | 12.45 | — | — |
| 11-15 | 100,000 | 20.65 | 4.68 | 6.06 | 9.29 | 11.73 | — | — |
| 6-10 | 100,000 | 20.46 | 4.46 | 5.78 | 8.75 | 10.97 | — | — |
| 1-5 | 100,000 | 19.80 | 2.52 | 3.78 | 8.00 | 10.01 | — | — |
| **All** | 1,040,276 | | **4.97** | **6.74** | **12.14** | **15.46** | **—** | **—** |

Model evaluations per second (from scratch, 1 thread): 1.81M.
