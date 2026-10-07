namespace Stello.Engine;

/// <summary>
/// Reads the C++ book file "OPENING" (Put_book/savebook in Book.cpp) into the new book. Only used to import the
/// old master book and old user books; books are always saved in <see cref="BookBinaryFormat"/>.
/// </summary>
/// <remarks>
/// Little-endian: int32 C++ allocation counter (ignored), then the chain of White's replies to d3. A chain is an
/// int16 number of nodes; each node is int16 move (legacy square number, 0 = pass), int16 value, int16 flags,
/// followed by the chain of its replies.
/// </remarks>
internal static class LegacyBookFormat
{
    private const int MaxChainLength = 64;
    private const int MaxDepth = 64;

    [Flags]
    private enum Flags : short
    {
        None = 0,
        Calculated = 0x1,
        Exact = 0x2,
        Inexact = 0x4,
    }

    /// <exception cref="InvalidDataException">The data is not a valid C++ book file.</exception>
    public static OpeningBook Read(byte[] data)
    {
        using var reader = new BinaryReader(new MemoryStream(data));
        List<Node> root;
        try
        {
            reader.ReadInt32();
            root = ReadChain(reader, depth: 0);
        }
        catch (EndOfStreamException exception)
        {
            throw new InvalidDataException("The opening book file is truncated.", exception);
        }

        OpeningBook book = OpeningBook.CreateEmpty();
        Import(book, root, OpeningBook.RootBoard, OpeningBook.RootPlayer);
        MarkBackedUp(book);
        return book;
    }

    // A leaf of one line can lead to a position that another line continues from (C++ took the value from there),
    // and a node can have children that were all left out.
    private static void MarkBackedUp(OpeningBook book)
    {
        foreach (BookLine line in BookTextFormat.Lines(book))
        {
            book.TryGetReplies(line.Board, line.Player, out List<BookEntry>? replies, out OpeningBook.Symmetry symmetry);
            foreach (BookEntry entry in replies!)
            {
                (Board child, Player opponent) = OpeningBook.Play(line.Board, line.Player, OpeningBook.Transform(entry.Move, symmetry));
                if (book.Contains(child, opponent))
                {
                    entry.Set(entry.Value, BookOrigin.BackedUp);
                }
                else if (entry.Origin == BookOrigin.BackedUp)
                {
                    entry.Set(entry.Value, BookOrigin.Unknown);
                }
            }
        }
    }

    private static List<Node> ReadChain(BinaryReader reader, int depth)
    {
        short count = reader.ReadInt16();
        if (count is < 0 or > MaxChainLength || (count > 0 && depth >= MaxDepth))
        {
            throw new InvalidDataException("The opening book file is not valid.");
        }

        var chain = new List<Node>(count);
        for (int i = 0; i < count; i++)
        {
            var node = new Node(reader.ReadInt16(), reader.ReadInt16(), (Flags)reader.ReadInt16());
            node.Children.AddRange(ReadChain(reader, depth + 1));
            chain.Add(node);
        }

        return chain;
    }

    // Depth first in file order, so the first line that reaches a position decides the order of its moves, as the
    // C++ lookup and the old C# index did. Moves that are not legal could never be played and are left out.
    private static void Import(OpeningBook book, List<Node> chain, Board board, Player player)
    {
        for (int i = 0; i < chain.Count; i++)
        {
            Node node = chain[i];
            if (ToMove(node.Move) is not { } move || !OpeningBook.IsLegal(board, player, move))
            {
                continue;
            }

            List<BookEntry> replies = book.GetOrAddReplies(board, player, out OpeningBook.Symmetry symmetry);
            Move canonical = OpeningBook.Transform(move, symmetry);
            BookOrigin origin = Origin(node, firstInChain: i == 0);
            BookEntry? known = replies.Find(entry => entry.Move == canonical);
            if (known is null)
            {
                replies.Add(new BookEntry(canonical, node.Value, origin));
            }
            else if (Rank(origin) > Rank(known.Origin))
            {
                known.Set(node.Value, origin);
            }

            (Board child, Player opponent) = OpeningBook.Play(board, player, move);
            Import(book, node.Children, child, opponent);
        }
    }

    private static Move? ToMove(short legacy)
    {
        if (legacy == 0)
        {
            return Move.Pass;
        }

        return legacy is >= 11 and <= 88 && legacy % 10 is >= 1 and <= 8 ? new Move(Square.FromLegacy(legacy)) : null;
    }

    private static BookOrigin Origin(Node node, bool firstInChain)
    {
        if (node.Children.Count > 0)
        {
            return BookOrigin.BackedUp;
        }

        if (!node.Flags.HasFlag(Flags.Calculated))
        {
            // A move from an added game (±32665) that was never searched.
            return BookOrigin.Unknown;
        }

        if (node.Flags.HasFlag(Flags.Exact))
        {
            return BookOrigin.Exact;
        }

        // C++ savebook wrote 0 instead of 32600 for the first node in a chain, so such a 0 cannot be trusted.
        if (firstInChain && node.Value == 0)
        {
            return BookOrigin.Unknown;
        }

        return node.Flags.HasFlag(Flags.Inexact) ? BookOrigin.WinLossDraw : BookOrigin.Heuristic;
    }

    // When two lines reach the same position with the same move, the better founded value is kept.
    private static int Rank(BookOrigin origin) => origin switch
    {
        BookOrigin.Exact => 4,
        BookOrigin.WinLossDraw => 3,
        BookOrigin.BackedUp => 2,
        BookOrigin.Heuristic => 1,
        _ => 0,
    };

    private sealed record Node(short Move, short Value, Flags Flags)
    {
        public List<Node> Children { get; } = [];
    }
}
