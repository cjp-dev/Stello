// SPDX-License-Identifier: GPL-3.0-only
// Adapted from Egaroucid src/engine/search.hpp, endsearch_common.hpp, move_ordering.hpp, flip_generic.hpp and
// mobility_generic.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana, GPL-3.0-or-later, through the C# port
// in Stello.Net/Egaroucid.Engine. Egaroucid numbers the squares the other way round (bit 63 is a1); reversing the bits
// turns the board half a turn, and every table here is the same after that turn, so they are used as they are.

using System.Numerics;
using System.Runtime.CompilerServices;

namespace Stello.Engine.Endgame;

/// <summary>Bit tricks and small tables for the endgame solver.</summary>
internal static class EndgameBits
{
    /// <summary>A value no legal-move set can have (the four centre squares), for "not computed yet".</summary>
    public const ulong LegalUndefined = 0x0000_0018_1800_0000UL;

    /// <summary>
    /// The neighbours of each square through which a disc can be flipped: the 3 × 3 neighbourhood without the
    /// neighbours in directions too short to flip anything (Egaroucid <c>bit_around</c>).
    /// </summary>
    public static readonly ulong[] FlipNeighbours =
    [
        0x0000000000000302UL, 0x0000000000000604UL, 0x0000000000000e0aUL, 0x0000000000001c14UL,
        0x0000000000003828UL, 0x0000000000007050UL, 0x0000000000006020UL, 0x000000000000c040UL,
        0x0000000000030200UL, 0x0000000000060400UL, 0x00000000000e0a00UL, 0x00000000001c1400UL,
        0x0000000000382800UL, 0x0000000000705000UL, 0x0000000000602000UL, 0x0000000000c04000UL,
        0x0000000003020300UL, 0x0000000006040600UL, 0x000000000e0a0e00UL, 0x000000001c141c00UL,
        0x0000000038283800UL, 0x0000000070507000UL, 0x0000000060206000UL, 0x00000000c040c000UL,
        0x0000000302030000UL, 0x0000000604060000UL, 0x0000000e0a0e0000UL, 0x0000001c141c0000UL,
        0x0000003828380000UL, 0x0000007050700000UL, 0x0000006020600000UL, 0x000000c040c00000UL,
        0x0000030203000000UL, 0x0000060406000000UL, 0x00000e0a0e000000UL, 0x00001c141c000000UL,
        0x0000382838000000UL, 0x0000705070000000UL, 0x0000602060000000UL, 0x0000c040c0000000UL,
        0x0003020300000000UL, 0x0006040600000000UL, 0x000e0a0e00000000UL, 0x001c141c00000000UL,
        0x0038283800000000UL, 0x0070507000000000UL, 0x0060206000000000UL, 0x00c040c000000000UL,
        0x0002030000000000UL, 0x0004060000000000UL, 0x000a0e0000000000UL, 0x00141c0000000000UL,
        0x0028380000000000UL, 0x0050700000000000UL, 0x0020600000000000UL, 0x0040c00000000000UL,
        0x0203000000000000UL, 0x0406000000000000UL, 0x0a0e000000000000UL, 0x141c000000000000UL,
        0x2838000000000000UL, 0x5070000000000000UL, 0x2060000000000000UL, 0x40c0000000000000UL,
    ];

    /// <summary>The quadrant of each square as one bit, for the parity of the empty squares per quadrant.</summary>
    public static readonly int[] QuadrantBit =
    [
        1, 1, 1, 1, 2, 2, 2, 2,
        1, 1, 1, 1, 2, 2, 2, 2,
        1, 1, 1, 1, 2, 2, 2, 2,
        1, 1, 1, 1, 2, 2, 2, 2,
        4, 4, 4, 4, 8, 8, 8, 8,
        4, 4, 4, 4, 8, 8, 8, 8,
        4, 4, 4, 4, 8, 8, 8, 8,
        4, 4, 4, 4, 8, 8, 8, 8,
    ];

    /// <summary>
    /// By number of discs: a null-window search at alpha this high or higher tries the stable-disc cutoff, because
    /// below it the opponent's stable discs can rarely prove a fail low.
    /// </summary>
    public static readonly int[] StabilityThresholdNws =
    [
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 64, 64, 64, 64, 64, 64, 64,
        64, 62, 62, 60, 60, 58, 58, 56,
        56, 54, 54, 52, 52, 50, 50, 48,
        48, 46, 44, 42, 40, 38, 36, 34,
        32, 30, 28, 26, 24, 22, 20, 16,
        14, 12, 10, 8, 6, 99, 99, 99,
        99,
    ];

