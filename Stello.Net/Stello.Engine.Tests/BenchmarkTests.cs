using Stello.Engine.Benchmark;

namespace Stello.Engine.Tests;

public class BenchmarkTests
{
    private static string DataFile(string name) => Path.Combine(AppContext.BaseDirectory, "Data", "Benchmark", name);

    [Fact]
    public void Parse_ReadsThePositionAndTheMovesWithTheBestScore()
    {
        BenchmarkPosition position = BenchmarkPosition.Parse(
            "43 --XXXXX---XXXX---OOOXX---OOXXXX--OOXXXO-OOOOXOO----XOX----XXXXX- O; C7:-12; G3:-12; B8:-14; G4:-14");

        Assert.Equal("43", position.Id);
        Assert.Equal(Player.White, position.Player);
        Assert.Equal(23, position.Board.EmptyCount);
        Assert.Equal(-12, position.Score);
        Assert.Equal(["c7", "g3"], position.BestMoves.Select(m => m.ToString()));
    }

    [Fact]
    public void Parse_WithoutScores_HasNoScore()
    {
        var board = Board.Initial;
        string line = new BenchmarkPosition("start", board, Player.Black, null, []).Format();

        BenchmarkPosition position = BenchmarkPosition.Parse(line);

        Assert.Equal(board, position.Board);
        Assert.Null(position.Score);
        Assert.Empty(position.BestMoves);
    }

    [Theory]
    [InlineData("40 too-short X")]
    [InlineData("40 O--OOOOX-OOOOOOXOOXXOOOXOOXOOOXXOOOOOOXX---OOOOX----O--X-------- Z")]
    [InlineData("40 O--OOOOX-OOOOOOXOOXXOOOXOOXOOOXXOOOOOOXX---OOOOX----O--X-------- X; A2=38")]
    public void Parse_RejectsBadLines(string line)
    {
        Assert.Throws<FormatException>(() => BenchmarkPosition.Parse(line));
    }

    [Fact]
    public void FfoFile_HoldsPositions40To59WithTheirScores()
    {
        List<BenchmarkPosition> positions = BenchmarkPosition.Load(DataFile("ffo-40-59.txt"));

        Assert.Equal(Enumerable.Range(40, 20).Select(i => i.ToString()), positions.Select(p => p.Id));
        Assert.All(positions, p =>
        {
            Assert.NotNull(p.Score);
            Assert.NotEmpty(p.BestMoves);
            Assert.All(p.BestMoves, move => Assert.True(p.Board.IsLegal(p.Player, move)));
        });

        // The five positions that the endgame tests solve.
        Assert.Equal([38, 0, 6, -12, -14], positions.Take(5).Select(p => p.Score!.Value));
    }

    [Fact]
    public void MidgameFile_HoldsPositionsWithAtLeastTwoMoves()
    {
        List<BenchmarkPosition> positions = BenchmarkPosition.Load(DataFile("midgame.txt"));

        Assert.Equal(100, positions.Count);
        Assert.Equal(positions.Count, positions.Select(p => p.Id).Distinct().Count());
        Assert.All(positions, p => Assert.True(System.Numerics.BitOperations.PopCount(p.Board.LegalMoves(p.Player)) >= 2, p.Id));
    }

    [Fact]
    public void StartFile_HoldsDistinctLegalLines()
    {
        using StreamReader reader = File.OpenText(DataFile("starts.txt"));
        List<string> starts = BookMatch.ReadStarts(reader);

        Assert.True(starts.Count >= 500);
        Assert.Equal(starts.Count, starts.Distinct().Count());
        Assert.All(starts, start =>
        {
            Assert.InRange(start.Length, 20, 24);
            (Board board, Player player) = BookTextFormat.Replay(start);
            Assert.True(board.HasLegalMove(player));
        });
    }

    [Fact]
    public void EngineMatch_PlaysPairsAndRecordsWhatEachSideDid()
    {
        List<string> starts = BookMatch.StartsAtPly(BookTestData.Master, 8).Take(2).ToList();
        var shallow = new MatchSide("depth 1", SearchLimits.FixedDepth(1, useOpeningBook: false), HashBits: 10);
        var deeper = new MatchSide("depth 3", SearchLimits.FixedDepth(3, useOpeningBook: false), HashBits: 10);

        IReadOnlyList<MatchPair> pairs = new EngineMatch(shallow, deeper, workers: 2).Play(starts, null, CancellationToken.None);

        Assert.Equal(starts, pairs.Select(pair => pair.Start));
        foreach (MatchGame game in pairs.SelectMany(pair => new[] { pair.AFirst, pair.BFirst }))
        {
            Assert.True(game.StatsA.Searched > 0 && game.StatsB.Searched > 0);
            Assert.True(game.StatsB.Nodes > game.StatsA.Nodes);
            Assert.Equal(1, game.StatsA.MidgameDepth / game.StatsA.MidgameMoves);
            Assert.Equal(3, game.StatsB.MidgameDepth / game.StatsB.MidgameMoves);
            Assert.True(game.StatsB.FirstSolve >= game.StatsA.FirstSolve);
        }

        // A side that only passes or has one move near the end never needs to solve, so not every game has a solve.
        Assert.Contains(pairs, pair => pair.AFirst.StatsA.FirstSolve > 0 || pair.BFirst.StatsA.FirstSolve > 0);
        Assert.NotEqual(pairs[0].AFirst.ColourB, pairs[0].BFirst.ColourB);
    }

    [Fact]
    public void EngineMatch_WithAGameClock_StaysWithinTheTimeForTheGame()
    {
        List<string> starts = BookMatch.StartsAtPly(BookTestData.Master, 8).Take(1).ToList();
        var clock = new MatchSide("1 s", SearchLimits.TimePerGame(TimeSpan.FromSeconds(1), useOpeningBook: false), HashBits: 12);
        var depth = new MatchSide("depth 2", SearchLimits.FixedDepth(2, useOpeningBook: false), HashBits: 12);

        IReadOnlyList<MatchPair> pairs = new EngineMatch(clock, depth, workers: 1).Play(starts, null, CancellationToken.None);

        foreach (MatchGame game in new[] { pairs[0].AFirst, pairs[0].BFirst })
        {
            Assert.True(game.StatsA.Searched > 5);
            Assert.InRange(game.StatsA.Time.TotalSeconds, 0.01, 1.5);
        }
    }

    [Fact]
    public void SideStats_SkipsMovesThatNeedNoSearch()
    {
        var book = new SearchResult(Move.Pass, 10, ScoreKind.Book, 0, 0, 0, TimeSpan.Zero);
        var single = new SearchResult(Move.Pass, 0, ScoreKind.None, 0, 0, 0, TimeSpan.Zero);
        var midgame = new SearchResult(Move.Pass, 5, ScoreKind.Heuristic, 8, 1000, 900, TimeSpan.FromMilliseconds(3));
        var solved = new SearchResult(Move.Pass, 2, ScoreKind.Exact, 18, 5000, 4000, TimeSpan.FromMilliseconds(7));

        SideStats stats = default(SideStats)
            .Add(book, 50, TimeSpan.FromMilliseconds(1)).Add(single, 40, TimeSpan.FromMilliseconds(1))
            .Add(midgame, 30, midgame.Elapsed).Add(solved, 18, solved.Elapsed).Add(solved, 16, TimeSpan.FromMilliseconds(8));

        Assert.Equal(new SideStats(3, 11_000, TimeSpan.FromMilliseconds(18), 1, 8, 18, TimeSpan.FromMilliseconds(8)), stats);
    }
}
