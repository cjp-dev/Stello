// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/bit_generic.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana, GPL-3.0-or-later.

using System.Numerics;
using System.Runtime.CompilerServices;

namespace Stello.Egaroucid;

/// <summary>Bit manipulation (bit_generic.hpp). <c>FirstBit</c> and <c>NextBit</c> give 64 when no bit is left, as in C++.</summary>
internal static class Bits
{
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int PopCount(ulong x) => BitOperations.PopCount(x);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int Ctz(ulong x) => BitOperations.TrailingZeroCount(x);

    /// <summary>The lowest set bit of <paramref name="x"/> (64 if none).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int FirstBit(ref ulong x) => BitOperations.TrailingZeroCount(x);

    /// <summary>Clears the lowest set bit of <paramref name="x"/> and returns the next one (64 if none).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int NextBit(ref ulong x)
    {
        x &= x - 1;
        return BitOperations.TrailingZeroCount(x);
    }

    public static ulong WhiteLineMirror(ulong x)
    {
        ulong a = (x ^ (x >> 7)) & 0x00AA00AA00AA00AAUL;
        x = x ^ a ^ (a << 7);
        a = (x ^ (x >> 14)) & 0x0000CCCC0000CCCCUL;
        x = x ^ a ^ (a << 14);
        a = (x ^ (x >> 28)) & 0x00000000F0F0F0F0UL;
        return x ^ a ^ (a << 28);
    }

    public static ulong BlackLineMirror(ulong x)
    {
        ulong a = (x ^ (x >> 9)) & 0x0055005500550055UL;
        x = x ^ a ^ (a << 9);
        a = (x ^ (x >> 18)) & 0x0000333300003333UL;
        x = x ^ a ^ (a << 18);
        a = (x ^ (x >> 36)) & 0x000000000F0F0F0FUL;
        return x ^ a ^ (a << 36);
    }

    public static ulong VerticalMirror(ulong x) => System.Buffers.Binary.BinaryPrimitives.ReverseEndianness(x);

    public static ulong HorizontalMirror(ulong x)
    {
        x = ((x >> 1) & 0x5555555555555555UL) | ((x << 1) & 0xAAAAAAAAAAAAAAAAUL);
        x = ((x >> 2) & 0x3333333333333333UL) | ((x << 2) & 0xCCCCCCCCCCCCCCCCUL);
        return ((x >> 4) & 0x0F0F0F0F0F0F0F0FUL) | ((x << 4) & 0xF0F0F0F0F0F0F0F0UL);
    }

    public static ulong Rotate90(ulong x) => VerticalMirror(WhiteLineMirror(x));

    public static ulong Rotate270(ulong x) => VerticalMirror(BlackLineMirror(x));

    public static ulong Rotate180(ulong x) => HorizontalMirror(VerticalMirror(x));

    /// <summary>Row <paramref name="t"/> (0 = the row of bits 0..7) as 8 bits.</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int JoinHLine(ulong x, int t) => (int)((x >> (HW * t)) & 0xFFU);

    /// <summary>Column <paramref name="c"/> (0 = the column of bit 0) as 8 bits, bit k from row k.</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int JoinVLine(ulong x, int c) => (int)((((x >> c) & 0x0101010101010101UL) * 0x0102040810204080UL) >> 56);

    private const int HW = Common.HW;
}
