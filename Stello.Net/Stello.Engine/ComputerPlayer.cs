using Stello.Engine.Search;

namespace Stello.Engine;

/// <summary>
/// Chooses the computer's move: from the opening book while the game is in the book, otherwise by searching
/// (C++: getcomputer). Not thread-safe; run one move at a time.
/// </summary>
public sealed class ComputerPlayer(SearchEngine engine, OpeningBook? book, Random random)
{
    public SearchEngine Engine { get; } = engine;

    public OpeningBook? Book { get; } = book;

    public BookTracker BookTracker { get; } = new();

    public SearchResult ChooseMove(
        Board board,
        Player player,
        SearchLimits limits,
        IProgress<SearchInfo>? progress = null,
        CancellationToken cancellationToken = default,
        CancellationToken moveNowToken = default)
    {
        if (limits.UseOpeningBook && Book is not null && board.HasLegalMove(player) && BookTracker.ShouldConsult)
        {
            bool found = Book.TryGetMove(board, player, random, out BookMove move, out ulong bookMoves, out bool leavesBook);
            BookTracker.Record(found);
            if (found)
            {
                var bookResult = new SearchResult(new Move(move.Square), move.Value, ScoreKind.Book, 0, 0, 0, TimeSpan.Zero);
                SearchOptions options = Engine.Options;
                ulong others = board.LegalMoves(player) & ~bookMoves;
                bool check = options.BookCheck == BookCheck.All || (options.BookCheck == BookCheck.Exit && leavesBook);
                if (!check || others == 0)
                {
                    return bookResult;
                }

                // A book move costs no time, so the move's time goes to the moves the book does not have.
                SearchResult other = Engine.Search(board, player, limits, progress, cancellationToken, moveNowToken, onlyMoves: others);
                if (!moveNowToken.IsCancellationRequested && BookSearch.BookValue(other) > move.Value + options.BookCheckMargin)
                {
                    return other;
                }

                return bookResult with { Nodes = other.Nodes, Evaluations = other.Evaluations, Elapsed = other.Elapsed };
            }
        }

        return Engine.Search(board, player, limits, progress, cancellationToken, moveNowToken);
    }
}
