namespace Stello.App.Services;

/// <summary>Opens and saves game records; on the desktop a name is a full path, in the browser a file name.</summary>
public interface IGameFileService
{
    /// <returns>The chosen file, or null if the user cancelled.</returns>
    /// <exception cref="IOException">The file could not be read.</exception>
    Task<GameFile?> OpenAsync();

    /// <param name="currentName">The file the game was last opened from or saved to, if any.</param>
    /// <param name="askForName">Ask the user for a name even if there is a current one (Save As).</param>
    /// <returns>The name the game was saved as, or null if the user cancelled.</returns>
    /// <exception cref="IOException">The file could not be written.</exception>
    Task<string?> SaveAsync(string text, string? currentName, bool askForName);
}

public sealed record GameFile(string Name, string Text);
