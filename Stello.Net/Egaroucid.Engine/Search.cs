// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/search.hpp (https://github.com/Nyanyan/Egaroucid), © Takuto Yamana, GPL-3.0-or-later.

using System.Runtime.CompilerServices;

namespace Stello.Egaroucid;

/// <summary>Search constants and tables (search.hpp).</summary>
internal static class SearchConst
{
    public const int END_FAST_DEPTH = 6;
    public const int END_SIMPLE_DEPTH = 10;
    public const int MID_TO_END_DEPTH = 13;
    public const int MID_TO_END_DEPTH_MPC = 9;
    public const int MID_SIMPLE_ORDERING_DEPTH = 4;
    public const ulong TIME_LIMIT_INF = ulong.MaxValue;
    public const int SCORE_UNDEFINED = -126;
    public const int MOVE_UNDEFINED = 125;
    public const int MOVE_NOMOVE = 65;
    public const int MOVE_PASS = 64;
    public const int SEARCH_BOOK = -1;
    public const int MAX_N_BRANCHES = 35;
    public const int N_STATIC_CELL_PRIORITY = 4;

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

    /// <summary>The quadrant bit of each square, for the parity of the empty squares per quadrant.</summary>
    public static readonly int[] CellDiv4 =
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

    /// <summary>The order in which the shallowest searches try squares: corners, centre box, A/B squares, X/C squares.</summary>
    public static readonly ulong[] StaticCellPriority =
    [
        0x8100000000000081UL,
        0x00003C24243C0000UL,
        0x3C3CC3C3C3C33C3CUL,
        0x42C300000000C342UL,
    ];

    /// <summary>
    /// The neighbours of each square through which a disc can be flipped (common.hpp <c>bit_around</c>): the 3 × 3
    /// neighbourhood without the neighbours in directions too short to flip anything.
    /// </summary>
    public static readonly ulong[] BitAround =
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
}

/// <summary>A move with its ordering value and the opponent's legal moves after it (search.hpp <c>Flip_value</c>).</summary>
internal struct FlipValue
{
    public Flip Flip;
    public int Value;
    public ulong NLegal;

    public void Reset()
    {
        Value = 0;
        NLegal = Common.LEGAL_UNDEFINED;
    }
}

/// <summary>The result of a search (search.hpp <c>Search_result</c>).</summary>
internal sealed class SearchResult
{
    public int Level;
    public int Policy = SearchConst.MOVE_UNDEFINED;
    public int Value = SearchConst.SCORE_UNDEFINED;
    public int Depth = -1;
    public ulong Time;
    public ulong Nodes;
    public ulong ClogTime;
    public ulong ClogNodes;
    public ulong Nps;
    public bool IsEndSearch;
    public int Probability;
}

/// <summary>A flag that a running search polls; clearing it stops the search (C++ <c>bool *searching</c>).</summary>
internal sealed class SearchingFlag
{
    public volatile bool Value = true;
}

/// <summary>A move found by the clog search: a move after which the game ends soon, with its exact value.</summary>
internal readonly record struct ClogResult(int Pos, int Val);

/// <summary>
/// The state of one search (search.hpp <c>Search</c>): the position, the number of discs, the parity of the empty squares
/// per quadrant, the selectivity, the node counter and the evaluation features. It also carries the engine context (the
/// transposition tables), which C++ keeps in globals.
/// </summary>
internal sealed class Search
{
    public Board Board;
    public int NDiscs;
    public int Parity;
    public int MpcLevel;
    public ulong NNodes;
    public readonly EvalSearch Eval = new();
    public bool UseMultiThread;
    public bool IsPresearch;
    public readonly SearchContext Context;

    public Search(SearchContext context, in Board board, int mpcLevel, bool useMultiThread, bool isPresearch)
    {
        Context = context;
        Board = board;
        NDiscs = board.NDiscs();
        MpcLevel = mpcLevel;
        UseMultiThread = useMultiThread;
        IsPresearch = isPresearch;
        Parity = CalcParity();
        Eval.Init(Board);
    }

    private int CalcParity()
    {
        ulong empty = ~(Board.Player | Board.Opponent);
        int res = 1 & Bits.PopCount(empty & 0x000000000F0F0F0FUL);
        res |= (1 & Bits.PopCount(empty & 0x00000000F0F0F0F0UL)) << 1;
        res |= (1 & Bits.PopCount(empty & 0x0F0F0F0F00000000UL)) << 2;
        res |= (1 & Bits.PopCount(empty & 0xF0F0F0F000000000UL)) << 3;
        return res;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void Move(in Flip flip)
    {
        Eval.Move(flip);
        Board.MoveBoard(flip);
        ++NDiscs;
        Parity ^= SearchConst.CellDiv4[flip.Pos];
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void Undo(in Flip flip)
    {
        Eval.Undo();
        Board.UndoBoard(flip);
        --NDiscs;
        Parity ^= SearchConst.CellDiv4[flip.Pos];
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void Pass()
    {
        Eval.Pass();
        Board.Pass();
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void MoveEndsearch(in Flip flip)
    {
        Eval.MoveEndsearch(flip);
        Board.MoveBoard(flip);
        ++NDiscs;
        Parity ^= SearchConst.CellDiv4[flip.Pos];
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void UndoEndsearch(in Flip flip)
    {
        Eval.Undo();
        Board.UndoBoard(flip);
        --NDiscs;
        Parity ^= SearchConst.CellDiv4[flip.Pos];
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void PassEndsearch()
    {
        Eval.Pass();
        Board.Pass();
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void MoveNoeval(in Flip flip)
    {
        Board.MoveBoard(flip);
        ++NDiscs;
        Parity ^= SearchConst.CellDiv4[flip.Pos];
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void UndoNoeval(in Flip flip)
    {
        Board.UndoBoard(flip);
        --NDiscs;
        Parity ^= SearchConst.CellDiv4[flip.Pos];
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public void PassNoeval() => Board.Pass();

    public int Phase() => NDiscs - 4;
}

/// <summary>
/// What C++ keeps in globals for a search: the transposition table, the endgame's local table (thread-local in C++)
/// and the global stop flag. One context serves one search at a time.
/// </summary>
internal sealed class SearchContext
{
    public SearchContext(int tableBits = HashTables.DefaultHashLevel)
    {
        TranspositionTable = new TranspositionTable(tableBits);
    }

    public TranspositionTable TranspositionTable { get; }

    public LocalTranspositionTable LocalTable { get; } = new();

    /// <summary>C++ <c>global_searching</c>.</summary>
    public volatile bool GlobalSearching = true;

    /// <summary>Clears both tables, as before a search that must not depend on earlier ones.</summary>
    public void Clear()
    {
        TranspositionTable.Init();
        LocalTable.Clear();
    }
}
