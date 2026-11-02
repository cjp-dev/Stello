using Stello.Engine.Evaluation;

namespace Stello.Engine.Search;

/// <summary>
/// The parts of the search that Stello 2's work is changing, to compare the old and the new way in matches
/// (Plans/improve-stello-strength-plan.md). Each choice disappears again once it is decided.
/// </summary>
internal sealed record SearchOptions
{
    /// <summary>The engine as the apps use it.</summary>
    public static SearchOptions Default { get; } = new();

    /// <summary>Another pattern evaluation, or null for the built-in <see cref="EvalWeights.Default"/>.</summary>
    public EvalWeights? Pattern { get; init; }

    /// <summary>
    /// No new iteration after this share of the time (null: 2/3). Kept as a switch: phase 5 found no significant gain
    /// from using more of the time, but a deeper search may gain more later.
    /// </summary>
    public int? SoftTimePercent { get; init; }

    /// <summary>Multi-ProbCut; off only to measure plain searches (the trainer's mpc command fits its error model).</summary>
    public bool Selective { get; init; } = true;

    /// <summary>
    /// Multi-ProbCut's t × 100: a cut needs the shallow search to be t standard deviations outside the window.
    /// Null: <see cref="SearchEngine.DefaultProbCutPercent"/>. Kept as a switch to retune t when the search changes.
    /// </summary>
    public int? ProbCutPercent { get; init; }

    /// <summary>
    /// Phase 5 item 6: in a book position, search the moves the book does not have and play the best of them if it
    /// beats the book move's value by <see cref="BookCheckMargin"/>.
    /// </summary>
    public BookCheck BookCheck { get; init; }

    /// <summary>How much better (1/100 disc) a searched move must be than the book move.</summary>
    public int BookCheckMargin { get; init; } = 100;
}

/// <summary>When <see cref="ComputerPlayer"/> searches the moves outside the book.</summary>
internal enum BookCheck
{
    /// <summary>Never: a book move is played without a search.</summary>
    Off,

    /// <summary>When the book move leads out of the book.</summary>
    Exit,

    /// <summary>In every book position.</summary>
    All,
}
