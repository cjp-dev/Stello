using System.Diagnostics;

namespace Stello.Engine;

/// <param name="Line">The moves to the leaf position, e.g. "d3c5f6".</param>
/// <param name="Value">The new value of the book moves that lead to the leaf position.</param>
internal sealed record RecalcProgress(int Done, int Total, int Ply, string Line, int Value, BookOrigin Origin, BookEffort Effort, TimeSpan Elapsed);

/// <summary>
/// Searches the book's leaves again with a given search limit, nearest the start first, on several engines at once
/// (book tool <c>recalc</c>). A leaf is a book move whose position after it is not in the book; book moves that lead
/// to the same position share one search. Values that are exact, or searched at least as hard with the current
/// engine version, are kept, so a stopped run can be started again.
/// </summary>
internal sealed class BookRecalculator
{
    private readonly SearchLimits _limits;
    private readonly int _workers;
    private readonly int _hashBits;
    private readonly BookEffort _target;
    private readonly List<Leaf> _pending;

    /// <param name="hashBits">Hash table size of each engine (see <see cref="SearchEngine"/>).</param>
    public BookRecalculator(OpeningBook book, SearchLimits limits, int workers, int hashBits = 19)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(workers, 1);
        _limits = limits;
        _workers = workers;
        _hashBits = hashBits;
        _target = BookEffort.For(limits, 0);

        List<Leaf> leaves = CollectLeaves(book);
        LeafPositions = leaves.Count;
        _pending = leaves.Where(leaf => leaf.Entries.Exists(NeedsSearch)).ToList();
    }

    /// <summary>The number of different positions after the book's leaves.</summary>
    public int LeafPositions { get; }

    /// <summary>The number of those positions that will be searched.</summary>
    public int ToSearch => _pending.Count;

    /// <summary>Searches the leaf positions that are not good enough yet and stores the values in the book.</summary>
    /// <param name="searched">Called after each search, one at a time; the book is not changed meanwhile.</param>
    /// <param name="checkpoint">Called after every <paramref name="saveEvery"/> searches, like <paramref name="searched"/>.</param>
    /// <exception cref="OperationCanceledException"><paramref name="cancellationToken"/> was cancelled.</exception>
    public void Run(Action<RecalcProgress>? searched, Action? checkpoint, int saveEvery, CancellationToken cancellationToken)
    {
        int next = -1;
        int done = 0;
        var gate = new object();

        void Work()
        {
            var engine = new SearchEngine(_hashBits);
            int index;
            while ((index = Interlocked.Increment(ref next)) < _pending.Count)
            {
                Leaf leaf = _pending[index];

                // A clean hash table makes the value independent of the order and the number of workers.
                engine.ClearHash();
                long start = Stopwatch.GetTimestamp();
                (int value, BookOrigin origin, BookEffort effort) =
                    BookSearch.PositionValue(engine, leaf.Board, leaf.Player, _limits, cancellationToken);
                short entryValue = BookMinimax.Clamp(-value);

                lock (gate)
                {
                    foreach (BookEntry entry in leaf.Entries.Where(NeedsSearch))
                    {
                        // A proven win, loss or draw is kept when the new search could not solve the position.
                        if (entry.Origin == BookOrigin.WinLossDraw && origin == BookOrigin.Heuristic)
                        {
                            entry.Set(entry.Value, BookOrigin.WinLossDraw, effort);
                        }
                        else
                        {
                            entry.Set(entryValue, origin, effort);
                        }
                    }

                    done++;
                    searched?.Invoke(new RecalcProgress(
                        done, _pending.Count, leaf.Ply, leaf.Line, entryValue, origin, effort, Stopwatch.GetElapsedTime(start)));
                    if (saveEvery > 0 && done % saveEvery == 0)
                    {
                        checkpoint?.Invoke();
                    }
                }
            }
        }

        ParallelWork.Run(Math.Min(_workers, _pending.Count), Work, cancellationToken);
    }

    private bool NeedsSearch(BookEntry entry) =>
        entry.Origin != BookOrigin.Exact
        && !(entry.IsSearched
            && entry.Effort.EngineVersion == OpeningBook.EngineVersion
            && entry.Effort.Kind == _target.Kind
            && entry.Effort.Amount >= _target.Amount);

    // In the order of the text book, so the leaves nearest the start come first.
    private static List<Leaf> CollectLeaves(OpeningBook book)
    {
        var leaves = new Dictionary<BookKey, Leaf>();
        var order = new List<Leaf>();
        foreach (BookLine line in BookTextFormat.Lines(book))
        {
            book.TryGetReplies(line.Board, line.Player, out List<BookEntry>? replies, out OpeningBook.Symmetry symmetry);
            foreach (BookEntry entry in replies!)
            {
                Move move = OpeningBook.Transform(entry.Move, symmetry);
                (Board child, Player opponent) = OpeningBook.Play(line.Board, line.Player, move);
                BookKey key = OpeningBook.Canonical(child, opponent).Key;
                if (book.TryGetReplies(key, out _))
                {
                    continue;
                }

                if (!leaves.TryGetValue(key, out Leaf? leaf))
                {
                    leaf = new Leaf(child, opponent, line.Moves.Length / 2 + 1, line.Moves + BookTextFormat.MoveText(move));
                    leaves.Add(key, leaf);
                    order.Add(leaf);
                }

                leaf.Entries.Add(entry);
            }
        }

        return order;
    }

    private sealed record Leaf(Board Board, Player Player, int Ply, string Line)
    {
        public List<BookEntry> Entries { get; } = [];
    }
}
