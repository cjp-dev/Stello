using Stello.Engine.Evaluation;

namespace Stello.Engine.Search;

/// <summary>
/// The parts of the search that Stello 2 is changing, to compare the old and the new way in matches
/// (Plans/improve-stello-strength-plan.md). Each choice disappears again once it is decided.
/// </summary>
internal sealed record SearchOptions
{
    /// <summary>Stello 1's search and evaluation.</summary>
    public static SearchOptions Classic { get; } = new();

    /// <summary>The trained pattern evaluation, or null for Stello 1's evaluation.</summary>
    public EvalWeights? Pattern { get; init; }

    /// <summary>
    /// The selective search's margin at depths 7 and 5: a shallow search must beat beta by |beta| ×
    /// <see cref="SelectivePercent"/> / 100 + <see cref="SelectiveMargin"/>. Null: Stello 1's 50 % and 50 units.
    /// </summary>
    public int? SelectiveMargin { get; init; }

    /// <inheritdoc cref="SelectiveMargin"/>
    public int? SelectivePercent { get; init; }

    /// <summary>
    /// Stello 1's extension: a corner the classic edge tables call unsettled is searched one ply deeper instead of
    /// being evaluated.
    /// </summary>
    public bool DangerousCorners { get; init; } = true;
}
