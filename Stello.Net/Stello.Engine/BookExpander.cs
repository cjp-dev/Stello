using System.Diagnostics;

namespace Stello.Engine;

internal enum ExpandStepKind
{
    Leaf,
    Deviation,
}

/// <param name="Line">Line to the expanded leaf (or deviation position), e.g. "d3c5f6".</param>
/// <param name="Move">Expanded/discovered move at the leaf or deviation.</param>
internal sealed record ExpandProgress(
    int Positions,
    int MaxPositions,
    int Ply,
    string Line,
    ExpandStepKind Kind,
    Square? Move,
    int Value,
    BookOrigin Origin,
    BookEffort Effort,
    long Priority,
    BookKey? PendingKeyAdded,
    BookKey? PendingKeyResolved,
    TimeSpan Elapsed);

/// <summary>
/// Lincke Drop-Out Expansion (<c>Stello.BookTool expand</c>, Phase 6 Item 4): expands the lowest-priority frontier
/// nodes (<c>omega * ply + cumulative_loss</c>) up to <c>maxPly</c> and <c>maxPositions</c>, with lazy one-shot
/// <c>onlyMoves</c> deviation searches on newly created book nodes and virtual reservation for parallel workers.
/// </summary>
internal sealed class BookExpander
{
    internal const int ProvenBoundaryPenalty = 1000;

    private readonly OpeningBook _book;
    private readonly SearchLimits _limits;
    private readonly int _omega;
    private readonly int _maxPly;
    private readonly int _maxPositions;
    private readonly int _maxIterations;
    private readonly TimeSpan? _maxTime;
    private readonly int _workers;
    private readonly int _hashBits;
    private readonly HashSet<BookKey> _pendingDeviations;
    private readonly HashSet<BookKey> _inFlightDeviations = [];
    private readonly HashSet<BookKey> _inFlightTargets = [];
    private readonly HashSet<BookKey> _terminalLeaves = [];

    public BookExpander(
        OpeningBook book,
        SearchLimits limits,
        int omega = 50,
        int maxPly = 28,
        int maxPositions = 6000,
        int maxIterations = int.MaxValue,
        TimeSpan? maxTime = null,
        int workers = 1,
        int hashBits = 19,
        IEnumerable<BookKey>? pendingDeviations = null)
    {
        ArgumentNullException.ThrowIfNull(book);
        ArgumentOutOfRangeException.ThrowIfNegative(omega);
        ArgumentOutOfRangeException.ThrowIfLessThan(maxPly, 1);
        ArgumentOutOfRangeException.ThrowIfLessThan(maxPositions, 1);
        ArgumentOutOfRangeException.ThrowIfLessThan(maxIterations, 1);
        ArgumentOutOfRangeException.ThrowIfLessThan(workers, 1);

        _book = book;
        _limits = limits;
        _omega = omega;
        _maxPly = maxPly;
        _maxPositions = maxPositions;
        _maxIterations = maxIterations;
        _maxTime = maxTime;
        _workers = workers;
        _hashBits = hashBits;
        _pendingDeviations = pendingDeviations is null ? [] : [.. pendingDeviations];
    }

    public IReadOnlyCollection<BookKey> PendingDeviations => _pendingDeviations;

