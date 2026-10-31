namespace Stello.Engine.Search;

/// <summary>Move ordering from Sort.cpp: hash move, response killers (C++: humres/comres) and square values.</summary>
internal sealed class MoveOrdering
{
    // C++: scores[100]; the squares next to the corners are recomputed for every position.
    private static readonly sbyte[] BaseSquareScores =
    [
        127, -32, 22, 11, 11, 22, -32, 127,
        -32, -64, -8, -5, -5, -8, -64, -32,
        22, -8, 12, 1, 1, 12, -8, 22,
        11, -5, 1, 0, 0, 1, -5, 11,
        11, -5, 1, 0, 0, 1, -5, 11,
        22, -8, 12, 1, 1, 12, -8, 22,
        -32, -64, -8, -5, -5, -8, -64, -32,
        127, -32, 22, 11, 11, 22, -32, 127,
    ];

    // Corner, the two C-squares with the square beyond each, and the X-square.
    private static readonly (int Corner, int C1, int Beyond1, int C2, int Beyond2, int X)[] CornerAreas =
    [
        (0, 1, 2, 8, 16, 9),
        (7, 6, 5, 15, 23, 14),
        (56, 48, 40, 57, 58, 49),
        (63, 62, 61, 55, 47, 54),
    ];

    // [replier * 4096 + previous move * 64 + reply]
    private readonly int[] _responses = new int[2 * 64 * 64];

    public void ClearResponses() => Array.Clear(_responses);

    public static int BaseScore(int square) => BaseSquareScores[square];

    /// <summary>A refutation (the reply caused a cutoff) counts 4, other best replies 1.</summary>
    public void RecordResponse(Player replier, int move, int reply, bool refuted)
    {
        if (reply >= 0)
        {
            _responses[ResponseRow(replier, move) + reply] += refuted ? 4 : 1;
        }
    }

    /// <summary>C++: sortlist. Hash move first, then moves with response values, then the rest by square value.</summary>
    public void Order(Span<int> moves, int hashMove, int previousMove, Player player, in Board board)
    {
        int start = 0;
        if (hashMove >= 0)
        {
            int index = moves.IndexOf(hashMove);
            if (index >= 0)
            {
                (moves[0], moves[index]) = (moves[index], moves[0]);
                start = 1;
            }
        }

        Span<int> keys = stackalloc int[moves.Length];
        if (previousMove >= 0)
        {
            int row = ResponseRow(player, previousMove);
            Span<int> rest = moves[start..];
            for (int i = 0; i < rest.Length; i++)
            {
                keys[i] = _responses[row + rest[i]];
            }

            SortDescending(rest, keys[..rest.Length]);

            for (int i = 0; moves.Length - start >= 2 && keys[i] != 0; i++)
            {
                start++;
            }
        }

        if (moves.Length - start <= 1)
        {
            return;
        }

        Span<sbyte> scores = stackalloc sbyte[64];
        SquareScores(board, player, scores);
        Span<int> remaining = moves[start..];
        for (int i = 0; i < remaining.Length; i++)
        {
            keys[i] = scores[remaining[i]];
        }

        SortDescending(remaining, keys[..remaining.Length]);
    }

    public static void SquareScores(in Board board, Player player, Span<sbyte> scores)
    {
        BaseSquareScores.CopyTo(scores);
        ulong own = board.Discs(player);
        ulong opponent = board.Discs(player.Opponent());

        foreach (var area in CornerAreas)
        {
            if ((own & (1UL << area.Corner)) != 0)
            {
                scores[area.C1] = 34;
                scores[area.C2] = 34;
                scores[area.X] = 24;
            }
            else if ((opponent & (1UL << area.Corner)) != 0)
            {
                scores[area.C1] = (sbyte)((opponent & (1UL << area.Beyond1)) != 0 ? 32 : -32);
                scores[area.C2] = (sbyte)((opponent & (1UL << area.Beyond2)) != 0 ? 32 : -32);
                scores[area.X] = -16;
            }
            else
            {
                scores[area.C1] = -16;
                scores[area.C2] = -16;
                scores[area.X] = -64;
            }
        }
    }

    // Stable insertion sort, highest key first (as the C++ linear sort).
    public static void SortDescending(Span<int> moves, Span<int> keys)
    {
        for (int i = 1; i < moves.Length; i++)
        {
            int move = moves[i];
            int key = keys[i];
            int j = i - 1;
            while (j >= 0 && keys[j] < key)
            {
                moves[j + 1] = moves[j];
                keys[j + 1] = keys[j];
                j--;
            }

            moves[j + 1] = move;
            keys[j + 1] = key;
        }
    }

    private static int ResponseRow(Player replier, int previousMove) => ((int)replier * 64 + previousMove) * 64;
}
