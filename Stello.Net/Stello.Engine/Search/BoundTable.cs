using System.Numerics;

namespace Stello.Engine.Search;

/// <summary>
/// The midgame hash table of phase 5 (<see cref="SearchOptions.BoundTable"/>): each entry keeps a lower and an upper
/// bound, the best move and the best move before it, and the search that stored it. A slot holds two entries; the
/// first keeps the deepest result of the current search, the second takes the rest. Entries from earlier searches
/// give way to new ones, so a game does not fill the table with old positions.
/// </summary>
internal sealed class BoundTable
{
    public const int NoBound = short.MaxValue;

    private readonly Entry[] _entries;
    private readonly int _shift;
    private byte _generation;

    /// <param name="bits">The table has 2^bits slots of two entries.</param>
    public BoundTable(int bits)
    {
        _entries = new Entry[2 << bits];
        _shift = 64 - bits;
    }

    /// <summary>Called at the start of each search; older entries become replaceable.</summary>
    public void NewSearch() => _generation = (byte)((_generation + 1) & 0x7F);

    public void Clear()
    {
        Array.Clear(_entries);
        _generation = 0;
    }

    /// <param name="tag">The side to move, 0 or 1.</param>
    public bool TryGet(ulong own, ulong opponent, int tag, out Entry entry)
    {
        int slot = Slot(own, opponent, tag);
        entry = _entries[slot];
        if (entry.Matches(own, opponent, tag))
        {
            return true;
        }

        entry = _entries[slot + 1];
        return entry.Matches(own, opponent, tag);
    }

    /// <summary>Stores the result of a search with the window (alpha, beta) that returned <paramref name="value"/>.</summary>
    public void Store(ulong own, ulong opponent, int tag, int depth, int alpha, int beta, int value, int move)
    {
        short lower = value > alpha ? (short)value : (short)-NoBound;
        short upper = value < beta ? (short)value : (short)NoBound;
        int slot = Slot(own, opponent, tag);
        for (int i = 0; i < 2; i++)
        {
            ref Entry same = ref _entries[slot + i];
            if (!same.Matches(own, opponent, tag))
            {
                continue;
            }

            int second = same.Move != move ? same.Move : same.Second;
            if (depth == same.Depth)
            {
                // The same depth again: the bounds together, unless the search changed its mind and they cross.
                short mergedLower = Math.Max(lower, same.Lower);
                short mergedUpper = Math.Min(upper, same.Upper);
                if (mergedLower <= mergedUpper)
                {
                    (lower, upper) = (mergedLower, mergedUpper);
                }
            }
            else if (depth < same.Depth && same.Generation == _generation)
            {
                // A shallower result does not replace a deeper one of this search; only its move order is kept.
                same = same with { Move = (sbyte)move, Second = (sbyte)second };
                return;
            }

            same = new Entry(own, opponent, lower, upper, (sbyte)depth, (sbyte)move, (sbyte)second, (byte)tag, _generation);
            return;
        }

        var entry = new Entry(own, opponent, lower, upper, (sbyte)depth, (sbyte)move, -1, (byte)tag, _generation);
        ref Entry deep = ref _entries[slot];
        if (deep.IsEmpty || deep.Generation != _generation || depth >= deep.Depth)
        {
            _entries[slot + 1] = deep;
            deep = entry;
        }
        else
        {
            _entries[slot + 1] = entry;
        }
    }

    private int Slot(ulong own, ulong opponent, int tag)
    {
        ulong hash = own * 0x9E37_79B9_7F4A_7C15UL;
        hash ^= BitOperations.RotateLeft(opponent * 0xC2B2_AE3D_27D4_EB4FUL, 31) + (ulong)tag;
        hash *= 0xD6E8_FEB8_6659_FD93UL;
        return (int)(hash >> _shift) * 2;
    }

    /// <param name="Lower">A lower bound of the value, or -<see cref="NoBound"/>.</param>
    /// <param name="Upper">An upper bound of the value, or <see cref="NoBound"/>.</param>
    /// <param name="Second">The best move of the result before, or -1.</param>
    internal readonly record struct Entry(
        ulong Own, ulong Opponent, short Lower, short Upper, sbyte Depth, sbyte Move, sbyte Second, byte Tag, byte Generation)
    {
        public bool IsEmpty => Own == 0 && Opponent == 0;

        public bool Matches(ulong own, ulong opponent, int tag) => !IsEmpty && Own == own && Opponent == opponent && Tag == tag;
    }
}
