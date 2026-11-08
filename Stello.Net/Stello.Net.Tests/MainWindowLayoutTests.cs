using System.Windows;
using System.Windows.Controls;
using Stello.App.Models;
using Stello.App.Services;
using Stello.App.ViewModels;
using Stello.Engine;

namespace Stello.Net.Tests;

public sealed class MainWindowLayoutTests
{
    [Fact]
    public async Task StatusHeader_DistributesLabelsLikeWeb()
    {
        var completion = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        var thread = new Thread(() =>
        {
            try
            {
                CheckStatusHeader();
                completion.SetResult();
            }
            catch (Exception exception)
            {
                completion.SetException(exception);
            }
        }) { IsBackground = true };
        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();

        await completion.Task.WaitAsync(TimeSpan.FromSeconds(15));
    }

    private static void CheckStatusHeader()
    {
        OpeningBook book = OpeningBook.CreateEmpty();
        var engine = new LocalEngineHost(
            new ComputerPlayer(new SearchEngine(hashBits: 12), book, new Random(0)), book, new FakeBookStore());
        using var vm = new MainViewModel(
            engine, new FakeDialogService(), new FakeGameFileService(), new FakeSettingsStore(AppSettings.Default));
        var window = new MainWindow(vm);
        var header = Assert.IsAssignableFrom<Panel>(window.FindName("StatusHeader"));
        Assert.Equal(3, header.Children.Count);
        var status = Assert.IsAssignableFrom<FrameworkElement>(header.Children[0]);
        var variant = Assert.IsAssignableFrom<FrameworkElement>(header.Children[1]);
        var setting = Assert.IsAssignableFrom<FrameworkElement>(header.Children[2]);

        AssertSpaced(header, status, variant, setting, 294);
        AssertSpaced(header, status, variant, setting, 277);
        vm.IsGameModified = true;
        Assert.Equal(Visibility.Visible, ((StackPanel)status).Children[1].Visibility);
        AssertSpaced(header, status, variant, setting, 277);

        vm.Settings = vm.Settings with { MinutesPerGame = GameSettings.MaxMinutesPerGame };
        Assert.Equal("60 min / game", ((TextBlock)((Border)setting).Child).Text);
        AssertSpaced(header, status, variant, setting, 277);

        vm.GameMode = GameMode.HumanVsHuman;
        Arrange(header, 277);
        Assert.Equal(Visibility.Collapsed, setting.Visibility);
        Assert.InRange(Math.Abs(Bounds(variant, header).Right - header.ActualWidth), 0, 0.5);

        vm.GameMode = GameMode.HumanVsComputer;
        AssertSpaced(header, status, variant, setting, 277);
    }

    private static void AssertSpaced(
        Panel header, FrameworkElement status, FrameworkElement variant, FrameworkElement setting, double width)
    {
        Arrange(header, width);
        Rect statusBounds = Bounds(status, header);
        Rect variantBounds = Bounds(variant, header);
        Rect settingBounds = Bounds(setting, header);

        Assert.Equal(Visibility.Visible, setting.Visibility);
        Assert.InRange(Math.Abs(statusBounds.Left), 0, 0.5);
        Assert.InRange(Math.Abs(header.ActualWidth - settingBounds.Right), 0, 0.5);
        Assert.True(statusBounds.Right <= variantBounds.Left,
            $"Status {statusBounds}; variant {variantBounds}; setting {settingBounds}");
        Assert.True(variantBounds.Right <= settingBounds.Left,
            $"Status {statusBounds}; variant {variantBounds}; setting {settingBounds}");
        double leftGap = variantBounds.Left - statusBounds.Right;
        double rightGap = settingBounds.Left - variantBounds.Right;
        Assert.InRange(Math.Abs(leftGap - rightGap), 0, 0.5);
    }

    private static void Arrange(Panel header, double width)
    {
        // The window is not shown, so remeasure its children after binding changes.
        foreach (UIElement child in header.Children)
        {
            child.InvalidateMeasure();
        }
        header.InvalidateMeasure();
        header.Measure(new Size(width, double.PositiveInfinity));
        header.Arrange(new Rect(0, 0, width, header.DesiredSize.Height));
        header.UpdateLayout();
    }

    private static Rect Bounds(FrameworkElement element, Panel header) =>
        new(element.TranslatePoint(new Point(), header), element.RenderSize);
}
