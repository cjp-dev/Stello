namespace Stello.Engine.Tests;

public class PerftTests
{
    [Theory]
    [InlineData(1, 4L)]
    [InlineData(2, 12L)]
    [InlineData(3, 56L)]
    [InlineData(4, 244L)]
    [InlineData(5, 1396L)]
    [InlineData(6, 8200L)]
    [InlineData(7, 55092L)]
    [InlineData(8, 390216L)]
    public void Perft_FromInitialPosition(int depth, long expected)
    {
        Assert.Equal(expected, Perft(Board.Initial, Player.Black, depth));
    }

    // A pass counts as one move; a finished game counts as one leaf.
    private static long Perft(Board board, Player player, int depth)
    {
        if (depth == 0)
        {
            return 1;
        }

        ulong moves = board.LegalMoves(player);
        if (moves == 0)
        {
            return board.HasLegalMove(player.Opponent())
                ? Perft(board, player.Opponent(), depth - 1)
                : 1;
        }

        long nodes = 0;
        foreach (Square move in Square.InMask(moves))
        {
            nodes += Perft(board.Play(player, move), player.Opponent(), depth - 1);
        }

        return nodes;
    }
}
