using Stello.Engine;

namespace Stello.App.Services;

/// <summary>The engine on a thread-pool thread in this process, as in the desktop app.</summary>
public sealed class LocalEngineHost(ComputerPlayer computer, OpeningBook book, IBookStore bookStore) : IEngineHost
{
    public bool SupportsLearning => true;

    public Task<SearchResult> ChooseMoveAsync(
        Board board,
        Player player,
        SearchLimits limits,
        IProgress<SearchInfo>? progress,
        CancellationToken cancellationToken,
        CancellationToken moveNowToken) =>
        Task.Run(() => computer.ChooseMove(board, player, limits, progress, cancellationToken, moveNowToken), CancellationToken.None);

    public void ResetBookTracker() => computer.BookTracker.Reset();

    public Task<BookUpdate> AddGameToBookAsync(IReadOnlyList<Move> moves, GameResult result)
    {
        // Adding a game does not search, so the limits do not matter.
        CreateLearner(SearchLimits.FixedDepth(1), hashBits: 10).AddGame(moves, result);
        return Task.FromResult(new BookUpdate(book.NodeCount, bookStore.Save(book)));
    }

    public async Task<BookLearningSummary> LearnAsync(
        BookLearningKind kind,
        SearchLimits limits,
        IProgress<BookLearningProgress>? progress,
        CancellationToken cancellationToken)
    {
        BookLearner learner = CreateLearner(limits, hashBits: 19);
        BookLearningOutcome outcome;
        string? error = null;
        try
        {
            await Task.Run(() => Learn(learner, kind, progress, cancellationToken), CancellationToken.None);
            outcome = BookLearningOutcome.Finished;
        }
        catch (OperationCanceledException)
        {
            outcome = BookLearningOutcome.Stopped;
        }
        catch (Exception exception)
        {
            outcome = BookLearningOutcome.Failed;
            error = exception.Message;
        }

        bool saved = bookStore.Save(book);
        return new BookLearningSummary(outcome, error, learner.PositionsEvaluated, learner.GamesPlayed, book.NodeCount, saved);
    }

    private void Learn(BookLearner learner, BookLearningKind kind, IProgress<BookLearningProgress>? progress, CancellationToken cancellationToken)
    {
        if (kind == BookLearningKind.EvaluateBook)
        {
            learner.EvaluatePositions(progress, cancellationToken);
            learner.Minimax();
        }
        else
        {
            learner.SelfPlay(
                progress,
                cancellationToken,
                (number, moves) => bookStore.AppendSelfPlayLog($"played game {number}: {string.Join(' ', moves)}"));
        }
    }

    private BookLearner CreateLearner(SearchLimits limits, int hashBits) =>
        new(book, new SearchEngine(hashBits), limits, new Random(), b => bookStore.Save(b));
}
