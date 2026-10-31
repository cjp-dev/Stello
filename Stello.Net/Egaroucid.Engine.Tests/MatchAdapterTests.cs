// SPDX-License-Identifier: GPL-3.0-only

using Stello.Benchmark;
using Stello.Engine;
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
    public void ParseSide_RejectsInvalidSides(string text)
    {
        Assert.Throws<ArgumentException>(() => MatchCommands.ParseSide(text));
    }

    [EgaroucidFact]
    public void EngineMatch_PlaysTheEgaroucidPortAgainstStello()
    {
        Fixtures.LoadResources();
        MatchSide stello = MatchCommands.ParseSide("depth=2,name=stello");
        MatchSide egaroucid = MatchCommands.ParseSide("engine=egaroucid,time-ms=20,hash-bits=16,book=on,name=egaroucid");
        Assert.Contains("Egaroucid port, 20 ms/move, table bits 16, book", MatchCommands.Describe(egaroucid), StringComparison.Ordinal);

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
}
