namespace Stello.Engine.Tests;

public class BookRecalculatorTests
{
    [Fact]
    public void Run_SearchesTheLeavesThatAreNotGoodEnough()
    {
        OpeningBook book = Book("d3 c5 f6 f5", "d3 c3 c4 c5", "d3 e3 f4 c3");
        BookTestData.Entry(book, "d3 c3 c4", "c5").Set(100, BookOrigin.Exact);
        BookTestData.Entry(book, "d3 e3 f4", "c3").Set(50, BookOrigin.Heuristic, new BookEffort(EffortKind.Depth, 2, 2, OpeningBook.EngineVersion));
        var recalculator = new BookRecalculator(book, SearchLimits.FixedDepth(2), workers: 1, hashBits: 10);

        Assert.Equal(3, recalculator.LeafPositions);
        Assert.Equal(1, recalculator.ToSearch);
        recalculator.Run(null, null, 0, CancellationToken.None);

        BookEntry f5 = BookTestData.Entry(book, "d3 c5 f6", "f5");
        Assert.Equal(BookOrigin.Heuristic, f5.Origin);
        Assert.Equal(new BookEffort(EffortKind.Depth, 2, 2, OpeningBook.EngineVersion), f5.Effort);
        Assert.Equal(100, BookTestData.Entry(book, "d3 c3 c4", "c5").Value);
        Assert.Equal(50, BookTestData.Entry(book, "d3 e3 f4", "c3").Value);

        // A deeper search makes both searched values not good enough; the exact value stays.
        Assert.Equal(2, new BookRecalculator(book, SearchLimits.FixedDepth(3), workers: 1, hashBits: 10).ToSearch);
    }

    [Fact]
    public void Run_SearchesATransposedLeafOnce()
    {
        (string first, string second, _) = BookTestData.Transposition();
        OpeningBook book = Book(first, second);
        var recalculator = new BookRecalculator(book, SearchLimits.FixedDepth(2), workers: 1, hashBits: 10);
        int searches = 0;

        recalculator.Run(_ => searches++, null, 0, CancellationToken.None);

        Assert.Equal(1, recalculator.LeafPositions);
        Assert.Equal(1, searches);
        BookEntry firstLeaf = LastEntry(book, first);
        BookEntry secondLeaf = LastEntry(book, second);
        Assert.Equal(BookOrigin.Heuristic, firstLeaf.Origin);
        Assert.Equal(firstLeaf.Value, secondLeaf.Value);
        Assert.Equal(firstLeaf.Effort, secondLeaf.Effort);
    }

    [Fact]
    public void Run_KeepsAProvenResultThatTheSearchCannotSolve()
    {
        OpeningBook book = Book("d3 c5 f6 f5");
        BookEntry f5 = BookTestData.Entry(book, "d3 c5 f6", "f5");
        f5.Set(32610, BookOrigin.WinLossDraw);

        new BookRecalculator(book, SearchLimits.FixedDepth(1), workers: 1, hashBits: 10).Run(null, null, 0, CancellationToken.None);

        Assert.Equal(32610, f5.Value);
        Assert.Equal(BookOrigin.WinLossDraw, f5.Origin);
        Assert.Equal(EffortKind.Depth, f5.Effort.Kind);
    }

    [Fact]
    public void Run_GivesTheSameBookWithAnyNumberOfWorkers()
    {
        string text = BookTestData.WriteText(RandomBook());
        OpeningBook one = BookTestData.ReadText(text);
        OpeningBook four = BookTestData.ReadText(text);

        new BookRecalculator(one, SearchLimits.FixedDepth(3), workers: 1, hashBits: 10).Run(null, null, 0, CancellationToken.None);
        new BookRecalculator(four, SearchLimits.FixedDepth(3), workers: 4, hashBits: 10).Run(null, null, 0, CancellationToken.None);

        Assert.Equal(BookTestData.WriteText(one), BookTestData.WriteText(four));
        BookMinimax.Run(one);
        Assert.True(BookAnalysis.Consistency(one).IsConsistent);
    }

    [Fact]
    public void Run_StoppedRunContinuesWhereItStopped()
    {
        OpeningBook book = RandomBook();
        SearchLimits limits = SearchLimits.FixedDepth(1);
        int total = new BookRecalculator(book, limits, workers: 1, hashBits: 10).ToSearch;
        using var cancel = new CancellationTokenSource();
        int checkpoints = 0;

        Assert.Throws<OperationCanceledException>(() => new BookRecalculator(book, limits, workers: 1, hashBits: 10).Run(
            progress =>
            {
                if (progress.Done == 5)
                {
                    cancel.Cancel();
                }
            },
            () => checkpoints++,
            saveEvery: 2,
            cancel.Token));

        Assert.Equal(2, checkpoints);
        var rest = new BookRecalculator(book, limits, workers: 1, hashBits: 10);
        Assert.Equal(total - 5, rest.ToSearch);
        rest.Run(null, null, 0, CancellationToken.None);
        Assert.Equal(0, new BookRecalculator(book, limits, workers: 1, hashBits: 10).ToSearch);
    }