    public void Run(
        Action<ExpandProgress>? stepped,
        Action? checkpoint,
        int saveEvery,
        CancellationToken cancellationToken)
    {
        if (!_book.Contains(OpeningBook.RootBoard, OpeningBook.RootPlayer)
            || (_book.PositionCount >= _maxPositions && _pendingDeviations.Count == 0))
        {
            return;
        }

        var clock = Stopwatch.StartNew();
        var gate = new object();
        int iterationsStarted = 0;
        int completions = 0;
        bool stopSignalled = false;

        using CancellationTokenRegistration cancelReg = cancellationToken.Register(() =>
        {
            lock (gate)
            {
                Monitor.PulseAll(gate);
            }
        });

        ExpandTask? AcquireTask()
        {
            while (true)
            {
                cancellationToken.ThrowIfCancellationRequested();

                bool allowLeaves = _book.PositionCount + _inFlightTargets.Count < _maxPositions;
                if (stopSignalled
                    || (!allowLeaves && _pendingDeviations.Count == 0 && _inFlightTargets.Count == 0)
                    || iterationsStarted >= _maxIterations
                    || (_maxTime is { } cap && clock.Elapsed >= cap))
                {
                    return null;
                }

                ExpandTask? task = SelectNextTask(allowLeaves);
                if (task is not null)
                {
                    iterationsStarted++;
                    if (task is DeviationTask dev)
                    {
                        _inFlightDeviations.Add(dev.Key);
                    }
                    else if (task is LeafTask leaf)
                    {
                        _inFlightTargets.Add(leaf.ChildKey);
                    }

                    return task;
                }

                if (_inFlightDeviations.Count == 0 && _inFlightTargets.Count == 0)
                {
                    return null;
                }

                Monitor.Wait(gate);
            }
        }

        void Work()
        {
            var engine = new SearchEngine(_hashBits);
            while (true)
            {
                ExpandTask? task;
                lock (gate)
                {
                    task = AcquireTask();
                }

                if (task is null)
                {
                    return;
                }

                try
                {
                    long start = Stopwatch.GetTimestamp();
                    switch (task)
                    {
                        case DeviationTask dev:
                            ExecuteDeviation(engine, dev, start, gate, stepped, checkpoint, saveEvery, ref completions, cancellationToken);
                            break;
                        case LeafTask leaf:
                            ExecuteLeaf(engine, leaf, start, gate, stepped, checkpoint, saveEvery, ref completions, cancellationToken);
                            break;
                    }
                }
                catch
                {
                    lock (gate)
                    {
                        stopSignalled = true;
                        ReleaseReservation(task);
                        Monitor.PulseAll(gate);
                    }

                    throw;
                }
            }
        }

        ParallelWork.Run(_workers, Work, cancellationToken);
        lock (gate)
        {
            BookMinimax.Run(_book);
        }
    }

    private void ExecuteDeviation(
        SearchEngine engine,
        DeviationTask dev,
        long start,
        object gate,
        Action<ExpandProgress>? stepped,
        Action? checkpoint,
        int saveEvery,
        ref int completions,
        CancellationToken cancellationToken)
    {
        ulong remaining = dev.Others;
        Square? linkedBest = null;
        short linkedBestVal = 0;

        lock (gate)
        {
            if (_book.TryGetReplies(dev.Key, out List<BookEntry>? replies))
            {
                short bestRaw = replies.Max(e => e.Value);
                foreach (Square sq in Square.InMask(remaining))
                {
                    (Board child, Player opp) = OpeningBook.Play(dev.Board, dev.Player, new Move(sq));
                    if (!_book.TryGetReplies(child, opp, out List<BookEntry>? childReplies, out _))
                    {
                        continue;
                    }

                    remaining &= ~sq.Bit;
                    short childVal = BookMinimax.Clamp(-childReplies.Max(e => (int)e.Value));
                    Move canonical = OpeningBook.Transform(new Move(sq), dev.Symmetry);
                    if (!replies.Exists(e => e.Move == canonical))
                    {
                        replies.Add(new BookEntry(canonical, childVal, BookOrigin.BackedUp));
                        if (linkedBest is null || childVal > linkedBestVal)
                        {
                            linkedBest = sq;
                            linkedBestVal = childVal;
                        }
                    }

                    _ = bestRaw;
                }
            }
        }

        if (remaining == 0)
        {
            lock (gate)
            {
                _pendingDeviations.Remove(dev.Key);
                _inFlightDeviations.Remove(dev.Key);
                BookMinimax.Run(_book);
                completions++;
                stepped?.Invoke(new ExpandProgress(
                    _book.PositionCount, _maxPositions, dev.Ply, dev.Line, ExpandStepKind.Deviation,
                    linkedBest, linkedBestVal, BookOrigin.BackedUp, default, dev.Priority,
                    null, dev.Key, Stopwatch.GetElapsedTime(start)));
                if (saveEvery > 0 && completions % saveEvery == 0)
                {
                    checkpoint?.Invoke();
                }

                Monitor.PulseAll(gate);
            }

            return;
        }

        engine.ClearHash();
        (int value, BookOrigin origin, BookEffort effort, Square square) =
            BookSearch.Search(engine, dev.Board, dev.Player, remaining, _limits, cancellationToken);
        short clamped = BookMinimax.Clamp(value);

        lock (gate)
        {
            if (_book.TryGetReplies(dev.Key, out List<BookEntry>? replies))
            {
                Move canonical = OpeningBook.Transform(new Move(square), dev.Symmetry);
                if (!replies.Exists(e => e.Move == canonical))
                {
                    replies.Add(new BookEntry(canonical, clamped, origin, effort));
                }
            }

            _pendingDeviations.Remove(dev.Key);
            _inFlightDeviations.Remove(dev.Key);
            BookMinimax.Run(_book);
            completions++;
            stepped?.Invoke(new ExpandProgress(
                _book.PositionCount, _maxPositions, dev.Ply, dev.Line, ExpandStepKind.Deviation,
                square, clamped, origin, effort, dev.Priority,
                null, dev.Key, Stopwatch.GetElapsedTime(start)));
            if (saveEvery > 0 && completions % saveEvery == 0)
            {
                checkpoint?.Invoke();
            }

            Monitor.PulseAll(gate);
        }
    }

