namespace Stello.Engine.Tests;

public class BookTrimmerTests
{
    private const string Header = BookTextFormat.FirstLine + "\n";

    [Fact]
    public void Trim_PrunesPositionsUnreachableWithinHeroAndOppLoss_WhilePreservingSeveredValues()
    {
        // At d3 (White to move): c5 (+100, best), c3 (-50, diff 150), e3 (-500, diff 600).
        // With heroLoss=50, oppLoss=300:
        //   - When Stello plays White, White only takes c5 (diff 0 <= 50), not c3 (150 > 50) or e3 (600 > 50).
        //   - When Stello plays Black, White (opponent) can take c5 or c3 (150 <= 300), NOT e3 (600 > 300).
        //   - So d3e3 is pruned and d3's e3:-500:B converts to a leaf keeping -500 and v5 provenance.
        // Inside d3c5 (Black to move): f6 (-100, best), d6 (-150, diff 50 <= 50).
        // Inside d3c5d6 (White to move): e3 (+150, best), c3 (-687, diff 837 > 300): neither player reaches d3c5d6c3!
        OpeningBook book = BookTestData.ReadText(Header + """
            d3 c5:100:B c3:-50:B e3:-500:B
            d3c3 c4:50:B e6:-706:H:60s:d32:v5
            d3c5 f6:-100:B d6:-150:B
            d3e3 f4:500:H:60s:d32:v5
            d3c3c4 c5:-50:H:60s:d30:v5
            d3c5d6 e3:150:B c3:-687:B
            d3c5f6 f5:100:X:60s:d27:v5
            d3c5d6c3 c4:687:W:60s:d30:v5
            d3c5d6e3 f4:-150:H:60s:d31:v5
            """);

        BookTrimResult result = BookTrimmer.Trim(book, heroLoss: 50, oppLoss: 300, maxPly: 10);
        OpeningBook trimmed = result.Book;

        // Kept positions: d3, d3c3, d3c5, d3c3c4, d3c5d6, d3c5f6, d3c5d6e3 (7 kept; d3e3 and d3c5d6c3 removed).
        Assert.Equal(9, result.PositionsBefore);
        Assert.Equal(7, result.PositionsAfter);
        Assert.Equal(2, result.SeveredEdges);

        Assert.Empty(BookTestData.Replies(trimmed, "d3 e3"));
        Assert.Empty(BookTestData.Replies(trimmed, "d3 c5 d6 c3"));
        Assert.NotEmpty(BookTestData.Replies(trimmed, "d3 c5 d6 e3"));
        Assert.NotEmpty(BookTestData.Replies(trimmed, "d3 c3 c4"));

        // Severed BackedUp edges become leaves with their exact Value and underlying Origin/Effort.
        BookEntry e3 = BookTestData.Entry(trimmed, "d3", "e3");
        Assert.Equal(-500, e3.Value);
        Assert.Equal(BookOrigin.Heuristic, e3.Origin);
        Assert.Equal(new BookEffort(EffortKind.Time, 60_000, 32, OpeningBook.EngineVersion), e3.Effort);

        BookEntry c3 = BookTestData.Entry(trimmed, "d3 c5 d6", "c3");
        Assert.Equal(-687, c3.Value);
        Assert.Equal(BookOrigin.WinLossDraw, c3.Origin);
        Assert.Equal(new BookEffort(EffortKind.Time, 60_000, 30, OpeningBook.EngineVersion), c3.Effort);

        // Minimax values across every remaining position and root d3 are completely unchanged.
        string beforeMinimax = BookTestData.WriteText(trimmed);
        BookMinimax.Run(trimmed);
        Assert.Equal(beforeMinimax, BookTestData.WriteText(trimmed));
        Assert.Equal(100, BookTestData.Entry(trimmed, "d3", "c5").Value);
        Assert.Equal(-50, BookTestData.Entry(trimmed, "d3", "c3").Value);
        Assert.True(BookAnalysis.Consistency(trimmed).IsConsistent);
    }

