namespace Stello.Engine;

/// <summary>Summary returned by <see cref="BookTrimmer.Trim"/>.</summary>
internal sealed record BookTrimResult(
    OpeningBook Book,
    int PositionsBefore,
    int MovesBefore,
    int LeavesBefore,
    int PositionsAfter,
    int MovesAfter,
    int LeavesAfter,
    int SeveredEdges);

/// <summary>
/// Prunes unreachable branches and over-deep self-play tails from an opening book (Phase 6 Item 2, <c>trim</c>),
/// keeping the union of positions reachable from <c>d3</c> at <c>ply &lt;= maxPly</c> when Stello plays Black
/// and when Stello plays White, and preserving minimax values across severed <c>BackedUp</c> edges.
/// </summary>
internal static class BookTrimmer
{
    /// <summary>
    /// Returns a trimmed copy of <paramref name="book"/> containing only positions reachable within
    /// <paramref name="heroLoss"/> (for Stello's side), <paramref name="oppLoss"/> (for the opponent, or null for
    /// unlimited), and <paramref name="maxPly"/> (where <c>d3</c> is ply 1).
    /// </summary>
    public static BookTrimResult Trim(OpeningBook book, int heroLoss, int? oppLoss, int maxPly)
    {
        ArgumentNullException.ThrowIfNull(book);
        ArgumentOutOfRangeException.ThrowIfNegative(heroLoss);
        if (oppLoss is < 0)
        {
            throw new ArgumentOutOfRangeException(nameof(oppLoss), "Opponent loss must be non-negative.");
        }

        ArgumentOutOfRangeException.ThrowIfLessThan(maxPly, 1);

        int positionsBefore = book.PositionCount;
        int movesBefore = book.NodeCount;
        int leavesBefore = CountLeaves(book);

        if (!book.Contains(OpeningBook.RootBoard, OpeningBook.RootPlayer))
        {
            return new BookTrimResult(
                OpeningBook.CreateEmpty(),
                positionsBefore, movesBefore, leavesBefore,
                0, 0, 0, 0);
        }

        HashSet<BookKey> keptKeys = ReachableKeys(book, Player.Black, heroLoss, oppLoss, maxPly);
        keptKeys.UnionWith(ReachableKeys(book, Player.White, heroLoss, oppLoss, maxPly));

        var provenanceCache = new Dictionary<BookKey, (BookOrigin Origin, BookEffort Effort)>();
        var visiting = new HashSet<BookKey>();

        OpeningBook trimmed = OpeningBook.CreateEmpty();
        int severedEdges = 0;

        foreach (BookLine line in BookTextFormat.Lines(book))
        {
            (BookKey key, _) = OpeningBook.Canonical(line.Board, line.Player);
            if (!keptKeys.Contains(key))
            {
                continue;
            }

            book.TryGetReplies(line.Board, line.Player, out List<BookEntry>? sourceReplies, out OpeningBook.Symmetry sourceSymmetry);
            List<BookEntry> targetReplies = trimmed.GetOrAddReplies(line.Board, line.Player, out OpeningBook.Symmetry targetSymmetry);

            foreach (BookEntry entry in sourceReplies!)
            {
                Move move = OpeningBook.Transform(entry.Move, sourceSymmetry);
                (Board child, Player opponent) = OpeningBook.Play(line.Board, line.Player, move);
                BookKey childKey = OpeningBook.Canonical(child, opponent).Key;

                BookOrigin origin = entry.Origin;
                BookEffort effort = entry.Effort;

                if (origin == BookOrigin.BackedUp && !keptKeys.Contains(childKey))
                {
                    if (book.Contains(child, opponent))
                    {
                        (origin, effort) = ResolveLeafProvenance(book, child, opponent, provenanceCache, visiting);
                    }
                    else
                    {
                        origin = Math.Abs((int)entry.Value) > SearchEngine.WinScore ? BookOrigin.WinLossDraw : BookOrigin.Heuristic;
                        effort = default;
                    }

                    severedEdges++;
                }

                Move canonicalTarget = OpeningBook.Transform(move, targetSymmetry);
                targetReplies.Add(new BookEntry(canonicalTarget, entry.Value, origin, effort));
            }
        }

        BookMinimax.Run(trimmed);

        return new BookTrimResult(
            trimmed,
            positionsBefore, movesBefore, leavesBefore,
            trimmed.PositionCount, trimmed.NodeCount, CountLeaves(trimmed),
            severedEdges);
    }

