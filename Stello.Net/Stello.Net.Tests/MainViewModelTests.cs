using System.IO;
using Stello.Engine;
using Stello.Net.Models;
using Stello.Net.ViewModels;

namespace Stello.Net.Tests;

public sealed class MainViewModelTests : IDisposable
{
    // After these moves Black has no legal move but White has.
    private const string BlackMustPass = "d3 c3 b3 b2 f5 a3 a1 c1";

    // Shortest possible game: White is wiped out, 13-0.
    private const string WhiteWipedOut = "d3 c3 b3 d2 e1 d6 d7 e3 f4";

    private static readonly GameSettings QuickSettings = new(TimeControlMode.FixedDepth, 2, 5, 5);
    private static readonly GameSettings SlowSettings = new(TimeControlMode.FixedDepth, 20, 5, 5);

    private readonly FakeDialogService _dialogs = new();
    private readonly string _file = Path.Combine(Path.GetTempPath(), $"stello-{Guid.NewGuid():N}.stello");

    public void Dispose() => File.Delete(_file);

    [Fact]
    public void Start_HumanPlaysBlackAndIsToMove()
    {
        MainViewModel vm = Create();

        Assert.Equal("Your move.", vm.Status);
        Assert.Equal(2, vm.BlackCount);
        Assert.Equal(2, vm.WhiteCount);
        Assert.Equal("You play Black", vm.SidesText);
        Assert.Equal(["d3", "c4", "f5", "e6"], vm.Squares.Where(s => s.IsLegalMove).Select(s => s.Name));
        Assert.Equal(Player.White, vm.Squares.Single(s => s.Name == "d4").Disc);
    }

    [Fact]
    public void Start_ShowsNotice()
    {
        MainViewModel vm = Create(notice: "The opening book could not be loaded.");

        Assert.Equal("The opening book could not be loaded. Your move.", vm.Status);
    }

    [Fact]
    public async Task Play_ComputerReplies()
    {
        MainViewModel vm = Create();

        Play(vm, "f5");
        await vm.Idle;

        Assert.Equal(2, vm.Game.Ply);
        Assert.False(vm.IsThinking);
        Assert.Equal("Your move.", vm.Status);
        SquareViewModel last = Assert.Single(vm.Squares, s => s.IsLastMove);
        Assert.Equal(vm.Game.LastMove!.Value.Square, last.Square);
        Assert.Equal("2 plies", vm.Analysis.Depth);
    }

    [Fact]
    public void Play_IllegalMoveBeeps()
    {
        MainViewModel vm = Create();

        Play(vm, "a1");

        Assert.Equal(1, _dialogs.Beeps);
        Assert.Equal(0, vm.Game.Ply);
    }

    [Fact]
    public async Task Play_WhileThinkingBeeps()
    {
        MainViewModel vm = Create(SlowSettings);
        Play(vm, "f5");

        Play(vm, "f4");

        Assert.Equal(1, _dialogs.Beeps);
        await vm.NewGameCommand.ExecuteAsync(null);
    }

    [Fact]
    public async Task UndoAndRedo_MoveBetweenTheHumansTurns()
    {
        MainViewModel vm = Create();
        Play(vm, "f5");
        await vm.Idle;

        await vm.UndoCommand.ExecuteAsync(null);
        await vm.Idle;

        Assert.Equal(0, vm.Game.Ply);
        Assert.True(vm.RedoCommand.CanExecute(null));
        Assert.False(vm.UndoCommand.CanExecute(null));

        await vm.RedoCommand.ExecuteAsync(null);
        await vm.Idle;

        Assert.Equal(2, vm.Game.Ply);
        Assert.True(vm.Game.IsHumanToMove);
    }

    [Fact]
    public async Task SwitchSides_ComputerMovesAtOnce()
    {
        MainViewModel vm = Create();

        await vm.SwitchSidesCommand.ExecuteAsync(null);
        await vm.Idle;

        Assert.Equal(1, vm.Game.Ply);
        Assert.Equal(Player.White, vm.Game.Human);
        Assert.Equal("You play White", vm.SidesText);
    }

