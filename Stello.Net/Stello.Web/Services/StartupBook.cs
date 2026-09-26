using Stello.Engine;

namespace Stello.Web.Services;

/// <summary>Loads the user's book from local storage, otherwise the book shipped with the app (C++: Get_book).</summary>
public sealed class StartupBook(HttpClient http, LocalStorageBookStore store)
{
    private const string DefaultBookUrl = "data/OPENING.bin";

    public OpeningBook Book { get; private set; } = OpeningBook.CreateEmpty();

    /// <summary>Shown in the status bar when a book could not be read.</summary>
    public string? Notice { get; private set; }

    public async Task LoadAsync()
    {
        try
        {
            if (store.Load() is { } saved)
            {
                Book = saved;
                return;
            }
        }
        catch (InvalidDataException)
        {
            Notice = "The saved opening book could not be read.";
        }

        try
        {
            using var stream = new MemoryStream(await http.GetByteArrayAsync(DefaultBookUrl));
            Book = OpeningBook.Load(stream);
        }
        catch (Exception exception) when (exception is HttpRequestException or InvalidDataException)
        {
            Notice = "No opening book was found.";
        }
    }
}
