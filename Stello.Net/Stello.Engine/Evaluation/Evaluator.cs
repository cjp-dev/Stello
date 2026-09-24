using System.Numerics;
using System.Runtime.CompilerServices;

namespace Stello.Engine.Evaluation;

/// <summary>
/// Port of <c>eval()</c> and <c>dangerous()</c> from Eval.cpp/Minmax.cpp. Scores are for the side to move.
/// The C++ logic works on a 10x10 mailbox with legacy square numbers, so the port builds one per call.
/// </summary>
internal static class Evaluator
{
    public const int WinScore = 32600;

    private const int CurrentMobilityWeight = 400; // C++: CURMOB
    private const int PotentialMobilityWeight = 600; // C++: POTMOB
    private const int StableDiscWeight = 60;

    // Mailbox values, as C++ enum contents.
    private const byte Light = 0;
    private const byte Dark = 1;
    private const byte EmptySquare = 2;
    private const byte BorderSquare = 3;

    private static readonly ColourFlags WhiteFlags = new(0x20, 0x10, 0x02, 0x01, 0x800, 0x400, 0x1000);
    private static readonly ColourFlags BlackFlags = new(0x80, 0x40, 0x200, 0x100, 0x08, 0x04, 0x2000);

    private static readonly CornerTables WhiteTables = new(EdgeTables.WhiteFirstCorner, EdgeTables.WhiteLastCorner, EdgeTables.WhiteMiddle);
    private static readonly CornerTables BlackTables = new(EdgeTables.BlackFirstCorner, EdgeTables.BlackLastCorner, EdgeTables.BlackMiddle);

    // Edges read from their first to their last square: row 1 (a1-h1), column h (h1-h8), row 8 (a8-h8), column a (a1-a8).
    private static readonly int[][] EdgeSquares =
    [
        [0, 1, 2, 3, 4, 5, 6, 7],
        [7, 15, 23, 31, 39, 47, 55, 63],
        [56, 57, 58, 59, 60, 61, 62, 63],
        [0, 8, 16, 24, 32, 40, 48, 56],
    ];

    // C++ corners h1-h4 = a1, h1, h8, a8, each the first or last square of two edges; Square/Step walk the diagonal.
    private static readonly CornerInfo[] Corners =
    [
        new(0, false, 3, false, 11, 11),
        new(0, true, 1, false, 18, 9),
        new(1, true, 2, true, 88, -11),
        new(3, true, 2, false, 81, -9),
    ];

    private static readonly int[] LegacyOfIndex = Enumerable.Range(0, 64).Select(i => (i >> 3) * 10 + (i & 7) + 11).ToArray();

    public static int Evaluate(in Board board, Player player, int alpha, int beta, int opponentMobility)
    {
        bool isBlack = player == Player.Black;
        ulong own = isBlack ? board.Black : board.White;
        ulong opponent = isBlack ? board.White : board.Black;
        int ownCount = BitOperations.PopCount(own);
        int opponentCount = BitOperations.PopCount(opponent);

        if (ownCount == 0)
        {
            return -WinScore - opponentCount;
        }

        if (opponentCount == 0)
        {
            return WinScore + ownCount;
        }

        if (alpha > WinScore)
        {
            return -WinScore;
        }

        Span<byte> sq = stackalloc byte[100];
        FillMailbox(board, sq);
        byte p = isBlack ? Dark : Light;
        byte o = isBlack ? Light : Dark;

        int score = EdgeScore(sq, EdgeIndices(board), isBlack, p, o, ownCount + opponentCount);
        score += CornerStability(sq, 11, 1, 10, 11, p)
            + CornerStability(sq, 18, -1, 10, 9, p)
            + CornerStability(sq, 81, 1, -10, -9, p)
            + CornerStability(sq, 88, -1, -10, -11, p);

        // Mobility cannot move the score back into the window.
        const int maxMobility = CurrentMobilityWeight + PotentialMobilityWeight;
        if (score + maxMobility <= alpha || score - maxMobility >= beta)
        {
            return score;
        }

        int ownMobility = BitOperations.PopCount(Bitboards.LegalMoves(own, opponent));
        score += CurrentMobilityWeight * (ownMobility - opponentMobility) / (ownMobility + opponentMobility + 2);

        int ownPotential = Bitboards.PotentialMobility(own, opponent);
        int opponentPotential = Bitboards.PotentialMobility(opponent, own);
        score += PotentialMobilityWeight * (ownPotential - opponentPotential) / (ownPotential + opponentPotential + 2);

        return score;
    }

    /// <summary>
    /// True if <paramref name="mover"/> just took a corner that the edge tables mark as unsettled,
    /// so the position must be searched one ply deeper instead of evaluated (C++: dangerous).
    /// </summary>
    public static bool IsDangerous(in Board after, Player mover, int moveIndex)
    {
        int corner = moveIndex switch
        {
            0 => 0,
            7 => 1,
            63 => 2,
            56 => 3,
            _ => -1,
        };

        if (corner < 0)
        {
            return false;
        }

        ColourFlags flags = mover == Player.Black ? BlackFlags : WhiteFlags;
        return CornerFlag(EdgeIndices(after), Corners[corner], flags.Danger1, flags.Danger2) != 0;
    }

