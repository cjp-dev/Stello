namespace Stello.Engine;

public enum ScoreKind
{
    /// <summary>No search was needed (pass or only one legal move).</summary>
    None,

    /// <summary>Evaluation units; beyond ±32600 the game is won or lost.</summary>
    Heuristic,

    /// <summary>Only the sign is known: win (&gt; 0), draw (0) or loss (&lt; 0).</summary>
    WinLossDraw,

    /// <summary>Final disc difference with perfect play.</summary>
    Exact,

    /// <summary>The move comes from the opening book; the score is the book value.</summary>
    Book,
}

/// <summary>Progress from a running search (C++: make_try/make_res).</summary>
/// <param name="Depth">Plies searched, or empty squares for the endgame search.</param>
public sealed record SearchInfo(
    int Depth,
    Square CurrentMove,
    Square? BestMove,
    int Score,
    ScoreKind Kind,
    long Nodes,
    long Evaluations,
    TimeSpan Elapsed);

public sealed record SearchResult(
    Move Move,
    int Score,
    ScoreKind Kind,
    int Depth,
    long Nodes,
    long Evaluations,
    TimeSpan Elapsed);
