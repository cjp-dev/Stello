using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Numerics;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Stello.App.Models;
using Stello.App.Services;
using Stello.Engine;

namespace Stello.App.ViewModels;

public sealed record MoveHistoryItem(int Ply, string Notation);

/// <summary>
/// The game window: coordinates game modes, AI search, clocks, board state, move history and sounds.
/// </summary>
public sealed partial class MainViewModel : ObservableObject, IDisposable
{
    private readonly IEngineHost _engine;
    private readonly IDialogService _dialogs;
    private readonly IGameFileService _files;
    private readonly ISettingsStore _settingsStore;
    private readonly ISoundService _soundService;
    private readonly bool _settingsLoaded;

    // Clock history before each ply so undo restores consumed time.
    private readonly Dictionary<int, (TimeSpan Black, TimeSpan White)> _timeLeftAtPly = [];

    private Game _game = new();
    private string? _fileName;
    private string? _notice;
    private TimeSpan _blackTimeLeft;
    private TimeSpan _whiteTimeLeft;
    private long _lastTickTimestamp;
    private readonly Timer _clockTimer;
    private readonly SynchronizationContext? _syncContext;
    private bool _hasHumanContinuedOnTime;
    private bool _isTimeoutPrompting;

    private CancellationTokenSource? _cancel;
    private CancellationTokenSource? _moveNow;
    private CancellationTokenSource? _learningCancel;
    private int _searchId;

    [ObservableProperty]
    private string _status = "";

    [ObservableProperty]
    private string _statusText = "";

    [ObservableProperty]
    private int _blackCount;

    [ObservableProperty]
    private int _whiteCount;

    [ObservableProperty]
    private int _blackMobility;

    [ObservableProperty]
    private int _whiteMobility;

    [ObservableProperty]
    private string _sidesText = "";

    [ObservableProperty]
    private string _clockText = "";

    [ObservableProperty]
    private string _title = "";

    [ObservableProperty]
    private GameMode _gameMode = GameMode.HumanVsComputer;

    [ObservableProperty]
    private bool _isGameModified;

