using Stello.Engine.Evaluation;

namespace Stello.Engine.Tests;

public class EvaluatorTests
{
    private const int AllEmptyEdge = 6560;

    [Fact]
    public void EdgeTables_HaveOneEntryPerEdge()
    {
        short[][] tables =
        [
            EdgeTables.Stability, EdgeTables.WhiteMiddle, EdgeTables.BlackMiddle, EdgeTables.WhiteFirstCorner,
            EdgeTables.WhiteLastCorner, EdgeTables.BlackFirstCorner, EdgeTables.BlackLastCorner, EdgeTables.CornerFlags,
        ];

        Assert.All(tables, table => Assert.Equal(6561, table.Length));
        Assert.Equal(0, EdgeTables.Stability[AllEmptyEdge]);
    }

    [Fact]
    public void EdgeTables_CornerMovesChangeTheCornerSquare()
    {
        // Digits: white = 0, black = 1, empty = 2; the first square is the most significant digit.
        Assert.Equal(AllEmptyEdge - 2 * 2187, EdgeTables.WhiteFirstCorner[AllEmptyEdge]);
        Assert.Equal(AllEmptyEdge - 2187, EdgeTables.BlackFirstCorner[AllEmptyEdge]);
        Assert.Equal(AllEmptyEdge - 2, EdgeTables.WhiteLastCorner[AllEmptyEdge]);
        Assert.Equal(AllEmptyEdge - 1, EdgeTables.BlackLastCorner[AllEmptyEdge]);
    }

    [Fact]
    public void EdgeIndices_InitialPositionHasEmptyEdges()
    {
        Evaluator.Edges edges = Evaluator.EdgeIndices(Board.Initial);

        for (int i = 0; i < 4; i++)
        {
            Assert.Equal(AllEmptyEdge, edges[i]);
        }
    }

    [Fact]
    public void EdgeIndices_ReadEdgesFromTheirFirstSquare()
    {
        var board = new Board(Square.Parse("h1").Bit, Square.Parse("a1").Bit);

        Evaluator.Edges edges = Evaluator.EdgeIndices(board);

        Assert.Equal(AllEmptyEdge - 2 * 2187 - 1, edges[0]); // a1 white first, h1 black last
        Assert.Equal(AllEmptyEdge - 2187, edges[1]);         // h1 black first
        Assert.Equal(AllEmptyEdge, edges[2]);
        Assert.Equal(AllEmptyEdge - 2 * 2187, edges[3]);     // a1 white first
    }

    [Fact]
    public void Evaluate_WipedOutPlayerLoses()
    {
        var board = new Board(Square.Parse("d4").Bit | Square.Parse("e4").Bit, 0);

        Assert.Equal(-Evaluator.WinScore - 2, Evaluator.Evaluate(board, Player.White, -32767, 32767, 0));
        Assert.Equal(Evaluator.WinScore + 2, Evaluator.Evaluate(board, Player.Black, -32767, 32767, 0));
    }

    [Fact]
    public void Evaluate_IsSymmetricInTheMainDiagonal()
    {
        foreach ((Board board, Player player) in TestPositions.RandomPositions(seed: 1, count: 200, plies: 30))
        {
            int mobility = board.Count(player.Opponent()) % 10;

            Assert.Equal(
                Evaluator.Evaluate(board, player, -32767, 32767, mobility),
                Evaluator.Evaluate(TestPositions.Transpose(board), player, -32767, 32767, mobility));
        }
    }

    [Fact]
    public void Evaluate_StartPositionIsBalanced()
    {
        int black = Evaluator.Evaluate(Board.Initial, Player.Black, -32767, 32767, 4);
        int white = Evaluator.Evaluate(Board.Initial, Player.White, -32767, 32767, 4);

        Assert.Equal(black, white);
    }

    [Fact]
    public void IsDangerous_OnlyForCornerMoves()
    {
        Board board = Board.Initial.Play(Player.Black, Square.Parse("f5"));

        Assert.False(Evaluator.IsDangerous(board, Player.Black, Square.Parse("f5").Index));
    }
}
