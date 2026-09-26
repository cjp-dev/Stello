using Stello.App.Models;
using Stello.App.Services;
using Stello.Engine;

namespace Stello.Net.Tests;

internal sealed class FakeDialogService : IDialogService
{
    public bool ConfirmAnswer { get; set; } = true;

    public GameResult? GameResultAnswer { get; set; }

    public int Confirmations { get; private set; }

    public GameSettings? NewSettings { get; set; }

    public List<string> Errors { get; } = [];

    public int Beeps { get; private set; }

    public int AboutShown { get; private set; }

    public Task<GameSettings?> EditSettingsAsync(GameSettings current) => Task.FromResult(NewSettings);

    public void ShowError(string message) => Errors.Add(message);

    public void ShowAbout() => AboutShown++;

    public void Beep() => Beeps++;

    public Task<bool> ConfirmAsync(string message)
    {
        Confirmations++;
        return Task.FromResult(ConfirmAnswer);
    }

    public Task<GameResult?> AskGameResultAsync() => Task.FromResult(GameResultAnswer);
}
