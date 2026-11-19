using System.Diagnostics;

namespace Stello.Engine;

/// <param name="Line">The shortest book line to the checked position, e.g. "d3c5".</param>
/// <param name="Added">How many missing moves were added at this position.</param>
/// <param name="BestAdded">Best missing move added, if any.</param>
/// <param name="BestAddedValue">Value of <paramref name="BestAdded"/> (or 0 if none added).</param>
/// <param name="Key">Canonical key stored in the resume log.</param>
internal sealed record CalclibProgress(
    int Done,
    int Total,
    int Ply,
    string Line,
    int Added,
    Square? BestAdded,
    short BestAddedValue,
    BookKey Key,
    TimeSpan Elapsed);

/// <summary>
/// Searches unbooked legal moves across book positions using <c>onlyMoves</c> (Phase 6 Item 3, <c>calclib</c>) and
/// adds any missing moves within a score threshold of the position's best book move.
/// </summary>
internal sealed class BookCalclib
{
    private readonly OpeningBook _book;
    private readonly SearchLimits _limits;
    private readonly int _threshold;
    private readonly int _maxAdds;
    private readonly int _workers;
    private readonly int _hashBits;
    private readonly List<PendingPosition> _pending;

    public BookCalclib(
        OpeningBook book,
        SearchLimits limits,
        int threshold = 100,
        int maxPly = 30,
        int maxAdds = 3,
        int workers = 1,
        int hashBits = 19,
        IReadOnlySet<BookKey>? completedKeys = null)
    {
        ArgumentNullException.ThrowIfNull(book);
        ArgumentOutOfRangeException.ThrowIfNegative(threshold);
        ArgumentOutOfRangeException.ThrowIfLessThan(maxPly, 1);
        ArgumentOutOfRangeException.ThrowIfLessThan(maxAdds, 1);
        ArgumentOutOfRangeException.ThrowIfLessThan(workers, 1);

        _book = book;
        _limits = limits;
        _threshold = threshold;
        _maxAdds = maxAdds;
        _workers = workers;
        _hashBits = hashBits;

        List<PendingPosition> candidates = CollectPositions(book, maxPly);
        CandidatePositions = candidates.Count;
        _pending = completedKeys is null
            ? candidates
            : candidates.Where(pos => !completedKeys.Contains(pos.Key)).ToList();
    }

    /// <summary>Positions in the book (within <c>maxPly</c>) that have at least one unbooked legal move.</summary>
    public int CandidatePositions { get; }

    /// <summary>Positions remaining to check after excluding resumed keys.</summary>
    public int ToSearch => _pending.Count;

