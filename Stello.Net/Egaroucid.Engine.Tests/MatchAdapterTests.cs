// SPDX-License-Identifier: GPL-3.0-only

using Stello.Benchmark;
using Stello.Engine;
using Stello.Engine.Benchmark;
using EngineBoard = Stello.Engine.Board;

namespace Stello.Egaroucid.Tests;

/// <summary>Phase 5 of Plans/egaroucid-port-plan.md: the port as a match side in the benchmark tool.</summary>
[Collection("Timing")]
public class MatchAdapterTests
{
    [Fact]
    public void ToEgaroucid_TheStartPositionIsEgaroucids()
    {
        Assert.Equal(Board.Initial, EgaroucidPlayer.ToEgaroucid(EngineBoard.Initial, Player.Black));
    }

    [Fact]
    public void SolveBench_SelectsFfoPositions50Through59()
    {
        List<BenchmarkPosition> all = BenchmarkPosition.Load(Program.DataFile("ffo-40-59.txt"));

        List<BenchmarkPosition> selected = BenchCommands.SelectByIdRange(all, 50, 59);

        Assert.Equal(Enumerable.Range(50, 10).Select(id => id.ToString()), selected.Select(position => position.Id));
    }

    /// <summary>Along random games, both boards have the same legal moves and give the same position after each move.</summary>
    [Fact]
    public void ToEgaroucid_KeepsMovesAndFlipsAlongRandomGames()
    {
        var random = new Random(5);
        for (int game = 0; game < 50; game++)
        {
            EngineBoard board = EngineBoard.Initial;
            Player player = Player.Black;
            while (!board.IsGameOver)
            {
                if (!board.HasLegalMove(player))
                {
                    player = player.Opponent();
                    continue;
                }

                Board egaroucid = EgaroucidPlayer.ToEgaroucid(board, player);
                ulong legal = board.LegalMoves(player);
                Assert.Equal(EgaroucidPlayer.ReverseBits(legal), egaroucid.GetLegal());

                Square[] moves = Square.InMask(legal).ToArray();
                Square move = moves[random.Next(moves.Length)];
                Assert.Equal(move, EgaroucidPlayer.ToSquare(63 - move.Index));
                var flip = new Flip();
                flip.Calc(egaroucid.Player, egaroucid.Opponent, 63 - move.Index);
                egaroucid.MoveBoard(flip);

                board = board.Play(player, move);
                player = player.Opponent();
                Assert.Equal(EgaroucidPlayer.ToEgaroucid(board, player), egaroucid);
            }
        }
    }

    [Theory]
    [InlineData("engine=egaroucid,depth=5")]
    [InlineData("level=5")]
    [InlineData("seed=3,time-ms=100")]
    [InlineData("engine=egaroucid,time-ms=100,level=5")]
    [InlineData("engine=egaroucid,time-ms=100,hash-bits=26")]
    [InlineData("engine=edax,time-ms=100")]
    [InlineData("time-ms=100,book=yes")]
    [InlineData("depth=5,opt=ybwc")]
    [InlineData("depth=5,opt=unknown=on")]
    [InlineData("depth=5,opt=selective=maybe")]
    [InlineData("depth=5,opt=soft-pct=101")]
    [InlineData("depth=5,opt=ybwc=off,opt=ybwc=on")]
    [InlineData("engine=stello-v2,depth=5,opt=ybwc=off")]
    public void ParseSide_RejectsInvalidSides(string text)
    {
        Assert.Throws<ArgumentException>(() => MatchCommands.ParseSide(text));
    }

    [Fact]
    public void ParseSide_AppliesRepeatedOptSearchSwitches()
    {
        MatchSide side = MatchCommands.ParseSide("depth=4,opt=selective=off,opt=soft-pct=50,opt=mpc=150,opt=ybwc=off");

        Assert.NotNull(side.Options);
        Assert.False(side.Options.Selective);
        Assert.Equal(50, side.Options.SoftTimePercent);
        Assert.Equal(150, side.Options.ProbCutPercent);
        Assert.False(side.Options.Ybwc);
        Assert.Contains("Multi-ProbCut off", MatchCommands.Describe(side), StringComparison.Ordinal);
        Assert.Contains("YBWC off", MatchCommands.Describe(side), StringComparison.Ordinal);
    }

    [Fact]
    public void ParseSide_KeepsExistingNamedSearchSwitches()
    {
        MatchSide side = MatchCommands.ParseSide("depth=4,soft-pct=50,mpc=150,ybwc=off");

        Assert.NotNull(side.Options);
        Assert.Equal(50, side.Options.SoftTimePercent);
        Assert.Equal(150, side.Options.ProbCutPercent);
        Assert.True(side.Options.Selective);
        Assert.False(side.Options.Ybwc);
    }