    [ObservableProperty]
    private bool _isSoundOn = true;

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(MoveNowCommand))]
    [NotifyCanExecuteChangedFor(nameof(SwitchSidesCommand))]
    private bool _isThinking;

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(
        nameof(NewGameCommand), nameof(OpenCommand), nameof(SwitchSidesCommand), nameof(UndoCommand), nameof(RedoCommand),
        nameof(EditSettingsCommand), nameof(AddGameToBookCommand), nameof(EvaluateBookCommand), nameof(SelfPlayCommand),
        nameof(StopLearningCommand))]
    private bool _isLearning;

    [ObservableProperty]
    private bool _isAnalysisVisible = true;

    [ObservableProperty]
    private GameSettings _settings = GameSettings.Default;

    public ObservableCollection<MoveHistoryItem> MoveHistoryList { get; } = [];

    public IReadOnlyList<SquareViewModel> Squares { get; }

    public AnalysisViewModel Analysis { get; } = new();

    public WindowPlacement? WindowPlacement { get; private set; }

    internal Task Idle { get; private set; } = Task.CompletedTask;

    internal Task Learning { get; private set; } = Task.CompletedTask;

    internal Game Game => _game;

    public bool IsHumanVsComputer => GameMode == GameMode.HumanVsComputer;
    public bool IsComputerVsHuman => GameMode == GameMode.ComputerVsHuman;
    public bool IsHumanVsHuman => GameMode == GameMode.HumanVsHuman;
    public bool IsVsComputer => GameMode is GameMode.HumanVsComputer or GameMode.ComputerVsHuman or GameMode.ComputerVsComputer;

    public bool IsStatusModified => IsVsComputer && IsGameModified;

    public bool IsGameSettingVisible => IsVsComputer;
    public bool IsClockVisible => Settings.Mode == TimeControlMode.TimePerGame;

    public string SettingsBadgeText => Settings.Summary;
    public string VariantBadgeText => Settings.VariantBadgeText;

    public string BlackPlayerLabel => GameMode switch
    {
        GameMode.HumanVsComputer => "Human",
        GameMode.ComputerVsHuman => "Computer",
        _ => "Player 1"
    };

    public string WhitePlayerLabel => GameMode switch
    {
        GameMode.HumanVsComputer => "Computer",
        GameMode.ComputerVsHuman => "Human",
        _ => "Player 2"
    };

    public string BlackMobilityText => $"Moves: {BlackMobility}";
    public string WhiteMobilityText => $"Moves: {WhiteMobility}";

    public string BlackClockText => FormatClock(_blackTimeLeft);
    public string WhiteClockText => FormatClock(_whiteTimeLeft);

    public string MoveCountText => $"{_game.Ply} {(_game.Ply == 1 ? "ply" : "plies")}";

    public TimeSpan ComputerTimeLeft => GameMode switch
    {
        GameMode.HumanVsComputer => _whiteTimeLeft,
        GameMode.ComputerVsHuman => _blackTimeLeft,
        _ => TimeSpan.Zero
    };

    public bool CanSwitchSides =>
        IsVsComputer &&
        !IsThinking &&
        !IsLearning &&
        !_game.IsGameOver &&
        IsHumanTurn();

    public bool CanUndo => _game.CanUndo && !IsLearning && !IsThinking;
    public bool CanRedo => _game.CanRedo && !IsLearning && !IsThinking;
    public Player ToMove => _game.ToMove;
    public bool IsGameOver => _game.IsGameOver;

    public MainViewModel(
        IEngineHost engine,
        IDialogService dialogs,
        IGameFileService files,
        ISettingsStore settingsStore,
        string? startupNotice = null,
        ISoundService? soundService = null)
    {
        _engine = engine;
        _dialogs = dialogs;
        _files = files;
        _settingsStore = settingsStore;
        _soundService = soundService ?? NullSoundService.Instance;

        AppSettings saved = settingsStore.Load();
        Settings = saved.Game;
        IsAnalysisVisible = saved.ShowAnalysis;
        WindowPlacement = saved.Window;
        _settingsLoaded = true;

        _blackTimeLeft = Settings.GameTime;
        _whiteTimeLeft = Settings.GameTime;
        _syncContext = SynchronizationContext.Current;
        _clockTimer = new Timer(OnClockTimerTick, null, 250, 250);

        _notice = startupNotice;
        Squares = Enumerable.Range(0, 64).Select(i => new SquareViewModel(new Square(i))).ToArray();
        Start();
    }

    partial void OnGameModeChanged(GameMode value)
    {
        OnPropertyChanged(nameof(IsHumanVsComputer));
        OnPropertyChanged(nameof(IsComputerVsHuman));
        OnPropertyChanged(nameof(IsHumanVsHuman));
        OnPropertyChanged(nameof(IsVsComputer));
        OnPropertyChanged(nameof(IsStatusModified));
        OnPropertyChanged(nameof(IsGameSettingVisible));
        OnPropertyChanged(nameof(BlackPlayerLabel));
        OnPropertyChanged(nameof(WhitePlayerLabel));
        SwitchSidesCommand.NotifyCanExecuteChanged();
    }

    partial void OnIsGameModifiedChanged(bool value)
    {
        OnPropertyChanged(nameof(IsStatusModified));
    }

    partial void OnIsAnalysisVisibleChanged(bool value) => SaveSettings();

    partial void OnIsSoundOnChanged(bool value)
    {
        _soundService.IsEnabled = value;
    }

    [RelayCommand]
    public void ToggleSound()
    {
        IsSoundOn = !IsSoundOn;
    }

    public async Task SetGameMode(GameMode mode)
    {
        if (GameMode == mode)
        {
            return;
        }

        if (_game.Ply > 0 && !_game.IsGameOver)
        {
            if (!await _dialogs.ConfirmAsync("Abandon current match and start a new game?"))
            {
                return;
            }
        }

        await StopAsync();
        GameMode = mode;
        Player human = mode == GameMode.ComputerVsHuman ? Player.White : Player.Black;
        _game = new Game(human);
        _fileName = null;
        IsGameModified = false;
        ResetClock();
        _engine.ResetBookTracker();
        Analysis.Clear();
        Start();
    }

    public void Stop()
    {
        _cancel?.Cancel();
        _learningCancel?.Cancel();
    }

    public void Dispose()
    {
        _clockTimer.Dispose();
        Stop();
    }

    public void SaveWindowPlacement(WindowPlacement placement)
    {
        WindowPlacement = placement;
        SaveSettings();
    }

    private void SaveSettings()
    {
        if (_settingsLoaded && !_settingsStore.Save(new AppSettings(Settings, IsAnalysisVisible, WindowPlacement)))
        {
            Status = $"The settings could not be saved. {Status}";
        }
    }

    private bool IsHumanTurn() =>
        GameMode == GameMode.HumanVsHuman || _game.IsHumanToMove;

    [RelayCommand]
    private void Play(SquareViewModel square)
    {
        if (IsThinking || IsLearning || _game.IsGameOver || !IsHumanTurn() || !_game.Board.IsLegal(_game.ToMove, square.Square))
        {
            _dialogs.Beep();
            return;
        }

        _soundService.Play(SoundType.Move);
        _game.Play(square.Square);
        Start();
    }

    [RelayCommand(CanExecute = nameof(CanChangeGame))]
    private async Task NewGame()
    {
        if (_game.Ply > 0 && !_game.IsGameOver)
        {
            if (!await _dialogs.ConfirmAsync("Abandon current match and start a new game?"))
            {
                return;
            }
        }

        await StartNewGameAsync();
    }

    private async Task StartNewGameAsync()
    {
        await StopAsync();
        Player human = GameMode == GameMode.ComputerVsHuman ? Player.White : Player.Black;
        _game = new Game(human);
        _fileName = null;
        IsGameModified = false;
        ResetClock();
        _engine.ResetBookTracker();
        Analysis.Clear();
        Start();
    }

    [RelayCommand(CanExecute = nameof(CanChangeGame))]
    private async Task Open()
    {
        GameFile? file;
        Game loaded;
        try
        {
            file = await _files.OpenAsync();
            if (file is null)
            {
                return;
            }

            loaded = GameRecordFormat.Parse(file.Text);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or FormatException)
        {
            _dialogs.ShowError($"The game could not be opened.\n\n{exception.Message}");
            return;
        }

        await StopAsync();

        if (GameMode != GameMode.HumanVsHuman && !loaded.IsHumanToMove)
        {
            loaded.SwitchSides();
        }

        _game = loaded;
        _fileName = file.Name;
        if (GameMode != GameMode.HumanVsHuman)
        {
            GameMode = loaded.Human == Player.Black ? GameMode.HumanVsComputer : GameMode.ComputerVsHuman;
        }
        ResetClock();
        _engine.ResetBookTracker();
        Analysis.Clear();
        Start();
    }

    [RelayCommand]
    private Task Save() => WriteAsync(askForName: false);

    [RelayCommand]
    private Task SaveAs() => WriteAsync(askForName: true);

    [RelayCommand(CanExecute = nameof(CanSwitchSides))]
    private async Task SwitchSides()
    {
        if (!CanSwitchSides)
        {
            return;
        }

        await StopAsync();
        GameMode = GameMode == GameMode.HumanVsComputer
            ? GameMode.ComputerVsHuman
            : GameMode.HumanVsComputer;

        _game.SwitchSides();
        IsGameModified = true;
        Start();
    }

    [RelayCommand(CanExecute = nameof(IsThinking))]
    private void MoveNow() => _moveNow?.Cancel();

    [RelayCommand(CanExecute = nameof(CanUndo))]
    private async Task Undo()
    {
        await StopAsync();
        if (IsVsComputer)
        {
            do
            {
                _game.Undo();
            }
            while (_game.CanUndo && !IsHumanTurn());
            IsGameModified = true;
        }
        else
        {
            _game.Undo();
        }

        RestoreClock();
        _engine.ResetBookTracker();
        Start();
    }

    [RelayCommand(CanExecute = nameof(CanRedo))]
    private async Task Redo()
    {
        await StopAsync();
        if (IsVsComputer)
        {
            do
            {
                _game.Redo();
            }
            while (_game.CanRedo && !IsHumanTurn());
        }
        else
        {
            _game.Redo();
        }

        RestoreClock();
        Start();
    }

    [RelayCommand(CanExecute = nameof(CanChangeGame))]
    private async Task EditSettings()
    {
        GameSettings? settings = await _dialogs.EditSettingsAsync(Settings);
        if (settings is null)
        {
            return;
        }

        bool clockChanged = settings.Mode != Settings.Mode || settings.MinutesPerGame != Settings.MinutesPerGame;
        if (_game.Ply > 0 && settings != Settings)
        {
            IsGameModified = true;
        }

        Settings = settings;
        SaveSettings();
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

    [RelayCommand(CanExecute = nameof(CanChangeGame))]
    private async Task AddGameToBook()
    {
        if (_game.Ply < 2)
        {
            _dialogs.ShowError("Play at least two moves before adding the game to the opening book.");
            return;
        }

        if (!await _dialogs.ConfirmAsync("Add this game to the opening book?"))
        {
            return;
        }

        GameResult? result = _game.IsGameOver
            ? _game.Winner switch
            {
                Player.Black => GameResult.BlackWins,
                Player.White => GameResult.WhiteWins,
                _ => GameResult.Draw,
            }
            : await _dialogs.AskGameResultAsync();
        if (result is null)
        {
            return;
        }

        await StopAsync();
        BookUpdate update = await _engine.AddGameToBookAsync(_game.PlayedMoves.ToList(), result.Value);
        _notice = update.Saved
            ? $"The game was added to the opening book ({update.NodeCount:N0} positions)."
            : "The opening book could not be saved.";
        Start();
    }

    [RelayCommand(CanExecute = nameof(CanLearn))]
    private async Task EvaluateBook()
    {
        if (await _dialogs.ConfirmAsync(
            "Search every new position in the opening book and add the best moves that are not in it?\n\n" +
            "This can take a long time. You can stop at any time with Book > Stop Learning."))
        {
            await LearnAsync(BookLearningKind.EvaluateBook);
        }
    }

    [RelayCommand(CanExecute = nameof(CanLearn))]
    private async Task SelfPlay()
    {
        if (await _dialogs.ConfirmAsync(
            "Let the computer play against itself and learn the games into the opening book?\n\n" +
            "This runs until you stop it with Book > Stop Learning."))
        {
            await LearnAsync(BookLearningKind.SelfPlay);
        }
    }

    [RelayCommand(CanExecute = nameof(IsLearning))]
    private void StopLearning() => _learningCancel?.Cancel();

    private bool CanChangeGame() => !IsLearning;

    private bool CanLearn() => !IsLearning && _engine.SupportsLearning;

    private async Task LearnAsync(BookLearningKind kind)
    {
        await StopAsync();
        using var cancel = new CancellationTokenSource();
        _learningCancel = cancel;
        IsLearning = true;
        Status = "Book learning…";

        var progress = new Progress<BookLearningProgress>(p =>
        {
            if (IsLearning)
            {
                Status = Describe(p);
            }
        });

        BookLearningSummary summary;
        try
        {
            Task<BookLearningSummary> learning = _engine.LearnAsync(kind, LearningLimits(), progress, cancel.Token);
            Learning = learning;
            summary = await learning;
        }
        finally
        {
            _learningCancel = null;
            IsLearning = false;
        }

        if (summary.Outcome == BookLearningOutcome.Failed)
        {
            _dialogs.ShowError($"Book learning failed.\n\n{summary.Error}");
        }

        string outcome = summary.Outcome switch
        {
            BookLearningOutcome.Finished => "Book learning finished.",
            BookLearningOutcome.Stopped => "Book learning stopped.",
            _ => "Book learning failed.",
        };
        _engine.ResetBookTracker();
        _notice = $"{outcome} {summary.PositionsEvaluated:N0} positions searched, {summary.GamesPlayed:N0} games played, " +
            $"{summary.NodeCount:N0} positions in the book.{(summary.Saved ? "" : " The opening book could not be saved.")}";
        Start();
    }

    private SearchLimits LearningLimits() => Settings.Mode == TimeControlMode.FixedDepth
        ? SearchLimits.FixedDepth(Settings.Depth)
        : SearchLimits.TimePerMove(TimeSpan.FromSeconds(Settings.SecondsPerMove));

    private static string Describe(BookLearningProgress progress) => progress.Stage switch
    {
        BookLearningStage.PlayingGame => $"Book learning: playing game {progress.GamesPlayed + 1:N0}…",
        _ => $"Book learning: {progress.PositionsEvaluated:N0} positions searched, {progress.NodeCount:N0} positions in the book…",
    };

    private void Start() => Idle = RunAsync();

    private async Task StopAsync()
    {
        _cancel?.Cancel();
        await Idle;
    }

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
                    StatusText = Status;
                    PlayGameOverSound();
                    return;
                }

                if (_game.MustPass)
                {
                    _soundService.Play(SoundType.Pass);
                    _notice = IsHumanTurn()
                        ? "You have no legal move and must pass."
                        : "The computer has no legal move and passes.";
                    _game.Pass();
                    continue;
                }

                if (IsHumanTurn())
                {
                    Status = WithNotice("Your move.");
                    StatusText = $"{_game.ToMove}'s Turn — Click a square to place a disc";
                    return;
                }

                Status = WithNotice("Thinking…");
                StatusText = "Thinking…";
                if (!await ComputerMoveAsync())
                {
                    Refresh();
                    Status = "Stopped.";
                    StatusText = "Stopped.";
                    return;
                }
            }
        }
        catch (Exception exception) when (exception is not OperationCanceledException)
        {
            Status = "Error.";
            StatusText = "Error.";
            _dialogs.ShowError($"Something went wrong.\n\n{exception.Message}");
        }
    }

    private void PlayGameOverSound()
    {
        if (IsVsComputer)
        {
            if (_game.Winner == _game.Human)
            {
                _soundService.Play(SoundType.Win);
            }
            else if (_game.Winner != null)
            {
                _soundService.Play(SoundType.Loss);
            }
        }
        else if (_game.Winner != null)
        {
            _soundService.Play(SoundType.Win);
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
        _timeLeftAtPly[_game.Ply] = (_blackTimeLeft, _whiteTimeLeft);
        TimeSpan computerTime = player == Player.Black ? _blackTimeLeft : _whiteTimeLeft;
        SearchLimits limits = Settings.ToLimits(computerTime);

        int searchId = ++_searchId;
        var progress = new Progress<SearchInfo>(info =>
        {
            if (searchId == _searchId && IsThinking)
            {
                Analysis.Update(info);
            }
        });

        var stopwatch = Stopwatch.StartNew();
        try
        {
            SearchResult result = await _engine.ChooseMoveAsync(board, player, limits, progress, cancel.Token, moveNow.Token);

            TimeSpan elapsed = stopwatch.Elapsed;
            if (player == Player.Black)
            {
                _blackTimeLeft = _blackTimeLeft > elapsed ? _blackTimeLeft - elapsed : TimeSpan.Zero;
            }
            else
            {
                _whiteTimeLeft = _whiteTimeLeft > elapsed ? _whiteTimeLeft - elapsed : TimeSpan.Zero;
            }

            Analysis.Update(result, elapsed);
            _soundService.Play(SoundType.Move);
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
        bool isHumanTurn = IsHumanTurn() && !_game.IsGameOver;
        ulong legal = isHumanTurn ? board.LegalMoves(_game.ToMove) : 0;
        Square? last = _game.LastMove?.Square;

        foreach (SquareViewModel square in Squares)
        {
            square.Disc = board[square.Square];
            square.IsLegalMove = (legal & square.Square.Bit) != 0;
            square.IsLastMove = square.Square == last;
        }

        BlackCount = board.Count(Player.Black);
        WhiteCount = board.Count(Player.White);

        BlackMobility = BitOperations.PopCount(board.LegalMoves(Player.Black));
        WhiteMobility = BitOperations.PopCount(board.LegalMoves(Player.White));

        SidesText = $"You play {_game.Human}";
        TimeSpan left = ComputerTimeLeft > TimeSpan.Zero ? ComputerTimeLeft : TimeSpan.Zero;
        ClockText = Settings.Mode == TimeControlMode.TimePerGame
            ? $"Computer time left: {(int)left.TotalMinutes}:{left.Seconds:00}"
            : "";

        Title = $"Stello – {(_fileName is null ? "Untitled" : Path.GetFileName(_fileName))}";

        RebuildMoveHistory();

        UndoCommand.NotifyCanExecuteChanged();
        RedoCommand.NotifyCanExecuteChanged();
        SwitchSidesCommand.NotifyCanExecuteChanged();

        OnPropertyChanged(nameof(BlackMobilityText));
        OnPropertyChanged(nameof(WhiteMobilityText));
        OnPropertyChanged(nameof(BlackClockText));
        OnPropertyChanged(nameof(WhiteClockText));
        OnPropertyChanged(nameof(MoveCountText));
    }

    private void RebuildMoveHistory()
    {
        MoveHistoryList.Clear();
        int ply = 1;
        foreach (Move move in _game.PlayedMoves)
        {
            string notation = move.Square is { } sq ? sq.ToString() : "pass";
            MoveHistoryList.Add(new MoveHistoryItem(ply++, notation));
        }
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
        _blackTimeLeft = Settings.GameTime;
        _whiteTimeLeft = Settings.GameTime;
        _timeLeftAtPly.Clear();
        _lastTickTimestamp = 0;
        _hasHumanContinuedOnTime = false;
        _isTimeoutPrompting = false;
        OnPropertyChanged(nameof(BlackClockText));
        OnPropertyChanged(nameof(WhiteClockText));
        OnPropertyChanged(nameof(ClockText));
    }

    private void RestoreClock()
    {
        int next = _timeLeftAtPly.Keys.Where(ply => ply >= _game.Ply).DefaultIfEmpty(-1).Min();
        if (next >= 0)
        {
            (_blackTimeLeft, _whiteTimeLeft) = _timeLeftAtPly[next];
        }
    }

    private void OnClockTimerTick(object? state)
    {
        if (_syncContext != null && SynchronizationContext.Current != _syncContext)
        {
            _syncContext.Post(_ => TickClock(), null);
        }
        else
        {
            TickClock();
        }
    }

    private void TickClock()
    {
        if (_isTimeoutPrompting ||
            Settings.Mode != TimeControlMode.TimePerGame ||
            _game.IsGameOver ||
            _game.Ply == 0)
        {
            _lastTickTimestamp = 0;
            return;
        }

        long now = Stopwatch.GetTimestamp();
        if (_lastTickTimestamp == 0)
        {
            _lastTickTimestamp = now;
            return;
        }

        double elapsedSeconds = (now - _lastTickTimestamp) / (double)Stopwatch.Frequency;
        _lastTickTimestamp = now;

        if (elapsedSeconds <= 0)
        {
            return;
        }

        TimeSpan elapsed = TimeSpan.FromSeconds(elapsedSeconds);
        Player active = _game.ToMove;

        if (active == Player.Black)
        {
            if (GameMode == GameMode.HumanVsComputer && _hasHumanContinuedOnTime)
            {
                _blackTimeLeft = TimeSpan.Zero;
            }
            else
            {
                _blackTimeLeft = _blackTimeLeft > elapsed ? _blackTimeLeft - elapsed : TimeSpan.Zero;
                if (_blackTimeLeft <= TimeSpan.Zero)
                {
                    _blackTimeLeft = TimeSpan.Zero;
                    _ = HandleTimeoutAsync(Player.Black);
                    return;
                }
            }
        }
        else
        {
            if (GameMode == GameMode.ComputerVsHuman && _hasHumanContinuedOnTime)
            {
                _whiteTimeLeft = TimeSpan.Zero;
            }
            else
            {
                _whiteTimeLeft = _whiteTimeLeft > elapsed ? _whiteTimeLeft - elapsed : TimeSpan.Zero;
                if (_whiteTimeLeft <= TimeSpan.Zero)
                {
                    _whiteTimeLeft = TimeSpan.Zero;
                    _ = HandleTimeoutAsync(Player.White);
                    return;
                }
            }
        }

        OnPropertyChanged(nameof(BlackClockText));
        OnPropertyChanged(nameof(WhiteClockText));
        OnPropertyChanged(nameof(ClockText));
    }

    private async Task HandleTimeoutAsync(Player timedOut)
    {
        if (_game.IsGameOver)
        {
            return;
        }

        _lastTickTimestamp = 0;
        bool isHumanTimedOut = (GameMode == GameMode.HumanVsComputer && timedOut == Player.Black) ||
                               (GameMode == GameMode.ComputerVsHuman && timedOut == Player.White);

        if (isHumanTimedOut && !_hasHumanContinuedOnTime)
        {
            _isTimeoutPrompting = true;
            bool continuePlaying = false;
            try
            {
                continuePlaying = await _dialogs.ConfirmAsync("You ran out of time!\n\nDo you want to continue playing?");
            }
            finally
            {
                _isTimeoutPrompting = false;
            }

            if (continuePlaying)
            {
                _hasHumanContinuedOnTime = true;
                IsGameModified = true;
                return;
            }
        }

        await StopAsync();
        Status = $"{timedOut} ran out of time. Game over.";
        StatusText = Status;
        PlayGameOverSound();
    }

    private static string FormatClock(TimeSpan time)
    {
        TimeSpan clamped = time > TimeSpan.Zero ? time : TimeSpan.Zero;
        return $"{(int)clamped.TotalMinutes}:{clamped.Seconds:02}";
    }

    private async Task WriteAsync(bool askForName)
    {
        try
        {
            string? name = await _files.SaveAsync(GameRecordFormat.Format(_game) + Environment.NewLine, _fileName, askForName);
            if (name is not null)
            {
                _fileName = name;
                Refresh();
            }
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            _dialogs.ShowError($"The game could not be saved.\n\n{exception.Message}");
        }
    }
}
