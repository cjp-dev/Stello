using System.Numerics;
using System.Text.Json.Serialization;
using Stello.Engine;

namespace Stello.App.Models;

/// <summary>How long the computer may think (C++: tid_kontrol with lookahead, tider[] and GameTid).</summary>
public sealed record GameSettings(
    TimeControlMode Mode,
    int Depth,
    int SecondsPerMove,
    int MinutesPerGame,
    bool UseOpeningBook = true,
    int TranspositionTableEntries = 1_048_576)
{
    public const int MaxDepth = 20;
    public const int MaxSecondsPerMove = 60;
    public const int MaxMinutesPerGame = 60;

    public const int DefaultTranspositionTableEntries = 1_048_576;
    public const int MinTranspositionTableEntries = 65_536;
    public const int MaxTranspositionTableEntries = 16_777_216;

    // C++ defaults without rev.cfg: 5 minutes for the game, level 8.
    public static GameSettings Default { get; } = new(TimeControlMode.TimePerGame, 8, 5, 5, UseOpeningBook: true, TranspositionTableEntries: DefaultTranspositionTableEntries);

    [JsonIgnore]
    public TimeSpan GameTime => TimeSpan.FromMinutes(MinutesPerGame);

    [JsonIgnore]
    public int HashBits => Math.Clamp(BitOperations.Log2((uint)Math.Max(MinTranspositionTableEntries, TranspositionTableEntries)) - 1, 10, 26);

    public string Summary => Mode switch
    {
        TimeControlMode.FixedDepth => $"{Depth} plies",
        TimeControlMode.TimePerMove => $"{SecondsPerMove}s / move",
        _ => $"{MinutesPerGame} min / game",
    };

    public string VariantBadgeText => "Othello";

    /// <summary>The same settings with every value in its allowed range.</summary>
    public GameSettings Normalize()
    {
        int entries = TranspositionTableEntries >= MinTranspositionTableEntries && TranspositionTableEntries <= MaxTranspositionTableEntries
            ? (int)BitOperations.RoundUpToPowerOf2((uint)TranspositionTableEntries)
            : DefaultTranspositionTableEntries;

        return new(
            Mode is TimeControlMode.FixedDepth or TimeControlMode.TimePerMove or TimeControlMode.TimePerGame ? Mode : Default.Mode,
            Math.Clamp(Depth, 1, MaxDepth),
            Math.Clamp(SecondsPerMove, 1, MaxSecondsPerMove),
            Math.Clamp(MinutesPerGame, 1, MaxMinutesPerGame),
            UseOpeningBook,
            entries);
    }

    public SearchLimits ToLimits(TimeSpan computerTimeLeft) => Mode switch
    {
        TimeControlMode.FixedDepth => SearchLimits.FixedDepth(Depth, UseOpeningBook, HashBits),
        TimeControlMode.TimePerMove => SearchLimits.TimePerMove(TimeSpan.FromSeconds(SecondsPerMove), UseOpeningBook, HashBits),
        _ => SearchLimits.TimePerGame(computerTimeLeft, UseOpeningBook, HashBits),
    };
}
