namespace Stello.Engine.Tests;

public class BookCalclibTests
{
    private const string Header = BookTextFormat.FirstLine + "\n";

    [Fact]
    public void Run_AddsStrongMissingMovesWithinThreshold_AndRejectsWeakOnes()
    {
        // At d3 (White to move): legal moves are c5, c3, e3.
        // Start with only e3 artificially scored low (-5000); calclib at depth=4 must discover both c5 and c3
        // (and reject a huge threshold failure when threshold=0 against a better move).
        OpeningBook book = BookTestData.ReadText(Header + "d3 e3:-5000:H:4ply:d4:v5\n");

        var calclib = new BookCalclib(
            book,
            SearchLimits.FixedDepth(4),
            threshold: 250,
            maxPly: 10,
            maxAdds: 3,
            workers: 1,
            hashBits: 10);

        Assert.Equal(1, calclib.CandidatePositions);
        Assert.Equal(1, calclib.ToSearch);

        calclib.Run(null, null, 0, CancellationToken.None);
        BookMinimax.Run(book);

        List<(Move Move, BookEntry Entry)> replies = BookTestData.Replies(book, "d3");
        Assert.True(replies.Count >= 2);
        Assert.Contains(replies, r => r.Move.ToString() == "c5");
        Assert.True(replies[0].Entry.Value > -5000);
        Assert.Equal(replies[0].Entry.Effort, new BookEffort(EffortKind.Depth, 4, 4, OpeningBook.EngineVersion));
        Assert.True(BookAnalysis.Consistency(book).IsConsistent);

        // With threshold: 0 on a position that already holds the depth-4 best move (c3 = -312 vs c5 = -325),
        // nothing weaker should be added.
        OpeningBook narrow = BookTestData.ReadText(Header + $"d3 c3:{BookTestData.Entry(book, "d3", "c3").Value}:H:4ply:d4:v5\n");
        new BookCalclib(narrow, SearchLimits.FixedDepth(4), threshold: 0, maxPly: 10, maxAdds: 3, workers: 1, hashBits: 10)
            .Run(null, null, 0, CancellationToken.None);
        Assert.Single(BookTestData.Replies(narrow, "d3"));
    }

    [Fact]
    public void Run_IsDeterministicAcrossWorkerCounts_AndResumesFromCompletedKeys()
    {
        string text = Header + """
            d3 c5:-2000:H:2ply:d2:v5
            d3c5 f6:-2000:H:2ply:d2:v5
            """;
        OpeningBook one = BookTestData.ReadText(text);
        OpeningBook four = BookTestData.ReadText(text);

        new BookCalclib(one, SearchLimits.FixedDepth(3), threshold: 300, maxPly: 10, maxAdds: 2, workers: 1, hashBits: 10)
            .Run(null, null, 0, CancellationToken.None);
        new BookCalclib(four, SearchLimits.FixedDepth(3), threshold: 300, maxPly: 10, maxAdds: 2, workers: 4, hashBits: 10)
            .Run(null, null, 0, CancellationToken.None);

        BookMinimax.Run(one);
        BookMinimax.Run(four);
        Assert.Equal(BookTestData.WriteText(one), BookTestData.WriteText(four));

        // Resume check: record keys on cancellation after 1 position, then finish with remaining 1.
        OpeningBook resumed = BookTestData.ReadText(text);
        var done = new HashSet<BookKey>();
        using var cancel = new CancellationTokenSource();
        int checkpoints = 0;

        Assert.Throws<OperationCanceledException>(() =>
            new BookCalclib(resumed, SearchLimits.FixedDepth(3), threshold: 300, maxPly: 10, maxAdds: 2, workers: 1, hashBits: 10)
                .Run(
                    p =>
                    {
                        done.Add(p.Key);
                        cancel.Cancel();
                    },
                    () => checkpoints++,
                    saveEvery: 1,
                    cancel.Token));

        Assert.Single(done);
        Assert.Equal(1, checkpoints);

        var rest = new BookCalclib(
            resumed, SearchLimits.FixedDepth(3), threshold: 300, maxPly: 10, maxAdds: 2, workers: 1, hashBits: 10, completedKeys: done);
        Assert.Equal(1, rest.ToSearch);
        rest.Run(null, null, 0, CancellationToken.None);
        BookMinimax.Run(resumed);

        Assert.Equal(BookTestData.WriteText(one), BookTestData.WriteText(resumed));
    }

