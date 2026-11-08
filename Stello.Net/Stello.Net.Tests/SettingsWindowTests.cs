using System.Runtime.ExceptionServices;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Automation;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Documents;
using System.Windows.Threading;
using Stello.App.Models;
using Stello.App.ViewModels;
using Stello.Engine;
using Stello.Net.Views;

namespace Stello.Net.Tests;

public sealed class SettingsWindowTests
{
    [Fact]
    public async Task SettingsDialog_ShowsDeviceThreadsAndAppliesChanges()
    {
        var completion = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        var thread = new Thread(() =>
        {
            try
            {
                CheckSettingsDialog();
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

    private static void CheckSettingsDialog()
    {
        var viewModel = new SettingsViewModel(GameSettings.Default, new ThreadCounts(Device: 8, Default: 7));
        var window = new SettingsWindow { DataContext = viewModel, ShowActivated = false, Opacity = 0 };
        Exception? callbackFailure = null;
        window.Loaded += (_, _) => window.Dispatcher.BeginInvoke(DispatcherPriority.ApplicationIdle, () =>
        {
            try
            {
                var root = Assert.IsType<StackPanel>(window.Content);
                var timing = Assert.IsType<Grid>(Assert.IsType<GroupBox>(root.Children[0]).Content);
                var perMove = timing.Children.OfType<RadioButton>().Single(radio => Equals(radio.Content, "Time per move"));
                perMove.SetCurrentValue(ToggleButton.IsCheckedProperty, true);
                var seconds = timing.Children.OfType<Slider>()
                    .Single(slider => AutomationProperties.GetName(slider) == "Seconds per move");
                seconds.SetCurrentValue(RangeBase.ValueProperty, 12d);
                Assert.True(viewModel.IsTimePerMove);
                Assert.Equal(12, viewModel.SecondsPerMove);

                var book = Assert.IsType<StackPanel>(Assert.IsType<GroupBox>(root.Children[1]).Content);
                var capacity = Assert.IsType<Grid>(book.Children.OfType<Grid>().Single()).Children.OfType<ComboBox>().Single();
                capacity.SetCurrentValue(Selector.SelectedValueProperty, 262_144);
                Assert.Equal(262_144, viewModel.TranspositionTableEntries);

                var threads = Assert.IsType<Grid>(Assert.IsType<GroupBox>(root.Children[2]).Content);
                var label = threads.Children.OfType<TextBlock>().Single();
                string text = string.Concat(label.Inlines.OfType<Run>().Select(run => run.Text));
                Assert.Equal("Search with (of 8 on this device):", Regex.Replace(text, @"\s+", " "));
                threads.Children.OfType<ComboBox>().Single().SetCurrentValue(Selector.SelectedValueProperty, 2);
                Assert.Equal(2, viewModel.Threads);

                var buttons = Assert.IsType<StackPanel>(root.Children[3]);
                Assert.IsType<Button>(buttons.Children[0]).RaiseEvent(new RoutedEventArgs(ButtonBase.ClickEvent));
            }
            catch (Exception exception)
            {
                callbackFailure = exception;
                window.Close();
            }
        });

        bool? result = window.ShowDialog();
        if (callbackFailure is not null)
        {
            ExceptionDispatchInfo.Capture(callbackFailure).Throw();
        }
        Assert.True(result);
        GameSettings saved = viewModel.ToSettings();
        Assert.Equal(TimeControlMode.TimePerMove, saved.Mode);
        Assert.Equal(12, saved.SecondsPerMove);
        Assert.Equal(262_144, saved.TranspositionTableEntries);
        Assert.Equal(2, saved.Threads);
    }
}
