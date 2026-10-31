using System.Diagnostics;
using System.Globalization;
using System.Text;
using System.Text.Json;
using Stello.BookTool;
using Stello.Egaroucid;
using Stello.Engine;
using Stello.Engine.Evaluation;
using Stello.Engine.Search;
using Stello.Engine.V1;
using Board = Stello.Engine.Board;

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
        MatchSide a = ParseSide(options.Text("a") ?? throw new ArgumentException("Give side A with --a, e.g. --a time-ms=100."));
        MatchSide b = ParseSide(options.Text("b") ?? throw new ArgumentException("Give side B with --b, e.g. --b time-ms=200."));
        List<string> starts = LoadStarts(options.Text("starts"));
        starts = starts.Take(options.Number("max-starts", starts.Count, 1, int.MaxValue)).ToList();
        int workers = options.Number("workers", Math.Max(1, Environment.ProcessorCount / 2), 1, 256);
        string name = options.Text("name") ?? $"{FileName(a.Name)}_vs_{FileName(b.Name)}";

        Console.WriteLine($"match: A = {a.Name} ({Describe(a)}), B = {b.Name} ({Describe(b)})");
        Console.WriteLine($"{starts.Count:N0} start positions, 2 games each, {workers} games at a time, 1 thread per engine.");

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
    /// A side such as <c>time-ms=100</c>, <c>depth=8</c>, <c>game-s=60,hash-bits=20,name=new</c> or
    /// <c>engine=egaroucid,time-ms=100,book=on</c>. The engines are <c>stello</c> (Stello 2, the default), <c>stello-v1</c>
    /// (Stello 1, frozen) and <c>egaroucid</c> (the port).
    /// </summary>
    /// <exception cref="ArgumentException">The text is not a valid side.</exception>
    public static MatchSide ParseSide(string text)
    {
        TimeControlMode? mode = null;
        int amount = 0;
        EngineKind engine = EngineKind.Stello;
        bool useBook = false;
        int? hashBits = null;
        int bookAccuracy = 0;
        int seed = 1;
        string? name = null;
        string? evalFile = null;
        int? selectiveMargin = null;
        int? selectivePercent = null;
        bool dangerousCorners = true;
        var keys = new HashSet<string>();
        foreach (string part in text.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries))
        {
            string[] keyValue = part.Split('=', 2, StringSplitOptions.TrimEntries);
            string key = keyValue[0];
            string value = keyValue.Length == 2 ? keyValue[1] : throw new ArgumentException($"Expected key=value in the side \"{text}\": {part}");
            keys.Add(key);
            switch (key)
            {
                case "engine":
                    engine = value switch
                    {
                        "stello" => EngineKind.Stello,
                        "stello-v1" => EngineKind.StelloV1,
                        "egaroucid" => EngineKind.Egaroucid,
                        _ => throw new ArgumentException($"engine must be stello, stello-v1 or egaroucid in the side \"{text}\"."),
                    };
                    break;
                case "time-ms":
                    SetLimit(TimeControlMode.TimePerMove, Number(key, value, 1, 3_600_000));
                    break;
                case "game-s":
                    SetLimit(TimeControlMode.TimePerGame, Number(key, value, 1, 36_000));
                    break;
                case "depth":
                case "level":
                    SetLimit(TimeControlMode.FixedDepth, Number(key, value, 1, 60));
                    break;
                case "hash-bits":
                    hashBits = Number(key, value, 10, 26);
                    break;
                case "book":
                    useBook = value switch
                    {
                        "on" => true,
                        "off" => false,
                        _ => throw new ArgumentException($"book must be on or off in the side \"{text}\"."),
                    };
                    break;
                case "book-acc":
                    bookAccuracy = Number(key, value, 0, 10);
                    break;
                case "seed":
                    seed = Number(key, value, 0, int.MaxValue);
                    break;
                case "name":
                    name = value;
                    break;
                case "eval":
                    evalFile = value;
                    break;
                case "sel-margin":
                    selectiveMargin = Number(key, value, -10_000, 10_000);
                    break;
                case "sel-percent":
                    selectivePercent = Number(key, value, 0, 1000);
                    break;
                case "danger":
                    dangerousCorners = value switch
                    {
                        "on" => true,
                        "off" => false,
                        _ => throw new ArgumentException($"danger must be on or off in the side \"{text}\"."),
                    };
                    break;
                default:
                    throw new ArgumentException($"Unknown key in the side \"{text}\": {key}");
            }
        }

        bool egaroucid = engine == EngineKind.Egaroucid;
        if (mode is null)
        {
            throw new ArgumentException($"The side \"{text}\" needs time-ms, game-s, {(egaroucid ? "level" : "depth")}.");
        }

        foreach ((string key, bool allowed) in new[]
        {
            ("depth", !egaroucid), ("level", egaroucid), ("book-acc", egaroucid), ("seed", egaroucid), ("eval", engine == EngineKind.Stello), ("sel-margin", engine == EngineKind.Stello),
            ("sel-percent", engine == EngineKind.Stello), ("danger", engine == EngineKind.Stello),
        })
        {
            if (keys.Contains(key) && !allowed)
            {
                throw new ArgumentException($"{key} is only for {(egaroucid ? "engine=stello and stello-v1" : "engine=egaroucid")} (side \"{text}\").");
            }
        }

        SearchLimits limits = mode switch
        {
            TimeControlMode.TimePerMove => SearchLimits.TimePerMove(TimeSpan.FromMilliseconds(amount), useBook),
            TimeControlMode.TimePerGame => SearchLimits.TimePerGame(TimeSpan.FromSeconds(amount), useBook),
            _ => SearchLimits.FixedDepth(amount, useBook),
        };
        string limitText = mode switch
        {
            TimeControlMode.TimePerMove => $"{amount} ms/move",
            TimeControlMode.TimePerGame => $"{amount} s/game",
            _ => egaroucid ? $"level {amount}" : $"depth {amount}",
        };

        if (!egaroucid)
        {
            int bits = hashBits ?? SearchLimits.DefaultHashBits;
            OpeningBook? book = useBook ? OpeningBook.Load(Program.DataFile("opening-book.txt")) : null;
            string description = $"{limitText}, hash bits {bits}, {(useBook ? "book" : "no book")}";
            return engine == EngineKind.StelloV1
                ? new MatchSide(name ?? text, limits, bits, CreatePlayer: () => new V1MatchPlayer(bits, book), Description: $"Stello 1, {description}")
                : StelloSide(name ?? text, limits, bits, book, description, evalFile, selectiveMargin, selectivePercent, dangerousCorners);
        }

        // 2^21 slots of 24 bytes take as much memory as Stello's default: two tables of 2^19 slots of two 24-byte entries.
        int tableBits = hashBits ?? EgaroucidDefaultTableBits;
        if (tableBits > HashTables.DefaultHashLevel)
        {
            throw new ArgumentException($"hash-bits for engine=egaroucid is at most {HashTables.DefaultHashLevel} (side \"{text}\").");
        }

        LoadEgaroucid();
        Book? egaroucidBook = useBook ? EgaroucidResources.Book : null;
        if (useBook && egaroucidBook!.Count == 0)
        {
            throw new ArgumentException($"Egaroucid's book.egbk3 is not in {EgaroucidResources.LoadedDirectory}.");
        }

        return new MatchSide(name ?? text, limits, tableBits,
            CreatePlayer: () => new EgaroucidPlayer(tableBits, egaroucidBook, bookAccuracy, seed),
            Description: $"Egaroucid port, {limitText}, table bits {tableBits}, " +
                (useBook ? $"book (accuracy {bookAccuracy}, seed {seed})" : "no book"));

        void SetLimit(TimeControlMode value, int number)
        {
            if (mode is not null)
            {
                throw new ArgumentException($"Give only one of time-ms, game-s and depth/level in the side \"{text}\".");
            }

            mode = value;
            amount = number;
        }
    }

    /// <summary>A Stello side with its search options: the evaluation file and the switches of Stello 2's work.</summary>
    private static MatchSide StelloSide(string name, SearchLimits limits, int bits, OpeningBook? book, string description,
        string? evalFile, int? selectiveMargin, int? selectivePercent, bool dangerousCorners)
    {
        SearchOptions options = (evalFile is null or "classic" ? SearchOptions.Classic : PatternOptions(evalFile)) with
        {
            SelectiveMargin = selectiveMargin,
            SelectivePercent = selectivePercent,
            DangerousCorners = dangerousCorners,
        };
        if (options == SearchOptions.Classic)
        {
            return new MatchSide(name, limits, bits, book, Description: $"Stello, {description}");
        }

        string extra = (evalFile is null or "classic" ? "" : $", evaluation {Path.GetFileName(evalFile)}")
            + (selectiveMargin is null && selectivePercent is null ? "" : $", selective margin {selectivePercent ?? 50} % + {selectiveMargin ?? 50}")
            + (dangerousCorners ? "" : ", no corner extension");
        return new MatchSide(name, limits, bits, book, CreatePlayer: () => new StelloMatchPlayer(bits, book, options),
            Description: $"Stello, {description}{extra}");
    }

    private static readonly Dictionary<string, SearchOptions> LoadedOptions = [];

    /// <summary>The search options with the pattern evaluation in <paramref name="evalFile"/>, loaded once per file.</summary>
    /// <exception cref="ArgumentException">The file does not exist or is not an evaluation file.</exception>
    /// <remarks><c>default</c> is Stello 2's evaluation, <c>Book/stello-eval.bin</c> (copied to the tool's Data folder).</remarks>
    public static SearchOptions PatternOptions(string evalFile)
    {
        string path = Path.GetFullPath(evalFile == "default" ? Program.DataFile("stello-eval.bin") : evalFile);
        lock (LoadedOptions)
        {
            if (!LoadedOptions.TryGetValue(path, out SearchOptions? options))
            {
                try
                {
                    options = new SearchOptions { Pattern = EvalWeights.Load(path) };
                }
                catch (Exception exception) when (exception is IOException or InvalidDataException or UnauthorizedAccessException)
                {
                    throw new ArgumentException($"Cannot read the evaluation file {evalFile}: {exception.Message}", exception);
                }

                LoadedOptions.Add(path, options);
            }

            return options;
        }
    }

    /// <summary>The transposition table size of an Egaroucid side: as much memory as Stello's default hash size.</summary>
    public const int EgaroucidDefaultTableBits = 21;

    /// <summary>Loads Egaroucid's resources (see <see cref="EgaroucidResources.FindDirectory"/>) before the first Egaroucid search.</summary>
    /// <exception cref="ArgumentException">They were not found.</exception>
    public static void LoadEgaroucid()
    {
        try
        {
            EgaroucidResources.Load();
        }
        catch (DirectoryNotFoundException exception)
        {
            throw new ArgumentException(exception.Message, exception);
        }
    }

    public static string Describe(MatchSide side) => side.Description ?? (side.Limits.Mode switch
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

/// <summary>The engine of a match side or benchmark.</summary>
internal enum EngineKind
{
    /// <summary>Stello.Engine: Stello 2 as it develops.</summary>
    Stello,

    /// <summary>Stello.Engine.V1: Stello 1, frozen as the baseline.</summary>
    StelloV1,

    /// <summary>Egaroucid.Engine: the port of Egaroucid 7.9.</summary>
    Egaroucid,
}
