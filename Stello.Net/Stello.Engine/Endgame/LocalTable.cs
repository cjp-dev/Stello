// SPDX-License-Identifier: GPL-3.0-only
// Adapted from Egaroucid src/engine/transposition_table.hpp (the local table of endsearch_nws.hpp)
// (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana, GPL-3.0-or-later.

using System.Runtime.CompilerServices;

namespace Stello.Engine.Endgame;

/// <summary>
/// A small table for the null-window solver at 7 to 13 empty squares: one row of 1024 slots per number of empty
/// squares, each slot a position with a lower and an upper bound. A new result simply replaces the old one.
/// </summary>
internal sealed class LocalTable
{
    private const int SizeBits = 10;
    private const int Size = 1 << SizeBits;
    private const int FirstEmpties = 6;
    private const int Rows = 13 - FirstEmpties;

    private readonly Entry[] _entries = new Entry[Rows * Size];

    public void Clear() => Array.Clear(_entries);

    /// <summary>The slot of a position with <paramref name="empties"/> (6 to 12) empty squares.</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public ref Entry Get(ulong own, ulong opponent, int empties)
    {
        int hash = (int)(((own * 0x9dda1c54cfe6b6e9UL) ^ (opponent * 0xa2e6c0300831e05aUL)) >> (64 - SizeBits));
        return ref _entries[(empties - FirstEmpties) * Size + hash];
    }

    internal struct Entry
    {
        public ulong Own;
        public ulong Opponent;
        public int Lower;
        public int Upper;

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public readonly bool Matches(ulong own, ulong opponent) => Own == own && Opponent == opponent;

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public void Set(ulong own, ulong opponent, int lower, int upper)
        {
            Own = own;
            Opponent = opponent;
            Lower = lower;
            Upper = upper;
        }
    }
}
