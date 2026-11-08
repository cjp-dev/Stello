using System.ComponentModel;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Threading;
using Stello.App.Models;
using Stello.App.ViewModels;

namespace Stello.Net;

public partial class MainWindow : Window
{
    private readonly MainViewModel _viewModel;

    public MainWindow(MainViewModel viewModel)
    {
        InitializeComponent();
        _viewModel = viewModel;
        DataContext = viewModel;
        _viewModel.PropertyChanged += OnViewModelPropertyChanged;
        // The history list scrolls itself to the latest move; without this the request also scrolls the side panel,
        // which hides the status and the player cards during a game.
        MoveHistoryList.RequestBringIntoView += (_, e) => e.Handled = true;
        Loaded += (_, _) => Dispatcher.InvokeAsync(ScrollHistoryToLatest, DispatcherPriority.Loaded);
        Restore(viewModel.WindowPlacement);
    }

    protected override void OnClosing(CancelEventArgs e)
    {
        _viewModel.PropertyChanged -= OnViewModelPropertyChanged;
        _viewModel.Stop();
        Rect bounds = WindowState == WindowState.Normal ? new Rect(Left, Top, Width, Height) : RestoreBounds;
        _viewModel.SaveWindowPlacement(new WindowPlacement(bounds.Left, bounds.Top, bounds.Width, bounds.Height, WindowState == WindowState.Maximized));
        base.OnClosing(e);
    }

    // Only restore a position that is still on a screen (a monitor may have been removed).
    private void Restore(WindowPlacement? placement)
    {
        if (placement is null)
        {
            return;
        }

        var screen = new Rect(SystemParameters.VirtualScreenLeft, SystemParameters.VirtualScreenTop, SystemParameters.VirtualScreenWidth, SystemParameters.VirtualScreenHeight);
        var window = new Rect(placement.Left, placement.Top, Math.Max(placement.Width, MinWidth), Math.Max(placement.Height, MinHeight));
        if (!screen.IntersectsWith(window) || !screen.Contains(new Point(window.Left + 50, window.Top + 20)))
        {
            return;
        }

        WindowStartupLocation = WindowStartupLocation.Manual;
        Left = window.Left;
        Top = window.Top;
        Width = window.Width;
        Height = window.Height;
        if (placement.Maximized)
        {
            WindowState = WindowState.Maximized;
        }
    }

    private void OnExit(object sender, RoutedEventArgs e) => Close();

    private void OnBoardStyleMenuOpened(object sender, RoutedEventArgs e)
    {
        GreenBoardMenuItem.IsChecked = _viewModel.Appearance.Board == BoardStyle.Green;
        GoldenOakBoardMenuItem.IsChecked = _viewModel.Appearance.Board == BoardStyle.GoldenOak;
        ReddishWoodBoardMenuItem.IsChecked = _viewModel.Appearance.Board == BoardStyle.ReddishWood;
    }

    private void OnBoardStyleSelected(object sender, RoutedEventArgs e)
    {
        if (sender is MenuItem { Tag: BoardStyle boardStyle })
        {
            _viewModel.Appearance = _viewModel.Appearance with { Board = boardStyle };
        }
    }

    private void OnPieces3DToggled(object sender, RoutedEventArgs e)
    {
        _viewModel.Appearance = _viewModel.Appearance with { Pieces3D = !_viewModel.Appearance.Pieces3D };
    }

    private void OnAnimateFlipsToggled(object sender, RoutedEventArgs e)
    {
        _viewModel.Appearance = _viewModel.Appearance with { AnimateFlips = !_viewModel.Appearance.AnimateFlips };
    }

    private void OnViewModelPropertyChanged(object? sender, PropertyChangedEventArgs e)
    {
        if (e.PropertyName == nameof(MainViewModel.MoveHistoryRows))
        {
            Dispatcher.InvokeAsync(ScrollHistoryToLatest, DispatcherPriority.Loaded);
        }
    }

    private void ScrollHistoryToLatest()
    {
        if (_viewModel.MoveHistoryRows.Count > 0)
        {
            MoveHistoryList.ScrollIntoView(_viewModel.MoveHistoryRows[^1]);
        }
    }

    private void OnPlayAreaSizeChanged(object sender, SizeChangedEventArgs e)
    {
        double size = Math.Max(0, Math.Min(e.NewSize.Height, e.NewSize.Width - SidePanels.Width));
        Board.Width = size;
        Board.Height = size;
        // Align the panels with the board below its coordinate labels.
        SidePanels.Margin = new Thickness(0, Math.Max(0, size * 20 / 440), 0, 0);
    }
}

internal sealed class EmptyTextToDashConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture) =>
        value is string text && !string.IsNullOrEmpty(text) ? text : "-";

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) =>
        throw new NotSupportedException();
}