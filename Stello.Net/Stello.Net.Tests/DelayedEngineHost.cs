using Stello.App.Services;
using Stello.Engine;

namespace Stello.Net.Tests;

/// <summary>Waits <paramref name="delayMs"/> before asking the real engine, so a search can be made slow or instant.</summary>
internal sealed class DelayedEngineHost(IEngineHost inner, int delayMs) : IEngineHost
{
    public bool SupportsLearning => inner.SupportsLearning;

    public async Task<SearchResult> ChooseMoveAsync(
        Board board,
        Player player,
        SearchLimits limits,
        IProgress<SearchInfo>? progress,
        CancellationToken cancellationToken,
        CancellationToken moveNowToken)
    {
        await Task.Delay(delayMs, cancellationToken);
        return await inner.ChooseMoveAsync(board, player, limits, progress, cancellationToken, moveNowToken);
    }

    public void ResetBookTracker() => inner.ResetBookTracker();

    public Task<BookUpdate> AddGameToBookAsync(IReadOnlyList<Move> moves, GameResult result) =>
        inner.AddGameToBookAsync(moves, result);

    public Task<BookLearningSummary> LearnAsync(
        BookLearningKind kind,
        SearchLimits limits,
        IProgress<BookLearningProgress>? progress,
        CancellationToken cancellationToken) =>
        inner.LearnAsync(kind, limits, progress, cancellationToken);
}
