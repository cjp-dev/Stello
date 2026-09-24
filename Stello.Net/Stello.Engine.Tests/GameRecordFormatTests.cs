namespace Stello.Engine.Tests;

public class GameRecordFormatTests
{
    [Fact]
    public void Format_NewGameIsEmpty()
    {
        Assert.Equal("", GameRecordFormat.Format(new Game()));
    }

    [Fact]
    public void Format_WritesPlayedMovesOnly()
    {
        Game game = TestGames.Play("f5 d6 c3 d3");
        game.Undo();

        Assert.Equal("f5 d6 c3", GameRecordFormat.Format(game));
    }

    [Fact]
    public void FormatAndParse_RoundTripWithPass()
    {
        Game game = TestGames.Play(TestGames.BlackMustPass);
        game.Pass();
        game.Play(Square.InMask(game.Board.LegalMoves(Player.White)).First());

        string text = GameRecordFormat.Format(game);
        Game loaded = GameRecordFormat.Parse(text);

        Assert.Contains(" pass ", text);
        Assert.Equal(game.Moves, loaded.Moves);
        Assert.Equal(game.Board, loaded.Board);
        Assert.Equal(game.ToMove, loaded.ToMove);
    }

    [Fact]
    public void Parse_IgnoresCaseAndExtraWhitespace()
    {
        Game game = GameRecordFormat.Parse("  F5\r\n d6\t C3 \n");

        Assert.Equal("f5 d6 c3", GameRecordFormat.Format(game));
        Assert.Equal(3, game.Ply);
    }

    [Fact]
    public void Parse_SetsHumanPlayer()
    {
        Game game = GameRecordFormat.Parse("f5", Player.White);

        Assert.Equal(Player.White, game.Human);
        Assert.True(game.IsHumanToMove);
    }

    [Fact]
    public void Parse_FinishedGame()
    {
        Game game = GameRecordFormat.Parse(TestGames.WhiteWipedOut);

        Assert.True(game.IsGameOver);
        Assert.Equal(Player.Black, game.Winner);
    }

    [Theory]
    [InlineData("f5 z9", "Move 2: 'z9' is not a square")]
    [InlineData("a1", "Move 1: a1 is not a legal move for Black")]
    [InlineData("f5 a1", "Move 2: a1 is not a legal move for White")]
    [InlineData("pass", "Move 1: Black has a legal move and cannot pass")]
    [InlineData(TestGames.BlackMustPass + " a8", "Move 9: a8 is not a legal move for Black")]
    [InlineData(TestGames.WhiteWipedOut + " a1", "Move 10: the game is already over")]
    public void Parse_RejectsInvalidRecord(string text, string message)
    {
        var exception = Assert.Throws<FormatException>(() => GameRecordFormat.Parse(text));

        Assert.StartsWith(message, exception.Message);
    }
}
