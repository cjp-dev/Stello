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
    /// The selective search's margin at depths 7 and 5: a shallow search must beat beta by |beta| ×
    /// <see cref="SelectivePercent"/> / 100 + <see cref="SelectiveMargin"/>. Null: 50 % and 50 units (1/100 disc).
    /// </summary>
    public int? SelectiveMargin { get; init; }

    /// <inheritdoc cref="SelectiveMargin"/>
    public int? SelectivePercent { get; init; }

    /// <summary>
    /// No new iteration after this share of the time (null: 2/3). Kept as a switch: phase 5 found no significant gain
    /// from using more of the time, but a deeper search may gain more later.
    /// </summary>
    public int? SoftTimePercent { get; init; }
}