    [Fact]
    public void Comparison_ReportsValuesAndFirstMovesThatChanged()
    {
        OpeningBook oldBook = BookTestData.ReadText(BookTextFormat.FirstLine + "\nd3 c5:-39:U c3:-40:U\n");
        OpeningBook newBook = BookTestData.ReadText(BookTextFormat.FirstLine + "\nd3 c3:-30:H c5:-39:U\n");

        BookComparison comparison = BookComparison.Of(oldBook, newBook);

        Assert.Equal(1, comparison.ValuesChanged);
        Assert.Equal((1, 1), comparison.Origins[(BookOrigin.Unknown, BookOrigin.Heuristic)]);
        Assert.Equal((1, 0), comparison.Origins[(BookOrigin.Unknown, BookOrigin.Unknown)]);
        BestMoveChange change = Assert.Single(comparison.BestMoveChanges);
        Assert.Equal(new BestMoveChange("d3", new Move(Square.Parse("c5")), -39, new Move(Square.Parse("c3")), -30), change);

        using var report = new StringWriter();
        comparison.Write(report, "Test");
        Assert.Contains("\nd3 c5:-39 c3:-30\n", report.ToString());
        Assert.Equal(["d3"], BookMatch.ReadStarts(new StringReader(report.ToString())));
    }

    [Fact]
    public void Match_TheSameBookScoresHalfWithTheColoursSwapped()
    {
        List<string> starts = BookMatch.StartsAtPly(BookTestData.Master, 8).Take(3).ToList();
        var match = new BookMatch(BookTestData.Master, BookTestData.Master, SearchLimits.FixedDepth(1), workers: 2, hashBits: 10);

        IReadOnlyList<MatchPair> pairs = match.Play(starts, null, CancellationToken.None);

        Assert.Equal(starts, pairs.Select(pair => pair.Start));
        foreach (MatchPair pair in pairs)
        {
            Assert.Equal(1, pair.PointsB);
            Assert.NotEqual(pair.BookAFirst.BookB, pair.BookBFirst.BookB);
            AssertFinishedGame(pair.Start, pair.BookAFirst.Moves);
            AssertFinishedGame(pair.Start, pair.BookBFirst.Moves);
        }

        Assert.Equal(0.5, MatchSummary.Of(pairs).Score);
    }

    [Fact]
    public void Match_RejectsAnIllegalStartPosition()
    {
        var match = new BookMatch(BookTestData.Master, BookTestData.Master, SearchLimits.FixedDepth(1), workers: 1, hashBits: 10);

        Assert.Throws<FormatException>(() => match.Play(["d3a1"], null, CancellationToken.None));
    }

    [Fact]
    public void MatchSummary_GivesTheScoreAndItsInterval()
    {
        MatchPair won = Pair(10, 6);
        MatchPair lost = Pair(-4, -2);

        MatchSummary summary = MatchSummary.Of([won, lost, won, lost]);

        Assert.Equal(4, summary.PointsB);
        Assert.Equal(0.5, summary.Score);
        Assert.True(summary.Low < 0.5 && summary.High > 0.5);
        Assert.Equal(2.5, summary.AverageDiscsB);
    }

    [Fact]
    public void ReadStarts_TakesTheFirstWordOfEveryOtherLine()
    {
        string text = "# comment\n\nd3c5 f6:39 e6:40\n  # indented comment\nd3c3\n";

        Assert.Equal(["d3c5", "d3c3"], BookMatch.ReadStarts(new StringReader(text)));
    }

    private static OpeningBook Book(params string[] games)
    {
        OpeningBook book = OpeningBook.CreateEmpty();
        var learner = new BookLearner(book, new SearchEngine(hashBits: 10), SearchLimits.FixedDepth(1), new Random(0));
        foreach (string game in games)
        {
            learner.AddGame(BookTestData.Moves(game), GameResult.BlackWins);
        }

        return book;
    }

    // 30 random games of 16 plies.
    private static OpeningBook RandomBook()
    {
        var random = new Random(3);
        OpeningBook book = OpeningBook.CreateEmpty();
        var learner = new BookLearner(book, new SearchEngine(hashBits: 10), SearchLimits.FixedDepth(1), new Random(0));
        for (int i = 0; i < 30; i++)
        {
            var game = new Game();
            while (game.Ply < 16 && !game.IsGameOver)
            {
                if (game.MustPass)
                {
                    game.Pass();
                    continue;
                }

                Square[] moves = Square.InMask(game.Board.LegalMoves(game.ToMove)).ToArray();
                game.Play(moves[random.Next(moves.Length)]);
            }

            learner.AddGame(game.PlayedMoves.ToList(), GameResult.Draw);
        }

        return book;
    }

    private static BookEntry LastEntry(OpeningBook book, string game)
    {
        string[] moves = game.Split(' ');
        return BookTestData.Entry(book, string.Join(' ', moves[..^1]), moves[^1]);
    }

    private static void AssertFinishedGame(string start, IReadOnlyList<Move> moves)
    {
        (Board board, Player player) = BookTextFormat.Replay(start);
        foreach (Move move in moves)
        {
            Assert.True(OpeningBook.IsLegal(board, player, move));
            (board, player) = OpeningBook.Play(board, player, move);
        }

        Assert.True(board.IsGameOver);
    }

    private static MatchPair Pair(int firstDiscs, int secondDiscs) =>
        new("d3", new MatchGame(Player.Black, [], firstDiscs), new MatchGame(Player.White, [], secondDiscs));
}
