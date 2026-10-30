// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/level.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana, GPL-3.0-or-later.

namespace Stello.Egaroucid;

/// <summary>
/// The levels 0-60 (level.hpp): the midgame depth and selectivity of each level, and from how many empty squares it
/// searches to the end of the game, with which selectivity.
/// </summary>
internal static class Level
{
    public const int N_LEVEL = 61;
    public const int MAX_LEVEL = N_LEVEL - 1;
    public const int DEFAULT_LEVEL = 21;
    public const int NODEPTH = 100;
    public const int LEVEL_TYPE_BOOK = 1000;

    public const int N_SELECTIVITY_LEVEL = 7;
    public const int MPC_74 = 0;
    public const int MPC_88 = 1;
    public const int MPC_93 = 2;
    public const int MPC_98 = 3;
    public const int MPC_99 = 4;
    public const int MPC_999 = 5;
    public const int MPC_100 = 6;

    /// <summary>The selectivity of each MPC level in percent: 74, 88, 93, 98, 99, 99.9, 100.</summary>
    public static ReadOnlySpan<double> SelectivityPercentage => [74, 88, 93, 98, 99, 99.9, 100];

    /// <param name="MidLookahead">The midgame depth.</param>
    /// <param name="Complete0">The most empty squares at which the level searches to the end of the game.</param>
    public readonly record struct Definition(
        int MidLookahead, int MidMpcLevel,
        int Complete0, int Complete0MpcLevel,
        int Complete1, int Complete1MpcLevel,
        int Complete2, int Complete2MpcLevel,
        int Complete3, int Complete3MpcLevel,
        int Complete4, int Complete4MpcLevel);

