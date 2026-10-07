using System.Globalization;
using System.Text;
using Stello.Engine;

namespace Stello.BookTool;

/// <summary>Command line tool for the opening book (docs/brain chapter 16).</summary>
internal static class Program
{
    private const string Usage = """
        Usage: Stello.BookTool <command> [arguments]

          import <OPENING> <book.txt>      Convert the C++ book file to the text book.
          format <book.txt>                Rewrite the text book in its normal form (after editing it by hand).
          build <book.txt> <book.bin>      Build the binary book used by the apps from the text book.
          verify <book.txt> [<book.bin>]   Check the text book, and that the binary book holds the same book.
          stats <book>                     Statistics for a book file (text, binary or C++).
          recalc <book.txt> [--time-s 60 | --depth N] [--workers N] [--save-every 10] [--hash-bits 19]
                                           Search the leaves again; writes <book>.report.txt.
          compare <old book> <new book> [--out <report.txt>]
                                           Report the values and first moves that changed.
          match <book A> <book B> (--starts <report.txt> | --starts-ply N) [--max-starts N]
                [--depth 10 | --time-s S] [--workers N] [--hash-bits 19]
                                           Play the books against each other, two games per start position.

        Ctrl+C stops recalc and match cleanly. --workers is the number of physical cores by default.
        """;

    public static int Main(string[] args)
    {
        CultureInfo.CurrentCulture = CultureInfo.InvariantCulture;
        if (args.Length == 0)
        {
            return Fail(Usage);
        }

        try
        {
            Options options = Options.Parse(args[1..]);
            if (args[0] is not ("recalc" or "compare" or "match"))
            {
                options.Allow();
            }

            return (args[0], options.Positional.Count) switch
            {
                ("import", 2) => Import(options[0], options[1]),
                ("format", 1) => Import(options[0], options[0]),
                ("build", 2) => Build(options[0], options[1]),
                ("verify", 1) => Verify(options[0], null),
                ("verify", 2) => Verify(options[0], options[1]),
                ("stats", 1) => Stats(options[0]),
                ("recalc", 1) => SearchCommands.Recalc(options),
                ("compare", 2) => Compare(options),
                ("match", 2) => SearchCommands.Match(options),
                _ => Fail(Usage),
            };
        }
        catch (ArgumentException exception)
        {
            return Fail(exception.Message);
        }
        catch (Exception exception) when (exception is InvalidDataException or FormatException or IOException or UnauthorizedAccessException)
        {
            return Fail(exception.Message);
        }
    }

