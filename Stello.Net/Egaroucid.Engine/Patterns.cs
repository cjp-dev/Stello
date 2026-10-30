// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/evaluate_common.hpp and evaluate_generic.hpp (https://github.com/Nyanyan/Egaroucid),
// © Takuto Yamana, GPL-3.0-or-later.

namespace Stello.Egaroucid;

/// <summary>The squares as bit indices (evaluate_common.hpp <c>COORD_*</c>): a1 = 63 .. h8 = 0.</summary>
internal static class Coord
{
    public const int A1 = 63;
    public const int B1 = 62;
    public const int C1 = 61;
    public const int D1 = 60;
    public const int E1 = 59;
    public const int F1 = 58;
    public const int G1 = 57;
    public const int H1 = 56;
    public const int A2 = 55;
    public const int B2 = 54;
    public const int C2 = 53;
    public const int D2 = 52;
    public const int E2 = 51;
    public const int F2 = 50;
    public const int G2 = 49;
    public const int H2 = 48;
    public const int A3 = 47;
    public const int B3 = 46;
    public const int C3 = 45;
    public const int D3 = 44;
    public const int E3 = 43;
    public const int F3 = 42;
    public const int G3 = 41;
    public const int H3 = 40;
    public const int A4 = 39;
    public const int B4 = 38;
    public const int C4 = 37;
    public const int D4 = 36;
    public const int E4 = 35;
    public const int F4 = 34;
    public const int G4 = 33;
    public const int H4 = 32;
    public const int A5 = 31;
    public const int B5 = 30;
    public const int C5 = 29;
    public const int D5 = 28;
    public const int E5 = 27;
    public const int F5 = 26;
    public const int G5 = 25;
    public const int H5 = 24;
    public const int A6 = 23;
    public const int B6 = 22;
    public const int C6 = 21;
    public const int D6 = 20;
    public const int E6 = 19;
    public const int F6 = 18;
    public const int G6 = 17;
    public const int H6 = 16;
    public const int A7 = 15;
    public const int B7 = 14;
    public const int C7 = 13;
    public const int D7 = 12;
    public const int E7 = 11;
    public const int F7 = 10;
    public const int G7 = 9;
    public const int H7 = 8;
    public const int A8 = 7;
    public const int B8 = 6;
    public const int C8 = 5;
    public const int D8 = 4;
    public const int E8 = 3;
    public const int F8 = 2;
    public const int G8 = 1;
    public const int H8 = 0;
}

/// <summary>
/// The pattern features of the evaluation (evaluate_generic.hpp): 16 patterns of 7 to 10 squares, each in 4 places on
/// the board, so 64 features. A feature's index is the base-3 number of its squares, the first square the most
/// significant digit, with 0 = side to move, 1 = opponent, 2 = empty.
/// </summary>
internal static class Patterns
{
    public const int N_PATTERNS = 16;
    public const int N_PATTERN_FEATURES = 64;
    public const int MAX_PATTERN_CELLS = 10;
    public const int MAX_EVALUATE_IDX = 59049;

    /// <summary>The features of the move-ordering evaluation of the endgame: patterns 8 to 11.</summary>
    public const int EVAL_IDX_START_MOVE_ORDERING_END = 32;
    public const int EVAL_IDX_END_MOVE_ORDERING_END = 48;
    public const int EVAL_FEATURE_START_MOVE_ORDERING_END = 8;
    public const int N_PATTERNS_MO_END = 4;

    public static ReadOnlySpan<int> PatternSizes => [8, 9, 8, 9, 8, 9, 7, 10, 10, 10, 10, 10, 10, 10, 10, 10];

    public static ReadOnlySpan<int> Pow3 => [1, 3, 9, 27, 81, 243, 729, 2187, 6561, 19683, 59049];

