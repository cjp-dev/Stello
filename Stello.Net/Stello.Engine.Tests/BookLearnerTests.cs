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

        BookNode reply = Assert.Single(book.Root);
        Assert.Equal(Square.Parse("c5").ToLegacy(), reply.Move);
        Assert.Equal(-Win, reply.Value);
        BookNode answer = Assert.Single(reply.Children);
        Assert.Equal(Square.Parse("f6").ToLegacy(), answer.Move);
        Assert.Equal(Win, answer.Value);
        Assert.Equal(2, book.NodeCount);
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
        Assert.Equal(-Win, book.Root[0].Value);
    }

    [Fact]
    public void AddGame_DrawGivesZeroValues()
    {
        OpeningBook book = OpeningBook.CreateEmpty();

        Learner(book).AddGame(Moves("d3 c5 f6"), GameResult.Draw);

        Assert.Equal(0, book.Root[0].Value);
        Assert.Equal(0, book.Root[0].Children[0].Value);
    }

    [Fact]
    public void AddGame_StoresPassesAsMoveZero()
    {
        Game game = TestGames.Play(TestGames.BlackMustPass);
        game.Pass();
        game.Play(Square.InMask(game.Board.LegalMoves(Player.White)).First());
        OpeningBook book = OpeningBook.CreateEmpty();

        Learner(book).AddGame(game.PlayedMoves.ToList(), GameResult.WhiteWins);

        Assert.Equal(game.Ply - 1, book.NodeCount);
        BookNode node = book.Root[0];
        while (node.Move != 0)
        {
            node = Assert.Single(node.Children);
        }

        Assert.Equal(0, node.Move);
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
        OpeningBook book = OpeningBook.Load(Path.Combine(AppContext.BaseDirectory, "Data", "OPENING"));
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
        Assert.Equal(2, book.Root.Count);
        Assert.Equal(2, book.Root[0].Children.Count);
        Assert.All(Leaves(book.Root), leaf => Assert.True(leaf.Flag.HasFlag(BookFlags.Calculated)));
        Assert.NotEqual(Win, book.Root[0].Children[0].Value);

        learner.EvaluatePositions();

        Assert.Equal(3, learner.PositionsEvaluated);
        Assert.Equal(4, book.NodeCount);
    }

    [Fact]
    public void EvaluatePositions_RemovesIllegalMoves()
    {
        byte[] data = Bytes([(Square.Parse("a1").ToLegacy(), 0), (Square.Parse("c5").ToLegacy(), 0)]);
        OpeningBook book = OpeningBook.Load(new MemoryStream(data));

        Learner(book).EvaluatePositions();

        Assert.DoesNotContain(book.Root, n => n.Move == Square.Parse("a1").ToLegacy());
    }

    [Fact]
    public void Minimax_BacksUpValuesAndSortsBestFirst()
    {
        OpeningBook book = OpeningBook.CreateEmpty();
        BookLearner learner = Learner(book);
        learner.AddGame(Moves("d3 c5 f6"), GameResult.BlackWins);
        learner.AddGame(Moves("d3 e3"), GameResult.WhiteWins);
        BookNode c5 = book.Root.Single(n => n.Move == Square.Parse("c5").ToLegacy());
        BookNode e3 = book.Root.Single(n => n.Move == Square.Parse("e3").ToLegacy());
        c5.Children[0].Value = 100;
        e3.Value = 20;

        learner.Minimax();

        Assert.Equal(-100, c5.Value);
        Assert.Equal([e3, c5], book.Root);
        Assert.True(book.TryGetMove(TestGames.Play("d3").Board, Player.White, new Random(0), out BookMove move));
        Assert.Equal(Square.Parse("e3"), move.Square);
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
    public void Save_KeepsLearnedNodesAndFlags()
    {
        OpeningBook book = OpeningBook.CreateEmpty();
        BookLearner learner = Learner(book);
        learner.AddGame(Moves("d3 c5 f6"), GameResult.BlackWins);
        learner.EvaluatePositions();
        using var stream = new MemoryStream();

        book.Save(stream);
        stream.Position = 0;
        OpeningBook loaded = OpeningBook.Load(stream);

        Assert.Equal(book.NodeCount, loaded.NodeCount);
        Assert.Equal(Leaves(book.Root).Select(n => n.Flag), Leaves(loaded.Root).Select(n => n.Flag));
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

    private static List<Move> Moves(string text) => GameRecordFormat.Parse(text).PlayedMoves.ToList();

    private static IEnumerable<BookNode> Leaves(List<BookNode> replies) =>
        replies.SelectMany(n => n.Children.Count == 0 ? [n] : Leaves(n.Children));

    private static byte[] Bytes((int Move, short Value)[] chain)
    {
        using var stream = new MemoryStream();
        using (var writer = new BinaryWriter(stream))
        {
            writer.Write(chain.Length);
            writer.Write((short)chain.Length);
            foreach ((int move, short value) in chain)
            {
                writer.Write((short)move);
                writer.Write(value);
                writer.Write((short)0);
                writer.Write((short)0);
            }
        }

        return stream.ToArray();
    }
}