    /// <summary>Base-3 edge index, first square most significant; white = 0, black = 1, empty = 2.</summary>
    internal static Edges EdgeIndices(in Board board)
    {
        var edges = new Edges();
        for (int e = 0; e < 4; e++)
        {
            int index = 0;
            foreach (int square in EdgeSquares[e])
            {
                ulong bit = 1UL << square;
                index = index * 3 + ((board.White & bit) != 0 ? Light : (board.Black & bit) != 0 ? Dark : EmptySquare);
            }

            edges[e] = index;
        }

        return edges;
    }

    private static void FillMailbox(in Board board, Span<byte> sq)
    {
        sq.Fill(BorderSquare);
        for (int index = 0; index < 64; index++)
        {
            ulong bit = 1UL << index;
            sq[LegacyOfIndex[index]] = (board.White & bit) != 0 ? Light : (board.Black & bit) != 0 ? Dark : EmptySquare;
        }
    }

    // A two-ply look-ahead on the edge tables: the opponent's corner and edge replies, then the player's.
    private static int EdgeScore(ReadOnlySpan<byte> sq, Edges e, bool isBlack, byte p, byte o, int discCount)
    {
        var context = new EdgeContext
        {
            // The tables are from white's point of view.
            Sign = isBlack ? -1 : 1,
            Own = isBlack ? BlackTables : WhiteTables,
            Opponent = isBlack ? WhiteTables : BlackTables,
            // Probability (per mille) that an X-square disc lets the opponent reach an empty corner later.
            Chance = (64 - discCount) * 1000 / 64 / 2 + 500,
        };

        ColourFlags own = isBlack ? BlackFlags : WhiteFlags;
        ColourFlags opp = isBlack ? WhiteFlags : BlackFlags;

        for (int c = 0; c < 4; c++)
        {
            int bit = 1 << c;
            CornerInfo corner = Corners[c];

            if (CornerFlag(e, corner, own.Danger1, own.Danger2) == 0)
            {
                bool canTake = CornerFlag(e, corner, own.Corner1, own.Corner2) != 0;
                if (!canTake && sq[corner.Square] == EmptySquare && sq[corner.Square + corner.Step] == o)
                {
                    context.OwnPossible |= bit;
                    canTake = DiagonalReaches(sq, corner, p, o);
                }

                if (canTake)
                {
                    context.OwnCorners |= bit;
                }
            }

            if (CornerFlag(e, corner, opp.Danger1, opp.Danger2) == 0)
            {
                bool canTake = CornerFlag(e, corner, opp.Stable1, opp.Stable2) != 0;
                if (!canTake && sq[corner.Square] == EmptySquare && sq[corner.Square + corner.Step] == p)
                {
                    context.OpponentPossible |= bit;
                    canTake = DiagonalReaches(sq, corner, o, p);
                }

                if (canTake)
                {
                    context.OpponentCorners |= bit;
                }
            }
        }

        for (int i = 0; i < 4; i++)
        {
            int flags = EdgeTables.CornerFlags[e[i]];
            if ((flags & own.Middle) != 0)
            {
                context.OwnMiddle |= 1 << i;
            }

            if ((flags & opp.Middle) != 0)
            {
                context.OpponentMiddle |= 1 << i;
            }
        }

        int flat = context.Score(e);
        int score = flat;

        for (int i = 0; i < 4; i++)
        {
            if ((context.OpponentMiddle & (1 << i)) != 0)
            {
                score = Math.Min(score, context.Score(With(e, i, context.Opponent.Middle[e[i]])));
            }
        }

        for (int c = 0; c < 4; c++)
        {
            int bit = 1 << c;
            if ((context.OpponentCorners & bit) != 0)
            {
                score = Math.Min(score, context.Score(TakeCorner(e, c, context.Opponent)));
            }
            else if ((context.OpponentPossible & bit) != 0 && (context.OwnCorners & bit) == 0)
            {
                score = Math.Min(score, context.Blend(context.Score(TakeCorner(e, c, context.Opponent)), flat));
            }
        }

        for (int i = 0; i < 4; i++)
        {
            if ((context.OwnMiddle & (1 << i)) != 0)
            {
                Edges t = With(e, i, context.Own.Middle[e[i]]);
                score = context.Reply(t, context.Score(t), score, excludedCorner: -1);
            }
        }

        for (int c = 0; c < 4; c++)
        {
            int bit = 1 << c;
            Edges t = TakeCorner(e, c, context.Own);
            if ((context.OwnCorners & bit) != 0)
            {
                score = context.Reply(t, context.Score(t), score, c);
            }
            else if ((context.OwnPossible & bit) != 0 && (context.OpponentCorners & bit) == 0)
            {
                score = context.Reply(t, context.Blend(context.Score(t), flat), score, c);
            }
        }

        return score;
    }

