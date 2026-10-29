namespace Stello.Engine;

/// <summary>What one side did in a game: the moves it searched, their nodes and time, and how deep it got.</summary>
/// <param name="Searched">Moves that needed a search (not book moves, passes or single legal moves).</param>
/// <param name="MidgameMoves">Searched moves that ended in the midgame search (a heuristic score).</param>
/// <param name="MidgameDepth">The depths of those moves added up.</param>
/// <param name="FirstSolve">The most empty squares at which this side solved the game (win/loss/draw or exact), or 0.</param>
internal readonly record struct SideStats(int Searched, long Nodes, TimeSpan Time, int MidgameMoves, long MidgameDepth, int FirstSolve)
{
    public SideStats Add(SearchResult result, int empties)
    {
        if (result.Nodes == 0 && result.Kind is ScoreKind.None or ScoreKind.Book)
        {
            return this;
        }

        bool midgame = result.Kind == ScoreKind.Heuristic;
        bool solved = result.Kind is ScoreKind.WinLossDraw or ScoreKind.Exact;
        return new SideStats(
            Searched + 1,
            Nodes + result.Nodes,
            Time + result.Elapsed,
            MidgameMoves + (midgame ? 1 : 0),
            MidgameDepth + (midgame ? result.Depth : 0),
            solved ? Math.Max(FirstSolve, empties) : FirstSolve);
    }
}

/// <param name="ColourB">The colour side B played.</param>
/// <param name="DiscsB">Side B's discs minus side A's discs at the end.</param>
internal sealed record MatchGame(Player ColourB, IReadOnlyList<Move> Moves, int DiscsB)
{
    public double PointsB => DiscsB > 0 ? 1 : DiscsB == 0 ? 0.5 : 0;

    public SideStats StatsA { get; init; }

    public SideStats StatsB { get; init; }
}

/// <param name="Start">The start position, as a line from d3.</param>
/// <param name="AFirst">Side A plays the side to move in the start position.</param>
/// <param name="BFirst">Side B plays the side to move.</param>
internal sealed record MatchPair(string Start, MatchGame AFirst, MatchGame BFirst)
{
    public double PointsB => AFirst.PointsB + BFirst.PointsB;
}

/// <param name="Score">Side B's share of the points, 0 to 1.</param>
/// <param name="Low">The low end of the 95 % interval of <paramref name="Score"/>, over the pairs.</param>
internal sealed record MatchSummary(int Pairs, double PointsB, double Score, double Low, double High, double AverageDiscsB)
{
    public static MatchSummary Of(IReadOnlyList<MatchPair> pairs)
    {
        if (pairs.Count == 0)
        {
            return new MatchSummary(0, 0, 0.5, 0.5, 0.5, 0);
        }

        double[] scores = pairs.Select(pair => pair.PointsB / 2).ToArray();
        double mean = scores.Average();
        double variance = pairs.Count > 1 ? scores.Sum(s => (s - mean) * (s - mean)) / (pairs.Count - 1) : 0;
        double margin = 1.96 * Math.Sqrt(variance / pairs.Count);
        double discs = pairs.Average(pair => (pair.AFirst.DiscsB + pair.BFirst.DiscsB) / 2.0);
        return new MatchSummary(pairs.Count, pairs.Sum(pair => pair.PointsB), mean, mean - margin, mean + margin, discs);
    }
}

/// <summary>One side of a match: its search limits, hash table size and opening book.</summary>
/// <param name="Limits">
/// The limits for every move. With <see cref="TimeControlMode.TimePerGame"/>, <see cref="SearchLimits.Time"/> is the time
/// for the whole game and the side's clock runs down during the game.
/// </param>
/// <param name="Book">The book to play from, or null to search every move.</param>
internal sealed record MatchSide(string Name, SearchLimits Limits, int HashBits = SearchLimits.DefaultHashBits, OpeningBook? Book = null);

/// <summary>
/// Plays two sides against each other. From each start position a pair of games is played with the colours swapped.
/// Every game starts with fresh hash tables and book trackers, so a game does not depend on the games before it.
/// </summary>
internal sealed class EngineMatch(MatchSide a, MatchSide b, int workers)
{
    public MatchSide A { get; } = a;

    public MatchSide B { get; } = b;

