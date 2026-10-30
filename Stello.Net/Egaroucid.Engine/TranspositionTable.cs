// SPDX-License-Identifier: GPL-3.0-only
// Ported from Egaroucid src/engine/transposition_table.hpp and endsearch_nws.hpp (https://github.com/Nyanyan/Egaroucid),
// © Takuto Yamana, GPL-3.0-or-later.

using System.Runtime.CompilerServices;

namespace Stello.Egaroucid;

/// <summary>
/// The transposition table (transposition_table.hpp). A position is looked for in 3 consecutive slots from its hash
/// code. A slot keeps the bounds of the position's value for a level (depth and selectivity), the two best moves and an
/// importance flag; when 40 % of the slots have been newly taken, every importance is cleared, so old entries can be
/// replaced.
/// </summary>
/// <remarks>
/// C++ indexes the table directly with the hash code, which has <see cref="HashTables.Level"/> bits (25), so the table
/// has 2^25 + 2 slots (about 800 MB here). A smaller table (<c>tableBits</c> below the hash level) uses the lower bits
/// of the hash code; it searches differently from C++ but is fine for matches. The single-threaded port has no locks.
/// </remarks>
internal sealed class TranspositionTable
{
    public const int N_LOOP = 3;
    public const int N_TRANSPOSITION_MOVES = 2;
    private const double RegisterThresholdRate = 0.4;

    private readonly Node[] _nodes;
    private readonly uint _mask;
    private readonly ulong _registeredThreshold;
    private ulong _registered;

    public TranspositionTable(int tableBits)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(tableBits, 10);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(tableBits, HashTables.MaxHashLevel);
        TableBits = tableBits;
        _mask = (uint)((1UL << tableBits) - 1);
        _nodes = new Node[(1L << tableBits) + N_LOOP - 1];
        _registeredThreshold = (ulong)(_nodes.Length * RegisterThresholdRate);
        Init();
    }

    public int TableBits { get; }

    public static uint GetLevelCommon(int depth, int mpcLevel) => ((uint)depth << 8) | (uint)mpcLevel;

    public void Init()
    {
        for (int i = 0; i < _nodes.Length; i++)
        {
            _nodes[i].Init();
        }

        _registered = 0;
    }

    public void Reg(Search search, uint hash, int depth, int alpha, int beta, int value, int policy)
    {
        uint level = GetLevelCommon(depth, search.MpcLevel);
        int index = (int)(hash & _mask);
        for (int i = 0; i < N_LOOP; ++i, ++index)
        {
            ref Node node = ref _nodes[index];
            uint nodeLevel = node.GetLevel();
            if (nodeLevel <= level)
            {
                if (node.Player == search.Board.Player && node.Opponent == search.Board.Opponent)
                {
                    if (nodeLevel == level)
                    {
                        node.RegSameLevel(alpha, beta, value, policy);
                    }
                    else
                    {
                        node.RegNewLevel(depth, search.MpcLevel, alpha, beta, value, policy);
                    }
                }
                else
                {
                    if (node.Importance == 0)
                    {
                        ++_registered;
                    }

                    node.Player = search.Board.Player;
                    node.Opponent = search.Board.Opponent;
                    node.RegNewData(depth, search.MpcLevel, alpha, beta, value, policy);
                }

                break;
            }
        }

        if (_registered >= _registeredThreshold)
        {
            ResetImportance();
        }
    }

    /// <summary>The moves of the position, and its bounds if they are for at least this level (<c>get</c>).</summary>
    public void Get(Search search, uint hash, int depth, ref int lower, ref int upper, Span<int> moves)
    {
        uint level = GetLevelCommon(depth, search.MpcLevel);
        int index = (int)(hash & _mask);
        for (int i = 0; i < N_LOOP; ++i, ++index)
        {
            ref Node node = ref _nodes[index];
            if (node.Player == search.Board.Player && node.Opponent == search.Board.Opponent)
            {
                moves[0] = node.Move0;
                moves[1] = node.Move1;
                if (node.GetLevelNoImportance() >= level)
                {
                    lower = node.Lower;
                    upper = node.Upper;
                }

                return;
            }
        }
    }

    /// <summary>The bounds if the position is there for at least this level (<c>get_bounds</c>).</summary>
    public bool GetBounds(Search search, uint hash, int depth, ref int lower, ref int upper)
    {
        uint level = GetLevelCommon(depth, search.MpcLevel);
        int index = (int)(hash & _mask);
        for (int i = 0; i < N_LOOP; ++i, ++index)
        {
            ref Node node = ref _nodes[index];
            if (node.Player == search.Board.Player && node.Opponent == search.Board.Opponent && node.GetLevelNoImportance() >= level)
            {
                lower = node.Lower;
                upper = node.Upper;
                return true;
            }
        }

        return false;
    }

    /// <summary>The moves of the position at any level (<c>get_moves_any_level</c>).</summary>
    public bool GetMovesAnyLevel(in Board board, uint hash, Span<int> moves)
    {
        int index = (int)(hash & _mask);
        for (int i = 0; i < N_LOOP; ++i, ++index)
        {
            ref Node node = ref _nodes[index];
            if (node.Player == board.Player && node.Opponent == board.Opponent)
            {
                moves[0] = node.Move0;
                moves[1] = node.Move1;
                return true;
            }
        }

        return false;
    }

    /// <summary>
    /// Whether the position is there at all; its bounds only if they are for at least this level
    /// (<c>has_node_any_level_get_bounds</c>, used by enhanced transposition cutoff).
    /// </summary>
    public bool HasNodeAnyLevelGetBounds(Search search, uint hash, int depth, ref int lower, ref int upper)
    {
        uint level = GetLevelCommon(depth, search.MpcLevel);
        int index = (int)(hash & _mask);
        for (int i = 0; i < N_LOOP; ++i, ++index)
        {
            ref Node node = ref _nodes[index];
            if (node.Player == search.Board.Player && node.Opponent == search.Board.Opponent)
            {
                if (node.GetLevelNoImportance() >= level)
                {
                    lower = node.Lower;
                    upper = node.Upper;
                }

                return true;
            }
        }

        return false;
    }

    private void ResetImportance()
    {
        for (int i = 0; i < _nodes.Length; i++)
        {
            _nodes[i].Importance = 0;
        }

        _registered = 0;
    }

    private struct Node
    {
        public ulong Player;
        public ulong Opponent;
        public byte MpcLevel;
        public byte Depth;
        public byte Importance;
        public sbyte Lower;
        public sbyte Upper;
        public byte Move0;
        public byte Move1;

        public void Init()
        {
            Player = 0;
            Opponent = 0;
            Lower = -Common.SCORE_MAX;
            Upper = Common.SCORE_MAX;
            Move0 = SearchConst.MOVE_UNDEFINED;
            Move1 = SearchConst.MOVE_UNDEFINED;
            MpcLevel = 0;
            Depth = 0;
            Importance = 0;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public readonly uint GetLevel() => Importance != 0 ? GetLevelCommon(Depth, MpcLevel) : 0;

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public readonly uint GetLevelNoImportance() => GetLevelCommon(Depth, MpcLevel);

        public void RegSameLevel(int alpha, int beta, int value, int policy)
        {
            if (value < beta && value < Upper)
            {
                Upper = (sbyte)value;
                if (alpha < value && value < Lower)
                {
                    Lower = (sbyte)value;
                }
            }

            if (alpha < value && Lower < value)
            {
                Lower = (sbyte)value;
                if (value < beta && Upper < value)
                {
                    Upper = (sbyte)value;
                }
            }

            if ((alpha < value || value == -Common.SCORE_MAX) && Move0 != (byte)policy && IsValidPolicy(policy))
            {
                Move1 = Move0;
                Move0 = (byte)policy;
            }

            Importance = 1;
        }

        public void RegNewLevel(int d, int ml, int alpha, int beta, int value, int policy)
        {
            Upper = value < beta ? (sbyte)value : (sbyte)Common.SCORE_MAX;
            Lower = alpha < value ? (sbyte)value : (sbyte)-Common.SCORE_MAX;
            if ((alpha < value || value == -Common.SCORE_MAX) && Move0 != (byte)policy && IsValidPolicy(policy))
            {
                Move1 = Move0;
                Move0 = (byte)policy;
            }

            Depth = (byte)d;
            MpcLevel = (byte)ml;
            Importance = 1;
        }

        public void RegNewData(int d, int ml, int alpha, int beta, int value, int policy)
        {
            Upper = value < beta ? (sbyte)value : (sbyte)Common.SCORE_MAX;
            Lower = alpha < value ? (sbyte)value : (sbyte)-Common.SCORE_MAX;
            Move0 = (alpha < value || value == -Common.SCORE_MAX) && IsValidPolicy(policy) ? (byte)policy : (byte)SearchConst.MOVE_UNDEFINED;
            Move1 = SearchConst.MOVE_UNDEFINED;
            Depth = (byte)d;
            MpcLevel = (byte)ml;
            Importance = 1;
        }

        private static bool IsValidPolicy(int policy) => policy is >= 0 and < Common.HW2;
    }
}

