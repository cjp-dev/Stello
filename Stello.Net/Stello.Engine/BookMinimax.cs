namespace Stello.Engine;

/// <summary>Backs the values up through the book and sorts every position's moves best first (C++: minmax_lib, sort_lib).</summary>
internal static class BookMinimax
{
    private const int Infinity = 32767;

    public static void Run(OpeningBook book)
    {
        if (book.Contains(OpeningBook.RootBoard, OpeningBook.RootPlayer))
        {
            BackUp(book, OpeningBook.RootBoard, OpeningBook.RootPlayer, []);
        }

        foreach (BookLine line in BookTextFormat.Lines(book))
        {
            book.TryGetReplies(line.Board, line.Player, out List<BookEntry>? replies, out _);
            Sort(replies!);
        }
    }

    public static short Clamp(int value) => (short)Math.Clamp(value, -Infinity, Infinity);

    // C++: mmlib. Returns the value of the position for the player to move. Each position is backed up once, after
    // all the positions below it, so one pass is enough (C++ needed 10 rounds for transpositions).
    private static int BackUp(OpeningBook book, Board board, Player player, Dictionary<BookKey, int> backedUp)
    {
        (BookKey key, OpeningBook.Symmetry symmetry) = OpeningBook.Canonical(board, player);
        if (backedUp.TryGetValue(key, out int known))
        {
            return known;
        }

        book.TryGetReplies(key, out List<BookEntry>? replies);
        int best = -Infinity;
        foreach (BookEntry entry in replies!)
        {
            (Board child, Player opponent) = OpeningBook.Play(board, player, OpeningBook.Transform(entry.Move, symmetry));
            if (book.Contains(child, opponent))
            {
                entry.Set(Clamp(-BackUp(book, child, opponent, backedUp)), BookOrigin.BackedUp);
            }

            best = Math.Max(best, entry.Value);
        }

        backedUp[key] = best;
        return best;
    }

    // C++: sort_lib. Stable, best value first.
    private static void Sort(List<BookEntry> replies)
    {
        List<BookEntry> sorted = replies.OrderByDescending(entry => entry.Value).ToList();
        replies.Clear();
        replies.AddRange(sorted);
    }
}
