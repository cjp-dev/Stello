// SPDX-License-Identifier: GPL-3.0-only

using System.Diagnostics;
using System.Globalization;

namespace Stello.Egaroucid.Tests;

/// <summary>The time-limit tests run alone: other tests allocating large tables (garbage-collection pauses) disturb the clock.</summary>
[CollectionDefinition("Timing", DisableParallelization = true)]
public sealed class TimingCollection;

/// <summary>Phase 4 of Plans/egaroucid-port-plan.md: Egaroucid's book and the time-limited search.</summary>
[Collection("Timing")]
public class BookAndTimeTests
{
    private static Book LoadBook()
    {
        Fixtures.LoadResources();
        return EgaroucidResources.Book;
    }

    [EgaroucidFact]
    public void Book_ValuesEqualReference()
    {
        Book book = LoadBook();
        Assert.True(book.Count > 1_000_000, $"only {book.Count} positions");
        int rows = 0;
        foreach (string[] row in Fixtures.Rows("book.txt"))
        {
            Board board = Board.FromString(row[1]);
            string own = book.TryGetValue(board, out int value) ? value.ToString(CultureInfo.InvariantCulture) : "-";
            Assert.True(row[2] == own, $"{row[0]}: position value {own}, expected {row[2]}");
            string children = string.Join(',', book.ChildValues(board, board.GetLegal())
                .Select(child => $"{Common.IdxToCoord(child.Policy)}:{child.Value.ToString(CultureInfo.InvariantCulture)}"));
            Assert.True((row.Length > 3 ? row[3] : "") == children, $"{row[0]}: children {children}, expected {(row.Length > 3 ? row[3] : "")}");
            rows++;
        }

        Assert.True(rows > 200, $"only {rows} rows");
    }

    [EgaroucidFact]
    public void Book_Accuracy0_PicksOneOfTheBestMoves()
    {
        Book book = LoadBook();
        var random = new Random(1);
        var seen = new HashSet<int>();
        Board start = Board.Initial;
        List<BookValue> children = book.ChildValues(start, start.GetLegal());
        int best = children.Max(child => child.Value);
        for (int i = 0; i < 200; i++)
        {
            BookValue choice = book.GetRandom(start, 0, start.GetLegal(), random);
            Assert.Equal(best, choice.Value);
            seen.Add(choice.Policy);
        }

        // The start position is symmetric: all four first moves are equally good, and a uniform draw finds them all.
        Assert.Equal(children.Count(child => child.Value == best), seen.Count);
    }

    [EgaroucidFact]
    public void Book_IsUsedInTheOpening()
    {
        LoadBook();
        var context = new SearchContext(20);
        SearchResult result = Ai.SearchTimePerMove(context, Board.Initial, 100, EgaroucidResources.Book, 0, new Random(1), new SearchingFlag());
        Assert.Equal(Level.LEVEL_TYPE_BOOK, result.Level);
        Assert.True((Board.Initial.GetLegal() & (1UL << result.Policy)) != 0);
    }

    /// <summary>As in C++: when the side to move must pass, <c>ai</c> searches nothing and gives no move.</summary>
    [EgaroucidFact]
    public void Search_WhenTheSideToMoveMustPass_GivesNoMove()
    {
        Fixtures.LoadResources();
        string[] parts = File.ReadLines(Fixtures.PathOf("positions-random.txt")).First(line => line.StartsWith('p')).Split(' ');
        Board board = Board.FromString(parts[1] + parts[2]);
        Assert.Equal(0UL, board.GetLegal());
        SearchResult result = Ai.Search(new SearchContext(20), board, 21, useMultiThread: false);
        Assert.Equal(SearchConst.MOVE_UNDEFINED, result.Policy);
        Assert.Equal(-SearchConst.SCORE_UNDEFINED, result.Value);
        Assert.Equal(-1, result.Depth);
    }

    [EgaroucidFact]
    public void TimePerMove_KeepsTheLimitAndGivesALegalMove()
    {
        Fixtures.LoadResources();
        var context = new SearchContext(20);
        Ai.SearchTimePerMove(context, Board.Initial, 50, book: null, 0, random: null, new SearchingFlag()); // JIT warm-up
        foreach (string[] row in Fixtures.Rows("search-midgame-level1.txt").Take(8))
        {
            Board board = Board.FromString(row[1]);
            context.Clear();
            var watch = Stopwatch.StartNew();
            SearchResult result = Ai.SearchTimePerMove(context, board, 200, book: null, 0, random: null, new SearchingFlag());
            watch.Stop();
            Assert.True((board.GetLegal() & (1UL << result.Policy)) != 0, $"{row[0]}: {Ai.PolicyName(result.Policy)} is not legal");
            Assert.True(watch.ElapsedMilliseconds < 200 * 1.25 + 50, $"{row[0]}: {watch.ElapsedMilliseconds} ms for 200 ms");
            Assert.True(result.Depth >= 1, $"{row[0]}: no completed iteration"); // depth itself depends on the machine load
        }
    }

    [EgaroucidFact]
    public void TimePerMove_StopsWhenTheFlagIsCleared()
    {
        Fixtures.LoadResources();
        var context = new SearchContext(20);
        Board board = Board.FromString(Fixtures.Rows("search-midgame-level1.txt").First()[1]);
        var searching = new SearchingFlag();
        var watch = Stopwatch.StartNew();
        using var stop = new Timer(_ => searching.Value = false, null, 100, Timeout.Infinite);
        SearchResult result = Ai.SearchTimePerMove(context, board, 10_000, book: null, 0, random: null, searching);
        watch.Stop();
        Assert.True(watch.ElapsedMilliseconds < 400, $"stopped after {watch.ElapsedMilliseconds} ms");
        Assert.True((board.GetLegal() & (1UL << result.Policy)) != 0, $"{Ai.PolicyName(result.Policy)} is not legal");
    }

    [Fact]
    public void TimePerGame_SplitsTheClock()
    {
        Board start = Board.Initial;
        ulong early = Ai.CalcTimeLimitPly(start, 60_000);
        Assert.InRange(early, 500UL, 10_000UL);
        Board late = Board.FromString("XXXXXXXXOOOOOOOOXXXXXXXXOOOOOOOOXXXXXXXXOOOOOOOOXXXXXXX---------  X".Replace(" ", "", StringComparison.Ordinal)[..64] + " X");
        Assert.True(Ai.CalcTimeLimitPly(late, 60_000) > early, "the solvable endgame gets a larger share");
    }
}