    private void ExecuteLeaf(
        SearchEngine engine,
        LeafTask leaf,
        long start,
        object gate,
        Action<ExpandProgress>? stepped,
        Action? checkpoint,
        int saveEvery,
        ref int completions,
        CancellationToken cancellationToken)
    {
        ulong childMoves = leaf.Child.LegalMoves(leaf.ChildPlayer);
        if (childMoves == 0)
        {
            engine.ClearHash();
            (int posValue, BookOrigin posOrigin, BookEffort posEffort) =
                BookSearch.PositionValue(engine, leaf.Child, leaf.ChildPlayer, _limits, cancellationToken);
            short parentValue = BookMinimax.Clamp(-posValue);
            bool gameOver = !leaf.Child.HasLegalMove(leaf.ChildPlayer.Opponent());

            lock (gate)
            {
                if (gameOver)
                {
                    _terminalLeaves.Add(leaf.ChildKey);
                    if (_book.TryGetReplies(leaf.ParentKey, out List<BookEntry>? parentReplies))
                    {
                        Move canonical = OpeningBook.Transform(leaf.ParentMove, leaf.ParentSymmetry);
                        parentReplies.FirstOrDefault(e => e.Move == canonical)?.Set(parentValue, posOrigin, posEffort);
                    }
                }
                else if (!_book.Contains(leaf.Child, leaf.ChildPlayer))
                {
                    List<BookEntry> newReplies = _book.GetOrAddReplies(leaf.Child, leaf.ChildPlayer, out _);
                    newReplies.Add(new BookEntry(Move.Pass, BookMinimax.Clamp(posValue), posOrigin, posEffort));
                }

                _inFlightTargets.Remove(leaf.ChildKey);
                BookMinimax.Run(_book);
                completions++;
                stepped?.Invoke(new ExpandProgress(
                    _book.PositionCount, _maxPositions, leaf.Ply, leaf.Line, ExpandStepKind.Leaf,
                    null, gameOver ? parentValue : BookMinimax.Clamp(posValue), posOrigin, posEffort, leaf.Priority,
                    null, null, Stopwatch.GetElapsedTime(start)));
                if (saveEvery > 0 && completions % saveEvery == 0)
                {
                    checkpoint?.Invoke();
                }

                Monitor.PulseAll(gate);
            }

            return;
        }

        engine.ClearHash();
        (int value, BookOrigin origin, BookEffort effort, Square square) =
            BookSearch.Search(engine, leaf.Child, leaf.ChildPlayer, childMoves, _limits, cancellationToken);
        short clamped = BookMinimax.Clamp(value);

        lock (gate)
        {
            BookKey? addedPending = null;
            if (!_book.Contains(leaf.Child, leaf.ChildPlayer))
            {
                List<BookEntry> newReplies = _book.GetOrAddReplies(leaf.Child, leaf.ChildPlayer, out OpeningBook.Symmetry sym);
                newReplies.Add(new BookEntry(OpeningBook.Transform(new Move(square), sym), clamped, origin, effort));
                if (leaf.Ply < _maxPly && (leaf.Child.LegalMoves(leaf.ChildPlayer) & ~square.Bit) != 0)
                {
                    _pendingDeviations.Add(leaf.ChildKey);
                    addedPending = leaf.ChildKey;
                }
            }

            _inFlightTargets.Remove(leaf.ChildKey);
            BookMinimax.Run(_book);
            completions++;
            stepped?.Invoke(new ExpandProgress(
                _book.PositionCount, _maxPositions, leaf.Ply, leaf.Line, ExpandStepKind.Leaf,
                square, clamped, origin, effort, leaf.Priority,
                addedPending, null, Stopwatch.GetElapsedTime(start)));
            if (saveEvery > 0 && completions % saveEvery == 0)
            {
                checkpoint?.Invoke();
            }

            Monitor.PulseAll(gate);
        }
    }

