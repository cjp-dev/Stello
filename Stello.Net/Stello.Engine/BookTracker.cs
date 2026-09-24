namespace Stello.Engine;

/// <summary>
/// Decides when to ask the opening book (C++: libon/tryagain): after three misses in a row the game has left
/// the book; a hit (the game transposed back into the book) starts the count again.
/// </summary>
public sealed class BookTracker
{
    private const int MaxMisses = 3;

    private int _misses;

    public bool ShouldConsult => _misses < MaxMisses;

    /// <summary>Call on a new game, and when moves are taken back.</summary>
    public void Reset() => _misses = 0;

    public void Record(bool found) => _misses = found ? 0 : _misses + 1;
}