    /// <summary>The same for a search with a window: the test is on beta.</summary>
    public static readonly int[] StabilityThreshold =
    [
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 62, 62, 60, 60, 58, 58, 56,
        56, 54, 54, 52, 52, 50, 50, 48,
        48, 46, 46, 44, 44, 42, 42, 40,
        40, 38, 36, 34, 32, 30, 28, 26,
        24, 22, 20, 18, 16, 14, 12, 8,
        6, 4, 2, 0, -2, 99, 99, 99,
        99,
    ];

    private static readonly ulong[] AllNeighbours = CreateAllNeighbours();

    // For each square and direction, the squares from the next one to the edge. Directions 0..3 go towards higher
    // bit numbers, 4..7 towards lower ones.
    private static readonly ulong[] Rays = CreateRays();

    /// <summary>The discs <paramref name="own"/> flips by playing <paramref name="square"/> (0 if the move is illegal).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ulong Flips(ulong own, ulong opponent, int square)
    {
        ulong flipped = 0;
        ulong notOpponent = ~opponent;
        ref ulong rays = ref Rays[square * 8];
        for (int d = 0; d < 4; d++)
        {
            ulong ray = Unsafe.Add(ref rays, d);
            ulong outflank = ray & notOpponent;
            outflank &= (ulong)-(long)outflank;
            if ((outflank & own) != 0)
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
                if ((top & own) != 0)
                {
                    flipped |= ray & ~((top << 1) - 1);
                }
            }
        }

