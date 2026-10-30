// SPDX-License-Identifier: GPL-3.0-only

using System.Globalization;

namespace Stello.Egaroucid.Tests;

/// <summary>
/// Phase 2 of Plans/egaroucid-port-plan.md: the board, move generation, hashing, stability and evaluation of the port
/// give exactly what the C++ Egaroucid gives (the fixtures of tools/Egaroucid.Reference).
/// </summary>
public class FoundationTests
{
    [Fact]
    public void Coordinates_MatchEgaroucid()
    {
        Assert.Equal("a1", Common.IdxToCoord(63));
        Assert.Equal("h1", Common.IdxToCoord(56));
        Assert.Equal("a8", Common.IdxToCoord(7));
        Assert.Equal("h8", Common.IdxToCoord(0));
        for (int idx = 0; idx < 64; idx++)
        {
            Assert.Equal(idx, Common.CoordToIdx(Common.IdxToCoord(idx)));
        }
    }

    [Fact]
    public void Board_ParsesAndFormats()
    {
        const string start = "---------------------------OX------XO--------------------------- X";
        Board board = Board.FromString(start);
        Assert.Equal(Board.Initial, board);
        Assert.Equal(start, board.ToString(blackToMove: true));
        Board white = Board.FromString(start[..64] + " O");
        Assert.Equal(new Board(board.Opponent, board.Player), white);
        Assert.Throws<FormatException>(() => Board.FromString("XO X"));
    }

    [Fact]
    public void Perft_EqualsReference()
    {
        int checkedRows = 0;
        foreach (string[] row in Fixtures.Rows("perft.txt"))
        {
            Board board = Board.FromString(row[1]);
            int depth = int.Parse(row[2], CultureInfo.InvariantCulture);
            Assert.Equal(long.Parse(row[3], CultureInfo.InvariantCulture), Perft(board, depth, passed: false, passCounts: true));
            Assert.Equal(long.Parse(row[4], CultureInfo.InvariantCulture), Perft(board, depth, passed: false, passCounts: false));
            checkedRows++;
        }

        Assert.True(checkedRows >= 40, $"only {checkedRows} rows");
    }

    [Fact]
    public void Stability_EqualsReference()
    {
        foreach (string[] row in Fixtures.Rows("eval.txt"))
        {
            Board board = Board.FromString(row[1]);
            Assert.True(ulong.Parse(row[5], NumberStyles.HexNumber, CultureInfo.InvariantCulture) == Stability.CalcStability(board.Player, board.Opponent), $"{row[0]} player");
            Assert.True(ulong.Parse(row[6], NumberStyles.HexNumber, CultureInfo.InvariantCulture) == Stability.CalcStability(board.Opponent, board.Player), $"{row[0]} opponent");
        }
    }

    [Fact]
    public void Features_EqualReference()
    {
        var eval = new EvalSearch();
        foreach (string[] row in Fixtures.Rows("eval.txt"))
        {
            eval.Init(Board.FromString(row[1]));
            Assert.Equal(row[7], string.Join(',', eval.RawFeatures.ToArray()));
        }
    }

    [EgaroucidFact]
    public void HashAndEvaluation_EqualReference()
    {
        Fixtures.LoadResources();
        var eval = new EvalSearch();
        int rows = 0;
        foreach (string[] row in Fixtures.Rows("eval.txt"))
        {
            Board board = Board.FromString(row[1]);
            Assert.True(uint.Parse(row[2], CultureInfo.InvariantCulture) == board.Hash(), $"{row[0]} hash");
            Assert.True(int.Parse(row[3], CultureInfo.InvariantCulture) == Evaluate.MidEvaluate(board), $"{row[0]} mid_evaluate");
            eval.Init(board);
            Assert.True(int.Parse(row[4], CultureInfo.InvariantCulture) == Evaluate.MidEvaluateMoveOrderingEnd(eval), $"{row[0]} move ordering end");
            rows++;
        }

        Assert.True(rows > 2000, $"only {rows} rows");
    }

    [EgaroucidFact]
    public void EvalFile_HasTheExpectedSize()
    {
        Fixtures.LoadResources();
        short[] weights = Evaluate.LoadUnzipEgev2(Path.Combine(EgaroucidResources.LoadedDirectory!, "eval.egev2"));
        Assert.Equal(60 * (612_360 + 65), weights.Length);
        Assert.Equal(612_360, Evaluate.PhaseSize);
    }

    /// <summary>
    /// Along random games, the incrementally updated features (as the side to move sees them) equal the features
    /// computed from scratch, after moves, passes and undos, for both the full and the endgame update.
    /// </summary>
    [Fact]
    public void IncrementalFeatures_EqualFromScratch()
    {
        var random = new Random(20261030);
        var incremental = new EvalSearch();
        var endsearch = new EvalSearch();
        var fresh = new EvalSearch();
        for (int game = 0; game < 200; game++)
        {
            Board board = Board.Initial;
            incremental.Init(board);
            endsearch.Init(board);
            var line = new Stack<(Flip Flip, bool Passed)>();
            while (!board.IsEnd())
            {
                ulong legal = board.GetLegal();
                if (legal == 0)
                {
                    board.Pass();
                    incremental.Pass();
                    endsearch.Pass();
                    line.Push((default, true));
                }
                else
                {
                    int pick = random.Next(Bits.PopCount(legal));
                    for (int i = 0; i < pick; i++)
                    {
                        legal &= legal - 1;
                    }

                    var flip = new Flip();
                    flip.Calc(board.Player, board.Opponent, Bits.Ctz(legal));
                    incremental.Move(flip);
                    endsearch.MoveEndsearch(flip);
                    board.MoveBoard(flip);
                    line.Push((flip, false));
                }

                fresh.Init(board);
                Assert.Equal(fresh.CurrentFeatures.ToArray(), incremental.CurrentFeatures.ToArray());
                Assert.Equal(fresh.CurrentFeatures[32..48].ToArray(), endsearch.CurrentFeatures[32..48].ToArray());
            }

            // Undo half of the game and check again on the way back. As in the search, a pass is undone by passing again.
            int undo = line.Count / 2;
            for (int i = 0; i < undo; i++)
            {
                (Flip flip, bool passed) = line.Pop();
                if (passed)
                {
                    incremental.Pass();
                    board.Pass();
                }
                else
                {
                    incremental.Undo();
                    board.UndoBoard(flip);
                }

                fresh.Init(board);
                Assert.Equal(fresh.CurrentFeatures.ToArray(), incremental.CurrentFeatures.ToArray());
            }
        }
    }

    /// <summary>The leaf count of Egaroucid's perft.hpp: with a pass counted as a move, or not.</summary>
    private static long Perft(Board board, int depth, bool passed, bool passCounts)
    {
        if (depth == 0)
        {
            return 1;
        }

        ulong legal = board.GetLegal();
        if (legal == 0)
        {
            if (passed)
            {
                return 1;
            }

            board.Pass();
            return Perft(board, passCounts ? depth - 1 : depth, passed: true, passCounts);
        }

        if (depth == 1)
        {
            return Bits.PopCount(legal);
        }

        long result = 0;
        var flip = new Flip();
        for (int cell = Bits.FirstBit(ref legal); legal != 0; cell = Bits.NextBit(ref legal))
        {
            flip.Calc(board.Player, board.Opponent, cell);
            result += Perft(board.MoveCopy(flip), depth - 1, passed: false, passCounts);
        }

        return result;
    }
}
