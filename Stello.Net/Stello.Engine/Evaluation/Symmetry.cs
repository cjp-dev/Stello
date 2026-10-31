using System.Buffers.Binary;
using System.Runtime.CompilerServices;

namespace Stello.Engine.Evaluation;

/// <summary>
/// The 8 symmetries of the board on bitboards (bit n = square n, a1 = 0, h1 = 7, a8 = 56). Symmetry number s
/// mirrors the columns if bit 0 is set, then the rows if bit 1 is set, then transposes (a2 ↔ b1) if bit 2 is set.
/// </summary>
internal static class Symmetry
{
    public const int Count = 8;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ulong Apply(ulong x, int symmetry)
    {
        if ((symmetry & 1) != 0)
        {
            x = MirrorColumns(x);
        }

        if ((symmetry & 2) != 0)
        {
            x = BinaryPrimitives.ReverseEndianness(x);
        }

        if ((symmetry & 4) != 0)
        {
            x = Transpose(x);
        }

        return x;
    }

    /// <summary>The square that <paramref name="square"/> moves to under <paramref name="symmetry"/>.</summary>
    public static int MapSquare(int square, int symmetry) =>
        System.Numerics.BitOperations.TrailingZeroCount(Apply(1UL << square, symmetry));

    /// <summary>a1 ↔ h1: reverses the bits of every byte.</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ulong MirrorColumns(ulong x)
    {
        x = ((x >> 1) & 0x5555_5555_5555_5555UL) | ((x & 0x5555_5555_5555_5555UL) << 1);
        x = ((x >> 2) & 0x3333_3333_3333_3333UL) | ((x & 0x3333_3333_3333_3333UL) << 2);
        return ((x >> 4) & 0x0F0F_0F0F_0F0F_0F0FUL) | ((x & 0x0F0F_0F0F_0F0F_0F0FUL) << 4);
    }

    /// <summary>Mirrors in the a1–h8 diagonal: the square at (column c, row r) moves to (column r, row c).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ulong Transpose(ulong x)
    {
        ulong t = 0x0F0F_0F0F_0000_0000UL & (x ^ (x << 28));
        x ^= t ^ (t >> 28);
        t = 0x3333_0000_3333_0000UL & (x ^ (x << 14));
        x ^= t ^ (t >> 14);
        t = 0x5500_5500_5500_5500UL & (x ^ (x << 7));
        return x ^ t ^ (t >> 7);
    }
}
