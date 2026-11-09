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

    /// <summary>Another pattern evaluation, or null for the native engine's built-in one (<c>Book/stello-eval.bin</c>).</summary>
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
    /// With more than 1 thread, YBWC with half of the threads on Lazy SMP helpers (the default); off is Lazy SMP alone on
    /// all threads (Plans/making-stello-3-plan.md, phase 6). Kept as a switch to compare the two in matches.
    /// </summary>
    public bool Ybwc { get; init; } = true;
}
