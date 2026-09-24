namespace Stello.Engine.Tests;

public class GameTests
{
    private static readonly Square F5 = Square.Parse("f5");
    private static readonly Square D6 = Square.Parse("d6");
    private static readonly Square C3 = Square.Parse("c3");

    [Fact]
    public void NewGame_StartsFromInitialPositionWithBlackToMove()
    {
        var game = new Game();

        Assert.Equal(Board.Initial, game.Board);
        Assert.Equal(Player.Black, game.ToMove);
        Assert.Equal(Player.Black, game.Human);
        Assert.Equal(Player.White, game.Computer);
        Assert.True(game.IsHumanToMove);
        Assert.Equal(0, game.Ply);
        Assert.Null(game.LastMove);
        Assert.False(game.CanUndo);
        Assert.False(game.CanRedo);
        Assert.False(game.IsGameOver);
        Assert.False(game.MustPass);
        Assert.Null(game.Winner);
    }

    [Fact]
    public void Play_AlternatesPlayersAndUpdatesBoard()
    {
        var game = new Game();

        game.Play(F5);

        Assert.Equal(Board.Initial.Play(Player.Black, F5), game.Board);
        Assert.Equal(Player.White, game.ToMove);
        Assert.Equal(new Move(F5), game.LastMove);
        Assert.Equal(1, game.Ply);

        game.Play(D6);

        Assert.Equal(Player.Black, game.ToMove);
        Assert.Equal([new Move(F5), new Move(D6)], game.Moves);
    }

    [Fact]
    public void Play_IllegalMoveThrowsAndKeepsState()
    {
        var game = new Game();

        Assert.Throws<InvalidOperationException>(() => game.Play(Square.Parse("a1")));

        Assert.Equal(Board.Initial, game.Board);
        Assert.Equal(0, game.Ply);
        Assert.Empty(game.Moves);
    }

    [Fact]
    public void UndoAndRedo_MoveThroughHistory()
    {
        Game game = TestGames.Play("f5 d6 c3");
        Board afterD6 = TestGames.Play("f5 d6").Board;
        Board afterC3 = game.Board;

        game.Undo();

        Assert.Equal(afterD6, game.Board);
        Assert.Equal(Player.Black, game.ToMove);
        Assert.Equal(new Move(D6), game.LastMove);
        Assert.True(game.CanRedo);
        Assert.Equal(3, game.Moves.Count);
        Assert.Equal([new Move(F5), new Move(D6)], game.PlayedMoves);

        game.Undo();
        game.Undo();

        Assert.Equal(Board.Initial, game.Board);
        Assert.False(game.CanUndo);
        Assert.Throws<InvalidOperationException>(game.Undo);

        game.Redo();
        game.Redo();
        game.Redo();

        Assert.Equal(afterC3, game.Board);
        Assert.Equal(Player.White, game.ToMove);
        Assert.False(game.CanRedo);
        Assert.Throws<InvalidOperationException>(game.Redo);
    }

    [Fact]
    public void Play_AfterUndoDiscardsUndoneMoves()
    {
        Game game = TestGames.Play("f5 d6 c3");
        game.Undo();
        game.Undo();

        Square f4 = Square.Parse("f4");
        game.Play(f4);

        Assert.Equal([new Move(F5), new Move(f4)], game.Moves);
        Assert.False(game.CanRedo);
    }

    [Fact]
    public void Pass_WhenPlayerHasNoLegalMove()
    {
        Game game = TestGames.Play(TestGames.BlackMustPass);
        Board before = game.Board;

        Assert.Equal(Player.Black, game.ToMove);
        Assert.True(game.MustPass);
        Assert.False(game.IsGameOver);

        game.Pass();

        Assert.Equal(before, game.Board);
        Assert.Equal(Player.White, game.ToMove);
        Assert.Equal(Move.Pass, game.LastMove);
        Assert.False(game.MustPass);

        game.Undo();

        Assert.Equal(Player.Black, game.ToMove);
        Assert.True(game.MustPass);
    }

    [Fact]
    public void Play_WithPassMoveCallsPass()
    {
        Game game = TestGames.Play(TestGames.BlackMustPass);

        game.Play(Move.Pass);

        Assert.Equal(Move.Pass, game.LastMove);
    }

    [Fact]
    public void Pass_WithLegalMoveThrows()
    {
        var game = new Game();

        Assert.Throws<InvalidOperationException>(game.Pass);
        Assert.Equal(0, game.Ply);
    }

    [Fact]
    public void GameOver_HasWinnerAndRejectsMoves()
    {
        Game game = TestGames.Play(TestGames.WhiteWipedOut);

        Assert.True(game.IsGameOver);
        Assert.False(game.MustPass);
        Assert.Equal(Player.Black, game.Winner);
        Assert.Equal(13, game.Board.Count(Player.Black));
        Assert.Throws<InvalidOperationException>(() => game.Play(Square.Parse("a1")));
        Assert.Throws<InvalidOperationException>(game.Pass);
    }

    [Fact]
    public void SwitchSides_SwapsHumanAndComputer()
    {
        var game = new Game();

        game.SwitchSides();

        Assert.Equal(Player.White, game.Human);
        Assert.Equal(Player.Black, game.Computer);
        Assert.False(game.IsHumanToMove);
    }

    [Fact]
    public void Constructor_CanLetHumanPlayWhite()
    {
        var game = new Game(Player.White);

        Assert.Equal(Player.Black, game.Computer);
        Assert.False(game.IsHumanToMove);
    }

    [Fact]
    public void NewGame_ClearsHistoryAndKeepsSides()
    {
        Game game = TestGames.Play("f5 d6 c3", Player.White);
        game.Undo();

        game.NewGame();

        Assert.Equal(Board.Initial, game.Board);
        Assert.Equal(Player.Black, game.ToMove);
        Assert.Equal(Player.White, game.Human);
        Assert.Empty(game.Moves);
        Assert.False(game.CanRedo);
    }
}
