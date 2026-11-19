namespace Stello.Engine.Tests;

public class BookExpanderTests
{
    private const string Header = BookTextFormat.FirstLine + "\n";

    [Fact]
    public void Run_ExpandsLowestLinckePriorityLeafFirst_WithLazyDeviations()
    {
        // At d3 (ply 1): c5 (+100, loss 0 -> child priority = 50 + 50 + 0 = 100) vs e3 (-500, loss 600 -> child prio = 700).
        // First step MUST expand c5 (creating d3c5 with 1 move, queued as pending lazy deviation at prio 100).
        // Next step visits d3c5 at prio 100 (< d3c5->child prio 150 and < d3e3 prio 700), runs its lazy onlyMoves
        // deviation search, adds a 2nd move to d3c5, and clears pending deviations!
        OpeningBook book = BookTestData.ReadText(Header + "d3 c5:100:H:2ply:d2:v5 e3:-500:H:2ply:d2:v5\n");
        var steps = new List<ExpandProgress>();

        var expander = new BookExpander(
            book,
            SearchLimits.FixedDepth(2),
            omega: 50,
            maxPly: 6,
            maxPositions: 10,
            maxIterations: 2,
            workers: 1,
            hashBits: 10);

        expander.Run(p => steps.Add(p), null, 0, CancellationToken.None);

        Assert.Equal(2, steps.Count);
        Assert.Equal(ExpandStepKind.Leaf, steps[0].Kind);
        Assert.Equal("d3c5", steps[0].Line);
        Assert.Equal(ExpandStepKind.Deviation, steps[1].Kind);
        Assert.Equal("d3c5", steps[1].Line);

        Assert.Equal(2, book.PositionCount);
        Assert.True(BookTestData.Replies(book, "d3 c5").Count >= 2);
        Assert.Empty(expander.PendingDeviations);
        Assert.True(BookAnalysis.Consistency(book).IsConsistent);
    }

    [Fact]
    public void Run_RespectsMaxPlyAndMaxPositions_AndSupportsParallelWorkers()
    {
        OpeningBook book = BookTestData.ReadText(Header + "d3 c5:100:H:2ply:d2:v5 c3:-50:H:2ply:d2:v5 e3:-500:H:2ply:d2:v5\n");

        var expander = new BookExpander(
            book,
            SearchLimits.FixedDepth(2),
            omega: 50,
            maxPly: 4,
            maxPositions: 7,
            workers: 4,
            hashBits: 10);

        expander.Run(null, null, 0, CancellationToken.None);

        Assert.Equal(7, book.PositionCount);
        // No newly created position exceeds maxPly = 4 (longest line <= maxPly + 1 including leaf move).
        Assert.True(BookAnalysis.Statistics(book).LongestLine <= 5);
        Assert.True(BookAnalysis.Consistency(book).IsConsistent);

        // Text -> binary -> text round-trip stays clean and consistent.
        string text = BookTestData.WriteText(book);
        OpeningBook reloaded = BookTestData.ReadText(text);
        Assert.Equal(text, BookTestData.WriteText(reloaded));
    }

    [Fact]
    public void Run_SkipsLazyDeviationOnLeavesCreatedAtMaxPlyBoundary()
    {
        // With maxPly = 2, expanding d3 (ply 1) creates d3c5 at ply 2; since ply 2 >= maxPly,
        // d3c5 cannot expand to ply 3 and must not leak into PendingDeviations or trigger lazy deviation searches.
        OpeningBook book = BookTestData.ReadText(Header + "d3 c5:100:H:2ply:d2:v5 e3:-500:H:2ply:d2:v5\n");
        var steps = new List<ExpandProgress>();

        var expander = new BookExpander(
            book,
            SearchLimits.FixedDepth(2),
            omega: 50,
            maxPly: 2,
            maxPositions: 10,
            workers: 1,
            hashBits: 10);

        expander.Run(p => steps.Add(p), null, 0, CancellationToken.None);

        Assert.Equal(2, steps.Count);
        Assert.All(steps, s => Assert.Equal(ExpandStepKind.Leaf, s.Kind));
        Assert.Equal(3, book.PositionCount);
        Assert.Single(BookTestData.Replies(book, "d3 c5"));
        Assert.Empty(expander.PendingDeviations);
    }

