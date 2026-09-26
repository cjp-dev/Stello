using System.Media;
using System.Windows;
using Stello.App.Models;
using Stello.App.Services;
using Stello.App.ViewModels;
using Stello.Engine;
using Stello.Net.Views;

namespace Stello.Net.Services;

internal sealed class DialogService : IDialogService
{
    private const string Caption = "Stello";

    private static Window? Owner => Application.Current?.MainWindow;

    public Task<bool> ConfirmAsync(string message) =>
        Task.FromResult(Ask(message, MessageBoxButton.YesNo) == MessageBoxResult.Yes);

    // C++: "Var det sort der vandt ?"
    public Task<GameResult?> AskGameResultAsync() => Task.FromResult<GameResult?>(Ask(
            "Did Black win the game?\n\nYes: Black won.\nNo: White won.\nCancel: do not add the game.",
            MessageBoxButton.YesNoCancel) switch
        {
            MessageBoxResult.Yes => GameResult.BlackWins,
            MessageBoxResult.No => GameResult.WhiteWins,
            _ => null,
        });

    public Task<GameSettings?> EditSettingsAsync(GameSettings current)
    {
        var viewModel = new SettingsViewModel(current);
        var window = new SettingsWindow { Owner = Owner, DataContext = viewModel };
        return Task.FromResult(window.ShowDialog() == true ? viewModel.ToSettings() : null);
    }

    public void ShowError(string message) => Show(message, Caption, MessageBoxImage.Error);

    public void ShowAbout() => new AboutWindow { Owner = Owner }.ShowDialog();

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
