using System.Windows;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Threading;
using Stello.App.Models;
using Stello.App.Services;
using Stello.App.ViewModels;
using Stello.Engine;

namespace Stello.Net.Tests;

public sealed class MainWindowLayoutTests
{
    [Fact]
    public Task StatusHeader_DistributesLabelsLikeWeb() => OnUiThread(CheckStatusHeader);

    [Fact]
    public Task AnalysisPanel_FollowsMenuShortcutAndSavedPreference() => OnUiThread(CheckAnalysisPanel);

    private static async Task OnUiThread(Action action)
    {
        var completion = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        var thread = new Thread(() =>
        {
            try
            {
                action();
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
        using var vm = CreateViewModel(new FakeSettingsStore(AppSettings.Default));
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

    private static void CheckAnalysisPanel()
    {
        var store = new FakeSettingsStore(AppSettings.Default);
        using (var vm = CreateViewModel(store))
        {
            var window = new MainWindow(vm) { ShowActivated = false, Opacity = 0 };
            try
            {
                window.Show();
                DrainDispatcher();
                var (panel, menu, move) = AnalysisControls(window);
                Assert.True(menu.IsChecked);
                Assert.Equal(Visibility.Visible, panel.Visibility);
                Assert.True(panel.IsVisible);
                Assert.Equal("-", move.Text);
                vm.Analysis.Move = "d3";
                Assert.Equal("d3", move.Text);

                Invoke(menu);
                Assert.False(vm.IsAnalysisVisible);
                Assert.False(menu.IsChecked);
                Assert.Equal(Visibility.Collapsed, panel.Visibility);
                Assert.False(panel.IsVisible);
                Assert.False(store.Settings.ShowAnalysis);

                Invoke(menu);
                Assert.True(vm.IsAnalysisVisible);
                Assert.True(menu.IsChecked);
                Assert.Equal(Visibility.Visible, panel.Visibility);
                Assert.True(panel.IsVisible);
                Assert.True(store.Settings.ShowAnalysis);
                Assert.Equal("d3", move.Text);

                Invoke(menu);
                Assert.False(store.Settings.ShowAnalysis);
            }
            finally
            {
                window.Close();
            }
        }

        using (var vm = CreateViewModel(store))
        {
            var window = new MainWindow(vm) { ShowActivated = false, Opacity = 0 };
            try
            {
                window.Show();
                DrainDispatcher();
                var (panel, menu, move) = AnalysisControls(window);
                Assert.False(menu.IsChecked);
                Assert.Equal(Visibility.Collapsed, panel.Visibility);
                Assert.False(panel.IsVisible);

                KeyBinding shortcut = window.InputBindings.OfType<KeyBinding>()
                    .Single(binding => binding.Key == Key.A && binding.Modifiers == ModifierKeys.Control);
                Assert.Same(vm.ToggleAnalysisCommand, shortcut.Command);
                shortcut.Command.Execute(null);
                DrainDispatcher();
                Assert.True(menu.IsChecked);
                Assert.Equal(Visibility.Visible, panel.Visibility);
                Assert.True(panel.IsVisible);
                Assert.True(store.Settings.ShowAnalysis);
                vm.Analysis.Move = "c4";
                Assert.Equal("c4", move.Text);
            }
            finally
            {
                window.Close();
            }
        }
    }

    private static (Border Panel, MenuItem Menu, TextBlock Move) AnalysisControls(MainWindow window)
    {
        var dock = Assert.IsType<DockPanel>(window.Content);
        var menu = Assert.IsType<Menu>(dock.Children[0]);
        var options = menu.Items.OfType<MenuItem>().Single(item => Equals(item.Header, "_Options"));
        var analysisMenu = options.Items.OfType<MenuItem>().Single(item => Equals(item.Header, "_Analysis"));

        var side = Assert.IsType<ScrollViewer>(window.FindName("SidePanels"));
        var panels = Assert.IsType<StackPanel>(side.Content);
        var analysisPanel = panels.Children.OfType<Border>().Single(border =>
            border.Child is StackPanel content && content.Children.OfType<TextBlock>().Any(text => text.Text == "ANALYSIS"));
        var analysisContent = Assert.IsType<StackPanel>(analysisPanel.Child);
        var fields = Assert.IsType<Grid>(analysisContent.Children[1]);
        var move = fields.Children.OfType<TextBlock>().Single(text => Grid.GetRow(text) == 0 && Grid.GetColumn(text) == 1);
        return (analysisPanel, analysisMenu, move);
    }

    private static void Invoke(MenuItem menu)
    {
        var peer = new MenuItemAutomationPeer(menu);
        Assert.IsAssignableFrom<IInvokeProvider>(peer.GetPattern(PatternInterface.Invoke)).Invoke();
        DrainDispatcher();
    }

    private static void DrainDispatcher()
    {
        var frame = new DispatcherFrame();
        Dispatcher.CurrentDispatcher.BeginInvoke(DispatcherPriority.ApplicationIdle, () => frame.Continue = false);
        Dispatcher.PushFrame(frame);
    }

    private static MainViewModel CreateViewModel(FakeSettingsStore store)
    {
        OpeningBook book = OpeningBook.CreateEmpty();
        var engine = new LocalEngineHost(
            new ComputerPlayer(new SearchEngine(hashBits: 12), book, new Random(0)), book, new FakeBookStore());
        return new MainViewModel(engine, new FakeDialogService(), new FakeGameFileService(), store);
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
