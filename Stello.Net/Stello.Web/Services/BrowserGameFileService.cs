using Microsoft.JSInterop;
using Stello.App.Services;

namespace Stello.Web.Services;

/// <summary>Opens a game with the browser's file picker and saves it as a download.</summary>
public sealed class BrowserGameFileService(IJSRuntime js, BrowserDialogService dialogs) : IGameFileService
{
    private const string DefaultExtension = ".stello";

    public async Task<GameFile?> OpenAsync()
    {
        string[]? file;
        try
        {
            file = await js.InvokeAsync<string[]?>("stello.openTextFile", ".stello,.txt");
        }
        catch (JSException exception)
        {
            throw new IOException(exception.Message, exception);
        }

        return file is [string name, string text] ? new GameFile(name, text) : null;
    }

    public async Task<string?> SaveAsync(string text, string? currentName, bool askForName)
    {
        string? name = currentName;
        if (askForName || name is null)
        {
            name = (await dialogs.AskTextAsync("Save the game as:", currentName ?? "Game" + DefaultExtension))?.Trim();
            if (string.IsNullOrEmpty(name))
            {
                return null;
            }

            if (Path.GetExtension(name).Length == 0)
            {
                name += DefaultExtension;
            }
        }

        await js.InvokeVoidAsync("stello.downloadTextFile", name, text);
        return name;
    }
}