    /// <summary>The squares of each feature, in digit order (evaluate_generic.hpp <c>feature_to_coord</c>).</summary>
    public static readonly int[][] FeatureToCoord =
    [
        // 0 hv2
        [Coord.A2, Coord.B2, Coord.C2, Coord.D2, Coord.E2, Coord.F2, Coord.G2, Coord.H2],
        [Coord.B1, Coord.B2, Coord.B3, Coord.B4, Coord.B5, Coord.B6, Coord.B7, Coord.B8],
        [Coord.A7, Coord.B7, Coord.C7, Coord.D7, Coord.E7, Coord.F7, Coord.G7, Coord.H7],
        [Coord.G1, Coord.G2, Coord.G3, Coord.G4, Coord.G5, Coord.G6, Coord.G7, Coord.G8],
        // 1 d6 + 2C + X
        [Coord.B1, Coord.C1, Coord.D2, Coord.E3, Coord.G2, Coord.F4, Coord.G5, Coord.H6, Coord.H7],
        [Coord.H2, Coord.H3, Coord.G4, Coord.F5, Coord.G7, Coord.E6, Coord.D7, Coord.C8, Coord.B8],
        [Coord.G8, Coord.F8, Coord.E7, Coord.D6, Coord.B7, Coord.C5, Coord.B4, Coord.A3, Coord.A2],
        [Coord.A7, Coord.A6, Coord.B5, Coord.C4, Coord.B2, Coord.D3, Coord.E2, Coord.F1, Coord.G1],
        // 2 hv3
        [Coord.A3, Coord.B3, Coord.C3, Coord.D3, Coord.E3, Coord.F3, Coord.G3, Coord.H3],
        [Coord.C1, Coord.C2, Coord.C3, Coord.C4, Coord.C5, Coord.C6, Coord.C7, Coord.C8],
        [Coord.A6, Coord.B6, Coord.C6, Coord.D6, Coord.E6, Coord.F6, Coord.G6, Coord.H6],
        [Coord.F1, Coord.F2, Coord.F3, Coord.F4, Coord.F5, Coord.F6, Coord.F7, Coord.F8],
        // 3 d7 + 2 corner
        [Coord.A1, Coord.B1, Coord.C2, Coord.D3, Coord.E4, Coord.F5, Coord.G6, Coord.H7, Coord.H8],
        [Coord.H1, Coord.H2, Coord.G3, Coord.F4, Coord.E5, Coord.D6, Coord.C7, Coord.B8, Coord.A8],
        [Coord.H8, Coord.G8, Coord.F7, Coord.E6, Coord.D5, Coord.C4, Coord.B3, Coord.A2, Coord.A1],
        [Coord.A8, Coord.A7, Coord.B6, Coord.C5, Coord.D4, Coord.E3, Coord.F2, Coord.G1, Coord.H1],
        // 4 hv4
        [Coord.A4, Coord.B4, Coord.C4, Coord.D4, Coord.E4, Coord.F4, Coord.G4, Coord.H4],
        [Coord.D1, Coord.D2, Coord.D3, Coord.D4, Coord.D5, Coord.D6, Coord.D7, Coord.D8],
        [Coord.A5, Coord.B5, Coord.C5, Coord.D5, Coord.E5, Coord.F5, Coord.G5, Coord.H5],
        [Coord.E1, Coord.E2, Coord.E3, Coord.E4, Coord.E5, Coord.E6, Coord.E7, Coord.E8],
        // 5 corner9
        [Coord.A1, Coord.B1, Coord.C1, Coord.A2, Coord.B2, Coord.C2, Coord.A3, Coord.B3, Coord.C3],
        [Coord.H1, Coord.G1, Coord.F1, Coord.H2, Coord.G2, Coord.F2, Coord.H3, Coord.G3, Coord.F3],
        [Coord.A8, Coord.B8, Coord.C8, Coord.A7, Coord.B7, Coord.C7, Coord.A6, Coord.B6, Coord.C6],
        [Coord.H8, Coord.G8, Coord.F8, Coord.H7, Coord.G7, Coord.F7, Coord.H6, Coord.G6, Coord.F6],
        // 6 d5 + 2X
        [Coord.B2, Coord.D1, Coord.E2, Coord.F3, Coord.G4, Coord.H5, Coord.G7],
        [Coord.G2, Coord.H4, Coord.G5, Coord.F6, Coord.E7, Coord.D8, Coord.B7],
        [Coord.G7, Coord.E8, Coord.D7, Coord.C6, Coord.B5, Coord.A4, Coord.B2],
        [Coord.B7, Coord.A5, Coord.B4, Coord.C3, Coord.D2, Coord.E1, Coord.G2],
        // 7 d8 + 2C
        [Coord.A1, Coord.B2, Coord.C3, Coord.D4, Coord.E5, Coord.F6, Coord.G7, Coord.H8, Coord.A2, Coord.B1],
        [Coord.H8, Coord.G7, Coord.F6, Coord.E5, Coord.D4, Coord.C3, Coord.B2, Coord.A1, Coord.H7, Coord.G8],
        [Coord.H1, Coord.G2, Coord.F3, Coord.E4, Coord.D5, Coord.C6, Coord.B7, Coord.A8, Coord.H2, Coord.G1],
        [Coord.A8, Coord.B7, Coord.C6, Coord.D5, Coord.E4, Coord.F3, Coord.G2, Coord.H1, Coord.A7, Coord.B8],
        // 8 edge + 2x
        [Coord.B2, Coord.A1, Coord.B1, Coord.C1, Coord.D1, Coord.E1, Coord.F1, Coord.G1, Coord.H1, Coord.G2],
        [Coord.B2, Coord.A1, Coord.A2, Coord.A3, Coord.A4, Coord.A5, Coord.A6, Coord.A7, Coord.A8, Coord.B7],
        [Coord.B7, Coord.A8, Coord.B8, Coord.C8, Coord.D8, Coord.E8, Coord.F8, Coord.G8, Coord.H8, Coord.G7],
        [Coord.G2, Coord.H1, Coord.H2, Coord.H3, Coord.H4, Coord.H5, Coord.H6, Coord.H7, Coord.H8, Coord.G7],
        // 9 triangle
        [Coord.A1, Coord.B1, Coord.C1, Coord.D1, Coord.A2, Coord.B2, Coord.C2, Coord.A3, Coord.B3, Coord.A4],
        [Coord.H1, Coord.G1, Coord.F1, Coord.E1, Coord.H2, Coord.G2, Coord.F2, Coord.H3, Coord.G3, Coord.H4],
        [Coord.A8, Coord.B8, Coord.C8, Coord.D8, Coord.A7, Coord.B7, Coord.C7, Coord.A6, Coord.B6, Coord.A5],
        [Coord.H8, Coord.G8, Coord.F8, Coord.E8, Coord.H7, Coord.G7, Coord.F7, Coord.H6, Coord.G6, Coord.H5],
        // 10 corner + block
        [Coord.A1, Coord.C1, Coord.D1, Coord.E1, Coord.F1, Coord.H1, Coord.C2, Coord.D2, Coord.E2, Coord.F2],
        [Coord.A1, Coord.A3, Coord.A4, Coord.A5, Coord.A6, Coord.A8, Coord.B3, Coord.B4, Coord.B5, Coord.B6],
        [Coord.A8, Coord.C8, Coord.D8, Coord.E8, Coord.F8, Coord.H8, Coord.C7, Coord.D7, Coord.E7, Coord.F7],
        [Coord.H1, Coord.H3, Coord.H4, Coord.H5, Coord.H6, Coord.H8, Coord.G3, Coord.G4, Coord.G5, Coord.G6],
        // 11 cross
        [Coord.A1, Coord.B2, Coord.C3, Coord.D4, Coord.B1, Coord.C2, Coord.D3, Coord.A2, Coord.B3, Coord.C4],
        [Coord.H1, Coord.G2, Coord.F3, Coord.E4, Coord.G1, Coord.F2, Coord.E3, Coord.H2, Coord.G3, Coord.F4],
        [Coord.A8, Coord.B7, Coord.C6, Coord.D5, Coord.B8, Coord.C7, Coord.D6, Coord.A7, Coord.B6, Coord.C5],
        [Coord.H8, Coord.G7, Coord.F6, Coord.E5, Coord.G8, Coord.F7, Coord.E6, Coord.H7, Coord.G6, Coord.F5],
        // 12 edge + y
        [Coord.C2, Coord.A1, Coord.B1, Coord.C1, Coord.D1, Coord.E1, Coord.F1, Coord.G1, Coord.H1, Coord.F2],
        [Coord.B3, Coord.A1, Coord.A2, Coord.A3, Coord.A4, Coord.A5, Coord.A6, Coord.A7, Coord.A8, Coord.B6],
        [Coord.C7, Coord.A8, Coord.B8, Coord.C8, Coord.D8, Coord.E8, Coord.F8, Coord.G8, Coord.H8, Coord.F7],
        [Coord.G3, Coord.H1, Coord.H2, Coord.H3, Coord.H4, Coord.H5, Coord.H6, Coord.H7, Coord.H8, Coord.G6],
        // 13 narrow triangle
        [Coord.A1, Coord.B1, Coord.C1, Coord.D1, Coord.E1, Coord.A2, Coord.B2, Coord.A3, Coord.A4, Coord.A5],
        [Coord.H1, Coord.G1, Coord.F1, Coord.E1, Coord.D1, Coord.H2, Coord.G2, Coord.H3, Coord.H4, Coord.H5],
        [Coord.A8, Coord.B8, Coord.C8, Coord.D8, Coord.E8, Coord.A7, Coord.B7, Coord.A6, Coord.A5, Coord.A4],
        [Coord.H8, Coord.G8, Coord.F8, Coord.E8, Coord.D8, Coord.H7, Coord.G7, Coord.H6, Coord.H5, Coord.H4],
        // 14 fish
        [Coord.A1, Coord.B1, Coord.A2, Coord.B2, Coord.C2, Coord.D2, Coord.B3, Coord.C3, Coord.B4, Coord.D4],
        [Coord.H1, Coord.G1, Coord.H2, Coord.G2, Coord.F2, Coord.E2, Coord.G3, Coord.F3, Coord.G4, Coord.E4],
        [Coord.A8, Coord.B8, Coord.A7, Coord.B7, Coord.C7, Coord.D7, Coord.B6, Coord.C6, Coord.B5, Coord.D5],
        [Coord.H8, Coord.G8, Coord.H7, Coord.G7, Coord.F7, Coord.E7, Coord.G6, Coord.F6, Coord.G5, Coord.E5],
        // 15 anvil
        [Coord.C6, Coord.D6, Coord.D7, Coord.D8, Coord.C8, Coord.F8, Coord.E8, Coord.E7, Coord.E6, Coord.F6],
        [Coord.C3, Coord.C4, Coord.B4, Coord.A4, Coord.A3, Coord.A6, Coord.A5, Coord.B5, Coord.C5, Coord.C6],
        [Coord.F3, Coord.E3, Coord.E2, Coord.E1, Coord.F1, Coord.C1, Coord.D1, Coord.D2, Coord.D3, Coord.C3],
        [Coord.F6, Coord.F5, Coord.G5, Coord.H5, Coord.H6, Coord.H3, Coord.H4, Coord.G4, Coord.F4, Coord.F3],
    ];

