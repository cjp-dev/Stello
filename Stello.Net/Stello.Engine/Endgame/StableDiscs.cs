// SPDX-License-Identifier: GPL-3.0-only
// Adapted from Egaroucid src/engine/stability.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana,
// GPL-3.0-or-later, through the C# port in Stello.Net/Egaroucid.Engine. The calculation is the same under a half turn
// of the board, so it works on Stello's square numbering unchanged.

namespace Stello.Engine.Endgame;

/// <summary>
/// Stable discs, which no later move can flip: on the edges from a table of all 8-square lines, inside from full
/// lines and by growing from discs already stable.
/// </summary>
internal static class StableDiscs
{
    // [own * 256 + opponent]: the stable squares of an edge line, laid out as a row (bits 0..7) and as a column
    // (bits 0, 8, .., 56).
    private static readonly ulong[] EdgeRow = new ulong[256 * 256];
    private static readonly ulong[] EdgeColumn = new ulong[256 * 256];

    static StableDiscs()
    {
        var memo = new short[256 * 256];
        Array.Fill(memo, (short)-1);
        for (int b = 0; b < 256; ++b)
        {
            for (int w = b; w < 256; ++w)
            {
                if ((b & w) != 0)
                {
                    continue;
                }

                int stable = LineStability(b, w, memo);
                ulong row = 0, column = 0;
                for (int place = 0; place < 8; ++place)
                {
                    if (((stable >> place) & 1) != 0)
                    {
                        row |= 1UL << place;
                        column |= 1UL << (place * 8);
                    }
                }

                EdgeRow[b * 256 + w] = EdgeRow[w * 256 + b] = row;
                EdgeColumn[b * 256 + w] = EdgeColumn[w * 256 + b] = column;
            }
        }
    }

    /// <summary>The stable discs of <paramref name="own"/> (Egaroucid <c>calc_stability</c>).</summary>
    public static ulong Of(ulong own, ulong opponent)
    {
        ulong stable = 0;
        ulong inner = own & 0x007E7E7E7E7E7E00UL;
        ulong found = EdgeRow[(int)(own & 0xFF) * 256 + (int)(opponent & 0xFF)];
        found |= EdgeRow[(int)(own >> 56) * 256 + (int)(opponent >> 56)] << 56;
        found |= EdgeColumn[Column(own, 0) * 256 + Column(opponent, 0)];
        found |= EdgeColumn[Column(own, 7) * 256 + Column(opponent, 7)] << 7;

        ulong discs = own | opponent;
        ulong fullH = FullRows(discs);
        ulong fullV = FullColumns(discs);
        FullDiagonals(discs, out ulong fullD7, out ulong fullD9);
        found |= fullH & fullV & fullD7 & fullD9;
        found &= own;
        while ((found & ~stable) != 0)
        {
            stable |= found;
            ulong h = (stable >> 1) | (stable << 1) | fullH;
            ulong v = (stable >> 8) | (stable << 8) | fullV;
            ulong d7 = (stable >> 7) | (stable << 7) | fullD7;
            ulong d9 = (stable >> 9) | (stable << 9) | fullD9;
            found = h & v & d7 & d9 & inner;
        }

        return stable;
    }

    // Column c (0 = the column of bit 0) as 8 bits, bit k from row k.
    private static int Column(ulong x, int c) => (int)((((x >> c) & 0x0101010101010101UL) * 0x0102040810204080UL) >> 56);

    // The line after the side owning p plays place, flipping o's discs towards both ends of the line.
    private static void PlayOnLine(int p, int o, int place, out int np, out int no)
    {
        int i, j;
        np = p | (1 << place);
        for (i = place - 1; i > 0 && ((o >> i) & 1) != 0; --i)
        {
        }

        if (((p >> i) & 1) != 0)
        {
            for (j = place - 1; j > i; --j)
            {
                np ^= 1 << j;
            }
        }

        for (i = place + 1; i < 7 && ((o >> i) & 1) != 0; ++i)
        {
        }

        if (((p >> i) & 1) != 0)
        {
            for (j = place + 1; j < i; ++j)
            {
                np ^= 1 << j;
            }
        }

        no = o & ~np;
    }

    private static int LineStability(int b, int w, short[] memo)
    {
        ref short known = ref memo[b * 256 + w];
        if (known >= 0)
        {
            return known;
        }

        int result = b | w;
        int empties = ~(b | w);
        for (int i = 0; i < 8; ++i)
        {
            if (((empties >> i) & 1) != 0)
            {
                PlayOnLine(b, w, i, out int nb, out int nw);
                result &= b | nw;
                result &= LineStability(nb, nw, memo);
                PlayOnLine(w, b, i, out nw, out nb);
                result &= w | nb;
                result &= LineStability(nb, nw, memo);
            }
        }

        memo[b * 256 + w] = (short)result;
        return result;
    }

    private static ulong FullRows(ulong full)
    {
        full &= full >> 1;
        full &= full >> 2;
        full &= full >> 4;
        return (full & 0x0101010101010101UL) * 0xFF;
    }

    private static ulong FullColumns(ulong full)
    {
        full &= (full >> 8) | (full << 56);
        full &= (full >> 16) | (full << 48);
        full &= (full >> 32) | (full << 32);
        return full;
    }

    private static void FullDiagonals(ulong full, out ulong fullD7, out ulong fullD9)
    {
        const ulong edge = 0xFF818181818181FFUL;
        ulong l7 = full, r7 = full;
        l7 &= edge | (l7 >> 7); r7 &= edge | (r7 << 7);
        l7 &= 0xFFFF030303030303UL | (l7 >> 14); r7 &= 0xC0C0C0C0C0C0FFFFUL | (r7 << 14);
        l7 &= 0xFFFFFFFF0F0F0F0FUL | (l7 >> 28); r7 &= 0xF0F0F0F0FFFFFFFFUL | (r7 << 28);
        fullD7 = l7 & r7;

        ulong l9 = full, r9 = full;
        l9 &= edge | (l9 >> 9); r9 &= edge | (r9 << 9);
        l9 &= 0xFFFFC0C0C0C0C0C0UL | (l9 >> 18); r9 &= 0x030303030303FFFFUL | (r9 << 18);
        fullD9 = l9 & r9 & (0x0F0F0F0FF0F0F0F0UL | (l9 >> 36) | (r9 << 36));
    }
}
