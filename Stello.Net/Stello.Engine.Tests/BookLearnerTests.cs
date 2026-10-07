namespace Stello.Engine.Tests;

public class BookLearnerTests
{
    private const short Win = 32665;
    private static readonly SearchLimits Quick = SearchLimits.FixedDepth(1);

    [Fact]
    public void AddGame_StoresTheLineNormalisedToD3()
    {
        OpeningBook book = OpeningBook.CreateEmpty();

        Learner(book).AddGame(Moves("f5 d6 c3"), GameResult.BlackWins);

        (Move reply, BookEntry replyEntry) = Assert.Single(BookTestData.Replies(book, "d3"));
        Assert.Equal("c5", reply.ToString());
        Assert.Equal(-Win, replyEntry.Value);
        Assert.Equal(BookOrigin.Unknown, replyEntry.Origin);
        (Move answer, BookEntry answerEntry) = Assert.Single(BookTestData.Replies(book, "d3 c5"));
        Assert.Equal("f6", answer.ToString());
        Assert.Equal(Win, answerEntry.Value);
        Assert.Equal(2, book.NodeCount);
        Assert.Equal(2, book.PositionCount);
    }

    [Theory]
    [InlineData("d3", "c5")]
    [InlineData("f5", "d6")]
    [InlineData("d3 c5", "f6")]
    [InlineData("f5 d6", "c3")]
    public void AddGame_TheBookThenSuggestsTheLine(string moves, string expected)
    {
        OpeningBook book = OpeningBook.CreateEmpty();
        Learner(book).AddGame(Moves("f5 d6 c3"), GameResult.BlackWins);
        Game game = GameRecordFormat.Parse(moves);

        Assert.True(book.TryGetMove(game.Board, game.ToMove, new Random(0), out BookMove move));
        Assert.Equal(Square.Parse(expected), move.Square);
    }

    [Fact]
    public void AddGame_KnownAndSymmetricLinesAddNoNodes()
    {
        OpeningBook book = OpeningBook.CreateEmpty();
        BookLearner learner = Learner(book);

        learner.AddGame(Moves("f5 d6 c3"), GameResult.BlackWins);
        learner.AddGame(Moves("f5 d6 c3"), GameResult.WhiteWins);
        learner.AddGame(Moves("d3 c5 f6"), GameResult.BlackWins);

        Assert.Equal(2, book.NodeCount);
        Assert.Equal(-Win, BookTestData.Replies(book, "d3")[0].Entry.Value);
    }

    [Fact]
    public void AddGame_TranspositionJoinsTheKnownPosition()
    {
        (string first, string second, string next) = Transposition();
        OpeningBook book = OpeningBook.CreateEmpty();
        BookLearner learner = Learner(book);

        learner.AddGame(Moves($"{first} {next}"), GameResult.BlackWins);
        BookEntry entry = Assert.Single(BookTestData.Replies(book, first)).Entry;
        short value = entry.Value;
        learner.AddGame(Moves($"{second} {next}"), GameResult.WhiteWins);

        Assert.Same(entry, Assert.Single(BookTestData.Replies(book, second)).Entry);
        Assert.Equal(value, entry.Value);
    }

    [Fact]
    public void AddGame_DrawGivesZeroValues()
    {
        OpeningBook book = OpeningBook.CreateEmpty();

        Learner(book).AddGame(Moves("d3 c5 f6"), GameResult.Draw);

        Assert.Equal(0, BookTestData.Replies(book, "d3")[0].Entry.Value);
        Assert.Equal(0, BookTestData.Replies(book, "d3 c5")[0].Entry.Value);
    }

    [Fact]
    public void AddGame_StoresPasses()
    {
        Game game = TestGames.Play(TestGames.BlackMustPass);
        game.Pass();
        game.Play(Square.InMask(game.Board.LegalMoves(Player.White)).First());
        OpeningBook book = OpeningBook.CreateEmpty();

        Learner(book).AddGame(game.PlayedMoves.ToList(), GameResult.WhiteWins);

        Assert.Equal(game.Ply - 1, book.NodeCount);
        Board beforePass = TestGames.Play(TestGames.BlackMustPass).Board;
        Assert.True(Assert.Single(BookTestData.Replies(book, beforePass, Player.Black)).Move.IsPass);
    }

