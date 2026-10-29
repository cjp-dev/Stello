using System.Diagnostics;
using System.Globalization;
using System.Text;
using System.Text.Json;
using Stello.BookTool;
using Stello.Engine;

namespace Stello.Benchmark;

/// <summary>The engine match (match) and its start positions (make-starts).</summary>
internal static class MatchCommands
{
    private static readonly JsonSerializerOptions Json = new()
    {
        WriteIndented = true,
        NumberHandling = System.Text.Json.Serialization.JsonNumberHandling.AllowNamedFloatingPointLiterals,
    };

    public static int Match(Options options)
    {
        options.Allow("a", "b", "starts", "max-starts", "workers", "name");
        MatchSide a = ParseSide(options.Text("a") ?? throw new ArgumentException("Give side A with --a, e.g. --a time-ms=100."), "A");
        MatchSide b = ParseSide(options.Text("b") ?? throw new ArgumentException("Give side B with --b, e.g. --b time-ms=200."), "B");
        List<string> starts = LoadStarts(options.Text("starts"));
        starts = starts.Take(options.Number("max-starts", starts.Count, 1, int.MaxValue)).ToList();
        int workers = options.Number("workers", Math.Max(1, Environment.ProcessorCount / 2), 1, 256);
        string name = options.Text("name") ?? $"{FileName(a.Name)}_vs_{FileName(b.Name)}";

        Console.WriteLine($"match: A = {a.Name} ({Describe(a)}), B = {b.Name} ({Describe(b)})");
        Console.WriteLine($"{starts.Count:N0} start positions, 2 games each, {workers} games at a time, no opening book.");

        var pairs = new List<MatchPair>();
        var clock = Stopwatch.StartNew();
        bool completed = Program.Cancellable(token => new EngineMatch(a, b, workers).Play(
            starts,
            pair =>
            {
                pairs.Add(pair);
                if (pairs.Count % 10 == 0 || pairs.Count == starts.Count)
                {
                    MatchSummary sofar = MatchSummary.Of(pairs);
                    Console.WriteLine(
                        $"  [{2 * pairs.Count,5:N0}/{2 * starts.Count:N0}] B {sofar.PointsB:0.#}/{2 * pairs.Count} ({sofar.Score:P1}), " +
                        $"discs {sofar.AverageDiscsB:+0.0;-0.0;0.0}, {Program.Duration(clock.Elapsed)}");
                }
            },
            token));

        if (pairs.Count == 0)
        {
            Console.WriteLine("Stopped before the first pair was finished.");
            return 2;
        }

        var report = MatchReport.Of(a, b, pairs, clock.Elapsed);
        string markdown = report.Markdown(completed);
        Console.WriteLine();
        Console.WriteLine(markdown);

        string folder = Path.Combine(Program.RepositoryFolder(), "Plans", "benchmark-results");
        Directory.CreateDirectory(folder);
        string basePath = Path.Combine(folder, $"match_{name}");
        File.WriteAllText(basePath + ".md", markdown);
        File.WriteAllText(basePath + ".json", JsonSerializer.Serialize(report, Json));
        Console.WriteLine($"Saved to {basePath}.md and .json");
        return completed ? 0 : 2;
    }

    public static int MakeStarts(Options options)
    {
        options.Allow("book", "plies", "max-value", "out");
        string bookPath = options.Text("book") ?? Program.DataFile("opening-book.txt");
        (int fromPly, int toPly) = Range(options.Text("plies") ?? "10-12");
        int maxValue = options.Number("max-value", 40, 0, 32000);
        string path = options.Text("out") ?? Program.DataFile("starts.txt");

        OpeningBook book = OpeningBook.Load(bookPath);
        var lines = BookTextFormat.Lines(book)
            .Where(line => line.Moves.Length >= 2 * fromPly && line.Moves.Length <= 2 * toPly)
            .Select(line => (line.Moves, Value: Value(book, line.Board, line.Player)))
            .Where(line => line.Value is { } value && Math.Abs(value) <= maxValue)
            .ToList();

        // A fixed shuffle, so that the first N start positions (--max-starts N) mix the plies and the openings.
        var random = new Random(1);
        random.Shuffle(System.Runtime.InteropServices.CollectionsMarshal.AsSpan(lines));

        var text = new StringBuilder();
        text.AppendLine(CultureInfo.InvariantCulture, $"# Start positions for match: the book positions at ply {fromPly} to {toPly} whose best book move has a value");
        text.AppendLine(CultureInfo.InvariantCulture, $"# of at most {maxValue} either way (in evaluation units, for the side to move), from {Path.GetFileName(bookPath)}, in a fixed shuffled order.");
        text.AppendLine("# <line from d3> <value of the best book move>");
        foreach ((string moves, int? value) in lines)
        {
            text.AppendLine(CultureInfo.InvariantCulture, $"{moves} {value}");
        }

        File.WriteAllText(path, text.ToString());
        Console.WriteLine($"Wrote {lines.Count:N0} start positions to {path}.");
        return 0;
    }

