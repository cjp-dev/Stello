using System.Diagnostics;
using System.IO;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Stello.Engine;
using Stello.Net.Models;
using Stello.Net.Services;

namespace Stello.Net.ViewModels;

/// <summary>
/// The game window: a human plays against the computer. The computer thinks on a background thread; commands
/// that change the game stop it first (C++: CMainFrame and CStelloView).
/// </summary>
public sealed partial class MainViewModel : ObservableObject
{
    private readonly ComputerPlayer _computer;
    private readonly IDialogService _dialogs;

    // Computer clock before its move at each ply, so taking back moves also gives the time back (C++: timesleft).
    private readonly Dictionary<int, TimeSpan> _timeLeftAtPly = [];

    private Game _game = new();
    private string? _filePath;
    private string? _notice;
    private TimeSpan _computerTimeLeft;
    private CancellationTokenSource? _cancel;
    private CancellationTokenSource? _moveNow;
    private int _searchId;

    [ObservableProperty]
    private string _status = "";

    [ObservableProperty]
    private int _blackCount;

    [ObservableProperty]
    private int _whiteCount;

    [ObservableProperty]
    private string _sidesText = "";

    [ObservableProperty]
    private string _clockText = "";

    [ObservableProperty]
    private string _title = "";

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(MoveNowCommand))]
    private bool _isThinking;

    [ObservableProperty]
    private bool _isAnalysisVisible = true;

    [ObservableProperty]
    private GameSettings _settings = GameSettings.Default;

    public MainViewModel(ComputerPlayer computer, IDialogService dialogs, GameSettings settings, string? startupNotice = null)
    {
        _computer = computer;
        _dialogs = dialogs;
        Settings = settings;
        _computerTimeLeft = settings.GameTime;
        _notice = startupNotice;
        Squares = Enumerable.Range(0, 64).Select(i => new SquareViewModel(new Square(i))).ToArray();
        Start();
    }

    public IReadOnlyList<SquareViewModel> Squares { get; }

    public AnalysisViewModel Analysis { get; } = new();

    /// <summary>Completes when the computer has moved and it is the human's turn or the game is over.</summary>
    internal Task Idle { get; private set; } = Task.CompletedTask;

    internal Game Game => _game;

    /// <summary>Stops the computer without waiting, e.g. when the window closes.</summary>
    public void Stop() => _cancel?.Cancel();

    [RelayCommand]
    private void Play(SquareViewModel square)
    {
        if (IsThinking || _game.IsGameOver || !_game.IsHumanToMove || !_game.Board.IsLegal(_game.ToMove, square.Square))
        {
            _dialogs.Beep();
            return;
        }

        _game.Play(square.Square);
        Start();
    }

    [RelayCommand]
    private async Task NewGame()
    {
        await StopAsync();
        _game.NewGame();
        _filePath = null;
        ResetClock();
        _computer.BookTracker.Reset();
        Analysis.Clear();
        Start();
    }

    [RelayCommand]
    private async Task Open()
    {
        string? path = _dialogs.ShowOpenDialog();
        if (path is null)
        {
            return;
        }

        Game loaded;
        try
        {
            loaded = GameRecordFormat.Parse(await File.ReadAllTextAsync(path));
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or FormatException)
        {
            _dialogs.ShowError($"The game could not be opened.\n\n{exception.Message}");
            return;
        }

        await StopAsync();

        // Like Forward/Back in C++, the human continues with the side to move.
        if (!loaded.IsHumanToMove)
        {
            loaded.SwitchSides();
        }

        _game = loaded;
        _filePath = path;
        ResetClock();
        _computer.BookTracker.Reset();
        Analysis.Clear();
        Start();
    }

    [RelayCommand]
    private void Save()
    {
        if (_filePath is null)
        {
            SaveAs();
        }
        else
        {
            Write(_filePath);
        }
    }

    [RelayCommand]
    private void SaveAs()
    {
        string? path = _dialogs.ShowSaveDialog(_filePath);
        if (path is not null)
        {
            Write(path);
        }
    }

    // C++: OnSkiftSide; the computer moves at once.
    [RelayCommand]
    private async Task SwitchSides()
    {
        await StopAsync();
        _game.SwitchSides();
        Start();
    }

    [RelayCommand(CanExecute = nameof(IsThinking))]
    private void MoveNow() => _moveNow?.Cancel();

    // Back to the previous position where the human is to move (C++: Tilbage).
    [RelayCommand(CanExecute = nameof(CanUndo))]
    private async Task Undo()
    {
        await StopAsync();
        do
        {
            _game.Undo();
        }
        while (_game.CanUndo && !_game.IsHumanToMove);

        RestoreClock();
        _computer.BookTracker.Reset();
        Start();
    }

    // Forward to the next position where the human is to move, or to the end (C++: Frem).
    [RelayCommand(CanExecute = nameof(CanRedo))]
    private async Task Redo()
    {
        await StopAsync();
        do
        {
            _game.Redo();
        }
        while (_game.CanRedo && !_game.IsHumanToMove);

        RestoreClock();
        Start();
    }

    [RelayCommand]
    private void EditSettings()
    {
        GameSettings? settings = _dialogs.EditSettings(Settings);
        if (settings is null)
        {
            return;
        }

        bool clockChanged = settings.Mode != Settings.Mode || settings.MinutesPerGame != Settings.MinutesPerGame;
        Settings = settings;
        if (clockChanged)
        {
            ResetClock();
        }

        Refresh();
    }

    [RelayCommand]
    private void ToggleAnalysis() => IsAnalysisVisible = !IsAnalysisVisible;

    [RelayCommand]
    private void About() => _dialogs.ShowAbout();

    private bool CanUndo() => _game.CanUndo;

    private bool CanRedo() => _game.CanRedo;

    private void Start() => Idle = RunAsync();

    private async Task StopAsync()
    {
        _cancel?.Cancel();
        await Idle;
    }

    // Plays passes and computer moves until the human is to move or the game is over.
    private async Task RunAsync()
    {
        try
        {
            while (true)
            {
                Refresh();
                if (_game.IsGameOver)
                {
                    Status = WithNotice(GameOverText());
                    return;
                }

                if (_game.MustPass)
                {
                    _notice = _game.IsHumanToMove
                        ? "You have no legal move and must pass."
                        : "The computer has no legal move and passes.";
                    _game.Pass();
                    continue;
                }

                if (_game.IsHumanToMove)
                {
                    Status = WithNotice("Your move.");
                    return;
                }

                Status = WithNotice("Thinking…");
                if (!await ComputerMoveAsync())
                {
                    Refresh();
                    Status = "Stopped.";
                    return;
                }
            }
        }
        catch (Exception exception) when (exception is not OperationCanceledException)
        {
            Status = "Error.";
            _dialogs.ShowError($"Something went wrong.\n\n{exception.Message}");
        }
    }

    private async Task<bool> ComputerMoveAsync()
    {
        using var cancel = new CancellationTokenSource();
        using var moveNow = new CancellationTokenSource();
        _cancel = cancel;
        _moveNow = moveNow;
        IsThinking = true;
        Analysis.Clear();

        Board board = _game.Board;
        Player player = _game.ToMove;
        _timeLeftAtPly[_game.Ply] = _computerTimeLeft;
        SearchLimits limits = Settings.ToLimits(_computerTimeLeft);

        int searchId = ++_searchId;
        var progress = new Progress<SearchInfo>(info =>
        {
            // Reports can arrive after the search has finished.
            if (searchId == _searchId && IsThinking)
            {
                Analysis.Update(info);
            }
        });

        var stopwatch = Stopwatch.StartNew();
        try
        {
            SearchResult result = await Task.Run(
                () => _computer.ChooseMove(board, player, limits, progress, cancel.Token, moveNow.Token),
                CancellationToken.None);

            _computerTimeLeft -= stopwatch.Elapsed;
            Analysis.Update(result, stopwatch.Elapsed);
            _game.Play(result.Move);
            return true;
        }
        catch (OperationCanceledException)
        {
            return false;
        }
        finally
        {
            _cancel = null;
            _moveNow = null;
            IsThinking = false;
        }
    }

    private void Refresh()
    {
        Board board = _game.Board;
        ulong legal = _game.IsHumanToMove && !_game.IsGameOver ? board.LegalMoves(_game.ToMove) : 0;
        Square? last = _game.LastMove?.Square;

        foreach (SquareViewModel square in Squares)
        {
            square.Disc = board[square.Square];
            square.IsLegalMove = (legal & square.Square.Bit) != 0;
            square.IsLastMove = square.Square == last;
        }

        BlackCount = board.Count(Player.Black);
        WhiteCount = board.Count(Player.White);
        SidesText = $"You play {_game.Human}";
        TimeSpan left = _computerTimeLeft > TimeSpan.Zero ? _computerTimeLeft : TimeSpan.Zero;
        ClockText = Settings.Mode == TimeControlMode.TimePerGame
            ? $"Computer time left: {(int)left.TotalMinutes}:{left.Seconds:00}"
            : "";
        Title = $"Stello – {(_filePath is null ? "Untitled" : Path.GetFileName(_filePath))}";
        UndoCommand.NotifyCanExecuteChanged();
        RedoCommand.NotifyCanExecuteChanged();
    }

    private string GameOverText()
    {
        string score = $"Black {BlackCount} – White {WhiteCount}";
        return _game.Winner switch
        {
            null => $"Game over, a draw: {score}.",
            Player winner when winner == _game.Human => $"Game over, you win: {score}.",
            _ => $"Game over, the computer wins: {score}.",
        };
    }

    private string WithNotice(string status)
    {
        string text = _notice is null ? status : $"{_notice} {status}";
        _notice = null;
        return text;
    }

    private void ResetClock()
    {
        _computerTimeLeft = Settings.GameTime;
        _timeLeftAtPly.Clear();
    }

    private void RestoreClock()
    {
        int next = _timeLeftAtPly.Keys.Where(ply => ply >= _game.Ply).DefaultIfEmpty(-1).Min();
        if (next >= 0)
        {
            _computerTimeLeft = _timeLeftAtPly[next];
        }
    }

    private void Write(string path)
    {
        try
        {
            File.WriteAllText(path, GameRecordFormat.Format(_game) + Environment.NewLine);
            _filePath = path;
            Refresh();
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            _dialogs.ShowError($"The game could not be saved.\n\n{exception.Message}");
        }
    }
}
