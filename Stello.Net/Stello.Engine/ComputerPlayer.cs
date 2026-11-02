using System.Diagnostics.CodeAnalysis;

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
        CancellationToken moveNowToken = default) =>
        TryChooseBookMove(board, player, limits, out SearchResult? bookMove)
            ? bookMove
            : Engine.Search(board, player, limits, progress, cancellationToken, moveNowToken);

    /// <summary>
    /// The first step of <see cref="ChooseMove"/>: the book move, if the limits allow the book and the game is still
    /// in it. Records the lookup in <see cref="BookTracker"/>. The web app calls it on the page and sends only the
    /// searches to its engine worker.
    /// </summary>
    public bool TryChooseBookMove(Board board, Player player, SearchLimits limits, [NotNullWhen(true)] out SearchResult? result)
    {
        if (limits.UseOpeningBook && Book is not null && board.HasLegalMove(player) && BookTracker.ShouldConsult)
        {
            bool found = Book.TryGetMove(board, player, random, out BookMove move);
            BookTracker.Record(found);
            if (found)
            {
                result = new SearchResult(new Move(move.Square), move.Value, ScoreKind.Book, 0, 0, 0, TimeSpan.Zero);
                return true;
            }
        }

        result = null;
        return false;
    }
}
