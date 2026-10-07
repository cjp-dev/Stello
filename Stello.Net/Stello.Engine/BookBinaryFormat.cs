using System.Text;

namespace Stello.Engine;

/// <summary>The book file used by the apps (Data/OPENING, the web version's OPENING.bin and the user's book).</summary>
/// <remarks>
/// Little-endian: "STBK", byte version (2), int32 number of positions, int32 number of moves, then the position
/// after d3 and, depth first in book order, every position under it. Positions are not stored: they are found by
/// playing the moves. A position is a byte number of moves, then for each move:
/// byte square (0-63, 64 = pass, in the frame of the board as it is reached), int16 value, byte origin;
/// for a searched origin also byte effort kind, 7-bit encoded amount, byte depth reached, byte engine version;
/// and a 7-bit encoded reference to the position after the move: 0 = not in the book, 1 = it follows here,
/// n + 2 = position number n (numbered in the order they are written), reached by another move order.
/// </remarks>
internal static class BookBinaryFormat
{
    private const byte Version = 2;
    private const int PassSquare = 64;
    private const int MaxDepth = 128;

    private static ReadOnlySpan<byte> Magic => "STBK"u8;

    public static bool IsBinary(byte[] data) => data.AsSpan().StartsWith(Magic);

    public static void Write(OpeningBook book, Stream stream)
    {
        using var body = new MemoryStream();
        var numbers = new Dictionary<BookKey, int>();
        int moves = 0;
        using (var writer = new BinaryWriter(body, Encoding.UTF8, leaveOpen: true))
        {
            if (book.Contains(OpeningBook.RootBoard, OpeningBook.RootPlayer))
            {
                WritePosition(writer, book, OpeningBook.RootBoard, OpeningBook.RootPlayer, numbers, ref moves);
            }
        }

        using var header = new BinaryWriter(stream, Encoding.UTF8, leaveOpen: true);
        header.Write(Magic);
        header.Write(Version);
        header.Write(numbers.Count);
        header.Write(moves);
        header.Write(body.GetBuffer(), 0, (int)body.Length);
    }

    /// <exception cref="InvalidDataException">The data is not a valid book.</exception>
    public static OpeningBook Read(byte[] data)
    {
        using var reader = new BinaryReader(new MemoryStream(data));
        try
        {
            reader.ReadBytes(Magic.Length);
            if (reader.ReadByte() != Version)
            {
                throw new InvalidDataException("The opening book file has an unknown version.");
            }

            int positions = reader.ReadInt32();
            int moves = reader.ReadInt32();
            OpeningBook book = OpeningBook.CreateEmpty();
            var keys = new List<BookKey>();
            if (positions > 0)
            {
                ReadPosition(reader, book, OpeningBook.RootBoard, OpeningBook.RootPlayer, keys, depth: 0);
            }

            if (keys.Count != positions || book.NodeCount != moves || reader.BaseStream.Position != data.Length)
            {
                throw Invalid();
            }

            return book;
        }
        catch (Exception exception) when (exception is EndOfStreamException or FormatException)
        {
            throw new InvalidDataException("The opening book file is truncated or damaged.", exception);
        }
    }

    private static void WritePosition(
        BinaryWriter writer, OpeningBook book, Board board, Player player, Dictionary<BookKey, int> numbers, ref int moves)
    {
        (BookKey key, OpeningBook.Symmetry symmetry) = OpeningBook.Canonical(board, player);
        book.TryGetReplies(key, out List<BookEntry>? replies);
        numbers.Add(key, numbers.Count);
        moves += replies!.Count;

        writer.Write((byte)replies.Count);
        foreach (BookEntry entry in replies)
        {
            Move move = OpeningBook.Transform(entry.Move, symmetry);
            writer.Write((byte)(move.Square?.Index ?? PassSquare));
            writer.Write(entry.Value);
            writer.Write((byte)entry.Origin);
            if (entry.IsSearched)
            {
                writer.Write((byte)entry.Effort.Kind);
                writer.Write7BitEncodedInt(entry.Effort.Amount);
                writer.Write((byte)entry.Effort.DepthReached);
                writer.Write((byte)entry.Effort.EngineVersion);
            }

            (Board child, Player opponent) = OpeningBook.Play(board, player, move);
            BookKey childKey = OpeningBook.Canonical(child, opponent).Key;
            if (!book.TryGetReplies(childKey, out _))
            {
                writer.Write7BitEncodedInt(0);
            }
            else if (numbers.TryGetValue(childKey, out int number))
            {
                writer.Write7BitEncodedInt(number + 2);
            }
            else
            {
                writer.Write7BitEncodedInt(1);
                WritePosition(writer, book, child, opponent, numbers, ref moves);
            }
        }
    }

    private static void ReadPosition(BinaryReader reader, OpeningBook book, Board board, Player player, List<BookKey> keys, int depth)
    {
        BookKey key = OpeningBook.Canonical(board, player).Key;
        int count = reader.ReadByte();
        if (depth > MaxDepth || count == 0 || book.TryGetReplies(key, out _))
        {
            throw Invalid();
        }

        List<BookEntry> replies = book.GetOrAddReplies(board, player, out OpeningBook.Symmetry symmetry);
        keys.Add(key);

        for (int i = 0; i < count; i++)
        {
            int square = reader.ReadByte();
            Move move = square == PassSquare ? Move.Pass
                : square < PassSquare ? new Move(new Square(square))
                : throw Invalid();
            short value = reader.ReadInt16();
            var origin = (BookOrigin)reader.ReadByte();
            Move canonical = OpeningBook.Transform(move, symmetry);
            if (!Enum.IsDefined(origin) || !OpeningBook.IsLegal(board, player, move) || replies.Exists(e => e.Move == canonical))
            {
                throw Invalid();
            }

            BookEffort effort = default;
            if (BookEntry.IsSearchOrigin(origin))
            {
                var kind = (EffortKind)reader.ReadByte();
                effort = Enum.IsDefined(kind)
                    ? new BookEffort(kind, reader.Read7BitEncodedInt(), reader.ReadByte(), reader.ReadByte())
                    : throw Invalid();
            }

            replies.Add(new BookEntry(canonical, value, origin, effort));

            int reference = reader.Read7BitEncodedInt();
            (Board child, Player opponent) = OpeningBook.Play(board, player, move);
            if (reference == 1)
            {
                ReadPosition(reader, book, child, opponent, keys, depth + 1);
            }
            else if (reference < 0 || (reference >= 2 && (reference - 2 >= keys.Count || keys[reference - 2] != OpeningBook.Canonical(child, opponent).Key)))
            {
                throw Invalid();
            }
        }
    }

    private static InvalidDataException Invalid() => new("The opening book file is not valid.");
}
