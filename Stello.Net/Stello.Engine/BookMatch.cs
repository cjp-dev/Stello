namespace Stello.Engine;

/// <param name="BookB">The colour book B played.</param>
/// <param name="DiscsB">Book B's discs minus book A's discs at the end.</param>
internal sealed record MatchGame(Player BookB, IReadOnlyList<Move> Moves, int DiscsB)
{
    public double PointsB => DiscsB > 0 ? 1 : DiscsB == 0 ? 0.5 : 0;
}

/// <param name="Start">The start position, as a line from d3.</param>
/// <param name="BookAFirst">Book A plays the side to move in the start position.</param>
/// <param name="BookBFirst">Book B plays the side to move.</param>
internal sealed record MatchPair(string Start, MatchGame BookAFirst, MatchGame BookBFirst)
{
    public double PointsB => BookAFirst.PointsB + BookBFirst.PointsB;
}

/// <param name="Score">Book B's share of the points, 0 to 1.</param>
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
        double discs = pairs.Average(pair => (pair.BookAFirst.DiscsB + pair.BookBFirst.DiscsB) / 2.0);
        return new MatchSummary(pairs.Count, pairs.Sum(pair => pair.PointsB), mean, mean - margin, mean + margin, discs);
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
            var engineA = new SearchEngine(hashBits);
            var engineB = new SearchEngine(hashBits);
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
        Board board, Player toMove, Player bookBColour, SearchEngine engineA, SearchEngine engineB, CancellationToken cancellationToken)
    {
        // Fresh hash tables and book trackers, so a game does not depend on the games before it.
        engineA.ClearHash();
        engineB.ClearHash();
        var a = new ComputerPlayer(engineA, bookA, new Random(0));
        var b = new ComputerPlayer(engineB, bookB, new Random(0));
        var moves = new List<Move>();

        while (!board.IsGameOver)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (!board.HasLegalMove(toMove))
            {
                moves.Add(Move.Pass);
                toMove = toMove.Opponent();
                continue;
            }

            ComputerPlayer player = toMove == bookBColour ? b : a;
            Move move = player.ChooseMove(board, toMove, limits, cancellationToken: cancellationToken).Move;
            board = board.Play(toMove, move.Square!.Value);
            moves.Add(move);
            toMove = toMove.Opponent();
        }

        return new MatchGame(bookBColour, moves, board.Count(bookBColour) - board.Count(bookBColour.Opponent()));
    }
}
