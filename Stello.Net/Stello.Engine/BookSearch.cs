using Stello.Engine.Evaluation;

namespace Stello.Engine;

/// <summary>Searches for book values, shared by book learning and the book tool.</summary>
internal static class BookSearch
{
    /// <summary>Searches only <paramref name="moves"/>, even a single one.</summary>
    public static (int Value, BookOrigin Origin, BookEffort Effort, Square Move) Search(
        SearchEngine engine, Board board, Player player, ulong moves, SearchLimits limits, CancellationToken cancellationToken)
    {
        SearchResult result = engine.Search(board, player, limits, cancellationToken: cancellationToken, onlyMoves: moves);

        // Solved positions are stored as wins/losses beyond the evaluation range, as in C++.
        int value = result.Kind is ScoreKind.Exact or ScoreKind.WinLossDraw
            ? result.Score > 0 ? Evaluator.WinScore + result.Score
                : result.Score < 0 ? result.Score - Evaluator.WinScore
                : 0
            : result.Score;

        BookOrigin origin = result.Kind switch
        {
            ScoreKind.Exact => BookOrigin.Exact,
            ScoreKind.WinLossDraw => BookOrigin.WinLossDraw,
            _ => BookOrigin.Heuristic,
        };

        return (value, origin, BookEffort.For(limits, result.Depth), result.Move.Square!.Value);
    }

    /// <summary>
    /// The value of the position for the player to move (C++: getvalue): a search, the opponent's value after a
    /// pass, or the disc count of a finished game.
    /// </summary>
    /// <param name="searched">Called after each search.</param>
    public static (int Value, BookOrigin Origin, BookEffort Effort) PositionValue(
        SearchEngine engine, Board board, Player player, SearchLimits limits, CancellationToken cancellationToken, Action? searched = null)
    {
        ulong moves = board.LegalMoves(player);
        if (moves != 0)
        {
            (int value, BookOrigin origin, BookEffort effort, _) = Search(engine, board, player, moves, limits, cancellationToken);
            searched?.Invoke();
            return (value, origin, effort);
        }

        Player opponent = player.Opponent();
        if (!board.HasLegalMove(opponent))
        {
            return (GameOverValue(board, player), BookOrigin.Exact, default);
        }

        (int passValue, BookOrigin passOrigin, BookEffort passEffort) = PositionValue(engine, board, opponent, limits, cancellationToken, searched);
        return (-passValue, passOrigin, passEffort);
    }

    private static int GameOverValue(Board board, Player player)
    {
        int difference = board.Count(player) - board.Count(player.Opponent());
        int empties = board.EmptyCount;
        return difference > 0 ? Evaluator.WinScore + difference + empties
            : difference < 0 ? difference - empties - Evaluator.WinScore
            : 0;
    }
}
