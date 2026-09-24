namespace Stello.Engine.Tests;

public class EndgameTests
{
    public static TheoryData<string, string, Player, int, string[]> FfoPositions => new()
    {
        { "#40", "O--OOOOX-OOOOOOXOOXXOOOXOOXOOOXXOOOOOOXX---OOOOX----O--X--------", Player.Black, 38, ["a2"] },
        { "#41", "-OOOOO----OOOOX--OOOOOO-XXXXXOO--XXOOX--OOXOXX----OXXO---OOO--O-", Player.Black, 0, ["h4"] },
        { "#42", "--OOO-------XX-OOOOOOXOO-OOOOXOOX-OOOXXO---OOXOO---OOOXO--OOOO--", Player.Black, 6, ["g2"] },
        { "#43", "--XXXXX---XXXX---OOOXX---OOXXXX--OOXXXO-OOOOXOO----XOX----XXXXX-", Player.White, -12, ["c7", "g3"] },
        { "#44", "--O-X-O---O-XO-O-OOXXXOOOOOOXXXOOOOOXX--XXOOXO----XXXX-----XXX--", Player.White, -14, ["d2", "b8"] },
    };

    [Theory]
    [MemberData(nameof(FfoPositions))]
    public void Solve_FindsExactScoreOfFfoPosition(string name, string position, Player player, int score, string[] bestMoves)
    {
        var engine = new SearchEngine();

        SearchResult result = engine.Search(Board.Parse(position), player, SearchLimits.Solve);

        Assert.True(result.Kind == ScoreKind.Exact, name);
        Assert.Equal(score, result.Score);
        Assert.Contains(result.Move.ToString(), bestMoves);
    }

    [Fact]
    public void Solve_MatchesMinimaxOnSmallEndgames()
    {
        var engine = new SearchEngine(hashBits: 16);

        foreach ((Board board, Player player) in TestPositions.RandomPositions(seed: 7, count: 40, plies: 52))
        {
            SearchResult result = engine.Search(board, player, SearchLimits.Solve);
            int expected = Minimax(board, player);

            Assert.Equal(expected, result.Score);
            Assert.Equal(expected, -Minimax(board.Play(player, result.Move.Square!.Value), player.Opponent()));
        }
    }

    [Fact]
    public void FixedDepth_SolvesTheEndgameWhenItIsNear()
    {
        (Board board, Player player) = TestPositions.RandomPositions(seed: 3, count: 1, plies: 50).Single();

        SearchResult result = new SearchEngine().Search(board, player, SearchLimits.FixedDepth(4));

        Assert.Equal(ScoreKind.Exact, result.Kind);
        Assert.Equal(Minimax(board, player), result.Score);
    }

    // Plain negamax to the end of the game; empty squares go to the winner.
    private static int Minimax(Board board, Player player)
    {
        ulong moves = board.LegalMoves(player);
        if (moves == 0)
        {
            if (!board.HasLegalMove(player.Opponent()))
            {
                int difference = board.Count(player) - board.Count(player.Opponent());
                return difference > 0 ? difference + board.EmptyCount
                    : difference < 0 ? difference - board.EmptyCount
                    : 0;
            }

            return -Minimax(board, player.Opponent());
        }

        int best = int.MinValue;
        foreach (Square move in Square.InMask(moves))
        {
            best = Math.Max(best, -Minimax(board.Play(player, move), player.Opponent()));
        }

        return best;
    }
}