    public void Run(
        Action<CalclibProgress>? checkedPosition,
        Action? checkpoint,
        int saveEvery,
        CancellationToken cancellationToken)
    {
        if (_pending.Count == 0)
        {
            return;
        }

        int next = -1;
        int done = 0;
        var gate = new object();

        void Work()
        {
            var engine = new SearchEngine(_hashBits);
            int index;
            while ((index = Interlocked.Increment(ref next)) < _pending.Count)
            {
                PendingPosition pos = _pending[index];
                ulong remaining = pos.Others;
                int added = 0;
                Square? bestAdded = null;
                short bestAddedValue = 0;
                long start = Stopwatch.GetTimestamp();

                for (int round = 0; round < _maxAdds && remaining != 0; round++)
                {
                    short bestRaw;
                    lock (gate)
                    {
                        if (!_book.TryGetReplies(pos.Key, out List<BookEntry>? existing))
                        {
                            break;
                        }

                        bestRaw = existing.Max(e => e.Value);

                        // Link any unbooked legal move whose child position is already in the book without searching it.
                        foreach (Square sq in Square.InMask(remaining))
                        {
                            (Board child, Player opponent) = OpeningBook.Play(pos.Board, pos.Player, new Move(sq));
                            if (!_book.TryGetReplies(child, opponent, out List<BookEntry>? childReplies, out _))
                            {
                                continue;
                            }

                            remaining &= ~sq.Bit;
                            short childVal = BookMinimax.Clamp(-childReplies.Max(e => (int)e.Value));
                            if (IsWithinThreshold(bestRaw, childVal, _threshold))
                            {
                                Move canonicalLink = OpeningBook.Transform(new Move(sq), pos.Symmetry);
                                if (!existing.Exists(e => e.Move == canonicalLink))
                                {
                                    existing.Add(new BookEntry(canonicalLink, childVal, BookOrigin.BackedUp));
                                    added++;
                                    if (childVal > bestRaw)
                                    {
                                        bestRaw = childVal;
                                    }

                                    if (bestAdded is null || childVal > bestAddedValue)
                                    {
                                        bestAdded = sq;
                                        bestAddedValue = childVal;
                                    }
                                }
                            }
                        }
                    }

                    if (remaining == 0)
                    {
                        break;
                    }

                    // Clean hash per position makes calclib deterministic regardless of worker count.
                    engine.ClearHash();
                    (int value, BookOrigin origin, BookEffort effort, Square square) =
                        BookSearch.Search(engine, pos.Board, pos.Player, remaining, _limits, cancellationToken);
                    short clamped = BookMinimax.Clamp(value);

                    bool beatBest;
                    lock (gate)
                    {
                        if (!_book.TryGetReplies(pos.Key, out List<BookEntry>? replies))
                        {
                            break;
                        }

                        bestRaw = replies.Max(e => e.Value);
                        if (!IsWithinThreshold(bestRaw, clamped, _threshold))
                        {
                            break;
                        }

                        beatBest = clamped > bestRaw;
                        Move canonicalMove = OpeningBook.Transform(new Move(square), pos.Symmetry);
                        if (!replies.Exists(e => e.Move == canonicalMove))
                        {
                            replies.Add(new BookEntry(canonicalMove, clamped, origin, effort));
                            added++;
                            if (bestAdded is null || clamped > bestAddedValue)
                            {
                                bestAdded = square;
                                bestAddedValue = clamped;
                            }
                        }
                    }

                    if (!beatBest)
                    {
                        break;
                    }

                    remaining &= ~square.Bit;
                }

                lock (gate)
                {
                    done++;
                    checkedPosition?.Invoke(new CalclibProgress(
                        done,
                        _pending.Count,
                        pos.Ply,
                        pos.Line,
                        added,
                        bestAdded,
                        bestAddedValue,
                        pos.Key,
                        Stopwatch.GetElapsedTime(start)));
                    if (saveEvery > 0 && done % saveEvery == 0)
                    {
                        BookMinimax.Run(_book);
                        checkpoint?.Invoke();
                    }
                }
            }
        }

        ParallelWork.Run(Math.Min(_workers, _pending.Count), Work, cancellationToken);
    }

    internal static bool IsWithinThreshold(short bestRaw, short candidateRaw, int threshold)
    {
        if (candidateRaw >= bestRaw)
        {
            return true;
        }

        // Do not add an unproven heuristic/loss alternative if the book already has a proven win.
        if (bestRaw > SearchEngine.WinScore && candidateRaw <= SearchEngine.WinScore)
        {
            return false;
        }

        int loss = Math.Max(0, BookTrimmer.ScoreInUnits(bestRaw) - BookTrimmer.ScoreInUnits(candidateRaw));
        return loss <= threshold;
    }

    private static List<PendingPosition> CollectPositions(OpeningBook book, int maxPly)
    {
        var list = new List<PendingPosition>();
        foreach (BookLine line in BookTextFormat.Lines(book))
        {
            int ply = line.Moves.Length / 2;
            if (ply > maxPly)
            {
                continue;
            }

            (BookKey key, OpeningBook.Symmetry symmetry) = OpeningBook.Canonical(line.Board, line.Player);
            book.TryGetReplies(key, out List<BookEntry>? replies);

            ulong inBook = 0;
            foreach (BookEntry entry in replies!)
            {
                Move realMove = OpeningBook.Transform(entry.Move, symmetry);
                if (realMove.Square is { } square)
                {
                    inBook |= square.Bit;
                }
            }

            ulong others = line.Board.LegalMoves(line.Player) & ~inBook;
            if (others != 0)
            {
                list.Add(new PendingPosition(line.Board, line.Player, ply, line.Moves, key, others, symmetry));
            }
        }

        return list;
    }

    private sealed record PendingPosition(
        Board Board,
        Player Player,
        int Ply,
        string Line,
        BookKey Key,
        ulong Others,
        OpeningBook.Symmetry Symmetry);
}
