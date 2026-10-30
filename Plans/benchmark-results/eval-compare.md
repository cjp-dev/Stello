### eval-compare: 267,174 positions (2026-10-30 18:12)

Data: Traning data/Egaroucid_Train_Data/0001_egaroucid_7_5_1_lv17 (26 files). The target is the final score of the game for the side to move, in discs.
Stello's evaluation is divided by a fitted scale per group (least squares); over all positions the scale is 75.2 units per disc.

| Empty squares | Positions | Draw guess MAE | Egaroucid MAE | Egaroucid RMSE | Egaroucid r | Stello units/disc | Stello MAE | Stello RMSE | Stello r |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 56-60 | 79 | 4.34 | 0.42 | 0.72 | 0.994 | 44.6 | 3.68 | 5.08 | 0.587 |
| 51-55 | 17,095 | 10.50 | 2.44 | 3.46 | 0.963 | 20.3 | 5.05 | 6.68 | 0.858 |
| 46-50 | 25,000 | 13.91 | 3.67 | 5.08 | 0.958 | 26.9 | 7.38 | 11.92 | 0.743 |
| 41-45 | 25,000 | 15.09 | 4.10 | 5.45 | 0.958 | 19.5 | 6.60 | 8.58 | 0.899 |
| 36-40 | 25,000 | 16.18 | 4.15 | 5.53 | 0.963 | 23.8 | 7.96 | 10.20 | 0.874 |
| 31-35 | 25,000 | 16.89 | 4.14 | 5.48 | 0.966 | 30.7 | 8.78 | 11.15 | 0.853 |
| 26-30 | 25,000 | 17.60 | 4.13 | 5.45 | 0.969 | 39.2 | 9.33 | 11.82 | 0.849 |
| 21-25 | 25,000 | 18.55 | 4.28 | 5.65 | 0.970 | 49.0 | 9.58 | 12.13 | 0.853 |
| 16-20 | 25,000 | 19.22 | 4.54 | 5.93 | 0.969 | 61.6 | 9.62 | 12.14 | 0.863 |
| 11-15 | 25,000 | 19.63 | 4.66 | 6.07 | 0.968 | 75.7 | 9.23 | 11.64 | 0.877 |
| 6-10 | 25,000 | 20.00 | 4.48 | 5.83 | 0.972 | 92.5 | 8.80 | 11.06 | 0.894 |
| 1-5 | 25,000 | 19.70 | 2.55 | 3.89 | 0.986 | 112.8 | 7.96 | 9.99 | 0.907 |
| **All** | 267,174 | | **3.96** | **5.36** | **0.969** | 75.2 | **11.07** | **14.02** | **0.764** |

Evaluations per second (from scratch, 1 thread): Egaroucid 0.33M, Stello 2.14M.
