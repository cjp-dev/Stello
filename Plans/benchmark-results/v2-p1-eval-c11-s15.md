### Evaluation report: c11-s15.bin (2026-10-31 09:50)

Model: pattern set classic11, 15 stages, 1,505,517 bytes. Test positions: 1,040,276 (the test split of the converted training data, not used in training). Target: the final disc difference for the side to move.
Stello 1's evaluation is divided by a fitted scale per group; Egaroucid's is in discs (and was trained on data that includes these games).

| Empty squares | Positions | Draw guess MAE | Model MAE | Model RMSE | Stello 1 MAE | Stello 1 RMSE | Egaroucid MAE | Egaroucid RMSE |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 56-60 | 4 | 1.50 | 3.03 | 4.14 | 1.53 | 2.24 | 0.00 | 0.00 |
| 51-55 | 40,272 | 12.69 | 6.16 | 8.49 | 8.08 | 12.31 | 5.96 | 8.27 |
| 46-50 | 100,000 | 14.96 | 6.39 | 8.61 | 7.91 | 10.43 | 6.13 | 8.37 |
| 41-45 | 100,000 | 17.08 | 6.48 | 8.62 | 8.34 | 10.97 | 6.12 | 8.22 |
| 36-40 | 100,000 | 18.45 | 6.34 | 8.35 | 9.50 | 12.27 | 5.88 | 7.84 |
| 31-35 | 100,000 | 19.54 | 6.14 | 8.04 | 10.22 | 13.06 | 5.68 | 7.56 |
| 26-30 | 100,000 | 20.21 | 5.78 | 7.51 | 10.50 | 13.32 | 5.20 | 6.88 |
| 21-25 | 100,000 | 20.57 | 5.32 | 6.93 | 10.20 | 12.93 | 4.75 | 6.31 |
| 16-20 | 100,000 | 20.73 | 5.24 | 6.74 | 9.86 | 12.45 | 4.68 | 6.10 |
| 11-15 | 100,000 | 20.65 | 5.26 | 6.74 | 9.29 | 11.73 | 4.73 | 6.16 |
| 6-10 | 100,000 | 20.46 | 5.02 | 6.44 | 8.75 | 10.97 | 4.50 | 5.87 |
| 1-5 | 100,000 | 19.80 | 3.30 | 4.49 | 8.00 | 10.01 | 2.51 | 3.84 |
| **All** | 1,040,276 | | **5.55** | **7.40** | **12.14** | **15.46** | **5.05** | **6.90** |

Model evaluations per second (from scratch, 1 thread): 4.80M.
