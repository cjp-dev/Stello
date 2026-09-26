using Microsoft.JSInterop;
using Stello.App.Services;
using Stello.Engine;

namespace Stello.Web.Services;

/// <summary>The book with the user's added games, base64 in local storage; the shipped book is never changed.</summary>
public sealed class LocalStorageBookStore(IJSInProcessRuntime js) : IBookStore
{
    private const string Key = "stello.book";

    /// <returns>The saved book, or null if the user has not added any games.</returns>
    /// <exception cref="InvalidDataException">The saved book is damaged.</exception>
    public OpeningBook? Load()
    {
        string? base64 = js.Invoke<string?>("localStorage.getItem", Key);
        if (base64 is null)
        {
            return null;
        }

        try
        {
            using var stream = new MemoryStream(Convert.FromBase64String(base64));
            return OpeningBook.Load(stream);
        }
        catch (FormatException exception)
        {
            throw new InvalidDataException("The saved opening book is damaged.", exception);
        }
    }

    public bool Save(OpeningBook book)
    {
        using var stream = new MemoryStream();
        book.Save(stream);
        return Save(stream.ToArray());
    }

    /// <param name="book">The book in the OPENING file format.</param>
    public bool Save(byte[] book)
    {
        try
        {
            js.InvokeVoid("localStorage.setItem", Key, Convert.ToBase64String(book));
            return true;
        }
        catch (JSException)
        {
            return false;
        }
    }

    // Self-play is not offered in the browser.
    public void AppendSelfPlayLog(string line)
    {
    }
}