    [Fact]
    public void AddGame_IllegalMoveThrows()
    {
        OpeningBook book = OpeningBook.CreateEmpty();

        Assert.Throws<ArgumentException>(() => Learner(book).AddGame([new Move(Square.Parse("d3")), new Move(Square.Parse("a1"))], GameResult.Draw));
    }

    [Fact]
    public void AddGame_KnownLineInTheMasterBookAddsNoNodes()
    {
        OpeningBook book = OpeningBook.Load(BookTestData.BinaryPath);
        int nodes = book.NodeCount;

        Learner(book).AddGame(Moves("f5 d6"), GameResult.BlackWins);

        Assert.Equal(nodes, book.NodeCount);
    }

    [Fact]
    public void EvaluatePositions_SearchesLeavesAndAddsTheBestMoveNotInTheBook()
    {
        OpeningBook book = OpeningBook.CreateEmpty();
        BookLearner learner = Learner(book);
        learner.AddGame(Moves("d3 c5 f6"), GameResult.BlackWins);

        learner.EvaluatePositions();

        // f6 is searched, then the best black move except f6 and the best white move except c5 are added.
        Assert.Equal(3, learner.PositionsEvaluated);
        Assert.Equal(4, book.NodeCount);
        Assert.Equal(2, BookTestData.Replies(book, "d3").Count);
        List<(Move Move, BookEntry Entry)> afterC5 = BookTestData.Replies(book, "d3 c5");
        Assert.Equal(2, afterC5.Count);
        Assert.All(afterC5, reply => Assert.True(reply.Entry.IsSearched));
        Assert.All(afterC5, reply => Assert.Equal(new BookEffort(EffortKind.Depth, 1, 1, OpeningBook.EngineVersion), reply.Entry.Effort));
        Assert.Equal(BookOrigin.BackedUp, BookTestData.Replies(book, "d3")[0].Entry.Origin);
        Assert.NotEqual(Win, afterC5[0].Entry.Value);

        learner.EvaluatePositions();

        Assert.Equal(3, learner.PositionsEvaluated);
        Assert.Equal(4, book.NodeCount);
    }

    [Fact]
    public void EvaluatePositions_SearchesATransposedPositionOnce()
    {
        (string first, string second, string next) = Transposition();
        OpeningBook book = OpeningBook.CreateEmpty();
        BookLearner learner = Learner(book);
        learner.AddGame(Moves($"{first} {next}"), GameResult.BlackWins);
        learner.EvaluatePositions();
        int searched = learner.PositionsEvaluated;

        learner.AddGame(Moves($"{second} {next}"), GameResult.BlackWins);
        learner.EvaluatePositions();

        // Only the positions on the new move order before the shared position are new.
        int newPositions = Moves(second).Count - 1 - Moves(first).Zip(Moves(second)).TakeWhile(p => p.First == p.Second).Count();
        Assert.Equal(searched + newPositions, learner.PositionsEvaluated);
    }

    [Fact]
    public void Minimax_BacksUpValuesAndSortsBestFirst()
    {
        OpeningBook book = OpeningBook.CreateEmpty();
        BookLearner learner = Learner(book);
        learner.AddGame(Moves("d3 c5 f6"), GameResult.BlackWins);
        learner.AddGame(Moves("d3 e3"), GameResult.WhiteWins);
        BookEntry c5 = Entry(book, "d3", "c5");
        BookEntry e3 = Entry(book, "d3", "e3");
        Entry(book, "d3 c5", "f6").Set(100, BookOrigin.Heuristic);
        e3.Set(20, BookOrigin.Heuristic);

        learner.Minimax();

        Assert.Equal(-100, c5.Value);
        Assert.Equal(BookOrigin.BackedUp, c5.Origin);
        Assert.Equal([e3, c5], BookTestData.Replies(book, "d3").Select(r => r.Entry));
        Assert.True(book.TryGetMove(TestGames.Play("d3").Board, Player.White, new Random(0), out BookMove move));
        Assert.Equal(Square.Parse("e3"), move.Square);
    }

    [Fact]
    public void Minimax_BacksUpThroughATranspositionToBothMoveOrders()
    {
        (string first, string second, string next) = Transposition();
        OpeningBook book = OpeningBook.CreateEmpty();
        BookLearner learner = Learner(book);
        learner.AddGame(Moves($"{first} {next}"), GameResult.BlackWins);
        learner.AddGame(Moves($"{second} {next}"), GameResult.BlackWins);
        Entry(book, first, next).Set(50, BookOrigin.Heuristic);

        learner.Minimax();

        string[] firstMoves = first.Split(' ');
        string[] secondMoves = second.Split(' ');
        Assert.Equal(-50, Entry(book, string.Join(' ', firstMoves[..^1]), firstMoves[^1]).Value);
        Assert.Equal(-50, Entry(book, string.Join(' ', secondMoves[..^1]), secondMoves[^1]).Value);
    }