    /// <summary>Writes the text book (to a temporary file first).</summary>
    public static void WriteText(OpeningBook book, string path) =>
        WriteAtomically(path, stream =>
        {
            using var writer = new StreamWriter(stream, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
            BookTextFormat.Write(book, writer);
        });

    private static int Import(string sourcePath, string textPath)
    {
        OpeningBook book = OpeningBook.Load(sourcePath);
        WriteText(book, textPath);
        Console.WriteLine($"Wrote {book.PositionCount:N0} positions and {book.NodeCount:N0} moves to {textPath}.");
        return 0;
    }

    private static int Compare(Options options)
    {
        options.Allow("out");
        BookComparison comparison = BookComparison.Of(OpeningBook.Load(options[0]), OpeningBook.Load(options[1]));
        string title = $"Comparison of {options[0]} (old) and {options[1]} (new)";
        if (options.Text("out") is { } path)
        {
            using var writer = new StreamWriter(path);
            comparison.Write(writer, title);
            Console.WriteLine($"Wrote {path}.");
        }
        else
        {
            comparison.Write(Console.Out, title);
        }

        return 0;
    }

    private static int Build(string textPath, string binaryPath)
    {
        OpeningBook book = OpeningBook.Load(textPath);
        WriteAtomically(binaryPath, book.Save);
        Console.WriteLine($"Built {binaryPath} ({new FileInfo(binaryPath).Length:N0} bytes, {book.PositionCount:N0} positions, {book.NodeCount:N0} moves).");
        return 0;
    }

    private static int Verify(string textPath, string? binaryPath)
    {
        string text = File.ReadAllText(textPath).Replace("\r\n", "\n", StringComparison.Ordinal);
        OpeningBook book = BookTextFormat.Read(new StringReader(text));
        string normal = ToText(book);
        int errors = 0;

        Console.WriteLine($"{textPath}: {book.PositionCount:N0} positions, {book.NodeCount:N0} moves, every move legal and every position reached from d3.");
        if (text != normal)
        {
            Console.WriteLine("Warning: the file is not in the normal form (header, order of the lines, or the line that keys a position); run format.");
        }

        BookConsistency consistency = BookAnalysis.Consistency(book);
        Report("Warning: moves whose value is not backed up from the position after them", consistency.NotBackedUp);
        Report("Warning: positions whose moves are not sorted best first", consistency.NotSorted);

        if (binaryPath is not null)
        {
            if (ToText(OpeningBook.Load(binaryPath)) == normal)
            {
                Console.WriteLine($"{binaryPath} holds the same book.");
            }
            else
            {
                Console.WriteLine($"Error: {binaryPath} does not hold the same book; run build again.");
                errors++;
            }
        }

        return errors == 0 ? 0 : 1;
    }

    private static int Stats(string path)
    {
        OpeningBook book = OpeningBook.Load(path);
        BookStatistics stats = BookAnalysis.Statistics(book);

        Console.WriteLine($"Positions:    {stats.Positions:N0}");
        Console.WriteLine($"Moves:        {stats.Moves:N0}");
        Console.WriteLine($"Leaf moves:   {stats.LeafMoves:N0}");
        Console.WriteLine($"Passes:       {stats.PassMoves:N0}");
        Console.WriteLine($"Longest line: {stats.LongestLine} plies");
        Console.WriteLine($"Replies to d3: {string.Join(", ", stats.RootMoves.Select(m => $"{m.Move} ({m.Value}, {m.Origin})"))}");

        Console.WriteLine();
        Console.WriteLine("Origin of the values:");
        foreach ((BookOrigin origin, int count) in stats.Origins)
        {
            Console.WriteLine($"  {origin,-12} {count,8:N0}");
        }

        Console.WriteLine();
        Console.WriteLine("Search limit of the searched values:");
        foreach ((string limit, int count) in stats.Limits)
        {
            Console.WriteLine($"  {(limit == "-" ? "unknown" : limit),-12} {count,8:N0}");
        }

        Console.WriteLine();
        Console.WriteLine("Ply  Positions  Leaf moves   (d3 is ply 1; a position counts at its shortest line)");
        foreach ((int ply, int positions, int leaves) in stats.PerPly)
        {
            Console.WriteLine($"{ply,3}  {positions,9:N0}  {leaves,10:N0}");
        }

        return 0;
    }

    private static string ToText(OpeningBook book)
    {
        using var writer = new StringWriter(CultureInfo.InvariantCulture);
        BookTextFormat.Write(book, writer);
        return writer.ToString();
    }

    private static void Report(string title, IReadOnlyList<string> items)
    {
        if (items.Count == 0)
        {
            return;
        }

        Console.WriteLine($"{title}: {items.Count:N0}, e.g.");
        foreach (string item in items.Take(10))
        {
            Console.WriteLine($"  {item}");
        }
    }

    // A temporary file first, so a failed write never leaves a half-written book.
    public static void WriteAtomically(string path, Action<Stream> write)
    {
        string temporary = path + ".tmp";
        using (FileStream stream = File.Create(temporary))
        {
            write(stream);
        }

        // A virus scanner or indexer can hold the file for a moment just after it was written.
        for (int attempt = 1; ; attempt++)
        {
            try
            {
                File.Move(temporary, path, overwrite: true);
                return;
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException && attempt < 20)
            {
                Thread.Sleep(100);
            }
        }
    }

    private static int Fail(string message)
    {
        Console.Error.WriteLine(message);
        return 1;
    }
}