/// <summary>
/// The small table of the endgame search from 7 to 13 empty squares (endsearch_nws.hpp <c>lttable</c>): 1024 slots per
/// number of empty squares, one position each, keeping a lower and an upper bound. C++ never clears it.
/// </summary>
internal sealed class LocalTranspositionTable
{
    public const int Size = 1024;
    private const int SizeBits = 10;
    private const int Rows = SearchConst.MID_TO_END_DEPTH - SearchConst.END_FAST_DEPTH;

    private readonly Entry[] _entries = new Entry[Rows * Size];

    public void Clear() => Array.Clear(_entries);

    /// <summary>The slot of <paramref name="board"/>, which has <paramref name="nDiscs"/> discs (<c>get_ltt</c>).</summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public ref Entry Get(in Board board, int nDiscs)
    {
        uint hash = (uint)(((board.Player * 0x9dda1c54cfe6b6e9UL) ^ (board.Opponent * 0xa2e6c0300831e05aUL)) >> (Common.HW2 - SizeBits));
        return ref _entries[(Common.HW2 - nDiscs - SearchConst.END_FAST_DEPTH) * Size + (int)hash];
    }

    internal struct Entry
    {
        public ulong Player;
        public ulong Opponent;
        public int Lower;
        public int Upper;

        public readonly bool Cmp(in Board board) => board.Player == Player && board.Opponent == Opponent;

        public void SetScore(in Board board, int lower, int upper)
        {
            Player = board.Player;
            Opponent = board.Opponent;
            Lower = lower;
            Upper = upper;
        }
    }
}
