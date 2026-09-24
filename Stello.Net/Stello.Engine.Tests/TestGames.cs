namespace Stello.Engine.Tests;

internal static class TestGames
{
    // After these moves Black has no legal move but White has.
    public const string BlackMustPass = "d3 c3 b3 b2 f5 a3 a1 c1";

    // Shortest possible game: White is wiped out, 13-0.
    public const string WhiteWipedOut = "d3 c3 b3 d2 e1 d6 d7 e3 f4";

    public static Game Play(string moves, Player human = Player.Black)
    {
        var game = new Game(human);
        foreach (string move in moves.Split(' ', StringSplitOptions.RemoveEmptyEntries))
        {
            game.Play(Square.Parse(move));
        }

        return game;
    }
}
