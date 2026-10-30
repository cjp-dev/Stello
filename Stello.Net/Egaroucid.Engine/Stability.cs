// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/stability.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana, GPL-3.0-or-later.

namespace Stello.Egaroucid;

/// <summary>Stable discs (stability.hpp): the edges from a table of all 8-square lines, the inside by growing from them.</summary>
internal static class Stability
{
    // [b * 256 + w][0]: the stable squares of an edge in row form (bits 0..7), [1]: in column form (bits 0, 8, .., 56).
    private static readonly ulong[] EdgeRow = new ulong[Common.N_8BIT * Common.N_8BIT];
    private static readonly ulong[] EdgeColumn = new ulong[Common.N_8BIT * Common.N_8BIT];

    static Stability()
    {
        var memo = new short[Common.N_8BIT * Common.N_8BIT];
        Array.Fill(memo, (short)-1);
        for (int b = 0; b < Common.N_8BIT; ++b)
        {
            for (int w = b; w < Common.N_8BIT; ++w)
            {
                if ((b & w) != 0)
                {
                    continue;
                }

                int stab = CalcStabilityLine(b, w, memo);
                ulong row = 0, column = 0;
                for (int place = 0; place < Common.HW; ++place)
                {
                    if ((1 & (stab >> place)) != 0)
                    {
                        row |= 1UL << place;
                        column |= 1UL << (place * Common.HW);
                    }
                }

                EdgeRow[b * Common.N_8BIT + w] = EdgeRow[w * Common.N_8BIT + b] = row;
                EdgeColumn[b * Common.N_8BIT + w] = EdgeColumn[w * Common.N_8BIT + b] = column;
            }
        }
    }

    /// <summary>The stable discs of <paramref name="player"/> (stability.hpp <c>calc_stability</c>).</summary>
    public static ulong CalcStability(ulong player, ulong opponent)
    {
        ulong playerStability = 0, nStability;
        ulong playerMask = player & 0x007E7E7E7E7E7E00UL;
        nStability = EdgeRow[(int)(player & 0xFF) * Common.N_8BIT + (int)(opponent & 0xFF)];
        nStability |= EdgeRow[(int)(player >> 56) * Common.N_8BIT + (int)(opponent >> 56)] << 56;
        nStability |= EdgeColumn[Bits.JoinVLine(player, 0) * Common.N_8BIT + Bits.JoinVLine(opponent, 0)];
        nStability |= EdgeColumn[Bits.JoinVLine(player, 7) * Common.N_8BIT + Bits.JoinVLine(opponent, 7)] << 7;
        FullStability(player | opponent, out ulong fullH, out ulong fullV, out ulong fullD7, out ulong fullD9);
        nStability |= fullH & fullV & fullD7 & fullD9;
        nStability &= player;
        while ((nStability & ~playerStability) != 0)
        {
            playerStability |= nStability;
            ulong h = (playerStability >> 1) | (playerStability << 1) | fullH;
            ulong v = (playerStability >> Common.HW) | (playerStability << Common.HW) | fullV;
            ulong d7 = (playerStability >> Common.HW_M1) | (playerStability << Common.HW_M1) | fullD7;
            ulong d9 = (playerStability >> Common.HW_P1) | (playerStability << Common.HW_P1) | fullD9;
            nStability = h & v & d7 & d9 & playerMask;
        }

        return playerStability;
    }

    /// <summary>The stable edge discs of <paramref name="player"/> only (stability.hpp <c>calc_edge_stability</c>).</summary>
    public static ulong CalcEdgeStability(ulong player, ulong opponent)
    {
        ulong nStability = EdgeRow[(int)(player & 0xFF) * Common.N_8BIT + (int)(opponent & 0xFF)];
        nStability |= EdgeRow[(int)(player >> 56) * Common.N_8BIT + (int)(opponent >> 56)] << 56;
        nStability |= EdgeColumn[Bits.JoinVLine(player, 0) * Common.N_8BIT + Bits.JoinVLine(opponent, 0)];
        nStability |= EdgeColumn[Bits.JoinVLine(player, 7) * Common.N_8BIT + Bits.JoinVLine(opponent, 7)] << 7;
        return nStability;
    }

    private static void ProbablyMoveLine(int p, int o, int place, out int np, out int no)
    {
        int i, j;
        np = p | (1 << place);
        for (i = place - 1; i > 0 && (1 & (o >> i)) != 0; --i)
        {
        }

        if ((1 & (p >> i)) != 0)
        {
            for (j = place - 1; j > i; --j)
            {
                np ^= 1 << j;
            }
        }

        for (i = place + 1; i < Common.HW_M1 && (1 & (o >> i)) != 0; ++i)
        {
        }

        if ((1 & (p >> i)) != 0)
        {
            for (j = place + 1; j < i; ++j)
            {
                np ^= 1 << j;
            }
        }

        no = o & ~np;
    }

    private static int CalcStabilityLine(int b, int w, short[] memo)
    {
        ref short memoElem = ref memo[b * Common.N_8BIT + w];
        if (memoElem >= 0)
        {
            return memoElem;
        }

        int res = b | w;
        int empties = ~(b | w);
        for (int i = 0; i < Common.HW; ++i)
        {
            if ((1 & (empties >> i)) != 0)
            {
                ProbablyMoveLine(b, w, i, out int nb, out int nw);
                res &= b | nw;
                res &= CalcStabilityLine(nb, nw, memo);
                ProbablyMoveLine(w, b, i, out nw, out nb);
                res &= w | nb;
                res &= CalcStabilityLine(nb, nw, memo);
            }
        }

        memo[b * Common.N_8BIT + w] = (short)res;
        return res;
    }

    private static void FullStability(ulong discs, out ulong h, out ulong v, out ulong d7, out ulong d9)
    {
        h = FullStabilityH(discs);
        v = FullStabilityV(discs);
        FullStabilityD(discs, out d7, out d9);
    }

    private static ulong FullStabilityH(ulong full)
    {
        full &= full >> 1;
        full &= full >> 2;
        full &= full >> 4;
        return (full & 0x0101010101010101UL) * 0xFF;
    }

    private static ulong FullStabilityV(ulong full)
    {
        full &= (full >> 8) | (full << 56);
        full &= (full >> 16) | (full << 48);
        full &= (full >> 32) | (full << 32);
        return full;
    }

    private static void FullStabilityD(ulong full, out ulong fullD7, out ulong fullD9)
    {
        const ulong edge = 0xFF818181818181FFUL;
        ulong l7, r7, l9, r9;
        l7 = r7 = full;
        l7 &= edge | (l7 >> 7); r7 &= edge | (r7 << 7);
        l7 &= 0xFFFF030303030303UL | (l7 >> 14); r7 &= 0xC0C0C0C0C0C0FFFFUL | (r7 << 14);
        l7 &= 0xFFFFFFFF0F0F0F0FUL | (l7 >> 28); r7 &= 0xF0F0F0F0FFFFFFFFUL | (r7 << 28);
        fullD7 = l7 & r7;
        l9 = r9 = full;
        l9 &= edge | (l9 >> 9); r9 &= edge | (r9 << 9);
        l9 &= 0xFFFFC0C0C0C0C0C0UL | (l9 >> 18); r9 &= 0x030303030303FFFFUL | (r9 << 18);
        fullD9 = l9 & r9 & (0x0F0F0F0FF0F0F0F0UL | (l9 >> 36) | (r9 << 36));
    }
}