    [Fact]
    public void IsWithinThreshold_NormalizesScoresAndRespectsProvenBoundaries()
    {
        Assert.True(BookCalclib.IsWithinThreshold(bestRaw: -200, candidateRaw: 50, threshold: 0));
        Assert.True(BookCalclib.IsWithinThreshold(bestRaw: 100, candidateRaw: 0, threshold: 100));
        Assert.False(BookCalclib.IsWithinThreshold(bestRaw: 100, candidateRaw: -1, threshold: 100));

        // Proven win vs heuristic: never add an unproven move.
        Assert.False(BookCalclib.IsWithinThreshold(bestRaw: 32602, candidateRaw: 450, threshold: 500));
        Assert.True(BookCalclib.IsWithinThreshold(bestRaw: 32604, candidateRaw: 32602, threshold: 200));

        // Unproven heuristic vs proven loss: never add a proven loss.
        Assert.False(BookCalclib.IsWithinThreshold(bestRaw: -500, candidateRaw: -32601, threshold: 0));
        Assert.False(BookCalclib.IsWithinThreshold(bestRaw: -500, candidateRaw: -32601, threshold: 500));
    }

    [Fact]
    public void Run_LinksExistingTranspositionsWithoutSearching_AndStopsMultiAddUnlessBestIsBeaten()
    {
        // Case 1: Build a valid DAG via BookTestData.Transposition() where `child` (reached via `first`) is
        // already in the book, while `parent2` (`second[..^1]`) lacks `second[^1] -> child`.
        // Calling calclib on `parent2` must link `second[^1]` as BackedUp (+120) without searching it.
        (string first, string second, string next) = BookTestData.Transposition();
        OpeningBook linkedBook = OpeningBook.CreateEmpty();
        var learner = new BookLearner(linkedBook, new SearchEngine(hashBits: 10), SearchLimits.FixedDepth(1), new Random(0));
        learner.AddGame(BookTestData.Moves(first + " " + next), GameResult.Draw);
        string[] secondMoves = second.Split(' ');
        string parent2Line = string.Join(' ', secondMoves[..^1]);
        string linkMoveName = secondMoves[^1];
        Game parent2Game = GameRecordFormat.Parse(parent2Line);
        Square otherSquare = Square.InMask(parent2Game.Board.LegalMoves(parent2Game.ToMove))
            .First(s => s.ToString() != linkMoveName);
        learner.AddGame(BookTestData.Moves(parent2Line + " " + otherSquare), GameResult.Draw);

        // Make `child -> next` worth -120 so linking `parent2 -> linkMove` yields +120, beating existing moves on `parent2`.
        BookTestData.Entry(linkedBook, first, next).Set(-120, BookOrigin.Heuristic, new BookEffort(EffortKind.Depth, 4, 4, OpeningBook.EngineVersion));
        BookMinimax.Run(linkedBook);

        (BookKey parent2Key, _) = OpeningBook.Canonical(parent2Game.Board, parent2Game.ToMove);
        BookTestData.Entry(linkedBook, parent2Line, otherSquare.ToString())
            .Set(-500, BookOrigin.Heuristic, new BookEffort(EffortKind.Depth, 4, 4, OpeningBook.EngineVersion));

        var linkedCalclib = new BookCalclib(
            linkedBook, SearchLimits.FixedDepth(4), threshold: 100, maxPly: 10, maxAdds: 3, workers: 1, hashBits: 10,
            completedKeys: AllKeysExcept(linkedBook, parent2Key));
        linkedCalclib.Run(null, null, 0, CancellationToken.None);
        BookMinimax.Run(linkedBook);

        BookEntry linkedEntry = BookTestData.Entry(linkedBook, parent2Line, linkMoveName);
        Assert.Equal(BookOrigin.BackedUp, linkedEntry.Origin);
        Assert.Equal(120, linkedEntry.Value);
        Assert.Equal(default, linkedEntry.Effort);

        // Case 2: d3 already has c3 (-312, best at depth 4) and e3 is omitted; c5 (-325) is within threshold 5000
        // of -312, so c5 is added on round 0, does NOT beat bestRaw (-312), and stops (leaving e3 unadded!).
        OpeningBook stopBook = BookTestData.ReadText(Header + "d3 c3:-312:H:4ply:d4:v5\n");
        new BookCalclib(stopBook, SearchLimits.FixedDepth(4), threshold: 5000, maxPly: 10, maxAdds: 3, workers: 1, hashBits: 10)
            .Run(null, null, 0, CancellationToken.None);

        List<(Move Move, BookEntry Entry)> stopReplies = BookTestData.Replies(stopBook, "d3");
        Assert.Equal(2, stopReplies.Count);
        Assert.Contains(stopReplies, r => r.Move.ToString() == "c5");
        Assert.DoesNotContain(stopReplies, r => r.Move.ToString() == "e3");
    }

    private static HashSet<BookKey> AllKeysExcept(OpeningBook book, BookKey keep)
    {
        var skip = new HashSet<BookKey>();
        foreach (BookLine line in BookTextFormat.Lines(book))
        {
            BookKey key = OpeningBook.Canonical(line.Board, line.Player).Key;
            if (key != keep)
            {
                skip.Add(key);
            }
        }

        return skip;
    }
}