    private void ReleaseReservation(ExpandTask task)
    {
        if (task is DeviationTask dev)
        {
            _inFlightDeviations.Remove(dev.Key);
        }
        else if (task is LeafTask leaf)
        {
            _inFlightTargets.Remove(leaf.ChildKey);
        }
    }

    private ExpandTask? SelectNextTask(bool allowLeaves)
    {
        if (!allowLeaves && _pendingDeviations.Count == 0)
        {
            return null;
        }

        (BookKey rootKey, _) = OpeningBook.Canonical(OpeningBook.RootBoard, OpeningBook.RootPlayer);
        var best = new Dictionary<BookKey, (long Priority, int Ply, string Line, Board Board, Player Player)>();
        var queue = new PriorityQueue<BookKey, (long Priority, int Ply, string Line)>(PriorityComparer.Instance);

        var rootState = ((long)_omega, 1, "d3", OpeningBook.RootBoard, OpeningBook.RootPlayer);
        best[rootKey] = rootState;
        queue.Enqueue(rootKey, ((long)_omega, 1, "d3"));

        ExpandTask? bestTask = null;

        while (queue.TryDequeue(out BookKey key, out (long Priority, int Ply, string Line) curPriority))
        {
            if (!best.TryGetValue(key, out var state)
                || curPriority.Priority > state.Priority
                || (curPriority.Priority == state.Priority && curPriority.Ply > state.Ply))
            {
                continue;
            }

            if (bestTask is not null && curPriority.Priority > bestTask.Priority)
            {
                // Dijkstra yields nodes in non-decreasing Priority; no later node can yield a smaller task priority.
                break;
            }

            if (!_book.TryGetReplies(key, out List<BookEntry>? replies) || replies.Count == 0)
            {
                continue;
            }

            (_, OpeningBook.Symmetry symmetry) = OpeningBook.Canonical(state.Board, state.Player);

            if (state.Ply < _maxPly && _pendingDeviations.Contains(key))
            {
                if (_inFlightDeviations.Contains(key))
                {
                    continue;
                }

                ulong inBook = 0;
                foreach (BookEntry entry in replies)
                {
                    if (OpeningBook.Transform(entry.Move, symmetry).Square is { } sq)
                    {
                        inBook |= sq.Bit;
                    }
                }

                ulong others = state.Board.LegalMoves(state.Player) & ~inBook;
                if (others == 0)
                {
                    _pendingDeviations.Remove(key);
                }
                else
                {
                    var candidate = new DeviationTask(key, state.Board, state.Player, symmetry, others, state.Ply, state.Line, curPriority.Priority);
                    if (IsBetter(candidate, bestTask))
                    {
                        bestTask = candidate;
                    }

                    continue;
                }
            }

            if (state.Ply >= _maxPly)
            {
                continue;
            }

            short bestRaw = replies.Max(e => e.Value);
            foreach (BookEntry entry in replies)
            {
                Move move = OpeningBook.Transform(entry.Move, symmetry);
                long edgeLoss = EdgeLoss(bestRaw, entry.Value);
                long nextPriority = curPriority.Priority + _omega + edgeLoss;
                int nextPly = state.Ply + 1;
                string nextLine = state.Line + BookTextFormat.MoveText(move);

                (Board child, Player opponent) = OpeningBook.Play(state.Board, state.Player, move);
                BookKey childKey = OpeningBook.Canonical(child, opponent).Key;

                if (_book.Contains(child, opponent))
                {
                    if (!best.TryGetValue(childKey, out var existing)
                        || nextPriority < existing.Priority
                        || (nextPriority == existing.Priority && nextPly < existing.Ply)
                        || (nextPriority == existing.Priority && nextPly == existing.Ply && string.CompareOrdinal(nextLine, existing.Line) < 0))
                    {
                        best[childKey] = (nextPriority, nextPly, nextLine, child, opponent);
                        queue.Enqueue(childKey, (nextPriority, nextPly, nextLine));
                    }
                }
                else if (allowLeaves && !_inFlightTargets.Contains(childKey) && !_terminalLeaves.Contains(childKey))
                {
                    var candidate = new LeafTask(
                        key, move, child, opponent, childKey, symmetry, nextPly, nextLine, nextPriority);
                    if (IsBetter(candidate, bestTask))
                    {
                        bestTask = candidate;
                    }
                }
            }
        }

        return bestTask;
    }

