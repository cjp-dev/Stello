using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
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
        Restore(viewModel.WindowPlacement);
    }

    protected override void OnClosing(CancelEventArgs e)
    {
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

    // The square board fills the height, or the width left beside the panels.
    // The panels' frames start level with the squares, below the a–h row (20 of BoardView's 440 units).
    private void OnPlayAreaSizeChanged(object sender, SizeChangedEventArgs e)
    {
        double size = Math.Max(0, Math.Min(e.NewSize.Height, e.NewSize.Width - SidePanels.Width));
        Board.Width = size;
        Board.Height = size;
        SidePanels.Margin = new Thickness(0, Math.Max(0, size * 20 / 440 - FrameTop(StatusBox)), 0, 0);
    }

    // A GroupBox draws its frame part-way down its header; the template's first Border (not the header) is the frame.
    private static double FrameTop(GroupBox box)
    {
        if (VisualTreeHelper.GetChildrenCount(box) == 0 || VisualTreeHelper.GetChild(box, 0) is not Panel root)
        {
            return 0;
        }

        Border? frame = root.Children.OfType<Border>().FirstOrDefault(border => border.Name != "Header");
        return frame?.TranslatePoint(new Point(), box).Y ?? 0;
    }
}