    [Fact]
    public void Learning_WorkedExampleOfChapter12()
    {
        OpeningBook book = OpeningBook.CreateEmpty();
        var learner = new BookLearner(book, new SearchEngine(hashBits: 12), SearchLimits.FixedDepth(4), new Random(0));
        learner.AddGame(Moves("d3 c5 f6 f5 e6"), GameResult.BlackWins);
        Assert.Equal(4, book.NodeCount);

        learner.EvaluatePositions();

        Assert.Equal(5, learner.PositionsEvaluated);
        Assert.Equal(8, book.NodeCount);
        Assert.Equal(-100, Entry(book, "d3", "c5").Value);
        Assert.Equal(17, Entry(book, "d3", "e3").Value);

        learner.Minimax();

        Assert.Equal(["e3", "c5"], BookTestData.Replies(book, "d3").Select(r => r.Move.ToString()));
        Assert.Equal([("e6", 100), ("f6", -54)], BookTestData.Replies(book, "d3 c5").Select(r => (r.Move.ToString(), (int)r.Entry.Value)));

        learner.AddGame(Moves("f5 d6 c3 d3 c4"), GameResult.BlackWins);
        Assert.Equal(8, book.NodeCount);
    }

    [Fact]
    public void PlayGame_PlaysALegalGameToTheEnd()
    {
        OpeningBook book = OpeningBook.CreateEmpty();

        (IReadOnlyList<Move> moves, GameResult result) = Learner(book).PlayGame();

        var game = new Game();
        foreach (Move move in moves)
        {
            game.Play(move);
        }

        Assert.True(moves.Count > 40);
        Assert.True(Enum.IsDefined(result));
    }

    [Fact]
    public void SelfPlay_RunsUntilCancelled()
    {
        OpeningBook book = OpeningBook.CreateEmpty();
        using var cancel = new CancellationTokenSource();
        int checkpoints = 0;
        var learner = new BookLearner(book, new SearchEngine(hashBits: 12), Quick, new Random(1), _ => checkpoints++);

        Assert.ThrowsAny<OperationCanceledException>(() =>
            learner.SelfPlay(null, cancel.Token, (_, _) => cancel.Cancel()));

        Assert.Equal(1, learner.GamesPlayed);
        Assert.True(book.NodeCount > 40);
        Assert.True(checkpoints >= 2);
    }

    [Fact]
    public void Save_KeepsLearnedMovesWithOriginAndEffort()
    {
        OpeningBook book = OpeningBook.CreateEmpty();
        BookLearner learner = Learner(book);
        learner.AddGame(Moves("d3 c5 f6"), GameResult.BlackWins);
        learner.EvaluatePositions();

        OpeningBook loaded = OpeningBook.Load(new MemoryStream(BookTestData.WriteBinary(book)));

        Assert.Equal(book.NodeCount, loaded.NodeCount);
        Assert.Equal(BookTestData.WriteText(book), BookTestData.WriteText(loaded));
    }

    [Fact]
    public void Search_OnlyMovesSearchesEvenASingleMove()
    {
        Square only = Square.Parse("d3");

        SearchResult result = new SearchEngine(hashBits: 12).Search(Board.Initial, Player.Black, Quick, onlyMoves: only.Bit);

        Assert.Equal(new Move(only), result.Move);
        Assert.Equal(ScoreKind.Heuristic, result.Kind);
    }

    [Fact]
    public void Search_OnlyMovesMustContainALegalMove()
    {
        Assert.Throws<ArgumentException>(() =>
            new SearchEngine(hashBits: 12).Search(Board.Initial, Player.Black, Quick, onlyMoves: Square.Parse("a1").Bit));
    }

    private static BookLearner Learner(OpeningBook book) => new(book, new SearchEngine(hashBits: 12), Quick, new Random(0));

    private static List<Move> Moves(string text) => BookTestData.Moves(text);

    private static BookEntry Entry(OpeningBook book, string moves, string move) => BookTestData.Entry(book, moves, move);

    private static (string First, string Second, string Next) Transposition() => BookTestData.Transposition();
}
