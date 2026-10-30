// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/common.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana, GPL-3.0-or-later.

namespace Stello.Egaroucid;

/// <summary>Constants shared by the whole engine (common.hpp). Squares are bit indices with a1 = 63 and h8 = 0.</summary>
internal static class Common
{
    public const int HW = 8;
    public const int HW_M1 = 7;
    public const int HW_P1 = 9;
    public const int HW2 = 64;
    public const int HW2_M1 = 63;
    public const int HW2_P1 = 65;

    public const int BLACK = 0;
    public const int WHITE = 1;
    public const int VACANT = 2;

    public const int N_8BIT = 256;
    public const int N_16BIT = 65536;

    public const int INF = 100000000;
    public const int SCORE_INF = 127;
    public const int SCORE_MAX = 64;

    /// <summary>The "not yet computed" value for a legal-move mask (the four centre squares).</summary>
    public const ulong LEGAL_UNDEFINED = 0x0000001818000000UL;

    /// <summary>The square name ("a1" .. "h8") of a bit index (common.hpp <c>idx_to_coord</c>).</summary>
    public static string IdxToCoord(int idx)
    {
        int y = HW_M1 - idx / HW;
        int x = HW_M1 - idx % HW;
        return $"{(char)('a' + x)}{(char)('1' + y)}";
    }

    /// <summary>The bit index of a square name ("a1" .. "h8"), or -1.</summary>
    public static int CoordToIdx(ReadOnlySpan<char> coord)
    {
        if (coord.Length != 2)
        {
            return -1;
        }

        int x = char.ToLowerInvariant(coord[0]) - 'a';
        int y = coord[1] - '1';
        return x is < 0 or >= HW || y is < 0 or >= HW ? -1 : HW2_M1 - (y * HW + x);
    }

    public static bool IsBlackLikeChar(char c) => c is 'B' or 'b' or 'X' or 'x' or '0' or '*';

    public static bool IsWhiteLikeChar(char c) => c is 'W' or 'w' or 'O' or 'o' or '1';
}
