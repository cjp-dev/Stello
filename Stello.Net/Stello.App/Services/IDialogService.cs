using Stello.App.Models;
using Stello.Engine;

namespace Stello.App.Services;

public interface IDialogService
{
    Task<bool> ConfirmAsync(string message);

    /// <returns>Who won the game, or null if the user cancelled.</returns>
    Task<GameResult?> AskGameResultAsync();

    /// <returns>The new settings, or null if the user cancelled.</returns>
    Task<GameSettings?> EditSettingsAsync(GameSettings current);

    void ShowError(string message);

    void ShowAbout();

    void Beep();
}