    public static readonly Definition[] Definitions =
    [
        new(0, MPC_100, 0, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(1, MPC_100, 2, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(2, MPC_100, 4, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(3, MPC_100, 6, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(4, MPC_100, 8, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(5, MPC_100, 10, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(6, MPC_100, 12, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(7, MPC_100, 14, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(8, MPC_100, 16, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(9, MPC_100, 18, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(10, MPC_100, 20, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(11, MPC_74, 25, MPC_98, 23, MPC_99, 21, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(12, MPC_74, 25, MPC_98, 23, MPC_99, 21, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(13, MPC_74, 27, MPC_93, 25, MPC_98, 23, MPC_99, 21, MPC_100, NODEPTH, MPC_100),
        new(14, MPC_74, 27, MPC_93, 25, MPC_98, 23, MPC_99, 21, MPC_100, NODEPTH, MPC_100),
        new(15, MPC_74, 27, MPC_98, 24, MPC_99, 22, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(16, MPC_74, 27, MPC_98, 24, MPC_99, 22, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(17, MPC_74, 27, MPC_98, 24, MPC_99, 22, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(18, MPC_74, 29, MPC_93, 27, MPC_98, 24, MPC_99, 22, MPC_100, NODEPTH, MPC_100),
        new(19, MPC_74, 29, MPC_93, 27, MPC_98, 24, MPC_99, 22, MPC_100, NODEPTH, MPC_100),
        new(20, MPC_74, 30, MPC_93, 28, MPC_98, 26, MPC_99, 24, MPC_100, NODEPTH, MPC_100),
        new(21, MPC_74, 30, MPC_93, 28, MPC_98, 26, MPC_99, 24, MPC_100, NODEPTH, MPC_100),
        new(22, MPC_74, 30, MPC_93, 28, MPC_98, 26, MPC_99, 24, MPC_100, NODEPTH, MPC_100),
        new(23, MPC_74, 32, MPC_93, 30, MPC_98, 28, MPC_99, 26, MPC_100, NODEPTH, MPC_100),
        new(24, MPC_74, 32, MPC_93, 30, MPC_98, 28, MPC_99, 26, MPC_100, NODEPTH, MPC_100),
        new(25, MPC_74, 34, MPC_88, 32, MPC_93, 30, MPC_98, 28, MPC_99, 26, MPC_100),
        new(26, MPC_74, 34, MPC_88, 32, MPC_93, 30, MPC_98, 28, MPC_99, 26, MPC_100),
        new(27, MPC_74, 36, MPC_88, 34, MPC_93, 32, MPC_98, 30, MPC_99, 28, MPC_100),
        new(28, MPC_74, 36, MPC_88, 34, MPC_93, 32, MPC_98, 30, MPC_99, 28, MPC_100),
        new(29, MPC_74, 36, MPC_93, 34, MPC_98, 32, MPC_99, 30, MPC_100, NODEPTH, MPC_100),
        new(30, MPC_74, 36, MPC_93, 34, MPC_98, 32, MPC_99, 30, MPC_100, NODEPTH, MPC_100),
        new(31, MPC_74, 38, MPC_88, 36, MPC_93, 34, MPC_98, 32, MPC_99, 30, MPC_100),
        new(32, MPC_74, 38, MPC_88, 36, MPC_93, 34, MPC_98, 32, MPC_99, 30, MPC_100),
        new(33, MPC_74, 38, MPC_93, 36, MPC_98, 34, MPC_99, 32, MPC_100, NODEPTH, MPC_100),
        new(34, MPC_74, 38, MPC_93, 36, MPC_98, 34, MPC_99, 32, MPC_100, NODEPTH, MPC_100),
        new(35, MPC_74, 40, MPC_88, 38, MPC_93, 36, MPC_98, 34, MPC_99, 32, MPC_100),
        new(36, MPC_74, 40, MPC_88, 38, MPC_93, 36, MPC_98, 34, MPC_99, 32, MPC_100),
        new(37, MPC_74, 40, MPC_93, 36, MPC_98, 36, MPC_99, 34, MPC_100, NODEPTH, MPC_100),
        new(38, MPC_74, 40, MPC_93, 38, MPC_98, 36, MPC_99, 34, MPC_100, NODEPTH, MPC_100),
        new(39, MPC_74, 42, MPC_88, 40, MPC_93, 38, MPC_98, 36, MPC_99, 34, MPC_100),
        new(40, MPC_74, 42, MPC_88, 40, MPC_93, 38, MPC_98, 36, MPC_99, 34, MPC_100),
        new(41, MPC_74, 42, MPC_93, 40, MPC_98, 38, MPC_99, 36, MPC_100, NODEPTH, MPC_100),
        new(42, MPC_74, 42, MPC_93, 40, MPC_98, 38, MPC_99, 36, MPC_100, NODEPTH, MPC_100),
        new(43, MPC_74, 44, MPC_88, 42, MPC_93, 40, MPC_98, 38, MPC_99, 36, MPC_100),
        new(44, MPC_74, 44, MPC_88, 42, MPC_93, 40, MPC_98, 38, MPC_99, 36, MPC_100),
        new(45, MPC_74, 44, MPC_93, 42, MPC_98, 40, MPC_99, 38, MPC_100, NODEPTH, MPC_100),
        new(46, MPC_74, 44, MPC_93, 42, MPC_98, 40, MPC_99, 38, MPC_100, NODEPTH, MPC_100),
        new(47, MPC_74, 46, MPC_88, 44, MPC_93, 42, MPC_98, 40, MPC_99, 38, MPC_100),
        new(48, MPC_74, 46, MPC_88, 44, MPC_93, 42, MPC_98, 40, MPC_99, 38, MPC_100),
        new(49, MPC_74, 46, MPC_93, 44, MPC_98, 42, MPC_99, 40, MPC_100, NODEPTH, MPC_100),
        new(50, MPC_74, 46, MPC_93, 44, MPC_98, 42, MPC_99, 40, MPC_100, NODEPTH, MPC_100),
        new(51, MPC_74, 48, MPC_88, 46, MPC_93, 44, MPC_98, 42, MPC_99, 40, MPC_100),
        new(52, MPC_74, 50, MPC_88, 48, MPC_93, 46, MPC_98, 44, MPC_99, 42, MPC_100),
        new(53, MPC_74, 54, MPC_88, 52, MPC_93, 50, MPC_98, 48, MPC_99, 46, MPC_100),
        new(54, MPC_74, 56, MPC_88, 54, MPC_93, 52, MPC_98, 50, MPC_99, 48, MPC_100),
        new(55, MPC_74, 58, MPC_88, 56, MPC_93, 54, MPC_98, 52, MPC_99, 50, MPC_100),
        new(56, MPC_88, 60, MPC_88, 58, MPC_93, 56, MPC_98, 54, MPC_99, 52, MPC_100),
        new(57, MPC_88, 60, MPC_93, 58, MPC_98, 56, MPC_99, 54, MPC_100, NODEPTH, MPC_100),
        new(58, MPC_93, 60, MPC_98, 58, MPC_99, 56, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(59, MPC_93, 60, MPC_99, 58, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
        new(60, MPC_100, 60, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100, NODEPTH, MPC_100),
    ];

    /// <summary>The depth and selectivity of a search at <paramref name="level"/> after <paramref name="nMoves"/> moves (level.hpp <c>get_level</c>).</summary>
    public static void GetLevel(int level, int nMoves, out bool isMidSearch, out int depth, out int mpcLevel)
    {
        level = Math.Clamp(level, 1, 60);
        Definition status = Definitions[level];
        int nEmpties = 60 - nMoves;
        if (nEmpties > status.Complete0)
        {
            isMidSearch = true;
            depth = status.MidLookahead;
            mpcLevel = status.MidMpcLevel;
        }
        else
        {
            isMidSearch = false;
            depth = nEmpties;
            if (nEmpties > status.Complete1)
            {
                mpcLevel = status.Complete0MpcLevel;
            }
            else if (nEmpties > status.Complete2)
            {
                mpcLevel = status.Complete1MpcLevel;
            }
            else if (nEmpties > status.Complete3)
            {
                mpcLevel = status.Complete2MpcLevel;
            }
            else if (nEmpties > status.Complete4)
            {
                mpcLevel = status.Complete3MpcLevel;
            }
            else
            {
                mpcLevel = status.Complete4MpcLevel;
            }
        }
    }
}
