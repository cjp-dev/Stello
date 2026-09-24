namespace Stello.Engine.Tests;

public class BoardTests
{
    private static ulong Mask(params string[] squares) =>
        squares.Aggregate(0UL, (mask, s) => mask | Square.Parse(s).Bit);

    private static string[] Names(ulong mask) =>
        Square.InMask(mask).Select(s => s.ToString()).ToArray();

    [Fact]
    public void Initial_HasFourDiscsInTheCentre()
    {
        Board board = Board.Initial;

        Assert.Equal(Player.White, board[Square.Parse("d4")]);
        Assert.Equal(Player.Black, board[Square.Parse("e4")]);
        Assert.Equal(Player.Black, board[Square.Parse("d5")]);
        Assert.Equal(Player.White, board[Square.Parse("e5")]);
        Assert.Null(board[Square.Parse("a1")]);
        Assert.Equal(2, board.Count(Player.Black));
        Assert.Equal(2, board.Count(Player.White));
        Assert.Equal(60, board.EmptyCount);
    }

    [Fact]
    public void Initial_LegalMoves()
    {
        Assert.Equal(["d3", "c4", "f5", "e6"], Names(Board.Initial.LegalMoves(Player.Black)));
        Assert.Equal(["e3", "f4", "c5", "d6"], Names(Board.Initial.LegalMoves(Player.White)));
    }

    [Fact]
    public void Play_FlipsAndReturnsNewBoard()
    {
        Board before = Board.Initial;

        Board after = before.Play(Player.Black, Square.Parse("f5"));

        Assert.Equal(4, after.Count(Player.Black));
        Assert.Equal(1, after.Count(Player.White));
        Assert.Equal(Player.Black, after[Square.Parse("e5")]);
        Assert.Equal(Board.Initial, before);
    }

    [Fact]
    public void Play_FlipsInAllEightDirections()
    {
        var board = new Board(
            Mask("f4", "b4", "d6", "d2", "f6", "b6", "f2", "b2"),
            Mask("e4", "c4", "d5", "d3", "e5", "c5", "e3", "c3"));
        Square move = Square.Parse("d4");

        Assert.Equal(board.White, board.Flips(Player.Black, move));

        Board after = board.Play(Player.Black, move);

        Assert.Equal(17, after.Count(Player.Black));
        Assert.Equal(0, after.Count(Player.White));
    }

    [Fact]
    public void Play_FlipsLongestPossibleLine()
    {
        var board = new Board(Mask("a1"), Mask("b1", "c1", "d1", "e1", "f1", "g1"));
        Square move = Square.Parse("h1");

        Assert.True(board.IsLegal(Player.Black, move));
        Assert.Equal(board.White, board.Flips(Player.Black, move));
    }

    [Fact]
    public void Flips_OnlyFlipsClosedLines()
    {
        // The line from c1 is closed by the black disc on a1; the diagonal from e4 runs off the board after b1.
        var board = new Board(Mask("a1"), Mask("b1", "c2", "d3"));

        Assert.Equal(["b1"], Names(board.Flips(Player.Black, Square.Parse("c1"))));
        Assert.Equal(0UL, board.Flips(Player.Black, Square.Parse("e4")));
    }

    [Theory]
    [InlineData("h1", "a2", "b2")]
    [InlineData("a2", "h1", "g1")]
    [InlineData("h2", "a2", "b1")]
    [InlineData("a1", "h1", "g2")]
    public void LegalMoves_DoNotWrapAroundTheEdge(string black, string white, string move)
    {
        var board = new Board(Mask(black), Mask(white));

        Assert.False(board.IsLegal(Player.Black, Square.Parse(move)));
        Assert.Equal(0UL, board.Flips(Player.Black, Square.Parse(move)));
        Assert.Equal(0UL, board.LegalMoves(Player.Black));
    }

    [Fact]
    public void Play_IllegalMoveThrows()
    {
        Assert.Throws<InvalidOperationException>(() => Board.Initial.Play(Player.Black, Square.Parse("d4")));
        Assert.Throws<InvalidOperationException>(() => Board.Initial.Play(Player.Black, Square.Parse("a1")));
    }

    [Fact]
    public void Pass_WhenOnlyOpponentCanMove()
    {
        var board = new Board(Mask("b1"), Mask("a1"));

        Assert.False(board.HasLegalMove(Player.Black));
        Assert.Equal(["c1"], Names(board.LegalMoves(Player.White)));
        Assert.False(board.IsGameOver);
    }

    [Fact]
    public void GameOver_WhenBoardIsFull()
    {
        Board board = Board.Parse(new string('X', 32) + new string('O', 32));

        Assert.Equal(0, board.EmptyCount);
        Assert.True(board.IsGameOver);
    }

    [Fact]
    public void GameOver_WhenOnePlayerIsWipedOut()
    {
        var board = new Board(Mask("d4", "e4", "d5"), 0);

        Assert.True(board.IsGameOver);
        Assert.Equal(0, board.Count(Player.White));
    }

    [Fact]
    public void Constructor_RejectsOverlappingDiscs()
    {
        Assert.Throws<ArgumentException>(() => new Board(Mask("a1"), Mask("a1")));
    }

    [Fact]
    public void Parse_ReadsSquaresInIndexOrder()
    {
        Board board = Board.Parse("XO" + new string('-', 61) + "x");

        Assert.Equal(Mask("a1", "h8"), board.Black);
        Assert.Equal(Mask("b1"), board.White);
    }

    [Fact]
    public void ToString_RoundTripsThroughParse()
    {
        Board board = Board.Initial.Play(Player.Black, Square.Parse("f5"));

        Assert.Equal(board, Board.Parse(board.ToString()));
    }

    [Theory]
    [InlineData("")]
    [InlineData("XO")]
    public void Parse_RejectsWrongLength(string text)
    {
        Assert.Throws<FormatException>(() => Board.Parse(text));
        Assert.Throws<FormatException>(() => Board.Parse(new string('-', 64) + text + "-"));
    }

    [Fact]
    public void Parse_RejectsInvalidCharacter()
    {
        Assert.Throws<FormatException>(() => Board.Parse(new string('-', 63) + "?"));
    }
}
