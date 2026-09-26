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
    private readonly OpeningBook _book;
    private readonly IBookStore _bookStore;
    private readonly IDialogService _dialogs;
    private readonly ISettingsStore _settingsStore;
    private readonly bool _settingsLoaded;

    // Computer clock before its move at each ply, so taking back moves also gives the time back (C++: timesleft).
    private readonly Dictionary<int, TimeSpan> _timeLeftAtPly = [];

    private Game _game = new();
    private string? _filePath;
    private string? _notice;
    private TimeSpan _computerTimeLeft;
    private CancellationTokenSource? _cancel;
    private CancellationTokenSource? _moveNow;
    private CancellationTokenSource? _learningCancel;
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
    [NotifyCanExecuteChangedFor(
        nameof(NewGameCommand), nameof(OpenCommand), nameof(SwitchSidesCommand), nameof(UndoCommand), nameof(RedoCommand),
        nameof(EditSettingsCommand), nameof(AddGameToBookCommand), nameof(EvaluateBookCommand), nameof(SelfPlayCommand),
        nameof(StopLearningCommand))]
    private bool _isLearning;

    [ObservableProperty]
    private bool _isAnalysisVisible = true;

    [ObservableProperty]
    private GameSettings _settings = GameSettings.Default;

    public MainViewModel(
        ComputerPlayer computer,
        OpeningBook book,
        IBookStore bookStore,
        IDialogService dialogs,
        ISettingsStore settingsStore,
        string? startupNotice = null)
    {
        _computer = computer;
        _book = book;
        _bookStore = bookStore;
        _dialogs = dialogs;
        _settingsStore = settingsStore;

        AppSettings saved = settingsStore.Load();
        Settings = saved.Game;
        IsAnalysisVisible = saved.ShowAnalysis;
        WindowPlacement = saved.Window;
        _settingsLoaded = true;

        _computerTimeLeft = Settings.GameTime;
        _notice = startupNotice;
        Squares = Enumerable.Range(0, 64).Select(i => new SquareViewModel(new Square(i))).ToArray();
        Start();
    }

    public IReadOnlyList<SquareViewModel> Squares { get; }

    public AnalysisViewModel Analysis { get; } = new();

    /// <summary>The saved window position, or null for the default position.</summary>
    public WindowPlacement? WindowPlacement { get; private set; }

    /// <summary>Completes when the computer has moved and it is the human's turn or the game is over.</summary>
    internal Task Idle { get; private set; } = Task.CompletedTask;

    /// <summary>Completes when book learning has stopped.</summary>
    internal Task Learning { get; private set; } = Task.CompletedTask;

    internal Game Game => _game;

    /// <summary>Stops the computer and book learning without waiting, e.g. when the window closes.</summary>
    public void Stop()
    {
        _cancel?.Cancel();
        _learningCancel?.Cancel();
    }

    public void SaveWindowPlacement(WindowPlacement placement)
    {
        WindowPlacement = placement;
        SaveSettings();
    }

    partial void OnIsAnalysisVisibleChanged(bool value) => SaveSettings();

    private void SaveSettings()
    {
        if (_settingsLoaded && !_settingsStore.Save(new AppSettings(Settings, IsAnalysisVisible, WindowPlacement)))
        {
            Status = $"The settings could not be saved. {Status}";
        }
    }

    [RelayCommand]
    private void Play(SquareViewModel square)
    {
        if (IsThinking || IsLearning || _game.IsGameOver || !_game.IsHumanToMove || !_game.Board.IsLegal(_game.ToMove, square.Square))
        {
            _dialogs.Beep();
            return;
        }

        _game.Play(square.Square);
        Start();
    }

    [RelayCommand(CanExecute = nameof(CanChangeGame))]
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

    [RelayCommand(CanExecute = nameof(CanChangeGame))]
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
    [RelayCommand(CanExecute = nameof(CanChangeGame))]
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

    [RelayCommand(CanExecute = nameof(CanChangeGame))]
    private void EditSettings()
    {
        GameSettings? settings = _dialogs.EditSettings(Settings);
        if (settings is null)
        {
            return;
        }

        bool clockChanged = settings.Mode != Settings.Mode || settings.MinutesPerGame != Settings.MinutesPerGame;
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

    // C++: Flet spil ("Spil i database ?", "Var det sort der vandt ?").
    [RelayCommand(CanExecute = nameof(CanChangeGame))]
    private async Task AddGameToBook()
    {
        if (_game.Ply < 2)
        {
            _dialogs.ShowError("Play at least two moves before adding the game to the opening book.");
            return;
        }

        if (!_dialogs.Confirm("Add this game to the opening book?"))
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
            : _dialogs.AskGameResult();
        if (result is null)
        {
            return;
        }

        await StopAsync();
        CreateLearner(hashBits: 10).AddGame(_game.PlayedMoves.ToList(), result.Value);
        _notice = _bookStore.Save(_book)
            ? $"The game was added to the opening book ({_book.NodeCount:N0} positions)."
            : "The opening book could not be saved.";
        Start();
    }

    // C++: Minmaxlib (calc_lib, minmax_lib, sort_lib).
    [RelayCommand(CanExecute = nameof(CanChangeGame))]
    private async Task EvaluateBook()
    {
        if (_dialogs.Confirm(
            "Search every new position in the opening book and add the best moves that are not in it?\n\n" +
            "This can take a long time. You can stop at any time with Book > Stop Learning."))
        {
            await LearnAsync((learner, progress, token) =>
            {
                learner.EvaluatePositions(progress, token);
                learner.Minimax();
            });
        }
    }

    // C++: Lær spil (selfplay).
    [RelayCommand(CanExecute = nameof(CanChangeGame))]
    private async Task SelfPlay()
    {
        if (_dialogs.Confirm(
            "Let the computer play against itself and learn the games into the opening book?\n\n" +
            "This runs until you stop it with Book > Stop Learning."))
        {
            await LearnAsync((learner, progress, token) => learner.SelfPlay(
                progress,
                token,
                (number, moves) => _bookStore.AppendSelfPlayLog($"played game {number}: {string.Join(' ', moves)}")));
        }
    }

    [RelayCommand(CanExecute = nameof(IsLearning))]
    private void StopLearning() => _learningCancel?.Cancel();

    private bool CanChangeGame() => !IsLearning;

    private bool CanUndo() => _game.CanUndo && !IsLearning;

    private bool CanRedo() => _game.CanRedo && !IsLearning;

    // Runs book learning in the background; the game waits until it has stopped.
    private async Task LearnAsync(Action<BookLearner, IProgress<BookLearningProgress>, CancellationToken> learn)
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

        BookLearner learner = CreateLearner();
        string summary;
        try
        {
            Learning = Task.Run(() => learn(learner, progress, cancel.Token), CancellationToken.None);
            await Learning;
            summary = "Book learning finished.";
        }
        catch (OperationCanceledException)
        {
            summary = "Book learning stopped.";
        }
        catch (Exception exception)
        {
            summary = "Book learning failed.";
            _dialogs.ShowError($"Book learning failed.\n\n{exception.Message}");
        }
        finally
        {
            _learningCancel = null;
            IsLearning = false;
        }

        bool saved = _bookStore.Save(_book);
        _computer.BookTracker.Reset();
        _notice = $"{summary} {learner.PositionsEvaluated:N0} positions searched, {learner.GamesPlayed:N0} games played, " +
            $"{_book.NodeCount:N0} positions in the book.{(saved ? "" : " The opening book could not be saved.")}";
        Start();
    }

    // Book learning searches as the current settings allow, but at most the time per move (C++: 2 minutes each).
    private BookLearner CreateLearner(int hashBits = 19) => new(
        _book,
        new SearchEngine(hashBits),
        Settings.Mode == TimeControlMode.FixedDepth
            ? SearchLimits.FixedDepth(Settings.Depth)
            : SearchLimits.TimePerMove(TimeSpan.FromSeconds(Settings.SecondsPerMove)),
        new Random(),
        book => _bookStore.Save(book));

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
