namespace Stello.Engine;

/// <summary>Text game record: the moves from the initial position separated by whitespace, e.g. "f5 d6 c3 pass e3".</summary>
public static class GameRecordFormat
{
    /// <summary>Writes the moves up to the current position; undone moves are not saved.</summary>
    public static string Format(Game game) => string.Join(' ', game.PlayedMoves);

    /// <exception cref="FormatException">A token is not a move, or a move is not legal in the position.</exception>
    public static Game Parse(string text, Player human = Player.Black)
    {
        var game = new Game(human);
        string[] tokens = text.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);

        for (int i = 0; i < tokens.Length; i++)
        {
            int number = i + 1;
            if (!Move.TryParse(tokens[i], out Move? move))
            {
                throw new FormatException($"Move {number}: '{tokens[i]}' is not a square (a1-h8) or 'pass'.");
            }

            if (game.IsGameOver)
            {
                throw new FormatException($"Move {number}: the game is already over.");
            }

            if (move.Value.IsPass && !game.MustPass)
            {
                throw new FormatException($"Move {number}: {game.ToMove} has a legal move and cannot pass.");
            }

            if (move.Value.Square is { } square && !game.Board.IsLegal(game.ToMove, square))
            {
                throw new FormatException($"Move {number}: {square} is not a legal move for {game.ToMove}.");
            }

            game.Play(move.Value);
        }

        return game;
    }
}
