using System.Globalization;
using System.Text;
using Stello.Engine;

namespace Stello.BookTool;

/// <summary>Command line tool for the opening book (docs chapter 12).</summary>
internal static class Program
{
    private const string Usage = """
        Usage: Stello.BookTool <command> [arguments]

          import <OPENING> <book.txt>      Convert the C++ book file to the text book.
          format <book.txt>                Rewrite the text book in its normal form (after editing it by hand).
          build <book.txt> <book.bin>      Build the binary book used by the apps from the text book.
          verify <book.txt> [<book.bin>]   Check the text book, and that the binary book holds the same book.
          stats <book>                     Statistics for a book file (text, binary or C++).
          trim <book.txt> [--hero-loss 100] [--opp-loss <N|unlimited>] [--max-ply 30] [--dry-run]
                                           Prune unreachable branches and over-deep tails from the book.
          recalc <book.txt> [--time-s 60 | --depth N] [--workers N] [--save-every 10] [--hash-bits 19]
                                           Search the leaves again; writes <book>.report.txt.
          calclib <book.txt> [--time-s 60 | --depth N] [--threshold 100] [--max-ply 30] [--max-adds 3] [--workers N] [--save-every 10] [--hash-bits 19] [--fresh]
                                           Search unbooked legal moves (`onlyMoves`) and add moves within threshold.
          expand <book.txt> [--time-s 60 | --depth N] [--omega 50] [--max-ply 28] [--max-positions 6000] [--iterations N] [--max-time-min M] [--workers N] [--save-every 10] [--hash-bits 19] [--fresh]
                                           Expand promising leaves with Lincke Drop-Out Expansion (DOE).
          compare <old book> <new book> [--out <report.txt>]
                                           Report the values and first moves that changed.
          match <book A> <book B> (--starts <report.txt> | --starts-ply N) [--max-starts N]
                [--depth 10 | --time-s S] [--workers N] [--threads 1] [--hash-bits 19]
                                           Play the books against each other, two games per start position.
                                           --threads: each engine's search threads (1-256); keep workers × threads
                                           within the cores.

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
            string[] flags = args[0] switch
            {
                "trim" => ["dry-run"],
                "calclib" or "expand" => ["fresh"],
                _ => [],
            };
            Options options = Options.Parse(args[1..], flags);
            if (args[0] is not ("trim" or "recalc" or "calclib" or "expand" or "compare" or "match"))
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
                ("trim", 1) => Trim(options),
                ("recalc", 1) => SearchCommands.Recalc(options),
                ("calclib", 1) => SearchCommands.Calclib(options),
                ("expand", 1) => SearchCommands.Expand(options),
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

    private static int Trim(Options options)
    {
        options.Allow("hero-loss", "opp-loss", "max-ply", "dry-run");
        string path = options[0];
        int heroLoss = options.Number("hero-loss", 100, 0, 100_000);
        int? oppLoss = ParseOppLoss(options.Text("opp-loss"));
        int maxPly = options.Number("max-ply", 30, 1, 60);
        bool dryRun = options.Flag("dry-run");

        OpeningBook book = OpeningBook.Load(path);
        BookTrimResult result = BookTrimmer.Trim(book, heroLoss, oppLoss, maxPly);
        BookStatistics stats = BookAnalysis.Statistics(result.Book);

        string oppText = oppLoss is { } opp ? opp.ToString(CultureInfo.InvariantCulture) : "unlimited";
        Console.WriteLine(
            $"{(dryRun ? "Dry run" : "Trimmed")} {path} (hero-loss={heroLoss}, opp-loss={oppText}, max-ply={maxPly}):");
        Console.WriteLine(
            $"  Positions:     {result.PositionsBefore:N0} -> {result.PositionsAfter:N0}");
        Console.WriteLine(
            $"  Moves:         {result.MovesBefore:N0} -> {result.MovesAfter:N0}");
        Console.WriteLine(
            $"  Leaf moves:    {result.LeavesBefore:N0} -> {result.LeavesAfter:N0}");
        Console.WriteLine(
            $"  Severed edges: {result.SeveredEdges:N0}");
        Console.WriteLine(
            $"  Longest line:  {stats.LongestLine} plies");
        Console.WriteLine(
            $"  Replies to d3: {string.Join(", ", stats.RootMoves.Select(m => $"{m.Move} ({m.Value}, {m.Origin})"))}");

        if (!dryRun)
        {
            WriteText(result.Book, path);
            Console.WriteLine($"Wrote {path}. Run build to update the binary book.");
        }

        return 0;
    }

    private static int? ParseOppLoss(string? text)
    {
        if (text is null || text.Equals("unlimited", StringComparison.OrdinalIgnoreCase))
        {
            return null;
        }

        return int.TryParse(text, NumberStyles.None, CultureInfo.InvariantCulture, out int value) && value <= 100_000
            ? value
            : throw new ArgumentException("--opp-loss must be a whole number from 0 to 100000, or 'unlimited'.");
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
