using System.Numerics;

namespace Stello.Engine.Search;

internal enum Bound : byte
{
    None,
    Exact,
    Lower, // C++: HI_HASH (score >= beta)
    Upper, // C++: LO_HASH (score <= alpha)
}

/// <summary>
/// Hash table keyed on the full position, so there are no false hits. Each slot holds two entries: one keeps the
/// deepest result, the other is always replaced (C++ f_hash_put: one entry; a deeper entry for the same position is kept).
/// </summary>
internal sealed class TranspositionTable
{
    private readonly Entry[] _entries;
    private readonly int _shift;

    /// <param name="bits">The table has 2^bits slots of two entries.</param>
    public TranspositionTable(int bits)
    {
        _entries = new Entry[2 << bits];
        _shift = 64 - bits;
    }

    /// <param name="tag">Anything else the value depends on, such as the side to move.</param>
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

    public void Store(ulong own, ulong opponent, int tag, int depth, Bound bound, int value, int move)
    {
        int slot = Slot(own, opponent, tag);
        ref Entry deep = ref _entries[slot];
        var entry = new Entry(own, opponent, (short)value, (sbyte)depth, bound, (sbyte)move, (byte)tag);

        if (deep.Matches(own, opponent, tag))
        {
            if (depth >= deep.Depth)
            {
                deep = entry;
            }
        }
        else if (deep.Bound == Bound.None || depth >= deep.Depth)
        {
            _entries[slot + 1] = deep;
            deep = entry;
        }
        else
        {
            _entries[slot + 1] = entry;
        }
    }

    public void Clear() => Array.Clear(_entries);

    private int Slot(ulong own, ulong opponent, int tag)
    {
        ulong hash = own * 0x9E37_79B9_7F4A_7C15UL;
        hash ^= BitOperations.RotateLeft(opponent * 0xC2B2_AE3D_27D4_EB4FUL, 31) + (ulong)tag;
        hash *= 0xD6E8_FEB8_6659_FD93UL;
        return (int)(hash >> _shift) * 2;
    }

    internal readonly record struct Entry(ulong Own, ulong Opponent, short Value, sbyte Depth, Bound Bound, sbyte Move, byte Tag)
    {
        public bool Matches(ulong own, ulong opponent, int tag) =>
            Bound != Bound.None && Own == own && Opponent == opponent && Tag == tag;
    }
}
