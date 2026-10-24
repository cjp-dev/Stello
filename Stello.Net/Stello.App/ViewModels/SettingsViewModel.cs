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

    public SettingsViewModel(GameSettings settings)
    {
        Mode = settings.Mode;
        Depth = settings.Depth;
        SecondsPerMove = settings.SecondsPerMove;
        MinutesPerGame = settings.MinutesPerGame;
        UseOpeningBook = settings.UseOpeningBook;
        TranspositionTableEntries = settings.TranspositionTableEntries;
    }

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
        <= 65_536 => "65,536 (3 MB)",
        <= 262_144 => "262,144 (12 MB)",
        <= 1_048_576 => "1,048,576 (48 MB) - Default",
        <= 4_194_304 => "4,194,304 (192 MB)",
        _ => "16,777,216 (768 MB)",
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
        TranspositionTableEntries).Normalize();

    private void SelectMode(bool selected, TimeControlMode mode)
    {
        if (selected)
        {
            Mode = mode;
        }
    }
}
