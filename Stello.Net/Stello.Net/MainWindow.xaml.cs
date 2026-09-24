using System.ComponentModel;
using System.Windows;
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
    }

    protected override void OnClosing(CancelEventArgs e)
    {
        _viewModel.Stop();
        base.OnClosing(e);
    }

    private void OnExit(object sender, RoutedEventArgs e) => Close();
}