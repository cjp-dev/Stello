using System.Numerics;
using CommunityToolkit.Mvvm.ComponentModel;
using Stello.App.Models;
using Stello.Engine;

namespace Stello.App.ViewModels;

/// <summary>The settings dialog (C++: Spiltid, "Tid for et spil").</summary>
public sealed partial class SettingsViewModel : ObservableObject
{
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
        TranspositionTableEntries = settings.TranspositionTableEntries;
    }

    /// <summary>The most threads this device offers.</summary>
    public int DeviceThreads { get; }

    /// <summary>The threads that the Automatic choice uses here.</summary>
    public int DefaultThreads { get; }

    /// <summary>The Threads setting's choices: Automatic, then 1 to <see cref="DeviceThreads"/>.</summary>
    public IReadOnlyList<ThreadChoice> ThreadChoices { get; }

    public string ThreadsText => Threads <= GameSettings.AutomaticThreads ? $"Automatic ({Plural(DefaultThreads)})" : Plural(Threads);

    public int MaxDepth => GameSettings.MaxDepth;

    public int MaxSecondsPerMove => GameSettings.MaxSecondsPerMove;

    public int MaxMinutesPerGame => GameSettings.MaxMinutesPerGame;

    public int MinTranspositionTablePower => 16;
    public int MaxTranspositionTablePower => 24;

    public int TranspositionTablePower
    {
        get
        {
            int power = BitOperations.Log2((uint)Math.Max(GameSettings.MinTranspositionTableEntries, TranspositionTableEntries));
            return Math.Clamp(power, MinTranspositionTablePower, MaxTranspositionTablePower);
        }
        set
        {
            int power = Math.Clamp(value, MinTranspositionTablePower, MaxTranspositionTablePower);
            TranspositionTableEntries = 1 << power;
            OnPropertyChanged(nameof(TranspositionTablePower));
            OnPropertyChanged(nameof(TranspositionTableEntriesText));
        }
    }

    public string TranspositionTableEntriesText => TranspositionTableEntries switch
    {
        <= 65_536 => "65,536 (4 MB)",
        <= 262_144 => "262,144 (16 MB)",
        <= 1_048_576 => "1,048,576 (64 MB) - Default",
        <= 4_194_304 => "4,194,304 (256 MB)",
        _ => "16,777,216 (1,024 MB)",
    };

    partial void OnTranspositionTableEntriesChanged(int value)
    {
        OnPropertyChanged(nameof(TranspositionTablePower));
        OnPropertyChanged(nameof(TranspositionTableEntriesText));
    }

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
