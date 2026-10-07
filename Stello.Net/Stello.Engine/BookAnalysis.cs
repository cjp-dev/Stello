namespace Stello.Engine;

/// <param name="PerPly">Positions and leaf moves by ply, counting d3 as ply 1 (a position by its shortest line).</param>
/// <param name="LongestLine">Plies in the longest line of book moves, d3 included.</param>
internal sealed record BookStatistics(
    int Positions,
    int Moves,
    int LeafMoves,
    int PassMoves,
    int LongestLine,
    IReadOnlyList<(int Ply, int Positions, int LeafMoves)> PerPly,
    IReadOnlyDictionary<BookOrigin, int> Origins,
    IReadOnlyDictionary<string, int> Limits,
    IReadOnlyList<(Move Move, int Value, BookOrigin Origin)> RootMoves);

/// <param name="NotBackedUp">Moves whose value is not minus the best value of the book position after them.</param>
/// <param name="NotSorted">Positions whose moves are not sorted best first.</param>
internal sealed record BookConsistency(IReadOnlyList<string> NotBackedUp, IReadOnlyList<string> NotSorted)
{
    public bool IsConsistent => NotBackedUp.Count == 0 && NotSorted.Count == 0;
}

/// <summary>Statistics and checks for the book tool.</summary>
internal static class BookAnalysis
{
    public static BookStatistics Statistics(OpeningBook book)
    {
        List<BookLine> lines = BookTextFormat.Lines(book);
        var perPly = new SortedDictionary<int, (int Positions, int LeafMoves)>();
        var origins = new SortedDictionary<BookOrigin, int>();
        var limits = new SortedDictionary<string, int>(StringComparer.Ordinal);
        int moves = 0, leaves = 0, passes = 0;

        foreach (BookLine line in lines)
        {
            int ply = line.Moves.Length / 2;
            (int Positions, int LeafMoves) current = perPly.GetValueOrDefault(ply);
            perPly[ply] = (current.Positions + 1, current.LeafMoves);

            foreach ((BookEntry entry, Board child, Player opponent) in Replies(book, line.Board, line.Player))
            {
                moves++;
                passes += entry.Move.IsPass ? 1 : 0;
                origins[entry.Origin] = origins.GetValueOrDefault(entry.Origin) + 1;
                if (entry.IsSearched)
                {
                    string limit = BookTextFormat.LimitText(entry.Effort);
                    limits[limit] = limits.GetValueOrDefault(limit) + 1;
                }

                if (!book.Contains(child, opponent))
                {
                    leaves++;
                    (int Positions, int LeafMoves) next = perPly.GetValueOrDefault(ply + 1);
                    perPly[ply + 1] = (next.Positions, next.LeafMoves + 1);
                }
            }
        }

        var rootMoves = new List<(Move, int, BookOrigin)>();
        if (book.TryGetReplies(OpeningBook.RootBoard, OpeningBook.RootPlayer, out List<BookEntry>? root, out OpeningBook.Symmetry symmetry))
        {
            rootMoves.AddRange(root.Select(entry => (OpeningBook.Transform(entry.Move, symmetry), (int)entry.Value, entry.Origin)));
        }

        int longest = lines.Count == 0 ? 0 : 1 + Longest(book, OpeningBook.RootBoard, OpeningBook.RootPlayer, []);
        return new BookStatistics(
            lines.Count,
            moves,
            leaves,
            passes,
            longest,
            perPly.Select(pair => (pair.Key, pair.Value.Positions, pair.Value.LeafMoves)).ToList(),
            origins,
            limits,
            rootMoves);
    }

    public static BookConsistency Consistency(OpeningBook book)
    {
        var notBackedUp = new List<string>();
        var notSorted = new List<string>();
        foreach (BookLine line in BookTextFormat.Lines(book))
        {
            int previous = int.MaxValue;
            bool sorted = true;
            OpeningBook.Symmetry symmetry = OpeningBook.Canonical(line.Board, line.Player).Symmetry;
            foreach ((BookEntry entry, Board child, Player opponent) in Replies(book, line.Board, line.Player))
            {
                sorted &= entry.Value <= previous;
                previous = entry.Value;
                if (book.TryGetReplies(child, opponent, out List<BookEntry>? next, out _)
                    && entry.Value != Math.Clamp(-next.Max(e => (int)e.Value), -short.MaxValue, short.MaxValue))
                {
                    notBackedUp.Add($"{line.Moves} {BookTextFormat.MoveText(OpeningBook.Transform(entry.Move, symmetry))}");
                }
            }

            if (!sorted)
            {
                notSorted.Add(line.Moves);
            }
        }

        return new BookConsistency(notBackedUp, notSorted);
    }

    private static IEnumerable<(BookEntry Entry, Board Child, Player Opponent)> Replies(OpeningBook book, Board board, Player player)
    {
        book.TryGetReplies(board, player, out List<BookEntry>? replies, out OpeningBook.Symmetry symmetry);
        foreach (BookEntry entry in replies!)
        {
            (Board child, Player opponent) = OpeningBook.Play(board, player, OpeningBook.Transform(entry.Move, symmetry));
            yield return (entry, child, opponent);
        }
    }

    // The most plies of book moves from the position.
    private static int Longest(OpeningBook book, Board board, Player player, Dictionary<BookKey, int> known)
    {
        BookKey key = OpeningBook.Canonical(board, player).Key;
        if (known.TryGetValue(key, out int plies))
        {
            return plies;
        }

        plies = Replies(book, board, player)
            .Max(reply => 1 + (book.Contains(reply.Child, reply.Opponent) ? Longest(book, reply.Child, reply.Opponent, known) : 0));
        known[key] = plies;
        return plies;
    }
}
