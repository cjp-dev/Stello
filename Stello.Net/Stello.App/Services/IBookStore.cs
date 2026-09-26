using Stello.Engine;

namespace Stello.Net.Services;

public interface IBookStore
{
    /// <returns>False if the book could not be written.</returns>
    bool Save(OpeningBook book);

    /// <summary>Appends a line to the self-play log (C++: the "selfplay" file).</summary>
    void AppendSelfPlayLog(string line);
}
