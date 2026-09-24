using System.IO;
using Stello.Engine;

namespace Stello.Net.Services;

public static class BookLoader
{
    /// <summary>Loads the first book file that exists and can be read.</summary>
    /// <param name="notice">Set when a file could not be read, or when no book was found.</param>
    public static OpeningBook? Load(IEnumerable<string> paths, out string? notice)
    {
        notice = null;
        foreach (string path in paths.Where(File.Exists))
        {
            try
            {
                return OpeningBook.Load(path);
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or InvalidDataException)
            {
                notice = $"The opening book {path} could not be read.";
            }
        }

        notice ??= "No opening book was found.";
        return null;
    }
}
