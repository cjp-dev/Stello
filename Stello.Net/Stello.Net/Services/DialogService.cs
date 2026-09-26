using System.IO;
using System.Media;
using System.Windows;
using Microsoft.Win32;
using Stello.App.Models;
using Stello.App.Services;
using Stello.App.ViewModels;
using Stello.Engine;
using Stello.Net.Views;

namespace Stello.Net.Services;

internal sealed class DialogService : IDialogService
{
    private const string Caption = "Stello";
    private const string GameFilter = "Stello games (*.stello)|*.stello|Text files (*.txt)|*.txt|All files (*.*)|*.*";

    private static Window? Owner => Application.Current?.MainWindow;

    public bool Confirm(string message) =>
        Ask(message, MessageBoxButton.YesNo) == MessageBoxResult.Yes;

    // C++: "Var det sort der vandt ?"
    public GameResult? AskGameResult() => Ask(
            "Did Black win the game?\n\nYes: Black won.\nNo: White won.\nCancel: do not add the game.",
            MessageBoxButton.YesNoCancel) switch
        {
            MessageBoxResult.Yes => GameResult.BlackWins,
            MessageBoxResult.No => GameResult.WhiteWins,
            _ => null,
        };

    public string? ShowOpenDialog()
    {
        var dialog = new OpenFileDialog { Filter = GameFilter, DefaultExt = ".stello" };
        return dialog.ShowDialog(Owner) == true ? dialog.FileName : null;
    }

    public string? ShowSaveDialog(string? currentPath)
    {
        var dialog = new SaveFileDialog
        {
            Filter = GameFilter,
            DefaultExt = ".stello",
            FileName = currentPath is null ? "Game" : Path.GetFileName(currentPath),
            InitialDirectory = currentPath is null ? "" : Path.GetDirectoryName(currentPath),
        };
        return dialog.ShowDialog(Owner) == true ? dialog.FileName : null;
    }

    public GameSettings? EditSettings(GameSettings current)
    {
        var viewModel = new SettingsViewModel(current);
        var window = new SettingsWindow { Owner = Owner, DataContext = viewModel };
        return window.ShowDialog() == true ? viewModel.ToSettings() : null;
    }

    public void ShowError(string message) => Show(message, Caption, MessageBoxImage.Error);

    public void ShowAbout() => Show(
        "Stello Version 2.0\n\nCopyright (C) 1998 Futuresoft\nC# and WPF version, 2026.",
        "About Stello",
        MessageBoxImage.Information);

    public void Beep() => SystemSounds.Beep.Play();

    private static void Show(string message, string caption, MessageBoxImage image)
    {
        if (Owner is { } owner)
        {
            MessageBox.Show(owner, message, caption, MessageBoxButton.OK, image);
        }
        else
        {
            MessageBox.Show(message, caption, MessageBoxButton.OK, image);
        }
    }

    private static MessageBoxResult Ask(string message, MessageBoxButton buttons) => Owner is { } owner
        ? MessageBox.Show(owner, message, Caption, buttons, MessageBoxImage.Question)
        : MessageBox.Show(message, Caption, buttons, MessageBoxImage.Question);
}
