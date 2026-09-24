using System.Numerics;

namespace Stello.Engine;

/// <summary>Move generation on raw bitboards (bit n = square index n) for the search.</summary>
internal static class Bitboards
{
    private const ulong NotColumnA = 0xFEFE_FEFE_FEFE_FEFEUL;
    private const ulong NotColumnH = 0x7F7F_7F7F_7F7F_7F7FUL;
    private const ulong InnerColumns = 0x7E7E_7E7E_7E7E_7E7EUL;
    private const ulong InnerRows = 0x00FF_FFFF_FFFF_FF00UL;
    private const ulong Inner = InnerColumns & InnerRows;

    public static ulong LegalMoves(ulong own, ulong opponent)
    {
        ulong empty = ~(own | opponent);
        ulong horizontal = opponent & InnerColumns;
        ulong vertical = opponent & InnerRows;
        ulong diagonal = opponent & Inner;

        ulong moves =
            ScanLeft(own, horizontal, 1) | ScanRight(own, horizontal, 1) |
            ScanLeft(own, vertical, 8) | ScanRight(own, vertical, 8) |
            ScanLeft(own, diagonal, 7) | ScanRight(own, diagonal, 7) |
            ScanLeft(own, diagonal, 9) | ScanRight(own, diagonal, 9);

        return moves & empty;
    }

    public static ulong Flips(ulong own, ulong opponent, int square)
    {
        ulong bit = 1UL << square;
        return RayLeft(own, opponent, bit, 1, NotColumnA)
            | RayLeft(own, opponent, bit, 7, NotColumnH)
            | RayLeft(own, opponent, bit, 8, ulong.MaxValue)
            | RayLeft(own, opponent, bit, 9, NotColumnA)
            | RayRight(own, opponent, bit, 1, NotColumnH)
            | RayRight(own, opponent, bit, 7, NotColumnA)
            | RayRight(own, opponent, bit, 8, ulong.MaxValue)
            | RayRight(own, opponent, bit, 9, NotColumnH);
    }

    /// <summary>Number of (empty square, direction) pairs where the neighbour is an opponent disc (C++: countmov potential).</summary>
    public static int PotentialMobility(ulong own, ulong opponent)
    {
        ulong empty = ~(own | opponent);
        return BitOperations.PopCount((opponent << 1) & NotColumnA & empty)
            + BitOperations.PopCount((opponent >> 1) & NotColumnH & empty)
            + BitOperations.PopCount((opponent << 8) & empty)
            + BitOperations.PopCount((opponent >> 8) & empty)
            + BitOperations.PopCount((opponent << 7) & NotColumnH & empty)
            + BitOperations.PopCount((opponent << 9) & NotColumnA & empty)
            + BitOperations.PopCount((opponent >> 7) & NotColumnA & empty)
            + BitOperations.PopCount((opponent >> 9) & NotColumnH & empty);
    }

    private static ulong ScanLeft(ulong own, ulong mask, int shift)
    {
        ulong run = mask & (own << shift);
        run |= mask & (run << shift);
        run |= mask & (run << shift);
        run |= mask & (run << shift);
        run |= mask & (run << shift);
        run |= mask & (run << shift);
        return run << shift;
    }

    private static ulong ScanRight(ulong own, ulong mask, int shift)
    {
        ulong run = mask & (own >> shift);
        run |= mask & (run >> shift);
        run |= mask & (run >> shift);
        run |= mask & (run >> shift);
        run |= mask & (run >> shift);
        run |= mask & (run >> shift);
        return run >> shift;
    }

    private static ulong RayLeft(ulong own, ulong opponent, ulong bit, int shift, ulong mask)
    {
        ulong line = 0;
        ulong next = (bit << shift) & mask;
        while ((next & opponent) != 0)
        {
            line |= next;
            next = (next << shift) & mask;
        }

        return (next & own) != 0 ? line : 0;
    }

    private static ulong RayRight(ulong own, ulong opponent, ulong bit, int shift, ulong mask)
    {
        ulong line = 0;
        ulong next = (bit >> shift) & mask;
        while ((next & opponent) != 0)
        {
            line |= next;
            next = (next >> shift) & mask;
        }

        return (next & own) != 0 ? line : 0;
    }
}
