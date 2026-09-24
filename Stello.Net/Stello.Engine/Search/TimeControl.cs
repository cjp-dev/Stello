namespace Stello.Engine.Search;

/// <param name="SoftMs">Do not start a new iteration after this many milliseconds.</param>
/// <param name="HardMs">Stop the search after this many milliseconds.</param>
internal readonly record struct TimeBudget(long SoftMs, long HardMs)
{
    public static TimeBudget Unlimited { get; } = new(long.MaxValue, long.MaxValue);
}

/// <summary>Time allocation from Kontrol.cpp (calc_time, calc_end_time, stop_nu).</summary>
internal static class TimeControl
{
    // C++: lookahead - 2 with the default level 8; the last moves take almost no time.
    private const int FastEndgameEmpties = 6;

    public static TimeBudget ForMidgame(SearchLimits limits, int empties) => limits.Mode switch
    {
        // C++: tider[] is about 2/3 of rtider[].
        TimeControlMode.TimePerMove => new TimeBudget(Milliseconds(limits.Time) * 2 / 3, Milliseconds(limits.Time)),
        TimeControlMode.TimePerGame => ForGame(Milliseconds(limits.Time), empties),
        _ => TimeBudget.Unlimited,
    };

    /// <summary>With a game clock the endgame search may use half of the time that is left (C++: calc_end_time).</summary>
    public static TimeBudget ForEndgame(SearchLimits limits, TimeBudget midgame, long elapsedMs)
    {
        if (limits.Mode != TimeControlMode.TimePerGame)
        {
            return midgame;
        }

        long rest = Milliseconds(limits.Time) - elapsedMs;
        if (rest < 0)
        {
            rest = 10;
        }

        long moveTime = rest / 2;
        return new TimeBudget(moveTime * 10 / 15, elapsedMs + moveTime);
    }

    // C++: calc_time. Early moves get about 1/4 of the average time per move, the last moves about all of it.
    private static TimeBudget ForGame(long remainingMs, int empties)
    {
        long slowEmpties = Math.Max(0, empties - FastEndgameEmpties);
        long movesLeft = slowEmpties >> 1;
        if (remainingMs < 0)
        {
            remainingMs = 10;
        }

        long moveTime = movesLeft > 0 ? remainingMs / movesLeft : remainingMs;
        moveTime = (moveTime >> 2) + 3 * moveTime * (64 - slowEmpties) / 256;
        return new TimeBudget(moveTime * 10 / 15, moveTime);
    }

    private static long Milliseconds(TimeSpan time) => (long)time.TotalMilliseconds;
}
