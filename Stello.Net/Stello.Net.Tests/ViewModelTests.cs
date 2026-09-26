using Stello.App.Models;
using Stello.App.ViewModels;
using Stello.Engine;

namespace Stello.Net.Tests;

public class AnalysisViewModelTests
{
    [Theory]
    [InlineData(123, ScoreKind.Heuristic, "+123")]
    [InlineData(-45, ScoreKind.Heuristic, "-45")]
    [InlineData(32610, ScoreKind.Heuristic, "Win")]
    [InlineData(-32610, ScoreKind.Heuristic, "Loss")]
    [InlineData(2, ScoreKind.WinLossDraw, "Win")]
    [InlineData(0, ScoreKind.WinLossDraw, "Draw")]
    [InlineData(-1, ScoreKind.WinLossDraw, "Loss")]
    [InlineData(12, ScoreKind.Exact, "+12 discs")]
    [InlineData(-39, ScoreKind.Book, "Book")]
    [InlineData(0, ScoreKind.None, "")]
    public void FormatScore(int score, ScoreKind kind, string expected)
    {
        Assert.Equal(expected, AnalysisViewModel.FormatScore(score, kind));
    }

    [Fact]
    public void Update_ShowsTheSearchProgress()
    {
        var analysis = new AnalysisViewModel();

        analysis.Update(new SearchInfo(5, Square.Parse("f5"), Square.Parse("d6"), 40, ScoreKind.Heuristic, 1234, 567, TimeSpan.FromSeconds(1.5)));

        Assert.Equal("f5", analysis.Move);
        Assert.Equal("5 plies", analysis.Depth);
        Assert.Equal("+40", analysis.Value);
        Assert.Equal("d6", analysis.BestMove);
        Assert.Equal("0:01.5", analysis.Time);
    }

    [Fact]
    public void Clear_EmptiesAllFields()
    {
        var analysis = new AnalysisViewModel();
        analysis.Update(new SearchInfo(5, Square.Parse("f5"), null, 40, ScoreKind.Exact, 1, 1, TimeSpan.Zero));

        analysis.Clear();

        Assert.Equal("", analysis.Move);
        Assert.Equal("", analysis.Value);
        Assert.Equal("", analysis.Depth);
    }
}

public class SettingsViewModelTests
{
    [Fact]
    public void Mode_SelectsOneRadioButton()
    {
        var settings = new SettingsViewModel(GameSettings.Default);

        settings.IsFixedDepth = true;

        Assert.Equal(TimeControlMode.FixedDepth, settings.Mode);
        Assert.False(settings.IsTimePerGame);
        Assert.False(settings.IsTimePerMove);
    }

    [Fact]
    public void ToSettings_KeepsValuesInRange()
    {
        var settings = new SettingsViewModel(GameSettings.Default) { Depth = 99, SecondsPerMove = 0, MinutesPerGame = 30 };

        Assert.Equal(new GameSettings(TimeControlMode.TimePerGame, 20, 1, 30), settings.ToSettings());
    }

    [Theory]
    [InlineData(TimeControlMode.FixedDepth)]
    [InlineData(TimeControlMode.TimePerMove)]
    [InlineData(TimeControlMode.TimePerGame)]
    public void GameSettings_ToLimitsUsesTheMode(TimeControlMode mode)
    {
        var settings = new GameSettings(mode, 6, 7, 8);

        SearchLimits limits = settings.ToLimits(TimeSpan.FromMinutes(3));

        Assert.Equal(mode, limits.Mode);
        Assert.Equal(mode switch
        {
            TimeControlMode.FixedDepth => TimeSpan.Zero,
            TimeControlMode.TimePerMove => TimeSpan.FromSeconds(7),
            _ => TimeSpan.FromMinutes(3),
        }, limits.Time);
    }
}