    [Fact]
    public async Task MoveNow_StopsTheSearch()
    {
        MainViewModel vm = Create(SlowSettings);
        Play(vm, "f5");
        Assert.True(vm.IsThinking);
        Assert.True(vm.MoveNowCommand.CanExecute(null));

        await Task.Delay(200);
        vm.MoveNowCommand.Execute(null);
        await WaitAsync(vm.Idle);

        Assert.Equal(2, vm.Game.Ply);
        Assert.False(vm.MoveNowCommand.CanExecute(null));
    }

    [Fact]
    public async Task NewGame_StopsTheSearch()
    {
        MainViewModel vm = Create(SlowSettings);
        Play(vm, "f5");

        await WaitAsync(vm.NewGameCommand.ExecuteAsync(null));

        Assert.Equal(0, vm.Game.Ply);
        Assert.False(vm.IsThinking);
        Assert.Equal("Your move.", vm.Status);
    }

    [Fact]
    public async Task SaveAndOpen_RoundTrip()
    {
        MainViewModel vm = Create();
        Play(vm, "f5");
        await vm.Idle;
        _dialogs.SavePath = _file;

        vm.SaveCommand.Execute(null);

        Assert.Equal(GameRecordFormat.Format(vm.Game), File.ReadAllText(_file).Trim());
        Assert.Contains(Path.GetFileName(_file), vm.Title);

        MainViewModel other = Create();
        _dialogs.OpenPath = _file;
        await other.OpenCommand.ExecuteAsync(null);
        await other.Idle;

        Assert.Equal(vm.Game.Moves, other.Game.Moves);
        Assert.True(other.Game.IsHumanToMove);
    }

    [Fact]
    public async Task Open_TheHumanContinuesWithTheSideToMove()
    {
        File.WriteAllText(_file, "f5");
        _dialogs.OpenPath = _file;
        MainViewModel vm = Create();

        await vm.OpenCommand.ExecuteAsync(null);
        await vm.Idle;

        Assert.Equal(1, vm.Game.Ply);
        Assert.Equal(Player.White, vm.Game.Human);
    }

    [Fact]
    public async Task Open_InvalidFileShowsError()
    {
        File.WriteAllText(_file, "f5 z9");
        _dialogs.OpenPath = _file;
        MainViewModel vm = Create();

        await vm.OpenCommand.ExecuteAsync(null);

        Assert.Contains("Move 2", Assert.Single(_dialogs.Errors));
        Assert.Equal(0, vm.Game.Ply);
    }

    [Fact]
    public async Task Open_HumanWithoutMovePassesAutomatically()
    {
        File.WriteAllText(_file, BlackMustPass);
        _dialogs.OpenPath = _file;
        MainViewModel vm = Create();

        await vm.OpenCommand.ExecuteAsync(null);
        await vm.Idle;

        Assert.Contains(Move.Pass, vm.Game.Moves);
        Assert.True(vm.Game.IsHumanToMove || vm.Game.IsGameOver);
    }

    [Fact]
    public async Task Open_FinishedGameShowsResult()
    {
        File.WriteAllText(_file, WhiteWipedOut);
        _dialogs.OpenPath = _file;
        MainViewModel vm = Create();

        await vm.OpenCommand.ExecuteAsync(null);
        await vm.Idle;

        // White is to move after 9 plies, so the human takes White.
        Assert.Equal("Game over, the computer wins: Black 13 – White 0.", vm.Status);
        Assert.DoesNotContain(vm.Squares, s => s.IsLegalMove);
    }

    [Fact]
    public void EditSettings_ChangesSettingsAndClock()
    {
        MainViewModel vm = Create();
        _dialogs.NewSettings = new GameSettings(TimeControlMode.TimePerGame, 8, 5, 10);

        vm.EditSettingsCommand.Execute(null);

        Assert.Equal(_dialogs.NewSettings, vm.Settings);
        Assert.Equal("Computer time left: 10:00", vm.ClockText);
    }

