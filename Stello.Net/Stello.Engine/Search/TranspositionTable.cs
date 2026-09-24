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
/// Hash table keyed on the full position, so there are no false hits. A deeper entry for the same position is
/// kept, otherwise the slot is replaced (C++: f_hash_put).
/// </summary>
internal sealed class TranspositionTable
{
    private readonly Entry[] _entries;
    private readonly int _shift;

    public TranspositionTable(int bits)
    {
        _entries = new Entry[1 << bits];
        _shift = 64 - bits;
    }

    /// <param name="tag">Anything else the value depends on, such as the side to move.</param>
    public bool TryGet(ulong own, ulong opponent, int tag, out Entry entry)
    {
        entry = _entries[Index(own, opponent, tag)];
        return entry.Bound != Bound.None && entry.Own == own && entry.Opponent == opponent && entry.Tag == tag;
    }

    public void Store(ulong own, ulong opponent, int tag, int depth, Bound bound, int value, int move)
    {
        ref Entry entry = ref _entries[Index(own, opponent, tag)];
        if (entry.Own == own && entry.Opponent == opponent && entry.Tag == tag && entry.Depth > depth)
        {
            return;
        }

        entry = new Entry(own, opponent, (short)value, (sbyte)depth, bound, (sbyte)move, (byte)tag);
    }

    public void Clear() => Array.Clear(_entries);

    private int Index(ulong own, ulong opponent, int tag)
    {
        ulong hash = own * 0x9E37_79B9_7F4A_7C15UL;
        hash ^= BitOperations.RotateLeft(opponent * 0xC2B2_AE3D_27D4_EB4FUL, 31) + (ulong)tag;
        hash *= 0xD6E8_FEB8_6659_FD93UL;
        return (int)(hash >> _shift);
    }

    internal readonly record struct Entry(ulong Own, ulong Opponent, short Value, sbyte Depth, Bound Bound, sbyte Move, byte Tag);
}
