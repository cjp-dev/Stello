using System.IO;

namespace Stello.Net.Services;

/// <summary>Where the app keeps its files; nothing depends on the current directory (C++ used it for everything).</summary>
public static class AppPaths
{
    public static string DataDirectory { get; } =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Stello");

    public static string SettingsFile => Path.Combine(DataDirectory, "settings.json");

    /// <summary>The user's own copy of the book, written when the book is changed (book learning).</summary>
    public static string UserBookFile => Path.Combine(DataDirectory, "OPENING");

    /// <summary>The book shipped with the app.</summary>
    public static string DefaultBookFile => Path.Combine(AppContext.BaseDirectory, "Data", "OPENING");

    /// <summary>The user's book first, then the shipped book.</summary>
    public static IEnumerable<string> BookFiles => [UserBookFile, DefaultBookFile];
}
