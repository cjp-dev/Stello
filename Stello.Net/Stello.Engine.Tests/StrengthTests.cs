namespace Stello.Engine.Tests;

public class StrengthTests
{
    private const int Games = 50;
    private const int RequiredWins = 48; // 95 % of 50, rounded up

    [Fact]
    public void Depth4_BeatsRandomPlayer()
    {
        var random = new Random(11);
        int wins = PlayMatch((board, player) => RandomMove(board, player, random));

        Assert.InRange(wins, RequiredWins, Games);
    }

    [Fact]
    public void Depth4_BeatsGreedyPlayer()
    {
        var random = new Random(12);
        int wins = PlayMatch((board, player) => GreedyMove(board, player, random));

        Assert.InRange(wins, RequiredWins, Games);
    }

    // The engine plays black in even games and white in odd games.
    private static int PlayMatch(Func<Board, Player, Square> opponent)
    {
        var engine = new SearchEngine(hashBits: 16);
        int wins = 0;

        for (int game = 0; game < Games; game++)
        {
            Player enginePlayer = game % 2 == 0 ? Player.Black : Player.White;
            Board board = Board.Initial;
            Player player = Player.Black;

            while (!board.IsGameOver)
            {
                if (board.HasLegalMove(player))
                {
                    Square move = player == enginePlayer
                        ? engine.Search(board, player, SearchLimits.FixedDepth(4)).Move.Square!.Value
                        : opponent(board, player);
                    board = board.Play(player, move);
                }

                player = player.Opponent();
            }

            if (board.Count(enginePlayer) > board.Count(enginePlayer.Opponent()))
            {
                wins++;
            }
        }

        return wins;
    }

    private static Square RandomMove(Board board, Player player, Random random)
    {
        Square[] moves = Square.InMask(board.LegalMoves(player)).ToArray();
        return moves[random.Next(moves.Length)];
    }

    private static Square GreedyMove(Board board, Player player, Random random)
    {
        Square[] moves = Square.InMask(board.LegalMoves(player)).ToArray();
        int most = moves.Max(m => System.Numerics.BitOperations.PopCount(board.Flips(player, m)));
        Square[] best = moves.Where(m => System.Numerics.BitOperations.PopCount(board.Flips(player, m)) == most).ToArray();
        return best[random.Next(best.Length)];
    }
}