    public static int FeatureToPattern(int feature) => feature >> 2;

    /// <summary>
    /// For each square, the features it is part of and the value of its digit in them (3 to the power of its place),
    /// as pairs (feature, power). Built from <see cref="FeatureToCoord"/>; the same as <c>coord_to_feature</c>.
    /// </summary>
    public static readonly (int Feature, int Power)[][] CoordToFeature = CreateCoordToFeature(0, N_PATTERN_FEATURES);

    /// <summary>The same for the features of the move-ordering evaluation of the endgame (32..47).</summary>
    public static readonly (int Feature, int Power)[][] CoordToFeatureMoveOrderingEnd =
        CreateCoordToFeature(EVAL_IDX_START_MOVE_ORDERING_END, EVAL_IDX_END_MOVE_ORDERING_END);

    private static (int Feature, int Power)[][] CreateCoordToFeature(int start, int end)
    {
        var lists = new List<(int, int)>[Common.HW2];
        for (int cell = 0; cell < Common.HW2; cell++)
        {
            lists[cell] = [];
        }

        for (int feature = start; feature < end; feature++)
        {
            int[] cells = FeatureToCoord[feature];
            for (int k = 0; k < cells.Length; k++)
            {
                lists[cells[k]].Add((feature, Pow3[cells.Length - 1 - k]));
            }
        }

        return lists.Select(list => list.ToArray()).ToArray();
    }
}