    private static HashSet<BookKey> ReachableKeys(
        OpeningBook book,
        Player hero,
        int heroLoss,
        int? oppLoss,
        int maxPly)
    {
        var reached = new HashSet<BookKey>();
        var queue = new Queue<(Board Board, Player Player, int Ply)>();

        BookKey rootKey = OpeningBook.Canonical(OpeningBook.RootBoard, OpeningBook.RootPlayer).Key;
        reached.Add(rootKey);
        queue.Enqueue((OpeningBook.RootBoard, OpeningBook.RootPlayer, 1));

        while (queue.Count > 0)
        {
            (Board board, Player player, int ply) = queue.Dequeue();
            if (ply >= maxPly)
            {
                continue;
            }

            if (!book.TryGetReplies(board, player, out List<BookEntry>? replies, out OpeningBook.Symmetry symmetry)
                || replies.Count == 0)
            {
                continue;
            }

            short bestRaw = replies.Max(e => e.Value);
            int? maxLoss = player == hero ? heroLoss : oppLoss;
            bool isHeroTurn = player == hero;

            foreach (BookEntry entry in replies)
            {
                if (!IsAllowed(bestRaw, entry.Value, maxLoss, isHeroTurn))
                {
                    continue;
                }

                Move move = OpeningBook.Transform(entry.Move, symmetry);
                (Board child, Player opponent) = OpeningBook.Play(board, player, move);
                BookKey childKey = OpeningBook.Canonical(child, opponent).Key;

                if (book.Contains(child, opponent) && reached.Add(childKey))
                {
                    queue.Enqueue((child, opponent, ply + 1));
                }
            }
        }

        return reached;
    }

    internal static bool IsAllowed(short bestRaw, short candidateRaw, int? maxLoss, bool isHeroTurn)
    {
        if (maxLoss is null)
        {
            return true;
        }

        if (candidateRaw >= bestRaw)
        {
            return true;
        }

        // Never allow hero to leave a proven win for an unproven (heuristic / loss) move.
        if (isHeroTurn && bestRaw > SearchEngine.WinScore && candidateRaw <= SearchEngine.WinScore)
        {
            return false;
        }

        int loss = Math.Max(0, ScoreInUnits(bestRaw) - ScoreInUnits(candidateRaw));
        return loss <= maxLoss.Value;
    }

    internal static int ScoreInUnits(short value)
    {
        if (value > SearchEngine.WinScore)
        {
            return (value - SearchEngine.WinScore) * 100;
        }

        if (value < -SearchEngine.WinScore)
        {
            return (value + SearchEngine.WinScore) * 100;
        }

        return value;
    }

    private static (BookOrigin Origin, BookEffort Effort) ResolveLeafProvenance(
        OpeningBook book,
        Board board,
        Player player,
        Dictionary<BookKey, (BookOrigin Origin, BookEffort Effort)> cache,
        HashSet<BookKey> visiting)
    {
        (BookKey key, OpeningBook.Symmetry symmetry) = OpeningBook.Canonical(board, player);
        if (cache.TryGetValue(key, out (BookOrigin Origin, BookEffort Effort) cached))
        {
            return cached;
        }

        book.TryGetReplies(key, out List<BookEntry>? replies);
        if (replies is null || replies.Count == 0 || !visiting.Add(key))
        {
            return (BookOrigin.Heuristic, default);
        }

        try
        {
            short bestValue = replies.Max(e => e.Value);
            (BookOrigin Origin, BookEffort Effort) bestProvenance = (BookOrigin.Unknown, default);
            int bestRank = -1;

            foreach (BookEntry entry in replies)
            {
                if (entry.Value != bestValue)
                {
                    continue;
                }

                Move move = OpeningBook.Transform(entry.Move, symmetry);
                (Board child, Player opponent) = OpeningBook.Play(board, player, move);
                (BookOrigin origin, BookEffort effort) = book.Contains(child, opponent)
                    ? ResolveLeafProvenance(book, child, opponent, cache, visiting)
                    : (entry.Origin, entry.Effort);

                if (origin == BookOrigin.BackedUp)
                {
                    origin = Math.Abs((int)entry.Value) > SearchEngine.WinScore ? BookOrigin.WinLossDraw : BookOrigin.Heuristic;
                    effort = default;
                }

                int rank = OriginRank(origin);
                if (rank > bestRank || (rank == bestRank && effort.EngineVersion > bestProvenance.Effort.EngineVersion))
                {
                    bestRank = rank;
                    bestProvenance = (origin, effort);
                }
            }

            cache[key] = bestProvenance;
            return bestProvenance;
        }
        finally
        {
            visiting.Remove(key);
        }
    }

    private static int OriginRank(BookOrigin origin) => origin switch
    {
        BookOrigin.Exact => 4,
        BookOrigin.WinLossDraw => 3,
        BookOrigin.Heuristic => 2,
        BookOrigin.Unknown => 1,
        _ => 0,
    };

    private static int CountLeaves(OpeningBook book)
    {
        int leaves = 0;
        foreach (BookLine line in BookTextFormat.Lines(book))
        {
            book.TryGetReplies(line.Board, line.Player, out List<BookEntry>? replies, out OpeningBook.Symmetry symmetry);
            foreach (BookEntry entry in replies!)
            {
                (Board child, Player opponent) = OpeningBook.Play(line.Board, line.Player, OpeningBook.Transform(entry.Move, symmetry));
                if (!book.Contains(child, opponent))
                {
                    leaves++;
                }
            }
        }

        return leaves;
    }
}
