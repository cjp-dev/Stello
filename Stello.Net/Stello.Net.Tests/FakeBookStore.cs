using Stello.App.Services;
using Stello.Engine;

namespace Stello.Net.Tests;

internal sealed class FakeBookStore : IBookStore
{
    private int _saves;

    public int Saves => _saves;

    public List<string> Log { get; } = [];

    // Checkpoints are saved from the learning thread.
    public bool Save(OpeningBook book)
    {
        Interlocked.Increment(ref _saves);
        return true;
    }

    public void AppendSelfPlayLog(string line)
    {
        lock (Log)
        {
            Log.Add(line);
        }
    }
}
