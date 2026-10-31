using Stello.Engine.Search;

namespace Stello.Engine;

/// <summary>What one side did in a game: the moves it searched, their nodes and time, and how deep it got.</summary>
/// <param name="Searched">Moves that needed a search (not book moves, passes or single legal moves).</param>
/// <param name="Time">The wall time of the searched moves, as the match measured it.</param>
/// <param name="MidgameMoves">Searched moves that ended in the midgame search (a heuristic score).</param>
/// <param name="MidgameDepth">The depths of those moves added up.</param>
/// <param name="FirstSolve">The most empty squares at which this side solved the game (win/loss/draw or exact), or 0.</param>
/// <param name="Longest">The longest wall time of a move.</param>
internal readonly record struct SideStats(int Searched, long Nodes, TimeSpan Time, int MidgameMoves, long MidgameDepth, int FirstSolve, TimeSpan Longest = default)
{
    public SideStats Add(SearchResult result, int empties, TimeSpan used)
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
            Time + used,
            MidgameMoves + (midgame ? 1 : 0),
            MidgameDepth + (midgame ? result.Depth : 0),
            solved ? Math.Max(FirstSolve, empties) : FirstSolve,
            used > Longest ? used : Longest);
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

/// <summary>An engine that plays one side of a match. Each match worker has its own player for each side.</summary>
internal interface IMatchPlayer
{
    /// <summary>Called before every game: clears the hash tables and the book state.</summary>
    void NewGame();

    /// <summary>The move for <paramref name="player"/>, who has a legal move.</summary>
    /// <param name="limits">The side's limits; with time per game, <see cref="SearchLimits.Time"/> is the time left.</param>
    SearchResult ChooseMove(Board board, Player player, SearchLimits limits, CancellationToken cancellationToken);
}

/// <summary>Stello's engine with its opening book as a match player.</summary>
internal sealed class StelloMatchPlayer(int hashBits, OpeningBook? book, SearchOptions? options = null) : IMatchPlayer
{
    private readonly SearchEngine _engine = new(hashBits, options ?? SearchOptions.Default);
    private ComputerPlayer? _player;

    public void NewGame()
    {
        _engine.ClearHash();
        _player = new ComputerPlayer(_engine, book, new Random(0));
    }

    public SearchResult ChooseMove(Board board, Player player, SearchLimits limits, CancellationToken cancellationToken) =>
        (_player ?? throw new InvalidOperationException("Call NewGame before the first move."))
            .ChooseMove(board, player, limits, cancellationToken: cancellationToken);
}

/// <summary>One side of a match: its search limits, hash table size and opening book.</summary>
/// <param name="Limits">
/// The limits for every move. With <see cref="TimeControlMode.TimePerGame"/>, <see cref="SearchLimits.Time"/> is the time
/// for the whole game and the side's clock runs down during the game.
/// </param>
/// <param name="Book">The book to play from, or null to search every move.</param>
/// <param name="CreatePlayer">Makes the engine of the side; null for Stello's engine with <paramref name="Book"/>.</param>
/// <param name="Description">What the side is, for the report; null for Stello's limits and hash size.</param>
internal sealed record MatchSide(
    string Name,
    SearchLimits Limits,
    int HashBits = SearchLimits.DefaultHashBits,
    OpeningBook? Book = null,
    Func<IMatchPlayer>? CreatePlayer = null,
    string? Description = null)
{
    public IMatchPlayer NewPlayer() => CreatePlayer?.Invoke() ?? new StelloMatchPlayer(HashBits, Book);
}

/// <summary>
/// Plays two sides against each other. From each start position a pair of games is played with the colours swapped.
/// Every game starts with fresh hash tables and book state, so a game does not depend on the games before it.
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
            IMatchPlayer playerA = A.NewPlayer();
            IMatchPlayer playerB = B.NewPlayer();
            int index;
            while ((index = Interlocked.Increment(ref next)) < starts.Count)
            {
                (Board board, Player toMove) = BookTextFormat.Replay(starts[index]);
                MatchGame first = PlayGame(board, toMove, toMove.Opponent(), playerA, playerB, cancellationToken);
                MatchGame second = PlayGame(board, toMove, toMove, playerA, playerB, cancellationToken);
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
        Board board, Player toMove, Player colourB, IMatchPlayer playerA, IMatchPlayer playerB, CancellationToken cancellationToken)
    {
        playerA.NewGame();
        playerB.NewGame();
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
            SearchResult result = (isB ? playerB : playerA).ChooseMove(board, toMove, limits, cancellationToken);
            TimeSpan used = System.Diagnostics.Stopwatch.GetElapsedTime(started);
            if (isB)
            {
                usedB += used;
                statsB = statsB.Add(result, empties, used);
            }
            else
            {
                usedA += used;
                statsA = statsA.Add(result, empties, used);
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
