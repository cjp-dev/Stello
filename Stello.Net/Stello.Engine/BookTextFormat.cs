using System.Globalization;
using System.Text;

namespace Stello.Engine;

/// <summary>The master book in git (Stello.Net/Book/opening-book.txt): one line per position, readable and diffable.</summary>
/// <remarks>
/// A position is keyed by the shortest line of book moves that reaches it from the start, written without spaces
/// and starting with d3 (lexically smallest if there are several; "--" is a pass). Its book moves follow in book
/// order, in the frame of that line: <c>move:value:origin</c>, for a searched value with
/// <c>:limit:d&lt;depth reached&gt;:v&lt;engine version&gt;</c>. Lines are sorted by length, then alphabetically,
/// so the file only changes where the book changes.
/// </remarks>
internal static class BookTextFormat
{
    public const string FirstLine = "# Stello opening book, format 2";

    private const string PassText = "--";

    public static bool IsText(byte[] data)
    {
        ReadOnlySpan<byte> text = data.AsSpan();
        if (text.StartsWith("\uFEFF"u8))
        {
            text = text[3..];
        }

        return text.StartsWith("# Stello opening book"u8);
    }

    public static void Write(OpeningBook book, TextWriter writer)
    {
        List<BookLine> lines = Lines(book);
        writer.Write(FirstLine + "\n");
        writer.Write(string.Create(CultureInfo.InvariantCulture, $"# {lines.Count} positions, {book.NodeCount} moves\n"));
        writer.Write("# <line> <move>:<value>:<origin>[:<limit>:d<depth reached>:v<engine version>] ...\n");
        writer.Write("# origin: U unknown, H heuristic, W win/loss/draw, X exact, B backed up; limit: time per move (s, ms), depth (ply), solve, - unknown\n");

        var text = new StringBuilder();
        foreach (BookLine line in lines)
        {
            book.TryGetReplies(line.Board, line.Player, out List<BookEntry>? replies, out OpeningBook.Symmetry symmetry);
            text.Clear().Append(line.Moves);
            foreach (BookEntry entry in replies!)
            {
                text.Append(' ');
                AppendEntry(text, OpeningBook.Transform(entry.Move, symmetry), entry);
            }

            writer.Write(text.Append('\n'));
        }
    }

    /// <exception cref="InvalidDataException">The text is not a valid book.</exception>
    public static OpeningBook Read(TextReader reader)
    {
        if (reader.ReadLine()?.TrimStart('\uFEFF') != FirstLine)
        {
            throw new InvalidDataException($"The opening book must start with \"{FirstLine}\".");
        }

        OpeningBook book = OpeningBook.CreateEmpty();
        int lineNumber = 1;
        while (reader.ReadLine() is { } text)
        {
            lineNumber++;
            if (string.IsNullOrWhiteSpace(text) || text.StartsWith('#'))
            {
                continue;
            }

            try
            {
                ReadPosition(book, text);
            }
            catch (FormatException exception)
            {
                throw new InvalidDataException($"Line {lineNumber}: {exception.Message}", exception);
            }
        }

        int reachable = Lines(book).Count;
        if (reachable != book.PositionCount)
        {
            throw new InvalidDataException(
                $"{book.PositionCount - reachable} positions cannot be reached from d3 by book moves.");
        }

        return book;
    }

    /// <summary>Every book position with the line that keys it, in file order.</summary>
    internal static List<BookLine> Lines(OpeningBook book)
    {
        var lines = new List<BookLine>();
        if (!book.Contains(OpeningBook.RootBoard, OpeningBook.RootPlayer))
        {
            return lines;
        }

        var seen = new HashSet<BookKey> { OpeningBook.Canonical(OpeningBook.RootBoard, OpeningBook.RootPlayer).Key };
        var level = new List<BookLine> { new("d3", OpeningBook.RootBoard, OpeningBook.RootPlayer) };
        while (level.Count > 0)
        {
            // Parents in alphabetical order, so a position is keyed by the smallest of its shortest lines.
            level.Sort((a, b) => string.CompareOrdinal(a.Moves, b.Moves));
            lines.AddRange(level);

            var next = new List<BookLine>();
            foreach (BookLine line in level)
            {
                book.TryGetReplies(line.Board, line.Player, out List<BookEntry>? replies, out OpeningBook.Symmetry symmetry);
                foreach (BookEntry entry in replies!)
                {
                    Move move = OpeningBook.Transform(entry.Move, symmetry);
                    (Board child, Player opponent) = OpeningBook.Play(line.Board, line.Player, move);
                    BookKey key = OpeningBook.Canonical(child, opponent).Key;
                    if (book.TryGetReplies(key, out _) && seen.Add(key))
                    {
                        next.Add(new BookLine(line.Moves + MoveText(move), child, opponent));
                    }
                }
            }

            level = next;
        }

        return lines;
    }

    private static void ReadPosition(OpeningBook book, string text)
    {
        string[] parts = text.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        string line = parts[0];
        (Board board, Player player) = Replay(line);
        if (parts.Length < 2)
        {
            throw new FormatException($"The position after {line} has no book moves.");
        }

        if (book.Contains(board, player))
        {
            throw new FormatException($"The position after {line} is already in the book.");
        }

        List<BookEntry> replies = book.GetOrAddReplies(board, player, out OpeningBook.Symmetry symmetry);
        foreach (string token in parts.Skip(1))
        {
            BookEntry entry = ParseEntry(token, board, player, symmetry);
            if (replies.Exists(e => e.Move == entry.Move))
            {
                throw new FormatException($"The move in '{token}' is given twice.");
            }

            replies.Add(entry);
        }
    }

