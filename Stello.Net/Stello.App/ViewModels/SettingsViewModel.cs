using CommunityToolkit.Mvvm.ComponentModel;
using Stello.App.Models;
using Stello.Engine;

namespace Stello.App.ViewModels;

/// <summary>The settings dialog (C++: Spiltid, "Tid for et spil").</summary>
public sealed partial class SettingsViewModel : ObservableObject
{
    private static readonly IReadOnlyList<TranspositionTableChoice> SharedTranspositionTableChoices = Array.AsReadOnly(
        new TranspositionTableChoice[]
        {
            new(65_536, "65,536 entries (4 MB)"),
            new(262_144, "262,144 entries (16 MB)"),
            new(1_048_576, "1,048,576 entries (64 MB) - Default"),
            new(4_194_304, "4,194,304 entries (256 MB)"),
            new(16_777_216, "16,777,216 entries (1,024 MB)"),
        });

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(IsFixedDepth), nameof(IsTimePerMove), nameof(IsTimePerGame))]
    private TimeControlMode _mode;

    [ObservableProperty]
    private int _depth;

    [ObservableProperty]
    private int _secondsPerMove;

    [ObservableProperty]
    private int _minutesPerGame;

    [ObservableProperty]
    private bool _useOpeningBook;

    [ObservableProperty]
    private int _transpositionTableEntries;

    /// <summary>The Threads setting: 1 to <see cref="DeviceThreads"/>, or <see cref="GameSettings.AutomaticThreads"/>.</summary>
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ThreadsText))]
    private int _threads;

    /// <param name="threads">This device's thread counts (default: this process's, as on the desktop).</param>
    public SettingsViewModel(GameSettings settings, ThreadCounts? threads = null)
    {
        ThreadCounts counts = threads ?? ThreadCounts.ForThisProcess();
        DeviceThreads = counts.Device;
        DefaultThreads = counts.Default;
        Threads = settings.Threads <= GameSettings.AutomaticThreads ? GameSettings.AutomaticThreads : Math.Min(settings.Threads, DeviceThreads);
        ThreadChoices = [new(GameSettings.AutomaticThreads, $"Automatic ({Plural(DefaultThreads)})"),
            .. Enumerable.Range(1, DeviceThreads).Select(n => new ThreadChoice(n, Plural(n)))];
        Mode = settings.Mode;
        Depth = settings.Depth;
        SecondsPerMove = settings.SecondsPerMove;
        MinutesPerGame = settings.MinutesPerGame;
        UseOpeningBook = settings.UseOpeningBook;
        TranspositionTableEntries = NearestTranspositionTableChoice(settings.TranspositionTableEntries);
    }

    // A saved capacity that is not one of the choices (an odd power of two) would leave the selection empty.
    private static int NearestTranspositionTableChoice(int entries) =>
        SharedTranspositionTableChoices.LastOrDefault(choice => choice.Value <= entries)?.Value
        ?? SharedTranspositionTableChoices[0].Value;

    /// <summary>The most threads this device offers.</summary>
    public int DeviceThreads { get; }

    /// <summary>The threads that the Automatic choice uses here.</summary>
    public int DefaultThreads { get; }

    /// <summary>The Threads setting's choices: Automatic, then 1 to <see cref="DeviceThreads"/>.</summary>
    public IReadOnlyList<ThreadChoice> ThreadChoices { get; }

    /// <summary>The transposition-table capacities offered by both apps.</summary>
    public IReadOnlyList<TranspositionTableChoice> TranspositionTableChoices => SharedTranspositionTableChoices;

    public string ThreadsText => Threads <= GameSettings.AutomaticThreads ? $"Automatic ({Plural(DefaultThreads)})" : Plural(Threads);

    public int MaxDepth => GameSettings.MaxDepth;

    public int MaxSecondsPerMove => GameSettings.MaxSecondsPerMove;

    public int MaxMinutesPerGame => GameSettings.MaxMinutesPerGame;

    public bool IsFixedDepth
    {
        get => Mode == TimeControlMode.FixedDepth;
        set => SelectMode(value, TimeControlMode.FixedDepth);
    }

    public bool IsTimePerMove
    {
        get => Mode == TimeControlMode.TimePerMove;
        set => SelectMode(value, TimeControlMode.TimePerMove);
    }

    public bool IsTimePerGame
    {
        get => Mode == TimeControlMode.TimePerGame;
        set => SelectMode(value, TimeControlMode.TimePerGame);
    }

    public GameSettings ToSettings() => new GameSettings(
        Mode,
        Depth,
        SecondsPerMove,
        MinutesPerGame,
        UseOpeningBook,
        TranspositionTableEntries,
        Threads).Normalize();

    private static string Plural(int threads) => threads == 1 ? "1 thread" : $"{threads} threads";

    private void SelectMode(bool selected, TimeControlMode mode)
    {
        if (selected)
        {
            Mode = mode;
        }
    }
}

/// <summary>One choice of the Threads setting: its value (<see cref="GameSettings.AutomaticThreads"/> or a count) and text.</summary>
public sealed record ThreadChoice(int Value, string Text);

/// <summary>One transposition-table capacity and its display text.</summary>
public sealed record TranspositionTableChoice(int Value, string Text);