    [Fact]
    public void EditSettings_CancelKeepsSettings()
    {
        MainViewModel vm = Create();

        vm.EditSettingsCommand.Execute(null);

        Assert.Equal(QuickSettings, vm.Settings);
        Assert.Equal("", vm.ClockText);
    }

    [Fact]
    public async Task TimePerGame_ComputerClockRunsDown()
    {
        MainViewModel vm = Create(new GameSettings(TimeControlMode.TimePerGame, 8, 5, 1));
        Play(vm, "f5");
        await vm.Idle;

        Assert.NotEqual("Computer time left: 1:00", vm.ClockText);
        Assert.StartsWith("Computer time left: 0:", vm.ClockText);
    }

    [Fact]
    public void ToggleAnalysis_HidesAndShowsThePanel()
    {
        MainViewModel vm = Create();

        vm.ToggleAnalysisCommand.Execute(null);
        Assert.False(vm.IsAnalysisVisible);

        vm.ToggleAnalysisCommand.Execute(null);
        Assert.True(vm.IsAnalysisVisible);
    }

    [Fact]
    public void About_ShowsTheAboutBox()
    {
        MainViewModel vm = Create();

        vm.AboutCommand.Execute(null);

        Assert.Equal(1, _dialogs.AboutShown);
    }

    [Fact]
    public void Start_UsesSavedSettings()
    {
        var placement = new WindowPlacement(10, 20, 900, 700, Maximized: false);
        var store = new FakeSettingsStore(new AppSettings(SlowSettings, ShowAnalysis: false, placement));

        MainViewModel vm = Create(store: store);

        Assert.Equal(SlowSettings, vm.Settings);
        Assert.False(vm.IsAnalysisVisible);
        Assert.Equal(placement, vm.WindowPlacement);
        Assert.Equal(0, store.Saves);
    }

    [Fact]
    public void EditSettings_SavesTheSettings()
    {
        var store = new FakeSettingsStore(new AppSettings(QuickSettings, true, null));
        MainViewModel vm = Create(store: store);
        _dialogs.NewSettings = new GameSettings(TimeControlMode.TimePerMove, 8, 12, 5);

        vm.EditSettingsCommand.Execute(null);

        Assert.Equal(_dialogs.NewSettings, store.Settings.Game);
    }

    [Fact]
    public void ToggleAnalysis_SavesTheSettings()
    {
        var store = new FakeSettingsStore(new AppSettings(QuickSettings, true, null));
        MainViewModel vm = Create(store: store);

        vm.ToggleAnalysisCommand.Execute(null);

        Assert.False(store.Settings.ShowAnalysis);
    }

    [Fact]
    public void SaveWindowPlacement_SavesTheSettings()
    {
        var store = new FakeSettingsStore(new AppSettings(QuickSettings, true, null));
        MainViewModel vm = Create(store: store);
        var placement = new WindowPlacement(1, 2, 800, 600, Maximized: true);

        vm.SaveWindowPlacement(placement);

        Assert.Equal(placement, store.Settings.Window);
        Assert.Equal(placement, vm.WindowPlacement);
    }

    [Fact]
    public void SaveSettings_FailureIsShownInTheStatus()
    {
        var store = new FakeSettingsStore(new AppSettings(QuickSettings, true, null)) { CanSave = false };
        MainViewModel vm = Create(store: store);

        vm.ToggleAnalysisCommand.Execute(null);

        Assert.StartsWith("The settings could not be saved.", vm.Status);
    }

    private MainViewModel Create(GameSettings? settings = null, string? notice = null, FakeSettingsStore? store = null) =>
        new(
            new ComputerPlayer(new SearchEngine(hashBits: 12), book: null, new Random(0)),
            _dialogs,
            store ?? new FakeSettingsStore(new AppSettings(settings ?? QuickSettings, ShowAnalysis: true, Window: null)),
            notice);

    private static void Play(MainViewModel vm, string square) =>
        vm.PlayCommand.Execute(vm.Squares.Single(s => s.Name == square));

    private static async Task WaitAsync(Task task)
    {
        Task finished = await Task.WhenAny(task, Task.Delay(TimeSpan.FromSeconds(5)));
        Assert.Same(task, finished);
        await task;
    }
}
