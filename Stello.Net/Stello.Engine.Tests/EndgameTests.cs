using System.Diagnostics;
using Stello.Engine.Evaluation;
using Stello.Engine.Search;

namespace Stello.Engine.Tests;

public class EndgameTests
{
    private static readonly Lazy<EvalWeights> Weights =
        new(() => EvalWeights.Load(Path.Combine(AppContext.BaseDirectory, "Data", "stello-eval.bin")));

    public static TheoryData<string> Solvers => ["classic", "new"];

    // Stello 1's solver, or Stello 2's (Endgame/SearchEngine.Endgame.cs) with the pattern evaluation.
    private static SearchEngine Engine(string solver, int hashBits = 19) => solver == "new"
        ? new SearchEngine(hashBits, new SearchOptions { Pattern = Weights.Value, NewEndgame = true })
        : new SearchEngine(hashBits);

    public static TheoryData<string, string, string, Player, int, string[]> FfoPositionsBySolver
    {
        get
        {
            var data = new TheoryData<string, string, string, Player, int, string[]>();
            foreach (string solver in new[] { "classic", "new" })
            {
                foreach (object[] row in FfoPositions)
                {
                    data.Add(solver, (string)row[0], (string)row[1], (Player)row[2], (int)row[3], (string[])row[4]);
                }
            }

            return data;
        }
    }

    public static TheoryData<string, string, Player, int, string[]> FfoPositions => new()
    {
        { "#40", "O--OOOOX-OOOOOOXOOXXOOOXOOXOOOXXOOOOOOXX---OOOOX----O--X--------", Player.Black, 38, ["a2"] },
        { "#41", "-OOOOO----OOOOX--OOOOOO-XXXXXOO--XXOOX--OOXOXX----OXXO---OOO--O-", Player.Black, 0, ["h4"] },
        { "#42", "--OOO-------XX-OOOOOOXOO-OOOOXOOX-OOOXXO---OOXOO---OOOXO--OOOO--", Player.Black, 6, ["g2"] },
        { "#43", "--XXXXX---XXXX---OOOXX---OOXXXX--OOXXXO-OOOOXOO----XOX----XXXXX-", Player.White, -12, ["c7", "g3"] },
        { "#44", "--O-X-O---O-XO-O-OOXXXOOOOOOXXXOOOOOXX--XXOOXO----XXXX-----XXX--", Player.White, -14, ["d2", "b8"] },
    };

    [Theory]
    [MemberData(nameof(FfoPositionsBySolver))]
    public void Solve_FindsExactScoreOfFfoPosition(string solver, string name, string position, Player player, int score, string[] bestMoves)
    {
        SearchEngine engine = Engine(solver);

        SearchResult result = engine.Search(Board.Parse(position), player, SearchLimits.Solve);

        Assert.True(result.Kind == ScoreKind.Exact, name);
        Assert.Equal(score, result.Score);
        Assert.Contains(result.Move.ToString(), bestMoves);
    }

    [Theory]
    [MemberData(nameof(Solvers))]
    public void Solve_MatchesMinimaxOnSmallEndgames(string solver)
    {
        SearchEngine engine = Engine(solver, hashBits: 16);

        foreach ((Board board, Player player) in TestPositions.RandomPositions(seed: 7, count: 40, plies: 52))
        {
            SearchResult result = engine.Search(board, player, SearchLimits.Solve);
            int expected = Minimax(board, player);

            Assert.Equal(expected, result.Score);
            Assert.Equal(expected, -Minimax(board.Play(player, result.Move.Square!.Value), player.Opponent()));
        }
    }

    [Theory]
    [MemberData(nameof(Solvers))]
    public void Solve_MatchesMinimaxOnTheLastFewSquares(string solver)
    {
        SearchEngine engine = Engine(solver, hashBits: 10);

        for (int plies = 57; plies <= 60; plies++)
        {
            foreach ((Board board, Player player) in TestPositions.RandomPositions(seed: plies, count: 75, plies: plies))
            {
                SearchResult result = engine.Search(board, player, SearchLimits.Solve);

                Assert.Equal(Minimax(board, player), result.Score);
            }
        }
    }

    [Theory]
    [MemberData(nameof(Solvers))]
    public void Solve_MatchesTheEndgameCSuite(string solver)
    {
        SearchEngine engine = Engine(solver, hashBits: 16);
        string[] lines = File.ReadAllLines(Path.Combine(AppContext.BaseDirectory, "Data", "endgame-c-positions.txt"))
            .Where(line => !line.StartsWith('#'))
            .ToArray();

        Assert.Equal(112, lines.Length);
        foreach (string line in lines)
        {
            string[] parts = line.Split(' ');
            Board board = Board.Parse(parts[0]);
            int expected = int.Parse(parts[1]);

            // White is to move; if White must pass, Black moves and the score is negated.
            Player player = board.HasLegalMove(Player.White) ? Player.White : Player.Black;
            int score = board.HasLegalMove(player)
                ? engine.Search(board, player, SearchLimits.Solve).Score
                : Minimax(board, player);

            Assert.True(expected == (player == Player.White ? score : -score), line);
        }
    }

    [Theory]
    [MemberData(nameof(Solvers))]
    public void FixedDepth_SolvesTheEndgameWhenItIsNear(string solver)
    {
        (Board board, Player player) = TestPositions.RandomPositions(seed: 3, count: 1, plies: 50).Single();

        SearchResult result = Engine(solver).Search(board, player, SearchLimits.FixedDepth(4));

        Assert.Equal(ScoreKind.Exact, result.Kind);
        Assert.Equal(Minimax(board, player), result.Score);
    }

    [Fact]
    public void NewSolver_AgreesWithTheClassicSolverAboveThirteenEmptySquares()
    {
        // 14 to 18 empty squares: the null-window search above the local table, its transposition table and ETC.
        SearchEngine classic = Engine("classic", hashBits: 16);
        SearchEngine solver = Engine("new", hashBits: 16);
        for (int plies = 42; plies <= 46; plies++)
        {
            foreach ((Board board, Player player) in TestPositions.RandomPositions(seed: 100 + plies, count: 6, plies: plies))
            {
                SearchResult expected = classic.Search(board, player, SearchLimits.Solve);
                SearchResult result = solver.Search(board, player, SearchLimits.Solve);

                Assert.Equal(ScoreKind.Exact, result.Kind);
                Assert.Equal(expected.Score, result.Score);
            }
        }
    }

    [Fact]
    public async Task NewSolver_MoveNowReturnsQuickly()
    {
        // FFO #49, 26 empty squares: far more than a second to solve.
        Board board = Board.Parse("--OX-O----XXOO--OOOOOXX-OOOOOX--OOOXOXX-OOOOXX-----OOX----X-O---");
        using var moveNow = new CancellationTokenSource();
        SearchEngine engine = Engine("new");
        Task<SearchResult> search = Task.Run(() =>
            engine.Search(board, Player.Black, SearchLimits.Solve, moveNowToken: moveNow.Token));

        await Task.Delay(500);
        Assert.False(search.IsCompleted);
        var stopwatch = Stopwatch.StartNew();
        await moveNow.CancelAsync();
        SearchResult result = await search;

        Assert.InRange(stopwatch.ElapsedMilliseconds, 0, 100);
        Assert.True(board.IsLegal(Player.Black, result.Move.Square!.Value));
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
