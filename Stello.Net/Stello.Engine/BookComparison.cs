using System.Globalization;

namespace Stello.Engine;

/// <param name="Line">The position, as the line that keys it in the new book.</param>
/// <param name="OldMove">The old book's first move, in the frame of <paramref name="Line"/>.</param>
internal sealed record BestMoveChange(string Line, Move OldMove, int OldValue, Move NewMove, int NewValue);

/// <param name="Origins">For each change of origin, the number of book moves and how many of them changed value.</param>
/// <param name="BestMoveChanges">Positions where the book now plays another move, nearest the start first.</param>
internal sealed record BookComparison(
    int OldPositions,
    int OldMoves,
    int NewPositions,
    int NewMoves,
    int ValuesChanged,
    IReadOnlyDictionary<(BookOrigin Old, BookOrigin New), (int Moves, int ValuesChanged)> Origins,
    IReadOnlyList<BestMoveChange> BestMoveChanges)
{
    /// <summary>Compares the moves the two books have in common, and the move each book plays first.</summary>
    public static BookComparison Of(OpeningBook oldBook, OpeningBook newBook)
    {
        var origins = new SortedDictionary<(BookOrigin, BookOrigin), (int Moves, int ValuesChanged)>();
        var changes = new List<BestMoveChange>();
        int valuesChanged = 0;

        foreach (BookLine line in BookTextFormat.Lines(newBook))
        {
            BookKey key = OpeningBook.Canonical(line.Board, line.Player).Key;
            if (!oldBook.TryGetReplies(key, out List<BookEntry>? oldReplies))
            {
                continue;
            }

            newBook.TryGetReplies(line.Board, line.Player, out List<BookEntry>? newReplies, out OpeningBook.Symmetry symmetry);
            foreach (BookEntry entry in newReplies!)
            {
                if (oldReplies.Find(old => old.Move == entry.Move) is not { } old)
                {
                    continue;
                }

                bool changed = old.Value != entry.Value;
                valuesChanged += changed ? 1 : 0;
                (int moves, int values) = origins.GetValueOrDefault((old.Origin, entry.Origin));
                origins[(old.Origin, entry.Origin)] = (moves + 1, values + (changed ? 1 : 0));
            }

            if (oldReplies[0].Move != newReplies[0].Move)
            {
                changes.Add(new BestMoveChange(
                    line.Moves,
                    OpeningBook.Transform(oldReplies[0].Move, symmetry),
                    oldReplies[0].Value,
                    OpeningBook.Transform(newReplies[0].Move, symmetry),
                    newReplies[0].Value));
            }
        }

        return new BookComparison(
            oldBook.PositionCount, oldBook.NodeCount, newBook.PositionCount, newBook.NodeCount, valuesChanged, origins, changes);
    }

    /// <summary>
    /// Writes the report. Everything but the changed positions is a comment, so the report can be given to
    /// <c>match</c> as its start positions.
    /// </summary>
    public void Write(TextWriter writer, string title)
    {
        writer.Write($"# {title}\n");
        writer.Write(Invariant($"# Positions: {NewPositions:N0} (before {OldPositions:N0}); book moves: {NewMoves:N0} (before {OldMoves:N0})\n"));
        writer.Write(Invariant($"# Values changed: {ValuesChanged:N0}\n"));
        writer.Write("# Origin before -> after: book moves (values changed)\n");
        foreach (((BookOrigin oldOrigin, BookOrigin newOrigin), (int moves, int values)) in Origins)
        {
            writer.Write(Invariant($"#   {oldOrigin,-11} -> {newOrigin,-11} {moves,7:N0} ({values:N0})\n"));
        }

        writer.Write(Invariant($"# Positions whose first book move changed: {BestMoveChanges.Count:N0}\n"));
        writer.Write("# <line> <old move>:<old value> <new move>:<new value>\n");
        foreach (BestMoveChange change in BestMoveChanges)
        {
            writer.Write(Invariant(
                $"{change.Line} {BookTextFormat.MoveText(change.OldMove)}:{change.OldValue} {BookTextFormat.MoveText(change.NewMove)}:{change.NewValue}\n"));
        }
    }

    private static string Invariant(FormattableString text) => text.ToString(CultureInfo.InvariantCulture);
}