        return flipped;
    }

    /// <summary>The legal moves of <paramref name="p"/> against <paramref name="o"/> (Egaroucid <c>calc_legal</c>).</summary>
    public static ulong LegalMoves(ulong p, ulong o)
    {
        ulong mO = o & 0x7e7e7e7e7e7e7e7eUL;
        ulong flip1 = mO & (p << 1), flip7 = mO & (p << 7), flip9 = mO & (p << 9), flip8 = o & (p << 8);
        flip1 |= mO & (flip1 << 1); flip7 |= mO & (flip7 << 7); flip9 |= mO & (flip9 << 9); flip8 |= o & (flip8 << 8);
        ulong pre1 = mO & (mO << 1), pre7 = mO & (mO << 7), pre9 = mO & (mO << 9), pre8 = o & (o << 8);
        flip1 |= pre1 & (flip1 << 2); flip7 |= pre7 & (flip7 << 14); flip9 |= pre9 & (flip9 << 18); flip8 |= pre8 & (flip8 << 16);
        flip1 |= pre1 & (flip1 << 2); flip7 |= pre7 & (flip7 << 14); flip9 |= pre9 & (flip9 << 18); flip8 |= pre8 & (flip8 << 16);
        ulong moves = (flip1 << 1) | (flip7 << 7) | (flip9 << 9) | (flip8 << 8);
        flip1 = mO & (p >> 1); flip7 = mO & (p >> 7); flip9 = mO & (p >> 9); flip8 = o & (p >> 8);
        flip1 |= mO & (flip1 >> 1); flip7 |= mO & (flip7 >> 7); flip9 |= mO & (flip9 >> 9); flip8 |= o & (flip8 >> 8);
        pre1 >>= 1; pre7 >>= 7; pre9 >>= 9; pre8 >>= 8;
        flip1 |= pre1 & (flip1 >> 2); flip7 |= pre7 & (flip7 >> 14); flip9 |= pre9 & (flip9 >> 18); flip8 |= pre8 & (flip8 >> 16);
        flip1 |= pre1 & (flip1 >> 2); flip7 |= pre7 & (flip7 >> 14); flip9 |= pre9 & (flip9 >> 18); flip8 |= pre8 & (flip8 >> 16);
        moves |= (flip1 >> 1) | (flip7 >> 7) | (flip9 >> 9) | (flip8 >> 8);
        return moves & ~(p | o);
    }

    /// <summary>The empty squares with no empty neighbour (Egaroucid <c>empty1_bb</c>).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ulong LoneEmpties(ulong own, ulong opponent)
    {
        ulong empty = ~(own | opponent);
        ulong t0 = (empty & 0x7f7f7f7f7f7f7f7fUL) << 1;
        ulong t1 = (empty & 0xfefefefefefefefeUL) >> 1;
        ulong e1 = t0 | t1;
        t0 = empty | e1;
        t1 = t0 << 8;
        t0 >>= 8;
        return ~(e1 | t0 | t1) & empty;
    }

    /// <summary>Whether no square next to <paramref name="square"/> is in <paramref name="empties"/>.</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool IsLoneEmpty(int square, ulong empties) => (AllNeighbours[square] & empties) == 0;

    /// <summary>The parity of the empty squares in each quadrant, one bit per quadrant as in <see cref="QuadrantBit"/>.</summary>
    public static int Parity(ulong empty) =>
        (BitOperations.PopCount(empty & 0x0000_0000_0F0F_0F0FUL) & 1)
        | ((BitOperations.PopCount(empty & 0x0000_0000_F0F0_F0F0UL) & 1) << 1)
        | ((BitOperations.PopCount(empty & 0x0F0F_0F0F_0000_0000UL) & 1) << 2)
        | ((BitOperations.PopCount(empty & 0xF0F0_F0F0_0000_0000UL) & 1) << 3);

    /// <summary>The number of moves with corners counted twice (Egaroucid <c>get_n_moves_cornerX2</c>).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int MovesCornersTwice(ulong legal) =>
        BitOperations.PopCount(legal) + BitOperations.PopCount(legal & 0x8100_0000_0000_0081UL);

    /// <summary>Twice the number of moves plus the corner moves (Egaroucid <c>get_weighted_n_moves</c>).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int WeightedMoves(ulong legal) =>
        BitOperations.PopCount(legal) * 2 + BitOperations.PopCount(legal & 0x8100_0000_0000_0081UL);

    /// <summary>The empty squares next to <paramref name="discs"/> (Egaroucid <c>get_potential_mobility</c>).</summary>
    public static int PotentialMobility(ulong discs, ulong empties)
    {
        ulong h = discs & 0x7E7E7E7E7E7E7E7EUL;
        ulong v = discs & 0x00FFFFFFFFFFFF00UL;
        ulong hv = discs & 0x007E7E7E7E7E7E00UL;
        ulong around = (h << 1) | (h >> 1) | (v << 8) | (v >> 8) | (hv << 7) | (hv >> 7) | (hv << 9) | (hv >> 9);
        return BitOperations.PopCount(empties & around);
    }

    /// <summary>
    /// The final score with <paramref name="empties"/> empty squares for the side owning <paramref name="own"/>, the
    /// empty squares going to the winner (Egaroucid <c>end_evaluate(board, e)</c>).
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int ScoreWithEmpties(ulong own, int empties)
    {
        int score = BitOperations.PopCount(own) * 2 + empties;
        score += (((score >> 6) & 1) + (((score + 63) >> 7) & 1) - 1) * empties;
        return score - 64;
    }

    /// <summary>The same for an odd number of empty squares, where there is no draw (<c>end_evaluate_odd</c>).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int ScoreWithOddEmpties(ulong own, int empties)
    {
        int score = BitOperations.PopCount(own) * 2 + empties;
        score += (((score >> 5) & 2) - 1) * empties;
        return score - 64;
    }

    private static ulong[] CreateAllNeighbours()
    {
        var neighbours = new ulong[64];
        for (int square = 0; square < 64; square++)
        {
            int row = square >> 3, column = square & 7;
            for (int r = Math.Max(row - 1, 0); r <= Math.Min(row + 1, 7); r++)
            {
                for (int c = Math.Max(column - 1, 0); c <= Math.Min(column + 1, 7); c++)
                {
                    if (r != row || c != column)
                    {
                        neighbours[square] |= 1UL << (r * 8 + c);
                    }
                }
            }
        }

        return neighbours;
    }

    private static ulong[] CreateRays()
    {
        (int Row, int Column)[] steps = [(0, 1), (1, 0), (1, -1), (1, 1), (0, -1), (-1, 0), (-1, 1), (-1, -1)];
        var rays = new ulong[64 * 8];
        for (int square = 0; square < 64; square++)
        {
            int row = square / 8, column = square % 8;
            for (int d = 0; d < 8; d++)
            {
                ulong ray = 0;
                for (int r = row + steps[d].Row, c = column + steps[d].Column; r is >= 0 and < 8 && c is >= 0 and < 8;
                     r += steps[d].Row, c += steps[d].Column)
                {
                    ray |= 1UL << (r * 8 + c);
                }

                rays[square * 8 + d] = ray;
            }
        }

        return rays;
    }
}