    [Fact]
    public void Trim_PrunesPositionsBeyondMaxPly_AndPreservesProvenDrawAndExactProvenance()
    {
        // Root d3 is ply 1, d3c5 is ply 2, d3c5f6 is ply 3, d3c5f6f5 is ply 4.
        OpeningBook book = BookTestData.ReadText(Header + """
            d3 c5:0:B
            d3c5 f6:0:B
            d3c5f6 f5:0:B
            d3c5f6f5 e6:0:W:60s:d30:v5
            """);

        BookTrimResult result = BookTrimmer.Trim(book, heroLoss: 100, oppLoss: 300, maxPly: 2);
        OpeningBook trimmed = result.Book;

        // Kept: ply 1 (d3) and ply 2 (d3c5). Pruned: ply 3 (d3c5f6) and ply 4 (d3c5f6f5).
        Assert.Equal(2, trimmed.PositionCount);
        Assert.Equal(1, result.SeveredEdges);

        BookEntry f6 = BookTestData.Entry(trimmed, "d3 c5", "f6");
        Assert.Equal(0, f6.Value);
        Assert.Equal(BookOrigin.WinLossDraw, f6.Origin);
        Assert.Equal(new BookEffort(EffortKind.Time, 60_000, 30, OpeningBook.EngineVersion), f6.Effort);

        BookMinimax.Run(trimmed);
        Assert.Equal(0, BookTestData.Entry(trimmed, "d3", "c5").Value);
        Assert.True(BookAnalysis.Consistency(trimmed).IsConsistent);
    }

    [Fact]
    public void IsAllowed_NormalizesSolvedAndHeuristicScores_WithoutCrossingProvenBoundaries()
    {
        // Crossing -WinScore (heuristic best vs proven WLD loss): hero always rejects; opponent adds ProvenBoundaryPenalty.
        Assert.False(BookTrimmer.IsAllowed(bestRaw: -500, candidateRaw: -32601, maxLoss: 0, isHeroTurn: true));
        Assert.False(BookTrimmer.IsAllowed(bestRaw: -500, candidateRaw: -32601, maxLoss: 300, isHeroTurn: false));
        Assert.True(BookTrimmer.IsAllowed(bestRaw: -500, candidateRaw: -32601, maxLoss: null, isHeroTurn: false));
        Assert.False(BookTrimmer.IsAllowed(bestRaw: 50, candidateRaw: -32602, maxLoss: 300, isHeroTurn: false));
        Assert.False(BookTrimmer.IsAllowed(bestRaw: 50, candidateRaw: -32602, maxLoss: 100, isHeroTurn: true));

        // Exact +8 win (+32608 -> +800) vs Exact +4 win (+32604 -> +400): diff is 400 -> rejected at 100, allowed at 400.
        Assert.False(BookTrimmer.IsAllowed(bestRaw: 32608, candidateRaw: 32604, maxLoss: 100, isHeroTurn: true));
        Assert.True(BookTrimmer.IsAllowed(bestRaw: 32608, candidateRaw: 32604, maxLoss: 400, isHeroTurn: true));

        // Crossing +WinScore (proven win vs heuristic): hero rejects; opponent with finite oppLoss cannot bypass via 0 clamp.
        Assert.False(BookTrimmer.IsAllowed(bestRaw: 32602, candidateRaw: 431, maxLoss: 100, isHeroTurn: true));
        Assert.False(BookTrimmer.IsAllowed(bestRaw: 32601, candidateRaw: 500, maxLoss: 300, isHeroTurn: false));
        Assert.True(BookTrimmer.IsAllowed(bestRaw: 32601, candidateRaw: 500, maxLoss: 1000, isHeroTurn: false));
    }

    [Fact]
    public void Trim_MasterBook_ProducesZeroConsistencyWarnings_AndRoundTripsThroughBinaryAndText()
    {
        BookTrimResult result = BookTrimmer.Trim(BookTestData.Master, heroLoss: 100, oppLoss: 300, maxPly: 30);
        OpeningBook trimmed = result.Book;

        Assert.True(result.PositionsAfter < result.PositionsBefore);
        Assert.True(BookAnalysis.Statistics(trimmed).LongestLine <= 31);

        // Zero minimax value changes vs original root and ancestors; consistency & text/binary round-trip pass clean.
        Assert.True(BookAnalysis.Consistency(trimmed).IsConsistent);
        Assert.Equal(BookTestData.Entry(BookTestData.Master, "d3", "c5").Value, BookTestData.Entry(trimmed, "d3", "c5").Value);

        string text = BookTestData.WriteText(trimmed);
        OpeningBook fromText = BookTestData.ReadText(text);
        OpeningBook fromBinary = OpeningBook.Load(new MemoryStream(BookTestData.WriteBinary(trimmed)));
        Assert.Equal(text, BookTestData.WriteText(fromText));
        Assert.Equal(text, BookTestData.WriteText(fromBinary));
    }
}
