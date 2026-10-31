using System.Numerics;
using Stello.Engine.Endgame;

namespace Stello.Engine.Tests;

public class EndgameBitsTests
{
    [Fact]
    public void FlipsAndLegalMoves_EqualTheSearchBitboards()
    {
        foreach ((Board board, Player player) in TestPositions.RandomPositions(seed: 21, count: 300, plies: 30))
        {
            ulong own = board.Discs(player);
            ulong opponent = board.Discs(player.Opponent());
            Assert.Equal(Bitboards.LegalMoves(own, opponent), EndgameBits.LegalMoves(own, opponent));
            for (int square = 0; square < 64; square++)
            {
                if ((~(own | opponent) & (1UL << square)) != 0)
                {
                    Assert.Equal(Bitboards.Flips(own, opponent, square), EndgameBits.Flips(own, opponent, square));
                }
            }
        }
    }

    [Fact]
    public void ScoreWithEmpties_GivesTheEmptySquaresToTheWinner()
    {
        foreach ((Board board, Player player) in TestPositions.RandomPositions(seed: 22, count: 300, plies: 50))
        {
            ulong own = board.Discs(player);
            int difference = board.Count(player) - board.Count(player.Opponent());
            int empties = board.EmptyCount;
            int expected = difference > 0 ? difference + empties : difference < 0 ? difference - empties : 0;

            Assert.Equal(expected, EndgameBits.ScoreWithEmpties(own, empties));
            if (empties % 2 == 1)
            {
                Assert.Equal(expected, EndgameBits.ScoreWithOddEmpties(own, empties));
            }
        }
    }

    [Fact]
    public void StableDiscs_AreNeverFlippedLater()
    {
        var random = new Random(23);
        foreach ((Board start, Player first) in TestPositions.RandomPositions(seed: 23, count: 200, plies: 40))
        {
            ulong stableBlack = StableDiscs.Of(start.Black, start.White);
            ulong stableWhite = StableDiscs.Of(start.White, start.Black);
            Assert.Equal(stableBlack, stableBlack & start.Black);
            Assert.Equal(stableWhite, stableWhite & start.White);

            Board board = start;
            Player player = first;
            while (true)
            {
                ulong moves = board.LegalMoves(player);
                if (moves == 0)
                {
                    player = player.Opponent();
                    moves = board.LegalMoves(player);
                    if (moves == 0)
                    {
                        break;
                    }
                }

                int pick = random.Next(BitOperations.PopCount(moves));
                for (int i = 0; i < pick; i++)
                {
                    moves &= moves - 1;
                }

                board = board.Play(player, new Square(BitOperations.TrailingZeroCount(moves)));
                player = player.Opponent();
                Assert.Equal(stableBlack, board.Black & stableBlack);
                Assert.Equal(stableWhite, board.White & stableWhite);
            }
        }
    }
}
