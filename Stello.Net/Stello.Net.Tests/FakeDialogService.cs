using Stello.Net.Models;
using Stello.Net.Services;

namespace Stello.Net.Tests;

internal sealed class FakeDialogService : IDialogService
{
    public string? OpenPath { get; set; }

    public string? SavePath { get; set; }

    public GameSettings? NewSettings { get; set; }

    public List<string> Errors { get; } = [];

    public int Beeps { get; private set; }

    public int AboutShown { get; private set; }

    public string? ShowOpenDialog() => OpenPath;

    public string? ShowSaveDialog(string? currentPath) => SavePath;

    public GameSettings? EditSettings(GameSettings current) => NewSettings;

    public void ShowError(string message) => Errors.Add(message);

    public void ShowAbout() => AboutShown++;

    public void Beep() => Beeps++;
}
