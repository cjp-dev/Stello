namespace Stello.Engine.Tests;

internal static class BookTestData
{
    public static readonly string LegacyPath = Path.Combine(AppContext.BaseDirectory, "Data", "OPENING");
    public static readonly string TextPath = Path.Combine(AppContext.BaseDirectory, "Data", "opening-book.txt");
    public static readonly string BinaryPath = Path.Combine(AppContext.BaseDirectory, "Data", "opening-book.bin");

    private static readonly Lazy<OpeningBook> LazyMaster = new(() => OpeningBook.Load(BinaryPath));

    /// <summary>The shipped book. Do not change it in a test.</summary>
    public static OpeningBook Master => LazyMaster.Value;

    /// <summary>The book moves after <paramref name="moves"/> on the real board, in book order.</summary>
    public static List<(Move Move, BookEntry Entry)> Replies(OpeningBook book, string moves)
    {
        Game game = GameRecordFormat.Parse(moves);
        return Replies(book, game.Board, game.ToMove);
    }

    public static List<(Move Move, BookEntry Entry)> Replies(OpeningBook book, Board board, Player player) =>
        book.TryGetReplies(board, player, out List<BookEntry>? replies, out OpeningBook.Symmetry symmetry)
            ? replies.Select(entry => (OpeningBook.Transform(entry.Move, symmetry), entry)).ToList()
            : [];

    public static OpeningBook ReadText(string text) => BookTextFormat.Read(new StringReader(text));

    public static List<Move> Moves(string text) => GameRecordFormat.Parse(text).PlayedMoves.ToList();

    /// <summary>The book move <paramref name="move"/> after <paramref name="moves"/>.</summary>
    public static BookEntry Entry(OpeningBook book, string moves, string move) =>
        Replies(book, moves).Single(r => r.Move.ToString() == move).Entry;

    /// <summary>Two move orders from d3 that reach the same position, and a legal move from there.</summary>
    public static (string First, string Second, string Next) Transposition()
    {
        var seen = new Dictionary<(Board, Player), string>();
        var lines = new Queue<string>(["d3"]);
        while (lines.TryDequeue(out string? line))
        {
            Game game = GameRecordFormat.Parse(line);
            if (seen.TryGetValue((game.Board, game.ToMove), out string? other))
            {
                return (other, line, Square.InMask(game.Board.LegalMoves(game.ToMove)).First().ToString());
            }

            seen.Add((game.Board, game.ToMove), line);
            foreach (Square square in Square.InMask(game.Board.LegalMoves(game.ToMove)))
            {
                lines.Enqueue($"{line} {square}");
            }
        }

        throw new InvalidOperationException("No transposition found.");
    }

    public static string WriteText(OpeningBook book)
    {
        using var writer = new StringWriter();
        BookTextFormat.Write(book, writer);
        return writer.ToString();
    }

    public static byte[] WriteBinary(OpeningBook book)
    {
        using var stream = new MemoryStream();
        book.Save(stream);
        return stream.ToArray();
    }

    /// <summary>A C++ book file with one chain of replies to d3: (legacy move, value, flags).</summary>
    public static byte[] Legacy(params (int Move, short Value, short Flags)[] chain)
    {
        using var stream = new MemoryStream();
        using (var writer = new BinaryWriter(stream))
        {
            writer.Write(chain.Length);
            writer.Write((short)chain.Length);
            foreach ((int move, short value, short flags) in chain)
            {
                writer.Write((short)move);
                writer.Write(value);
                writer.Write(flags);
                writer.Write((short)0);
            }
        }

        return stream.ToArray();
    }
}

/// <summary>The lookup of the old tree-shaped book (before format 2), to check that the import plays the same moves.</summary>
internal sealed class LegacyLookup
{
    private readonly Dictionary<(ulong, ulong, Player), List<(short Move, short Value)>> _index = [];

    public LegacyLookup(string path)
    {
        using var reader = new BinaryReader(File.OpenRead(path));
        reader.ReadInt32();
        Index(ReadChain(reader), OpeningBook.RootBoard, OpeningBook.RootPlayer);
    }

    /// <summary>Every position in the old index, with the player to move.</summary>
    public IEnumerable<(Board Board, Player Player)> Positions =>
        _index.Keys.Select(key => (new Board(key.Item1, key.Item2), key.Item3));

    /// <summary>In how many mirrored frames the old book stored the position.</summary>
    public int Frames(Board board, Player player) =>
        Enum.GetValues<OpeningBook.Symmetry>()
            .Select(symmetry => OpeningBook.Transform(board, symmetry))
            .Distinct()
            .Count(transformed => _index.ContainsKey((transformed.Black, transformed.White, player)));

    public Square? Move(Board board, Player player)
    {
        foreach (OpeningBook.Symmetry symmetry in Enum.GetValues<OpeningBook.Symmetry>())
        {
            Board transformed = OpeningBook.Transform(board, symmetry);
            if (!_index.TryGetValue((transformed.Black, transformed.White, player), out var replies))
            {
                continue;
            }

            foreach ((short move, _) in replies.Where(r => r.Move != 0))
            {
                Square square = OpeningBook.Transform(Square.FromLegacy(move), symmetry);
                if (board.IsLegal(player, square))
                {
                    return square;
                }
            }
        }

        return null;
    }

    private static List<Node> ReadChain(BinaryReader reader)
    {
        short count = reader.ReadInt16();
        var chain = new List<Node>();
        for (int i = 0; i < count; i++)
        {
            short move = reader.ReadInt16();
            short value = reader.ReadInt16();
            reader.ReadInt16();
            chain.Add(new Node(move, value, ReadChain(reader)));
        }

        return chain;
    }

    private void Index(List<Node> chain, Board board, Player player)
    {
        if (chain.Count == 0)
        {
            return;
        }

        _index.TryAdd((board.Black, board.White, player), chain.Select(n => (n.Move, n.Value)).ToList());
        foreach (Node node in chain)
        {
            if (node.Move == 0)
            {
                if (!board.HasLegalMove(player))
                {
                    Index(node.Children, board, player.Opponent());
                }
            }
            else if (board.IsLegal(player, Square.FromLegacy(node.Move)))
            {
                Index(node.Children, board.Play(player, Square.FromLegacy(node.Move)), player.Opponent());
            }
        }
    }

    private sealed record Node(short Move, short Value, List<Node> Children);
}