    [EgaroucidFact]
    public void EngineMatch_PlaysTheEgaroucidPortAgainstStello()
    {
        Fixtures.LoadResources();
        MatchSide stello = MatchCommands.ParseSide("depth=2,name=stello");
        MatchSide egaroucid = MatchCommands.ParseSide("engine=egaroucid,time-ms=20,hash-bits=16,book=on,name=egaroucid");
        Assert.Contains("Egaroucid port, 20 ms/move, table bits 16, book", MatchCommands.Describe(egaroucid), StringComparison.Ordinal);
        Assert.Contains("strict time on", MatchCommands.Describe(egaroucid), StringComparison.Ordinal);

        IReadOnlyList<MatchPair> pairs = new EngineMatch(stello, egaroucid, 2).Play(["d3c3c4c5f6f3f4e2d2c2", "d3c5e6d2c6d6c7f6f4f3b4"], null, CancellationToken.None);

        Assert.Equal(2, pairs.Count);
        foreach ((string start, MatchGame game) in pairs.SelectMany(pair => new[] { (pair.Start, pair.AFirst), (pair.Start, pair.BFirst) }))
        {
            (EngineBoard board, Player toMove) = BookTextFormat.Replay(start);
            foreach (Move move in game.Moves)
            {
                board = move.IsPass ? board : board.Play(toMove, move.Square!.Value);
                toMove = toMove.Opponent();
            }

            Assert.True(board.IsGameOver, "the game was played to the end");
            Assert.Equal(game.DiscsB, board.Count(game.ColourB) - board.Count(game.ColourB.Opponent()));
            Assert.True(game.StatsB.Searched > 5);
            Assert.InRange(game.StatsB.Longest.TotalMilliseconds, 1, 500);
        }
    }

    [Fact]
    public void EngineMatch_StelloOne_PlaysWholeGamesAgainstStello()
    {
        // Stello 1, the frozen copy of the engine Stello 2 started from, as a match side.
        MatchSide stello = MatchCommands.ParseSide("depth=3,name=stello");
        MatchSide stelloOne = MatchCommands.ParseSide("engine=stello-v1,depth=3,name=stello-v1");
        Assert.StartsWith("Stello 1, depth 3", MatchCommands.Describe(stelloOne), StringComparison.Ordinal);

        IReadOnlyList<MatchPair> pairs = new EngineMatch(stello, stelloOne, 2).Play(["d3c3c4c5f6f3f4e2d2c2", "d3c5e6d2c6d6c7f6f4f3b4"], null, CancellationToken.None);

        Assert.Equal(2, pairs.Count);
        Assert.All(pairs, pair => Assert.True(pair.AFirst.Moves.Count > 40 && pair.BFirst.Moves.Count > 40));
    }

    [EgaroucidFact]
    public void EgaroucidPlayer_WithTheSameSeed_PlaysTheSameBookMoves()
    {
        Fixtures.LoadResources();
        var first = new EgaroucidPlayer(16, EgaroucidResources.Book, 2, 7);
        var second = new EgaroucidPlayer(16, EgaroucidResources.Book, 2, 7);
        foreach (EgaroucidPlayer player in new[] { first, second })
        {
            player.NewGame();
        }

        EngineBoard board = EngineBoard.Initial;
        Player toMove = Player.Black;
        for (int ply = 0; ply < 8; ply++)
        {
            Stello.Engine.SearchResult a = first.ChooseMove(board, toMove, SearchLimits.TimePerMove(TimeSpan.FromMilliseconds(20)), CancellationToken.None);
            Stello.Engine.SearchResult b = second.ChooseMove(board, toMove, SearchLimits.TimePerMove(TimeSpan.FromMilliseconds(20)), CancellationToken.None);
            Assert.Equal(ScoreKind.Book, a.Kind);
            Assert.Equal(a.Move, b.Move);
            board = board.Play(toMove, a.Move.Square!.Value);
            toMove = toMove.Opponent();
        }
    }

    [Theory]
    [InlineData("engine=egaroucid,time-ms=20", "strict time on")]
    [InlineData("engine=egaroucid,time-ms=20,time-strict=off", "strict time off")]
    public void ParseSide_EgaroucidTimeStrictDefaultsOnAndCanBeDisabled(string sideText, string expected)
    {
        MatchSide side = MatchCommands.ParseSide(sideText);

        Assert.Contains(expected, MatchCommands.Describe(side), StringComparison.Ordinal);
    }

    [Fact]
    public void ParseSide_TimeStrictIsOnlyValidForEgaroucid()
    {
        ArgumentException exception = Assert.Throws<ArgumentException>(() => MatchCommands.ParseSide("time-ms=20,time-strict=on"));

        Assert.Contains("time-strict is only for engine=egaroucid", exception.Message, StringComparison.Ordinal);
    }
}
