namespace Stello.Engine.Tests;

internal static class TestPositions
{
    /// <summary>Positions from seeded random games, taken after <paramref name="plies"/> moves (passes included).</summary>
    public static IEnumerable<(Board Board, Player ToMove)> RandomPositions(int seed, int count, int plies)
    {
        var random = new Random(seed);
        int found = 0;
        while (found < count)
        {
            Board board = Board.Initial;
            Player player = Player.Black;
            int played = 0;
            while (played < plies && !board.IsGameOver)
            {
                Square[] moves = Square.InMask(board.LegalMoves(player)).ToArray();
                if (moves.Length > 0)
                {
                    board = board.Play(player, moves[random.Next(moves.Length)]);
                }

                player = player.Opponent();
                played++;
            }

            if (!board.IsGameOver && board.HasLegalMove(player))
            {
                found++;
                yield return (board, player);
            }
        }
    }

    /// <summary>Mirrors the board in the a1-h8 diagonal.</summary>
    public static Board Transpose(Board board) =>
        new(Transpose(board.Black), Transpose(board.White));

    private static ulong Transpose(ulong bits)
    {
        ulong result = 0;
        foreach (Square square in Square.InMask(bits))
        {
            result |= Square.At(square.Row, square.Column).Bit;
        }

        return result;
    }
}

internal sealed class SyncProgress<T>(Action<T> report) : IProgress<T>
{
    public void Report(T value) => report(value);
}
