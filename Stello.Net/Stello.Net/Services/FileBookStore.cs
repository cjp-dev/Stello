using System.IO;
using Stello.App.Services;
using Stello.Engine;

namespace Stello.Net.Services;

/// <summary>Writes the learned book to the user's folder; the book shipped with the app is never changed.</summary>
public sealed class FileBookStore(string bookPath, string selfPlayLogPath) : IBookStore
{
    public bool Save(OpeningBook book)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(bookPath))!);

            // Write to a temporary file first so a crash cannot leave half a book.
            string temporary = bookPath + ".tmp";
            book.Save(temporary);
            File.Move(temporary, bookPath, overwrite: true);
            return true;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            return false;
        }
    }

    public void AppendSelfPlayLog(string line)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(selfPlayLogPath))!);
            File.AppendAllText(selfPlayLogPath, line + Environment.NewLine);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            // The log is only informational.
        }
    }
}
