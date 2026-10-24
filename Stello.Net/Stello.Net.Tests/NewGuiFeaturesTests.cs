using System.IO;
using Stello.App.Models;
using Stello.App.Services;
using Stello.App.ViewModels;
using Stello.Engine;

namespace Stello.Net.Tests;

public sealed class NewGuiFeaturesTests
{
    private static readonly GameSettings QuickSettings = new(TimeControlMode.FixedDepth, 2, 5, 5);

    private readonly FakeDialogService _dialogs = new();
    private readonly FakeGameFileService _files = new();
    private readonly FakeBookStore _bookStore = new();
    private readonly FakeSoundService _soundService = new();
    private readonly OpeningBook _book = OpeningBook.CreateEmpty();

    private MainViewModel Create(GameSettings? settings = null) =>
        new(
            new LocalEngineHost(new ComputerPlayer(new SearchEngine(hashBits: 12), _book, new Random(0)), _book, _bookStore),
            _dialogs,
            _files,
            new FakeSettingsStore(new AppSettings(settings ?? QuickSettings, ShowAnalysis: true, Window: null)),
            startupNotice: null,
            soundService: _soundService);

    [Fact]
    public void AnalysisViewModel_SpeedAndUpdatedEvent()
    {
        var vm = new AnalysisViewModel();
        int eventCount = 0;
        vm.Updated += () => eventCount++;

        var info = new SearchInfo(4, Square.Parse("f5"), Square.Parse("d6"), 20, ScoreKind.Heuristic, 100_000, 50_000, TimeSpan.FromSeconds(0.5));
        vm.Update(info);

        Assert.Equal(1, eventCount);
        Assert.Equal("f5", vm.Move);
        Assert.Equal("4 plies", vm.Depth);
        Assert.Equal("+20", vm.Score);
        Assert.Equal("+20", vm.Value); // Alias
        Assert.Equal("d6", vm.BestMove);
        Assert.Contains("nps", vm.Speed);
        Assert.Equal(vm.Speed, vm.Evaluations); // Alias

        vm.Clear();
        Assert.Equal(2, eventCount);
        Assert.Equal("", vm.Move);
        Assert.Equal("", vm.Score);
        Assert.Equal("", vm.Speed);
    }

    [Fact]
    public void InitialState_HasCorrectDefaults()
    {
        MainViewModel vm = Create();

        Assert.Equal(GameMode.HumanVsComputer, vm.GameMode);
        Assert.True(vm.IsHumanVsComputer);
        Assert.False(vm.IsComputerVsHuman);
        Assert.False(vm.IsHumanVsHuman);
        Assert.True(vm.IsVsComputer);
        Assert.False(vm.IsStatusModified);
        Assert.False(vm.IsGameModified);
        Assert.Equal("Othello", vm.VariantBadgeText);
        Assert.Equal("Human", vm.BlackPlayerLabel);
        Assert.Equal("Computer", vm.WhitePlayerLabel);
        Assert.Equal(4, vm.BlackMobility); // 4 legal moves at game start (d3, c4, f5, e6)
        Assert.Equal(4, vm.WhiteMobility);
        Assert.Equal("Moves: 4", vm.BlackMobilityText);
        Assert.Empty(vm.MoveHistoryList);
    }

    [Fact]
    public async Task PlayingMove_UpdatesHistoryAndSounds()
    {
        MainViewModel vm = Create();

        SquareViewModel square = vm.Squares.Single(s => s.Name == "f5");
        vm.PlayCommand.Execute(square);
        await vm.Idle;

        Assert.Contains(SoundType.Move, _soundService.PlayedSounds);
        Assert.Equal(2, vm.MoveHistoryList.Count);
        Assert.Equal(1, vm.MoveHistoryList[0].Ply);
        Assert.Equal("f5", vm.MoveHistoryList[0].Notation);
        Assert.Equal(2, vm.MoveHistoryList[1].Ply);
        Assert.NotEmpty(vm.MoveHistoryList[1].Notation);
    }

    [Fact]
    public async Task Undo_MarksGameModified()
    {
        MainViewModel vm = Create();

        SquareViewModel square = vm.Squares.Single(s => s.Name == "f5");
        vm.PlayCommand.Execute(square);
        await vm.Idle;

        Assert.False(vm.IsGameModified);
        Assert.False(vm.IsStatusModified);

        await vm.UndoCommand.ExecuteAsync(null);
        await vm.Idle;

        Assert.True(vm.IsGameModified);
        Assert.True(vm.IsStatusModified);
    }