    public static List<string> LoadStarts(string? path)
    {
        using StreamReader reader = File.OpenText(path ?? Program.DataFile("starts.txt"));
        List<string> starts = BookMatch.ReadStarts(reader);
        return starts.Count > 0 ? starts : throw new ArgumentException("The start file holds no start positions.");
    }

    /// <summary>
    /// A side such as <c>time-ms=100</c>, <c>depth=8</c> or <c>game-s=60,hash-bits=20,name=new</c>.
    /// </summary>
    /// <exception cref="ArgumentException">The text is not a valid side.</exception>
    public static MatchSide ParseSide(string text, string defaultName)
    {
        SearchLimits? limits = null;
        int hashBits = SearchLimits.DefaultHashBits;
        string? name = null;
        foreach (string part in text.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries))
        {
            string[] keyValue = part.Split('=', 2, StringSplitOptions.TrimEntries);
            string key = keyValue[0];
            string value = keyValue.Length == 2 ? keyValue[1] : throw new ArgumentException($"Expected key=value in the side \"{text}\": {part}");
            switch (key)
            {
                case "time-ms":
                    SetLimits(SearchLimits.TimePerMove(TimeSpan.FromMilliseconds(Number(key, value, 1, 3_600_000)), useOpeningBook: false));
                    break;
                case "game-s":
                    SetLimits(SearchLimits.TimePerGame(TimeSpan.FromSeconds(Number(key, value, 1, 36_000)), useOpeningBook: false));
                    break;
                case "depth":
                    SetLimits(SearchLimits.FixedDepth(Number(key, value, 1, 60), useOpeningBook: false));
                    break;
                case "hash-bits":
                    hashBits = Number(key, value, 10, 26);
                    break;
                case "name":
                    name = value;
                    break;
                default:
                    throw new ArgumentException($"Unknown key in the side \"{text}\": {key}");
            }
        }

        if (limits is null)
        {
            throw new ArgumentException($"The side \"{text}\" needs time-ms, game-s or depth.");
        }

        return new MatchSide(name ?? text, limits, hashBits);

        void SetLimits(SearchLimits value) =>
            limits = limits is null ? value : throw new ArgumentException($"Give only one of time-ms, game-s and depth in the side \"{text}\".");
    }

    public static string Describe(MatchSide side) => (side.Limits.Mode switch
    {
        TimeControlMode.FixedDepth => $"depth {side.Limits.Depth}",
        TimeControlMode.TimePerMove => $"{side.Limits.Time.TotalMilliseconds:0} ms/move",
        TimeControlMode.TimePerGame => $"{side.Limits.Time.TotalSeconds:0} s/game",
        _ => "solve",
    }) + $", hash bits {side.HashBits}";

    private static int? Value(OpeningBook book, Board board, Player player) =>
        book.TryGetReplies(board, player, out List<BookEntry>? replies, out _) && replies.Count > 0 ? replies.Max(r => r.Value) : null;

    private static (int From, int To) Range(string text)
    {
        string[] parts = text.Split('-');
        return parts.Length is 1 or 2 && int.TryParse(parts[0], out int from) && int.TryParse(parts[^1], out int to) && from >= 1 && to >= from && to <= 60
            ? (from, to)
            : throw new ArgumentException("--plies must be a ply or a range such as 10-12.");
    }

    private static int Number(string key, string value, int min, int max) =>
        int.TryParse(value, NumberStyles.None, CultureInfo.InvariantCulture, out int number) && number >= min && number <= max
            ? number
            : throw new ArgumentException($"{key} must be a whole number from {min} to {max}.");

    private static string FileName(string name) =>
        string.Concat(name.Select(c => char.IsLetterOrDigit(c) ? c : '-')).Trim('-');
}
