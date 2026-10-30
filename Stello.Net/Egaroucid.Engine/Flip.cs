// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/flip_generic.hpp and mobility_generic.hpp (https://github.com/Nyanyan/Egaroucid),
// © Takuto Yamana, GPL-3.0-or-later. The flip calculation is written differently (rays and bit scans instead of the
// lookup tables of the C++ code); the result, the set of flipped discs, is the same by definition.

using System.Numerics;
using System.Runtime.CompilerServices;

namespace Stello.Egaroucid;

/// <summary>A move: the square played and the discs it flips (flip_generic.hpp <c>Flip</c>).</summary>
internal struct Flip
{
    public int Pos;
    public ulong Flipped;

    // For each square and each of the 8 directions, the squares from the next one to the edge. Directions 0..3 go
    // towards higher bit indices, 4..7 towards lower ones.
    private static readonly ulong[] Rays = CreateRays();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public ulong Calc(ulong player, ulong opponent, int place)
    {
        Pos = place;
        ulong flipped = 0;
        ulong notOpponent = ~opponent;
        ref ulong rays = ref Rays[place * 8];
        for (int d = 0; d < 4; d++)
        {
            ulong ray = Unsafe.Add(ref rays, d);
            ulong outflank = ray & notOpponent;
            outflank &= (ulong)-(long)outflank;
            if ((outflank & player) != 0)
            {
                flipped |= (outflank - 1) & ray;
            }
        }

        for (int d = 4; d < 8; d++)
        {
            ulong ray = Unsafe.Add(ref rays, d);
            ulong outflank = ray & notOpponent;
            if (outflank != 0)
            {
                ulong top = 1UL << (63 - BitOperations.LeadingZeroCount(outflank));
                if ((top & player) != 0)
                {
                    flipped |= ray & ~((top << 1) - 1);
                }
            }
        }

        Flipped = flipped;
        return flipped;
    }

    /// <summary>The legal moves of <paramref name="p"/> against <paramref name="o"/> (mobility_generic.hpp <c>calc_legal</c>).</summary>
    public static ulong CalcLegal(ulong p, ulong o)
    {
        ulong moves, mO;
        ulong flip1, flip7, flip9, flip8, pre1, pre7, pre9, pre8;
        mO = o & 0x7e7e7e7e7e7e7e7eUL;
        flip1 = mO & (p << 1); flip7 = mO & (p << 7); flip9 = mO & (p << 9); flip8 = o & (p << 8);
        flip1 |= mO & (flip1 << 1); flip7 |= mO & (flip7 << 7); flip9 |= mO & (flip9 << 9); flip8 |= o & (flip8 << 8);
        pre1 = mO & (mO << 1); pre7 = mO & (mO << 7); pre9 = mO & (mO << 9); pre8 = o & (o << 8);
        flip1 |= pre1 & (flip1 << 2); flip7 |= pre7 & (flip7 << 14); flip9 |= pre9 & (flip9 << 18); flip8 |= pre8 & (flip8 << 16);
        flip1 |= pre1 & (flip1 << 2); flip7 |= pre7 & (flip7 << 14); flip9 |= pre9 & (flip9 << 18); flip8 |= pre8 & (flip8 << 16);
        moves = flip1 << 1; moves |= flip7 << 7; moves |= flip9 << 9; moves |= flip8 << 8;
        flip1 = mO & (p >> 1); flip7 = mO & (p >> 7); flip9 = mO & (p >> 9); flip8 = o & (p >> 8);
        flip1 |= mO & (flip1 >> 1); flip7 |= mO & (flip7 >> 7); flip9 |= mO & (flip9 >> 9); flip8 |= o & (flip8 >> 8);
        pre1 >>= 1; pre7 >>= 7; pre9 >>= 9; pre8 >>= 8;
        flip1 |= pre1 & (flip1 >> 2); flip7 |= pre7 & (flip7 >> 14); flip9 |= pre9 & (flip9 >> 18); flip8 |= pre8 & (flip8 >> 16);
        flip1 |= pre1 & (flip1 >> 2); flip7 |= pre7 & (flip7 >> 14); flip9 |= pre9 & (flip9 >> 18); flip8 |= pre8 & (flip8 >> 16);
        moves |= flip1 >> 1; moves |= flip7 >> 7; moves |= flip9 >> 9; moves |= flip8 >> 8;
        return moves & ~(p | o);
    }

    private static ulong[] CreateRays()
    {
        // In bit terms a step is +1 (towards a1 along a row), +8 (towards row 1), +7 and +9 (diagonals), and the
        // negatives. The geometry is checked on (row, column) so rays stop at the edge.
        (int Row, int Col)[] steps = [(0, 1), (1, 0), (1, -1), (1, 1), (0, -1), (-1, 0), (-1, 1), (-1, -1)];
        var rays = new ulong[64 * 8];
        for (int square = 0; square < 64; square++)
        {
            int row = square / 8, col = square % 8;
            for (int d = 0; d < 8; d++)
            {
                ulong ray = 0;
                for (int r = row + steps[d].Row, c = col + steps[d].Col; r is >= 0 and < 8 && c is >= 0 and < 8;
                     r += steps[d].Row, c += steps[d].Col)
                {
                    ray |= 1UL << (r * 8 + c);
                }

                rays[square * 8 + d] = ray;
            }
        }

        return rays;
    }
}