    [Fact]
    public void EdgeLoss_AppliesProvenBoundaryPenaltyAcrossBothWinAndLossBoundaries()
    {
        // Proven loss vs unproven heuristic (-500 vs -32601) must incur ProvenBoundaryPenalty.
        Assert.Equal(BookExpander.ProvenBoundaryPenalty, BookExpander.EdgeLoss(bestRaw: -500, candidateRaw: -32601));

        // Proven win vs unproven heuristic (+32601 vs 0 -> 100 + ProvenBoundaryPenalty).
        Assert.Equal(100 + BookExpander.ProvenBoundaryPenalty, BookExpander.EdgeLoss(bestRaw: 32601, candidateRaw: 0));
        Assert.Equal(BookExpander.ProvenBoundaryPenalty, BookExpander.EdgeLoss(bestRaw: 32601, candidateRaw: 500));

        // Same-side comparisons do not add ProvenBoundaryPenalty.
        Assert.Equal(150, BookExpander.EdgeLoss(bestRaw: 100, candidateRaw: -50));
        Assert.Equal(200, BookExpander.EdgeLoss(bestRaw: 32604, candidateRaw: 32602));
    }

    [Fact]
    public void Run_DrainsPendingDeviationsOnceMaxPositionsReached_WithoutExceedingMaxPositions()
    {
        // Starting with 1 position (d3) and maxPositions=2 (maxPly=6): expanding d3c5 hits maxPositions=2 and queues
        // d3c5 as a pending deviation. BookExpander must stop creating new leaf positions while draining d3c5's deviation.
        OpeningBook book = BookTestData.ReadText(Header + "d3 c5:100:H:2ply:d2:v5 e3:-500:H:2ply:d2:v5\n");
        var steps = new List<ExpandProgress>();

        var expander = new BookExpander(
            book,
            SearchLimits.FixedDepth(2),
            omega: 50,
            maxPly: 6,
            maxPositions: 2,
            workers: 2,
            hashBits: 10);

        expander.Run(p => steps.Add(p), null, 0, CancellationToken.None);

        Assert.Equal(2, book.PositionCount);
        Assert.Empty(expander.PendingDeviations);
        Assert.Equal(2, steps.Count);
        Assert.Contains(steps, s => s.Kind == ExpandStepKind.Leaf && s.Line == "d3c5");
        Assert.Contains(steps, s => s.Kind == ExpandStepKind.Deviation && s.Line == "d3c5");
        Assert.True(BookTestData.Replies(book, "d3 c5").Count >= 2);
        Assert.True(BookAnalysis.Consistency(book).IsConsistent);
    }

    [Fact]
    public void Run_SignalsStopOnWorkerException_HaltingRemainingWorkersQuickly()
    {
        OpeningBook book = BookTestData.ReadText(Header + "d3 c5:100:H:2ply:d2:v5 c3:-50:H:2ply:d2:v5 e3:-500:H:2ply:d2:v5\n");
        var expander = new BookExpander(
            book,
            SearchLimits.FixedDepth(2),
            omega: 50,
            maxPly: 8,
            maxPositions: 30,
            workers: 4,
            hashBits: 10);

        int checkpointCalls = 0;
        Exception exception = Assert.ThrowsAny<Exception>(() =>
            expander.Run(
                null,
                () =>
                {
                    if (Interlocked.Increment(ref checkpointCalls) == 1)
                    {
                        throw new IOException("Simulated checkpoint failure");
                    }
                },
                saveEvery: 1,
                CancellationToken.None));

        Assert.True(
            exception is IOException
            || (exception is AggregateException aggregate && aggregate.InnerExceptions.Any(e => e is IOException)));
        // At most the initial in-flight batch of 4 workers completes; remaining 25+ positions must not run.
        Assert.True(book.PositionCount <= 6, $"Expected workers to halt quickly, but book grew to {book.PositionCount} positions.");
    }

    [Fact]
    public void Run_PassLeafReportsProgressValueWithSameSignAsStoredPassEntry()
    {
        Game game = TestGames.Play(TestGames.BlackMustPass);
        List<Move> moves = game.PlayedMoves.ToList();
        OpeningBook book = OpeningBook.CreateEmpty();
        var learner = new BookLearner(book, new SearchEngine(hashBits: 10), SearchLimits.FixedDepth(1), new Random(0));
        learner.AddGame(moves, GameResult.Draw);

        var steps = new List<ExpandProgress>();
        var expander = new BookExpander(
            book,
            SearchLimits.FixedDepth(2),
            omega: 50,
            maxPly: moves.Count + 2,
            maxPositions: book.PositionCount + 1,
            workers: 1,
            hashBits: 10);

        expander.Run(p => steps.Add(p), null, 0, CancellationToken.None);

        ExpandProgress passStep = Assert.Single(steps);
        Assert.Equal(ExpandStepKind.Leaf, passStep.Kind);
        Assert.Null(passStep.Move);

        BookEntry passEntry = Assert.Single(BookTestData.Replies(book, game.Board, Player.Black)).Entry;
        Assert.True(passEntry.Move.IsPass);
        Assert.Equal(passEntry.Value, passStep.Value);
    }
}
