using Microsoft.JSInterop;
using Stello.App.Models;
using Stello.App.Services;
using Stello.App.ViewModels;
using Stello.Engine;
using Stello.Web.Components;
using GameShell;

namespace Stello.Web.Services;

/// <summary>The game's dialogs, shown by GameShell's DialogHost through ShellDialogs.</summary>
public sealed class BrowserDialogService(IJSInProcessRuntime js, ShellDialogs dialogs) : IDialogService
{
    private const string Caption = "Stello";

    public async Task<bool> ConfirmAsync(string message) =>
        await ShowAsync(new DialogRequest { Title = Caption, Message = message, Buttons = ["Yes", "No"] }) == 0;

    // C++: "Var det sort der vandt ?"
    public async Task<GameResult?> AskGameResultAsync() =>
        await ShowAsync(new DialogRequest { Title = Caption, Message = "Who won the game?", Buttons = ["Black won", "White won", "Cancel"] }) switch
        {
            0 => GameResult.BlackWins,
            1 => GameResult.WhiteWins,
            _ => null,
        };

    public async Task<GameSettings?> EditSettingsAsync(GameSettings current)
    {
        var settings = new SettingsViewModel(current);
        int button = await ShowAsync(new DialogRequest { Title = "Settings", Body = SettingsForm.For(settings), Buttons = ["OK", "Cancel"] });
        return button == 0 ? settings.ToSettings() : null;
    }

    public void ShowError(string message) =>
        _ = ShowAsync(new DialogRequest { Title = Caption, Message = message, Buttons = ["OK"] });

    public void ShowAbout() => _ = ShowAsync(new DialogRequest
    {
        Title = AboutInfo.Title,
        Heading = AboutInfo.Version,
        Paragraphs = AboutInfo.Paragraphs,
        Picture = "images/claus-pedersen.jpg",
        PictureCaption = AboutInfo.PictureCaption,
        Buttons = ["OK"],
    });

    public void Beep() => js.InvokeVoid("stello.beep");

    /// <returns>The text the user entered, or null if the user cancelled.</returns>
    public async Task<string?> AskTextAsync(string message, string text)
    {
        var request = new DialogRequest { Title = Caption, Message = message, Text = text, Buttons = ["OK", "Cancel"] };
        return await ShowAsync(request) == 0 ? request.Text : null;
    }

    private Task<int> ShowAsync(DialogRequest request) => dialogs.ShowAsync(request);
}
