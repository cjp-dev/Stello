using Stello.Engine;

namespace Stello.App.Services;

/// <summary>Runs the engine and owns the opening book; the desktop runs it in-process, the browser in a Web Worker.</summary>
public interface IEngineHost
{
    /// <summary>False where only games can be added to the book (no Evaluate Book or Self-play).</summary>
    bool SupportsLearning { get; }

    /// <param name="cancellationToken">Stops the search and throws <see cref="OperationCanceledException"/>.</param>
    /// <param name="moveNowToken">Stops the search and returns the best move found so far.</param>
    Task<SearchResult> ChooseMoveAsync(
        Board board,
        Player player,
        SearchLimits limits,
        IProgress<SearchInfo>? progress,
        CancellationToken cancellationToken,
        CancellationToken moveNowToken);

    /// <summary>Lets the computer look in the book again, e.g. after a new game or taking back moves.</summary>
    void ResetBookTracker();

    Task<BookUpdate> AddGameToBookAsync(IReadOnlyList<Move> moves, GameResult result);

    /// <summary>Learns until finished or cancelled, then saves the book; never throws.</summary>
    Task<BookLearningSummary> LearnAsync(
        BookLearningKind kind,
        SearchLimits limits,
        IProgress<BookLearningProgress>? progress,
        CancellationToken cancellationToken);
}

public sealed record BookUpdate(int NodeCount, bool Saved);

public enum BookLearningKind
{
    /// <summary>C++: Minmaxlib.</summary>
    EvaluateBook,

    /// <summary>C++: Lær spil.</summary>
    SelfPlay,
}

public enum BookLearningOutcome
{
    Finished,
    Stopped,
    Failed,
}

public sealed record BookLearningSummary(
    BookLearningOutcome Outcome,
    string? Error,
    int PositionsEvaluated,
    int GamesPlayed,
    int NodeCount,
    bool Saved);
