// SPDX-License-Identifier: GPL-3.0-only

using System.Buffers.Binary;
using System.Diagnostics;
using Stello.Egaroucid;
using Stello.Engine;
using EgaroucidBoard = Stello.Egaroucid.Board;
using EgaroucidResult = Stello.Egaroucid.SearchResult;
using SearchResult = Stello.Engine.SearchResult;

namespace Stello.Benchmark;

/// <summary>
/// The C# port of Egaroucid (Egaroucid.Engine) as a match player, on 1 thread. It keeps its transposition table through
/// a game, as Egaroucid does, and clears it before the next.
/// </summary>
/// <param name="tableBits">The transposition table has 2^tableBits slots of 24 bytes.</param>
/// <param name="book">Egaroucid's book, or null to search every move.</param>
/// <param name="bookAccuracy">0 plays one of the best book moves; up to 10 allows worse ones (Egaroucid's book accuracy).</param>
/// <param name="seed">Seeds the book's choice anew every game, so a game can be played again.</param>
internal sealed class EgaroucidPlayer(int tableBits, Book? book, int bookAccuracy, int seed) : IMatchPlayer
{
    private readonly SearchContext _context = new(tableBits);
    private Random _random = new(seed);

    public void NewGame()
    {
        _context.Clear();
        _random = new Random(seed);
    }

    /// <remarks>
    /// Time per move and time per game use Egaroucid's time-limited search. Fixed depth is read as Egaroucid's level
    /// (<see cref="SearchLimits.Depth"/> = level); solve is level 60 without the book.
    /// </remarks>
    public SearchResult ChooseMove(Engine.Board board, Player player, SearchLimits limits, CancellationToken cancellationToken)
    {
        EgaroucidBoard position = ToEgaroucid(board, player);
        var searching = new SearchingFlag();
        using CancellationTokenRegistration stop = cancellationToken.Register(() => searching.Value = false);
        long started = Stopwatch.GetTimestamp();
        EgaroucidResult result = limits.Mode switch
        {
            TimeControlMode.TimePerMove => Ai.SearchTimePerMove(_context, position, Milliseconds(limits.Time), book, bookAccuracy, _random, searching),
            TimeControlMode.TimePerGame => Ai.SearchTimePerGame(_context, position, Milliseconds(limits.Time), book, bookAccuracy, _random, searching),
            TimeControlMode.FixedDepth => Ai.AiCommon(_context, position, -Common.SCORE_MAX, Common.SCORE_MAX, limits.Depth, book, bookAccuracy, _random,
                useMultiThread: false, position.GetLegal(), SearchConst.TIME_LIMIT_INF, searching, log: null),
            _ => Ai.Search(_context, position, Level.MAX_LEVEL, useMultiThread: false),
        };
        TimeSpan used = Stopwatch.GetElapsedTime(started);
        cancellationToken.ThrowIfCancellationRequested();
        return ToStello(result, board, player, used);
    }

    /// <summary>Stello's board (a1 = bit 0, black and white) as Egaroucid's (a1 = bit 63, side to move and opponent).</summary>
    public static EgaroucidBoard ToEgaroucid(Engine.Board board, Player player) =>
        new(ReverseBits(board.Discs(player)), ReverseBits(board.Discs(player.Opponent())));

    /// <summary>Egaroucid's move (a1 = 63) as Stello's square (a1 = 0).</summary>
    public static Square ToSquare(int policy) => new(63 - policy);

    /// <summary>Egaroucid's result in Stello's terms. The score is in discs for every kind.</summary>
    /// <exception cref="InvalidOperationException">Egaroucid gave no legal move.</exception>
    public static SearchResult ToStello(EgaroucidResult result, Engine.Board board, Player player, TimeSpan used)
    {
        if (result.Policy is < 0 or >= Common.HW2 || !board.IsLegal(player, ToSquare(result.Policy)))
        {
            throw new InvalidOperationException($"Egaroucid gave the move {result.Policy}, which is not legal.");
        }

        ScoreKind kind = result.Level == Level.LEVEL_TYPE_BOOK ? ScoreKind.Book
            : result.IsEndSearch && result.Probability == 100 ? ScoreKind.Exact
            : ScoreKind.Heuristic;
        return new SearchResult(
            new Move(ToSquare(result.Policy)), result.Value, kind, Math.Max(0, result.Depth),
            (long)(result.Nodes + result.ClogNodes), 0, used);
    }

    /// <summary>Reverses the 64 bits: a1 = bit 0 becomes a1 = bit 63, h8 = bit 63 becomes h8 = bit 0.</summary>
    public static ulong ReverseBits(ulong x)
    {
        x = ((x >> 1) & 0x5555555555555555UL) | ((x & 0x5555555555555555UL) << 1);
        x = ((x >> 2) & 0x3333333333333333UL) | ((x & 0x3333333333333333UL) << 2);
        x = ((x >> 4) & 0x0F0F0F0F0F0F0F0FUL) | ((x & 0x0F0F0F0F0F0F0F0FUL) << 4);
        return BinaryPrimitives.ReverseEndianness(x);
    }

    private static ulong Milliseconds(TimeSpan time) => (ulong)Math.Max(1, Math.Round(time.TotalMilliseconds));
}
