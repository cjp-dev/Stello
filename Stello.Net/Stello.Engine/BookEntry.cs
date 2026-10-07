namespace Stello.Engine;

/// <summary>Where the value of a book move comes from.</summary>
internal enum BookOrigin : byte
{
    /// <summary>Not searched: a move from an added game (±32665), or an imported value that cannot be trusted.</summary>
    Unknown,

    /// <summary>A midgame search of the position after the move.</summary>
    Heuristic,

    /// <summary>A search that solved the position for win/loss/draw.</summary>
    WinLossDraw,

    /// <summary>A search that solved the position exactly, or a finished game.</summary>
    Exact,

    /// <summary>Minus the value of the position after the move, which is in the book.</summary>
    BackedUp,
}

internal enum EffortKind : byte
{
    /// <summary>Not known (imported from the C++ book, or a game clock).</summary>
    None,

    /// <summary>Time per move; the amount is in milliseconds.</summary>
    Time,

    /// <summary>Fixed depth; the amount is in plies.</summary>
    Depth,

    /// <summary>Solved with no time limit.</summary>
    Solve,
}

/// <summary>How hard a book value was searched for.</summary>
/// <param name="DepthReached">Plies searched, or empty squares when the endgame was solved.</param>
/// <param name="EngineVersion">The <see cref="OpeningBook.EngineVersion"/> that found the value.</param>
internal readonly record struct BookEffort(EffortKind Kind, int Amount, int DepthReached, int EngineVersion)
{
    public static BookEffort For(SearchLimits limits, int depthReached) => limits.Mode switch
    {
        TimeControlMode.TimePerMove => new(EffortKind.Time, (int)limits.Time.TotalMilliseconds, depthReached, OpeningBook.EngineVersion),
        TimeControlMode.FixedDepth => new(EffortKind.Depth, limits.Depth, depthReached, OpeningBook.EngineVersion),
        TimeControlMode.Solve => new(EffortKind.Solve, 0, depthReached, OpeningBook.EngineVersion),
        _ => new(EffortKind.None, 0, depthReached, OpeningBook.EngineVersion),
    };
}

/// <summary>A book move. The move is stored in the position's canonical frame (see <see cref="OpeningBook"/>).</summary>
/// <remarks>The value is from the point of view of the player who makes the move.</remarks>
internal sealed class BookEntry(Move move, short value, BookOrigin origin, BookEffort effort = default)
{
    public Move Move { get; } = move;

    public short Value { get; private set; } = value;

    public BookOrigin Origin { get; private set; } = origin;

    public BookEffort Effort { get; private set; } = effort;

    /// <summary>The value comes from a search of the position after the move (C++: CALCULATED).</summary>
    public bool IsSearched => IsSearchOrigin(Origin);

    public static bool IsSearchOrigin(BookOrigin origin) =>
        origin is BookOrigin.Heuristic or BookOrigin.WinLossDraw or BookOrigin.Exact;

    public void Set(short value, BookOrigin origin, BookEffort effort = default)
    {
        Value = value;
        Origin = origin;
        Effort = effort;
    }
}
