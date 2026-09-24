using System.Diagnostics;

namespace Stello.Engine.Tests;

public class SearchEngineTests
{
    private static readonly Board Midgame = TestGames.Play("f5 d6 c3 d3 c4 f4 c5 b3 c2 e6").Board;

    [Fact]
    public void Search_WithoutLegalMoveReturnsPass()
    {
        Board board = TestGames.Play(TestGames.BlackMustPass).Board;

        SearchResult result = new SearchEngine().Search(board, Player.Black, SearchLimits.FixedDepth(4));

        Assert.True(result.Move.IsPass);
        Assert.Equal(ScoreKind.None, result.Kind);
    }

    [Fact]
    public void Search_WithOneLegalMoveReturnsItAtOnce()
    {
        var board = new Board(Square.Parse("a1").Bit, Square.Parse("b1").Bit);

        SearchResult result = new SearchEngine().Search(board, Player.Black, SearchLimits.FixedDepth(10));

        Assert.Equal(new Move(Square.Parse("c1")), result.Move);
        Assert.Equal(ScoreKind.None, result.Kind);
        Assert.Equal(0, result.Nodes);
    }

    [Theory]
    [InlineData(1)]
    [InlineData(4)]
    [InlineData(7)]
    public void FixedDepth_SearchesToTheRequestedDepth(int depth)
    {
        SearchResult result = new SearchEngine().Search(Midgame, Player.Black, SearchLimits.FixedDepth(depth));

        Assert.Equal(depth, result.Depth);
        Assert.Equal(ScoreKind.Heuristic, result.Kind);
        Assert.True(Midgame.IsLegal(Player.Black, result.Move.Square!.Value));
    }

    [Fact]
    public void FixedDepth_IsDeterministic()
    {
        SearchResult first = new SearchEngine().Search(Midgame, Player.Black, SearchLimits.FixedDepth(8));
        SearchResult second = new SearchEngine().Search(Midgame, Player.Black, SearchLimits.FixedDepth(8));

        Assert.Equal(first.Move, second.Move);
        Assert.Equal(first.Score, second.Score);
        Assert.Equal(first.Nodes, second.Nodes);
    }

    [Fact]
    public void Search_ReportsProgressForEveryDepth()
    {
        var depths = new List<int>();
        var progress = new SyncProgress<SearchInfo>(info => depths.Add(info.Depth));

        new SearchEngine().Search(Midgame, Player.Black, SearchLimits.FixedDepth(5), progress);

        Assert.Equal([1, 2, 3, 4, 5], depths.Distinct());
    }

    [Fact]
    public void TimePerMove_StopsInTime()
    {
        var stopwatch = Stopwatch.StartNew();

        SearchResult result = new SearchEngine().Search(Midgame, Player.Black, SearchLimits.TimePerMove(TimeSpan.FromMilliseconds(300)));

        Assert.InRange(stopwatch.ElapsedMilliseconds, 0, 400);
        Assert.True(result.Depth >= 1);
        Assert.True(Midgame.IsLegal(Player.Black, result.Move.Square!.Value));
    }

    [Fact]
    public void TimePerGame_UsesPartOfTheRemainingTime()
    {
        var stopwatch = Stopwatch.StartNew();

        SearchResult result = new SearchEngine().Search(Midgame, Player.Black, SearchLimits.TimePerGame(TimeSpan.FromSeconds(10)));

        Assert.InRange(stopwatch.ElapsedMilliseconds, 0, 2000);
        Assert.True(result.Depth >= 1);
    }

    [Fact]
    public async Task MoveNow_ReturnsBestMoveQuickly()
    {
        using var moveNow = new CancellationTokenSource();
        var engine = new SearchEngine();
        Task<SearchResult> search = Task.Run(() =>
            engine.Search(Midgame, Player.Black, SearchLimits.FixedDepth(60), moveNowToken: moveNow.Token));

        await Task.Delay(300);
        Assert.False(search.IsCompleted);
        var stopwatch = Stopwatch.StartNew();
        await moveNow.CancelAsync();
        SearchResult result = await search;

        Assert.InRange(stopwatch.ElapsedMilliseconds, 0, 100);
        Assert.True(Midgame.IsLegal(Player.Black, result.Move.Square!.Value));
        Assert.Equal(ScoreKind.Heuristic, result.Kind);
    }

    [Fact]
    public async Task Cancel_StopsTheSearchQuickly()
    {
        using var cancel = new CancellationTokenSource();
        var engine = new SearchEngine();
        Task<SearchResult> search = Task.Run(() =>
            engine.Search(Midgame, Player.Black, SearchLimits.FixedDepth(60), cancellationToken: cancel.Token));

        await Task.Delay(300);
        var stopwatch = Stopwatch.StartNew();
        await cancel.CancelAsync();

        await Assert.ThrowsAnyAsync<OperationCanceledException>(() => search);
        Assert.InRange(stopwatch.ElapsedMilliseconds, 0, 100);
    }

    [Fact]
    public void Search_AlreadyCancelledThrows()
    {
        Assert.Throws<OperationCanceledException>(() =>
            new SearchEngine().Search(Midgame, Player.Black, SearchLimits.FixedDepth(4), cancellationToken: new CancellationToken(true)));
    }
}
