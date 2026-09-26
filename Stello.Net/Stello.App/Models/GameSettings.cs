using System.Text.Json.Serialization;
using Stello.Engine;

namespace Stello.App.Models;

/// <summary>How long the computer may think (C++: tid_kontrol with lookahead, tider[] and GameTid).</summary>
public sealed record GameSettings(TimeControlMode Mode, int Depth, int SecondsPerMove, int MinutesPerGame)
{
    public const int MaxDepth = 20;
    public const int MaxSecondsPerMove = 60;
    public const int MaxMinutesPerGame = 60;

    // C++ defaults without rev.cfg: 5 minutes for the game, level 8.
    public static GameSettings Default { get; } = new(TimeControlMode.TimePerGame, 8, 5, 5);

    [JsonIgnore]
    public TimeSpan GameTime => TimeSpan.FromMinutes(MinutesPerGame);

    /// <summary>The same settings with every value in its allowed range.</summary>
    public GameSettings Normalize() => new(
        Mode is TimeControlMode.FixedDepth or TimeControlMode.TimePerMove or TimeControlMode.TimePerGame ? Mode : Default.Mode,
        Math.Clamp(Depth, 1, MaxDepth),
        Math.Clamp(SecondsPerMove, 1, MaxSecondsPerMove),
        Math.Clamp(MinutesPerGame, 1, MaxMinutesPerGame));

    public SearchLimits ToLimits(TimeSpan computerTimeLeft) => Mode switch
    {
        TimeControlMode.FixedDepth => SearchLimits.FixedDepth(Depth),
        TimeControlMode.TimePerMove => SearchLimits.TimePerMove(TimeSpan.FromSeconds(SecondsPerMove)),
        _ => SearchLimits.TimePerGame(computerTimeLeft),
    };
}
