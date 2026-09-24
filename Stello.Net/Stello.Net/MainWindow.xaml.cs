using System.ComponentModel;
using System.Windows;
using Stello.Net.Models;
using Stello.Net.ViewModels;

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
}