    internal static long EdgeLoss(short bestRaw, short candidateRaw)
    {
        if (candidateRaw >= bestRaw)
        {
            return 0;
        }

        long baseLoss = Math.Max(0, BookTrimmer.ScoreInUnits(bestRaw) - BookTrimmer.ScoreInUnits(candidateRaw));
        if ((bestRaw > SearchEngine.WinScore && candidateRaw <= SearchEngine.WinScore)
            || (bestRaw >= -SearchEngine.WinScore && candidateRaw < -SearchEngine.WinScore))
        {
            baseLoss += ProvenBoundaryPenalty;
        }

        return baseLoss;
    }

    private static bool IsBetter(ExpandTask candidate, ExpandTask? current) =>
        current is null
        || candidate.Priority < current.Priority
        || (candidate.Priority == current.Priority && candidate.Ply < current.Ply)
        || (candidate.Priority == current.Priority && candidate.Ply == current.Ply && string.CompareOrdinal(candidate.Line, current.Line) < 0);

    private sealed class PriorityComparer : IComparer<(long Priority, int Ply, string Line)>
    {
        public static readonly PriorityComparer Instance = new();

        public int Compare((long Priority, int Ply, string Line) x, (long Priority, int Ply, string Line) y)
        {
            int cmp = x.Priority.CompareTo(y.Priority);
            if (cmp != 0)
            {
                return cmp;
            }

            cmp = x.Ply.CompareTo(y.Ply);
            return cmp != 0 ? cmp : string.CompareOrdinal(x.Line, y.Line);
        }
    }

    private abstract record ExpandTask(int Ply, string Line, long Priority);

    private sealed record DeviationTask(
        BookKey Key,
        Board Board,
        Player Player,
        OpeningBook.Symmetry Symmetry,
        ulong Others,
        int Ply,
        string Line,
        long Priority) : ExpandTask(Ply, Line, Priority);

    private sealed record LeafTask(
        BookKey ParentKey,
        Move ParentMove,
        Board Child,
        Player ChildPlayer,
        BookKey ChildKey,
        OpeningBook.Symmetry ParentSymmetry,
        int Ply,
        string Line,
        long Priority) : ExpandTask(Ply, Line, Priority);
}
