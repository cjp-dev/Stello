namespace Stello.Engine;

public enum TimeControlMode
{
    /// <summary>Search a fixed number of plies (C++: sogedybde).</summary>
    FixedDepth,

    /// <summary>A fixed thinking time for each move (C++: tid_per_trek).</summary>
    TimePerMove,

    /// <summary>Share the remaining time for the whole game over the remaining moves (C++: spil_tid).</summary>
    TimePerGame,

    /// <summary>Solve the position to the end with no time limit.</summary>
    Solve,
}

public sealed record SearchLimits
{
    public const int DefaultHashBits = 19;

    private SearchLimits(TimeControlMode mode, int depth, TimeSpan time, bool useOpeningBook = true, int hashBits = DefaultHashBits)
    {
        Mode = mode;
        Depth = depth;
        Time = time;
        UseOpeningBook = useOpeningBook;
        HashBits = hashBits;
    }

    public static SearchLimits Solve { get; } = new(TimeControlMode.Solve, 0, TimeSpan.Zero, useOpeningBook: false);

    public TimeControlMode Mode { get; }

    /// <summary>Plies to search in <see cref="TimeControlMode.FixedDepth"/>.</summary>
    public int Depth { get; }

    /// <summary>Time for this move, or the time left for the whole game.</summary>
    public TimeSpan Time { get; }

    /// <summary>Whether to query the opening book before running tree search.</summary>
    public bool UseOpeningBook { get; }

    /// <summary>Hash table size (log2 slots) for the search engine.</summary>
    public int HashBits { get; }

    public static SearchLimits FixedDepth(int plies, bool useOpeningBook = true, int hashBits = DefaultHashBits)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(plies, 1);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(plies, 60);
        return new SearchLimits(TimeControlMode.FixedDepth, plies, TimeSpan.Zero, useOpeningBook, hashBits);
    }

    public static SearchLimits TimePerMove(TimeSpan time, bool useOpeningBook = true, int hashBits = DefaultHashBits)
    {
        ArgumentOutOfRangeException.ThrowIfLessThanOrEqual(time, TimeSpan.Zero);
        return new SearchLimits(TimeControlMode.TimePerMove, 0, time, useOpeningBook, hashBits);
    }

    /// <param name="remaining">Time left on the computer's clock; may be negative when the time is used up.</param>
    public static SearchLimits TimePerGame(TimeSpan remaining, bool useOpeningBook = true, int hashBits = DefaultHashBits) =>
        new(TimeControlMode.TimePerGame, 0, remaining, useOpeningBook, hashBits);
}
