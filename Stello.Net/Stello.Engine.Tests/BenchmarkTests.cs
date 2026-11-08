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
    public void EngineMatch_WarmsBothPlayersBeforeRecordingGames()
    {
        List<string> starts = BookMatch.StartsAtPly(BookTestData.Master, 8).Take(1).ToList();
        var playerA = new RecordingMatchPlayer(hashBits: 10);
        var playerB = new RecordingMatchPlayer(hashBits: 10);

        _ = new EngineMatch(RecordedSide("A", playerA), RecordedSide("B", playerB), workers: 1)
            .Play(starts, null, CancellationToken.None);

        (Board board, Player toMove) = BookTextFormat.Replay(starts[0]);
        Assert.Equal((board, toMove), playerA.Positions[0]);
        Assert.Equal((board, toMove), playerB.Positions[0]);
        Assert.Equal(1, playerA.Limits[0].Depth);
        Assert.Equal(1, playerB.Limits[0].Depth);
        Assert.True(playerA.Limits.Skip(1).All(limits => limits.Depth == 3));
        Assert.True(playerB.Limits.Skip(1).All(limits => limits.Depth == 3));
        Assert.Equal([1, 2, 3], playerA.ResetCountsAtMoves.Distinct());
        Assert.Equal([1, 2, 3], playerB.ResetCountsAtMoves.Distinct());
    }

    [Theory]
    [InlineData("d3c3b3b2f5a3a1c1", 0)]
    [InlineData("d3e3f5c3d2f6f7d6b4g7c6f8f3e2f2e1h8c2f4h7d1g2c1b2h6c5c4b6f1g4" +
        "c7a4h3g5a6b5d7b3a1c8e7b1a3e8b7b8a8d8g8g1a7a5g3e6h5h1h2g6", 1)]
    public void EngineMatch_WarmsPlayablePositionWhenStartHasFewerThanTwoMoves(string start, int legalMoves)
    {
        (Board board, Player toMove) = BookTextFormat.Replay(start);
        Assert.False(board.IsGameOver);
        Assert.Equal(legalMoves, System.Numerics.BitOperations.PopCount(board.LegalMoves(toMove)));
        var playerA = new RecordingMatchPlayer(hashBits: 10);
        var playerB = new RecordingMatchPlayer(hashBits: 10);

        IReadOnlyList<MatchPair> pairs = new EngineMatch(RecordedSide("A", playerA), RecordedSide("B", playerB), workers: 1)
            .Play([start], null, CancellationToken.None);

        Assert.Equal((Board.Initial, Player.Black), playerA.Positions[0]);
        Assert.Equal((Board.Initial, Player.Black), playerB.Positions[0]);
        Assert.NotEmpty(pairs[0].AFirst.Moves);
        Assert.NotEmpty(pairs[0].BFirst.Moves);
    }

    [Fact]
    public void EngineMatch_DefersWarmUpPastFinishedStarts()
    {
        string finished = TestGames.WhiteWipedOut.Replace(" ", "");
        Assert.True(BookTextFormat.Replay(finished).Board.IsGameOver);
        var playerA = new RecordingMatchPlayer(hashBits: 10);
        var playerB = new RecordingMatchPlayer(hashBits: 10);
        var calls = new List<(int Start, int A, int B)>();

        _ = new EngineMatch(RecordedSide("A", playerA), RecordedSide("B", playerB), workers: 1)
            .Play([finished, "d3c3b3b2f5a3a1c1"], null, CancellationToken.None,
                gamePlayed: (index, _, _) => calls.Add((index, playerA.Limits.Count, playerB.Limits.Count)));

        Assert.Equal(4, calls.Count);
        Assert.Equal((0, 0, 0), calls[0]);
        Assert.Equal((0, 0, 0), calls[1]);
        Assert.Equal(1, calls[2].Start);
        Assert.True(calls[2].A > 0 && calls[2].B > 0);
        Assert.Equal((Board.Initial, Player.Black), playerA.Positions[0]);
        Assert.Equal((Board.Initial, Player.Black), playerB.Positions[0]);
    }

    [Theory]
    [InlineData(TimeControlMode.FixedDepth)]
    [InlineData(TimeControlMode.TimePerMove)]
    [InlineData(TimeControlMode.TimePerGame)]
    [InlineData(TimeControlMode.Solve)]
    public void EngineMatch_WarmUpLimitsPreserveTimedPathsAndDisableTheBook(TimeControlMode mode)
    {
        SearchLimits limits = (mode switch
        {
            TimeControlMode.FixedDepth => SearchLimits.FixedDepth(3, useOpeningBook: true, hashBits: 12),
            TimeControlMode.TimePerMove => SearchLimits.TimePerMove(TimeSpan.FromMilliseconds(500), useOpeningBook: true, hashBits: 12),
            TimeControlMode.TimePerGame => SearchLimits.TimePerGame(TimeSpan.FromMilliseconds(500), useOpeningBook: true, hashBits: 12),
            TimeControlMode.Solve => SearchLimits.Solve,
            _ => throw new ArgumentOutOfRangeException(nameof(mode)),
        }) with { Threads = 2 };

        SearchLimits warmup = EngineMatch.WarmUpLimits(limits);

        bool timed = mode is TimeControlMode.TimePerMove or TimeControlMode.TimePerGame;
        Assert.Equal(timed ? mode : TimeControlMode.FixedDepth, warmup.Mode);
        Assert.Equal(timed ? TimeSpan.FromMilliseconds(100) : TimeSpan.Zero, warmup.Time);
        Assert.Equal(timed ? 0 : 1, warmup.Depth);
        Assert.False(warmup.UseOpeningBook);
        Assert.Equal(limits.HashBits, warmup.HashBits);
        Assert.Equal(2, warmup.Threads);
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

    private static MatchSide RecordedSide(string name, RecordingMatchPlayer player) =>
        new(name, SearchLimits.FixedDepth(3, useOpeningBook: false), HashBits: 10, CreatePlayer: () => player);

    private sealed class RecordingMatchPlayer(int hashBits) : IMatchPlayer
    {
        private readonly StelloMatchPlayer _inner = new(hashBits, null);
        private int _resetCount;

        public List<SearchLimits> Limits { get; } = [];

        public List<(Board Board, Player Player)> Positions { get; } = [];

        public List<int> ResetCountsAtMoves { get; } = [];

        public void NewGame()
        {
            _resetCount++;
            _inner.NewGame();
        }

        public SearchResult ChooseMove(Board board, Player player, SearchLimits limits, CancellationToken cancellationToken)
        {
            Limits.Add(limits);
            Positions.Add((board, player));
            ResetCountsAtMoves.Add(_resetCount);
            return _inner.ChooseMove(board, player, limits, cancellationToken);
        }
    }
}
