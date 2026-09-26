using System.IO;
using Stello.App.Services;

namespace Stello.Net.Tests;

/// <summary>Game files in memory; the name the user would pick in the dialogs is set beforehand.</summary>
internal sealed class FakeGameFileService : IGameFileService
{
    public Dictionary<string, string> Files { get; } = [];

    public string? OpenName { get; set; }

    public string? SaveName { get; set; }

    public Task<GameFile?> OpenAsync()
    {
        if (OpenName is null)
        {
            return Task.FromResult<GameFile?>(null);
        }

        return Files.TryGetValue(OpenName, out string? text)
            ? Task.FromResult<GameFile?>(new GameFile(OpenName, text))
            : Task.FromException<GameFile?>(new FileNotFoundException($"Could not find file '{OpenName}'."));
    }

    public Task<string?> SaveAsync(string text, string? currentName, bool askForName)
    {
        string? name = askForName || currentName is null ? SaveName : currentName;
        if (name is not null)
        {
            Files[name] = text;
        }

        return Task.FromResult(name);
    }
}