    // Walks from the X-square towards the far corner: true if the line of opponent discs is closed by a mover disc.
    private static bool DiagonalReaches(ReadOnlySpan<byte> sq, CornerInfo corner, byte mover, byte other)
    {
        for (int distance = 2; distance <= 7; distance++)
        {
            byte value = sq[corner.Square + distance * corner.Step];
            if (value == mover)
            {
                return true;
            }

            if (value != other)
            {
                return false;
            }
        }

        return false;
    }

    // Counts discs that are stable because they grow diagonally out of an occupied corner.
    private static int CornerStability(ReadOnlySpan<byte> sq, int corner, int horizontal, int vertical, int diagonal, byte player)
    {
        byte colour = sq[corner];
        if (colour != Light && colour != Dark)
        {
            return 0;
        }

        int x = 0;
        for (int square = corner + horizontal; sq[square] == colour; square += horizontal)
        {
            x++;
        }

        int y = 0;
        for (int square = corner + vertical; sq[square] == colour; square += vertical)
        {
            y++;
        }

        // The edges themselves are already scored by the edge tables.
        int stable = 0;
        int start = corner;
        while ((x >= 1 && y > 1) || (x > 1 && y >= 1))
        {
            start += diagonal;
            stable++;

            int limit = x - 2;
            x = 0;
            for (int square = start + horizontal; sq[square] == colour && limit-- > 0; square += horizontal)
            {
                x++;
            }

            stable += x;

            limit = y - 2;
            y = 0;
            for (int square = start + vertical; sq[square] == colour && limit-- > 0; square += vertical)
            {
                y++;
            }

            stable += y;
        }

        return (colour == player ? StableDiscWeight : -StableDiscWeight) * stable;
    }

    private static int CornerFlag(Edges e, CornerInfo corner, int firstFlag, int lastFlag) =>
        (EdgeTables.CornerFlags[e[corner.EdgeA]] & (corner.LastOfA ? lastFlag : firstFlag))
        | (EdgeTables.CornerFlags[e[corner.EdgeB]] & (corner.LastOfB ? lastFlag : firstFlag));

    private static Edges With(Edges e, int edge, int index)
    {
        e[edge] = index;
        return e;
    }

    private static Edges TakeCorner(Edges e, int corner, CornerTables tables)
    {
        CornerInfo info = Corners[corner];
        e[info.EdgeA] = (info.LastOfA ? tables.Last : tables.First)[e[info.EdgeA]];
        e[info.EdgeB] = (info.LastOfB ? tables.Last : tables.First)[e[info.EdgeB]];
        return e;
    }

    [InlineArray(4)]
    internal struct Edges
    {
        private int _element;
    }

    private readonly record struct ColourFlags(int Danger1, int Danger2, int Corner1, int Corner2, int Stable1, int Stable2, int Middle);

    // Edge index after this colour plays on the first corner, the last corner, or its best middle square of an edge.
    private readonly record struct CornerTables(short[] First, short[] Last, short[] Middle);

    private readonly record struct CornerInfo(int EdgeA, bool LastOfA, int EdgeB, bool LastOfB, int Square, int Step);

    private struct EdgeContext
    {
        public int Sign;
        public CornerTables Own;
        public CornerTables Opponent;
        public int Chance;
        public int OwnCorners;       // C++: hj
        public int OwnPossible;      // C++: phj
        public int OpponentCorners;  // C++: sj
        public int OpponentPossible; // C++: psj
        public int OwnMiddle;        // C++: midt
        public int OpponentMiddle;   // C++: smidt

        public readonly int Score(Edges e) =>
            Sign * (EdgeTables.Stability[e[0]] + EdgeTables.Stability[e[1]] + EdgeTables.Stability[e[2]] + EdgeTables.Stability[e[3]]);

        public readonly int Blend(int possible, int current) =>
            (int)(((long)Chance * possible + (1000L - Chance) * current) / 1000);

        // Opponent's best edge reply after one of our edge moves.
        public readonly int Reply(Edges t, int start, int score, int excludedCorner)
        {
            int result = start;
            for (int j = 0; j < 4; j++)
            {
                if ((OpponentMiddle & (1 << j)) != 0)
                {
                    result = Math.Min(result, Score(With(t, j, Opponent.Middle[t[j]])));
                }
            }

            for (int c = 0; c < 4; c++)
            {
                int bit = 1 << c;
                if (c == excludedCorner)
                {
                    continue;
                }

                if ((OpponentCorners & bit) != 0)
                {
                    result = Math.Min(result, Score(TakeCorner(t, c, Opponent)));
                }
                else if ((OpponentPossible & bit) != 0 && (OwnCorners & bit) == 0)
                {
                    // Kept from C++: this overwrites the best score so far instead of lowering the reply score.
                    score = Math.Min(result, Blend(Score(TakeCorner(t, c, Opponent)), start));
                }
            }

            return Math.Max(result, score);
        }
    }
}