    /// <summary>Plays a line such as "d3c5f6" from the start.</summary>
    /// <exception cref="FormatException">The line does not start with d3 or has a move that is not legal.</exception>
    internal static (Board Board, Player Player) Replay(string line)
    {
        if (line.Length % 2 != 0 || !line.StartsWith("d3", StringComparison.Ordinal))
        {
            throw new FormatException($"'{line}' is not a line of moves starting with d3.");
        }

        Board board = Board.Initial;
        Player player = Player.Black;
        for (int i = 0; i < line.Length; i += 2)
        {
            Move move = ParseMove(line.Substring(i, 2), board, player);
            (board, player) = OpeningBook.Play(board, player, move);
        }

        return (board, player);
    }

    private static BookEntry ParseEntry(string token, Board board, Player player, OpeningBook.Symmetry symmetry)
    {
        string[] fields = token.Split(':');
        if (fields.Length is not (3 or 6)
            || !short.TryParse(fields[1], NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out short value))
        {
            throw new FormatException($"'{token}' is not a book move (move:value:origin).");
        }

        Move move = OpeningBook.Transform(ParseMove(fields[0], board, player), symmetry);
        BookOrigin origin = fields[2] switch
        {
            "U" => BookOrigin.Unknown,
            "H" => BookOrigin.Heuristic,
            "W" => BookOrigin.WinLossDraw,
            "X" => BookOrigin.Exact,
            "B" => BookOrigin.BackedUp,
            _ => throw new FormatException($"'{fields[2]}' in '{token}' is not an origin (U, H, W, X, B)."),
        };

        if (fields.Length == 3)
        {
            return new BookEntry(move, value, origin);
        }

        if (!BookEntry.IsSearchOrigin(origin)
            || !TryParseLimit(fields[3], out EffortKind kind, out int amount)
            || !TryParsePrefixed(fields[4], 'd', out int depth)
            || !TryParsePrefixed(fields[5], 'v', out int engine))
        {
            throw new FormatException($"'{token}' does not have a valid search effort (limit:d<depth>:v<engine>).");
        }

        return new BookEntry(move, value, origin, new BookEffort(kind, amount, depth, engine));
    }

    private static Move ParseMove(string text, Board board, Player player)
    {
        Move move = text == PassText ? Move.Pass
            : Square.TryParse(text, out Square? square) ? new Move(square.Value)
            : throw new FormatException($"'{text}' is not a move.");
        return OpeningBook.IsLegal(board, player, move) ? move : throw new FormatException($"{move} is not legal for {player}.");
    }

    private static bool TryParseLimit(string text, out EffortKind kind, out int amount)
    {
        (kind, amount, bool valid) = text switch
        {
            "-" => (EffortKind.None, 0, true),
            "solve" => (EffortKind.Solve, 0, true),
            _ when text.EndsWith("ms", StringComparison.Ordinal) && TryParseNumber(text[..^2], out int ms) => (EffortKind.Time, ms, true),
            _ when text.EndsWith('s') && TryParseNumber(text[..^1], out int seconds) => (EffortKind.Time, seconds * 1000, true),
            _ when text.EndsWith("ply", StringComparison.Ordinal) && TryParseNumber(text[..^3], out int plies) => (EffortKind.Depth, plies, true),
            _ => (EffortKind.None, 0, false),
        };
        return valid;
    }

    private static bool TryParsePrefixed(string text, char prefix, out int number)
    {
        number = 0;
        return text.Length > 1 && text[0] == prefix && TryParseNumber(text[1..], out number);
    }

    private static bool TryParseNumber(string text, out int number) =>
        int.TryParse(text, NumberStyles.None, CultureInfo.InvariantCulture, out number);

    private static void AppendEntry(StringBuilder text, Move move, BookEntry entry)
    {
        char origin = entry.Origin switch
        {
            BookOrigin.Heuristic => 'H',
            BookOrigin.WinLossDraw => 'W',
            BookOrigin.Exact => 'X',
            BookOrigin.BackedUp => 'B',
            _ => 'U',
        };
        text.Append(CultureInfo.InvariantCulture, $"{MoveText(move)}:{entry.Value}:{origin}");

        BookEffort effort = entry.Effort;
        if (entry.IsSearched && effort != default)
        {
            text.Append(CultureInfo.InvariantCulture, $":{LimitText(effort)}:d{effort.DepthReached}:v{effort.EngineVersion}");
        }
    }

    /// <summary>The search limit, e.g. "60s", "500ms", "8ply", "solve", or "-" if unknown.</summary>
    internal static string LimitText(BookEffort effort) => effort.Kind switch
    {
        EffortKind.Time when effort.Amount % 1000 == 0 => string.Create(CultureInfo.InvariantCulture, $"{effort.Amount / 1000}s"),
        EffortKind.Time => string.Create(CultureInfo.InvariantCulture, $"{effort.Amount}ms"),
        EffortKind.Depth => string.Create(CultureInfo.InvariantCulture, $"{effort.Amount}ply"),
        EffortKind.Solve => "solve",
        _ => "-",
    };

    internal static string MoveText(Move move) => move.Square?.ToString() ?? PassText;
}

/// <param name="Moves">The moves from the start, without spaces, e.g. "d3c5f6".</param>
internal sealed record BookLine(string Moves, Board Board, Player Player);
