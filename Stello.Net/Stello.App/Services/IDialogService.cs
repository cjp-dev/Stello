using Stello.Engine;
using Stello.Net.Models;

namespace Stello.Net.Services;

public interface IDialogService
{
    bool Confirm(string message);

    /// <returns>Who won the game, or null if the user cancelled.</returns>
    GameResult? AskGameResult();

    /// <returns>The chosen file, or null if the user cancelled.</returns>
    string? ShowOpenDialog();

    /// <returns>The chosen file, or null if the user cancelled.</returns>
    string? ShowSaveDialog(string? currentPath);

    /// <returns>The new settings, or null if the user cancelled.</returns>
    GameSettings? EditSettings(GameSettings current);

    void ShowError(string message);

    void ShowAbout();

    void Beep();
}