    /// <param name="played">Called after each pair, one at a time.</param>
    /// <returns>The pairs in the order of <paramref name="starts"/>.</returns>
    /// <exception cref="FormatException">A start position is not a legal line from d3.</exception>
    /// <exception cref="OperationCanceledException"><paramref name="cancellationToken"/> was cancelled.</exception>
    public IReadOnlyList<MatchPair> Play(IReadOnlyList<string> starts, Action<MatchPair>? played, CancellationToken cancellationToken)
    {
        foreach (string start in starts)
        {
            BookTextFormat.Replay(start);
        }

        var pairs = new MatchPair[starts.Count];
        int next = -1;
        var gate = new object();

        void Work()
        {
            var engineA = new SearchEngine(A.HashBits);
            var engineB = new SearchEngine(B.HashBits);
            int index;
            while ((index = Interlocked.Increment(ref next)) < starts.Count)
            {
                (Board board, Player toMove) = BookTextFormat.Replay(starts[index]);
                MatchGame first = PlayGame(board, toMove, toMove.Opponent(), engineA, engineB, cancellationToken);
                MatchGame second = PlayGame(board, toMove, toMove, engineA, engineB, cancellationToken);
                var pair = new MatchPair(starts[index], first, second);
                lock (gate)
                {
                    pairs[index] = pair;
                    played?.Invoke(pair);
                }
            }
        }

        ParallelWork.Run(Math.Min(workers, starts.Count), Work, cancellationToken);
        return pairs;
    }

    private MatchGame PlayGame(
        Board board, Player toMove, Player colourB, SearchEngine engineA, SearchEngine engineB, CancellationToken cancellationToken)
    {
        engineA.ClearHash();
        engineB.ClearHash();
        var playerA = new ComputerPlayer(engineA, A.Book, new Random(0));
        var playerB = new ComputerPlayer(engineB, B.Book, new Random(0));
        var moves = new List<Move>();
        SideStats statsA = default;
        SideStats statsB = default;
        TimeSpan usedA = TimeSpan.Zero;
        TimeSpan usedB = TimeSpan.Zero;

        while (!board.IsGameOver)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (!board.HasLegalMove(toMove))
            {
                moves.Add(Move.Pass);
                toMove = toMove.Opponent();
                continue;
            }

            bool isB = toMove == colourB;
            MatchSide side = isB ? B : A;
            SearchLimits limits = side.Limits.Mode == TimeControlMode.TimePerGame
                ? SearchLimits.TimePerGame(side.Limits.Time - (isB ? usedB : usedA), side.Limits.UseOpeningBook, side.HashBits)
                : side.Limits;

            int empties = board.EmptyCount;
            long started = System.Diagnostics.Stopwatch.GetTimestamp();
            SearchResult result = (isB ? playerB : playerA).ChooseMove(board, toMove, limits, cancellationToken: cancellationToken);
            TimeSpan used = System.Diagnostics.Stopwatch.GetElapsedTime(started);
            if (isB)
            {
                usedB += used;
                statsB = statsB.Add(result, empties);
            }
            else
            {
                usedA += used;
                statsA = statsA.Add(result, empties);
            }

            board = board.Play(toMove, result.Move.Square!.Value);
            moves.Add(result.Move);
            toMove = toMove.Opponent();
        }

        return new MatchGame(colourB, moves, board.Count(colourB) - board.Count(colourB.Opponent()))
        {
            StatsA = statsA,
            StatsB = statsB,
        };
    }
}

/// <summary>
/// Plays two books against each other with the same engine and search limits, so only the books differ (book tool
/// <c>match</c>). From each start position a pair of games is played with the colours swapped.
/// </summary>
internal sealed class BookMatch(OpeningBook bookA, OpeningBook bookB, SearchLimits limits, int workers, int hashBits = 19)
{
    /// <summary>The start positions in a file: the first word of every line that is not empty or a comment.</summary>
    public static List<string> ReadStarts(TextReader reader)
    {
        var starts = new List<string>();
        while (reader.ReadLine() is { } line)
        {
            if (!string.IsNullOrWhiteSpace(line) && !line.TrimStart().StartsWith('#'))
            {
                starts.Add(line.Split(' ', StringSplitOptions.RemoveEmptyEntries)[0]);
            }
        }

        return starts;
    }

    /// <summary>The lines that key the book's positions at <paramref name="ply"/> (d3 is ply 1).</summary>
    public static List<string> StartsAtPly(OpeningBook book, int ply) =>
        BookTextFormat.Lines(book).Where(line => line.Moves.Length == 2 * ply).Select(line => line.Moves).ToList();

    /// <inheritdoc cref="EngineMatch.Play"/>
    public IReadOnlyList<MatchPair> Play(IReadOnlyList<string> starts, Action<MatchPair>? played, CancellationToken cancellationToken) =>
        new EngineMatch(new MatchSide("A", limits, hashBits, bookA), new MatchSide("B", limits, hashBits, bookB), workers)
            .Play(starts, played, cancellationToken);
}