    [Fact]
    public async Task SwitchSides_MarksGameModified()
    {
        MainViewModel vm = Create();

        Assert.False(vm.IsGameModified);
        await vm.SwitchSidesCommand.ExecuteAsync(null);
        await vm.Idle;

        Assert.True(vm.IsGameModified);
        Assert.True(vm.IsStatusModified);
        Assert.Equal(GameMode.ComputerVsHuman, vm.GameMode);
    }

    [Fact]
    public async Task NewGame_ResetsModifiedStatus()
    {
        MainViewModel vm = Create();

        await vm.SwitchSidesCommand.ExecuteAsync(null);
        await vm.Idle;
        Assert.True(vm.IsGameModified);

        await vm.NewGameCommand.ExecuteAsync(null);
        await vm.Idle;

        Assert.False(vm.IsGameModified);
        Assert.False(vm.IsStatusModified);
    }

    [Fact]
    public async Task SetGameMode_SwitchesModesCorrectly()
    {
        MainViewModel vm = Create();

        await vm.SetGameMode(GameMode.HumanVsHuman);
        await vm.Idle;

        Assert.Equal(GameMode.HumanVsHuman, vm.GameMode);
        Assert.True(vm.IsHumanVsHuman);
        Assert.False(vm.IsVsComputer);
        Assert.Equal("Player 1", vm.BlackPlayerLabel);
        Assert.Equal("Player 2", vm.WhitePlayerLabel);

        await vm.SetGameMode(GameMode.ComputerVsHuman);
        await vm.Idle;

        Assert.Equal(GameMode.ComputerVsHuman, vm.GameMode);
        Assert.True(vm.IsComputerVsHuman);
        Assert.Equal("Computer", vm.BlackPlayerLabel);
        Assert.Equal("Human", vm.WhitePlayerLabel);
    }

    [Fact]
    public void Clock_InitialValuesAreFormattedAs5Minutes()
    {
        var settings = new GameSettings(TimeControlMode.TimePerGame, 8, 5, 5);
        MainViewModel vm = Create(settings);

        Assert.Equal("5:00", vm.BlackClockText);
        Assert.Equal("5:00", vm.WhiteClockText);
        Assert.True(vm.IsClockVisible);
    }

    [Fact]
    public void SettingsViewModel_OpeningBookAndTranspositionTableConfigurable()
    {
        var vm = new SettingsViewModel(GameSettings.Default);

        Assert.True(vm.UseOpeningBook);
        Assert.Equal(GameSettings.DefaultTranspositionTableEntries, vm.TranspositionTableEntries);
        Assert.Equal(20, vm.TranspositionTablePower);
        Assert.Equal("1,048,576 (48 MB) - Default", vm.TranspositionTableEntriesText);

        vm.UseOpeningBook = false;
        vm.TranspositionTableEntries = 262_144;

        Assert.False(vm.UseOpeningBook);
        Assert.Equal(18, vm.TranspositionTablePower);
        Assert.Equal("262,144 (12 MB)", vm.TranspositionTableEntriesText);

        GameSettings saved = vm.ToSettings();
        Assert.False(saved.UseOpeningBook);
        Assert.Equal(262_144, saved.TranspositionTableEntries);
        Assert.Equal(17, saved.HashBits);
    }

    [Fact]
    public void ComputerPlayer_RespectsUseOpeningBookFlag()
    {
        string bookText = "# Stello opening book, format 2\nd3 c5:10:H\n";
        using var stream = new MemoryStream(System.Text.Encoding.UTF8.GetBytes(bookText));
        OpeningBook book = OpeningBook.Load(stream);

        Board board = Board.Initial.Play(Player.Black, Square.Parse("d3"));

        var player = new ComputerPlayer(new SearchEngine(hashBits: 10), book, new Random(0));

        SearchLimits withBook = SearchLimits.FixedDepth(1, useOpeningBook: true, hashBits: 10);
        SearchResult bookResult = player.ChooseMove(board, Player.White, withBook);
        Assert.Equal(ScoreKind.Book, bookResult.Kind);
        Assert.Equal("c5", bookResult.Move.Square?.ToString());

        player.BookTracker.Reset();
        SearchLimits withoutBook = SearchLimits.FixedDepth(1, useOpeningBook: false, hashBits: 10);
        SearchResult searchResult = player.ChooseMove(board, Player.White, withoutBook);
        Assert.NotEqual(ScoreKind.Book, searchResult.Kind);
    }
}

