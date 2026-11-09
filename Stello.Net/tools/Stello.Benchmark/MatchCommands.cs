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
        string folder = Path.Combine(Program.RepositoryFolder(), "Plans", "benchmark-results");
        return Match(options, folder, Program.Cancellable);
    }

    internal static int Match(Options options, string folder, Func<Action<CancellationToken>, bool> cancellable)
    {
        options.Allow("a", "b", "starts", "max-starts", "workers", "name", "resume");
        string sideAText = options.Text("a") ?? throw new ArgumentException("Give side A with --a, e.g. --a time-ms=100.");
        string sideBText = options.Text("b") ?? throw new ArgumentException("Give side B with --b, e.g. --b time-ms=200.");
        MatchSide a = ParseSide(sideAText);
        MatchSide b = ParseSide(sideBText);
        List<string> starts = LoadStarts(options.Text("starts"));
        starts = starts.Take(options.Number("max-starts", starts.Count, 1, int.MaxValue)).ToList();
        int workers = WorkerCount(options, a, b, ProcessorTopology.PhysicalCores);
        string name = options.Text("name") ?? $"{FileName(a.Name)}_vs_{FileName(b.Name)}";
        string basePath = Path.Combine(folder, $"match_{name}");
        string gamesPath = basePath + ".games.jsonl";
        var settings = MatchRunSettings.Create(sideAText, sideBText, starts);
        using MatchJournal journal = options.Flag("resume")
            ? MatchJournal.Resume(gamesPath, settings, starts)
            : MatchJournal.Create(gamesPath, settings, starts);

        Console.WriteLine($"match: A = {a.Name} ({Describe(a)}), B = {b.Name} ({Describe(b)})");
        // With threads=N sides, --workers × N should not exceed the cores (each game's engines search in turn).
        string threads = a.Limits.Threads == b.Limits.Threads
            ? $"{a.Limits.Threads} thread{(a.Limits.Threads == 1 ? "" : "s")} per engine"
            : $"{a.Limits.Threads} threads for A and {b.Limits.Threads} for B";
        Console.WriteLine($"{starts.Count:N0} start positions, 2 games each, {workers} games at a time, {threads}.");

        var pairs = journal.Pairs();
        var clock = Stopwatch.StartNew();
        bool completed = cancellable(token => new EngineMatch(a, b, workers).Play(
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
            token,
            journal.AFirstGames(),
            journal.BFirstGames(),
            (startIndex, second, game) => journal.Append(new MatchGameRecord(
                MatchJournal.FormatVersion, settings, startIndex, second ? 1 : 0, starts[startIndex], MatchGameData.From(game)))));
        pairs = journal.Pairs();

        if (pairs.Count == 0)
        {
            Console.WriteLine("Stopped before a complete pair of games was finished. Completed games are saved in " + gamesPath);
            return 2;
        }

        var report = MatchReport.Of(a, b, pairs, clock.Elapsed);
        string markdown = report.Markdown(completed);
        Console.WriteLine();
        Console.WriteLine(markdown);

        Directory.CreateDirectory(folder);
        File.WriteAllText(basePath + ".md", markdown);
        File.WriteAllText(basePath + ".json", JsonSerializer.Serialize(report, Json));
        Console.WriteLine($"Saved to {basePath}.md and .json; completed games are in {gamesPath}");
        return completed ? 0 : 2;
    }

    public static int WorkerCount(Options options, MatchSide a, MatchSide b, Func<int> physicalCores)
    {
        if (options.Text("workers") is not null)
        {
            return options.Number("workers", 1, 1, 256);
        }

        return Math.Max(1, physicalCores() / Math.Max(a.Limits.Threads, b.Limits.Threads));
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
    /// A side such as <c>time-ms=100</c>, <c>depth=8</c>, <c>game-s=60,hash-bits=20,name=new,opt=ybwc=off</c> or
    /// <c>engine=egaroucid,time-ms=100,book=on</c>. The engines are <c>stello</c> (the current C++ 4.0 engine, the
    /// default), <c>stello-v3</c> (Stello 3.1 in Rust, preserved), <c>stello-v2</c> (Stello 2.0's C# engine, frozen),
    /// <c>stello-v1</c> (Stello 1, frozen), <c>stello-lib</c> (a separate native library) and <c>egaroucid</c> (the port).
    /// </summary>
    /// <exception cref="ArgumentException">The text is not a valid side.</exception>
    public static MatchSide ParseSide(string text, bool allowLibrary = true)
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
        string? bookFile = null;
        string? libraryFile = null;
        string? librarySha256 = null;
        int? softTimePercent = null;
        int? probCutPercent = null;
        int threads = 1;
        bool selective = true;
        bool ybwc = true;
        bool timeStrict = true;
        var keys = new HashSet<string>();
        var searchOptionKeys = new HashSet<string>();
        foreach (string part in text.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries))
        {
            string[] keyValue = part.Split('=', 2, StringSplitOptions.TrimEntries);
            string key = keyValue[0];
            string value = keyValue.Length == 2 ? keyValue[1] : throw new ArgumentException($"Expected key=value in the side \"{text}\": {part}");
            if (!keys.Add(key) && (key is "lib" or "sha256"))
            {
                throw new ArgumentException($"{key} is given more than once in the side \"{text}\".");
            }

            switch (key)
            {
                case "engine":
                    engine = value switch
                    {
                        "stello" => EngineKind.Stello,
                        "stello-v1" => EngineKind.StelloV1,
                        "stello-v2" => EngineKind.StelloV2,
                        "stello-v3" => EngineKind.StelloV3,
                        "stello-lib" => EngineKind.StelloLibrary,
                        "egaroucid" => EngineKind.Egaroucid,
                        _ => throw new ArgumentException($"engine must be stello, stello-v3, stello-v2, stello-v1, stello-lib or egaroucid in the side \"{text}\"."),
                    };
                    break;
                case "lib":
                    libraryFile = value;
                    break;
                case "sha256":
                    librarySha256 = value;
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
                case "book-file":
                    bookFile = value;
                    useBook = true;
                    break;
                case "book-acc":
                    bookAccuracy = Number(key, value, 0, 10);
                    break;
                case "seed":
                    seed = Number(key, value, 0, int.MaxValue);
                    break;
                case "time-strict":
                    timeStrict = OnOff(key, value, text);
                    break;
                case "name":
                    name = value;
                    break;
                case "eval":
                    SetSearchOption(key, value);
                    break;
                case "soft-pct":
                    SetSearchOption(key, value);
                    break;
                case "mpc":
                    SetSearchOption(key, value);
                    break;
                case "selective":
                    SetSearchOption(key, value);
                    break;
                case "threads":
                    threads = Number(key, value, 1, SearchLimits.MaxThreads);
                    break;
                case "ybwc":
                    SetSearchOption(key, value);
                    break;
                case "opt":
                    string[] option = value.Split('=', 2, StringSplitOptions.TrimEntries);
                    if (option.Length != 2)
                    {
                        throw new ArgumentException($"opt must be key=value in the side \"{text}\".");
                    }

                    SetSearchOption(option[0], option[1]);
                    break;
                default:
                    throw new ArgumentException($"Unknown key in the side \"{text}\": {key}");
            }
        }

        bool egaroucid = engine == EngineKind.Egaroucid;
        bool library = engine == EngineKind.StelloLibrary;
        bool nativeRef = engine is EngineKind.StelloLibrary or EngineKind.StelloV3;
        if (mode is null)
        {
            throw new ArgumentException($"The side \"{text}\" needs time-ms, game-s, {(egaroucid ? "level" : "depth")}.");
        }

        if (library && !allowLibrary)
        {
            throw new ArgumentException("engine=stello-lib is only supported by match --a and --b.");
        }

        if (nativeRef)
        {
            string label = engine == EngineKind.StelloV3 ? "stello-v3" : "stello-lib";
            string? unsupported = new[] { "book-file", "book-acc", "seed", "time-strict", "eval", "soft-pct", "mpc", "selective", "ybwc", "opt" }
                .FirstOrDefault(keys.Contains);
            if (unsupported is not null || useBook)
            {
                throw new ArgumentException($"engine={label} supports no book or search switches (side \"{text}\").");
            }

            if (library && (string.IsNullOrWhiteSpace(libraryFile) || string.IsNullOrWhiteSpace(librarySha256)))
            {
                throw new ArgumentException($"engine=stello-lib needs lib and sha256 in the side \"{text}\".");
            }
        }
        if (!library && (keys.Contains("lib") || keys.Contains("sha256")))
        {
            throw new ArgumentException($"lib and sha256 are only for engine=stello-lib (side \"{text}\").");
        }

        // Only native engines search with threads; the frozen C# engines and Egaroucid port use 1.
        if (keys.Contains("threads") && engine is not (EngineKind.Stello or EngineKind.StelloLibrary or EngineKind.StelloV3))
        {
            throw new ArgumentException($"threads is only for engine=stello, stello-v3 or stello-lib (side \"{text}\").");
        }

        if (keys.Contains("ybwc") && engine != EngineKind.Stello)
        {
            throw new ArgumentException($"ybwc is only for engine=stello (side \"{text}\").");
        }

        if (keys.Contains("opt") && engine != EngineKind.Stello)
        {
            throw new ArgumentException($"opt is only for engine=stello (side \"{text}\").");
        }

        foreach ((string key, bool allowed) in new[]
        {
            ("depth", !egaroucid), ("level", egaroucid), ("book-acc", egaroucid), ("seed", egaroucid), ("eval", engine == EngineKind.Stello),
            ("book-file", !egaroucid), ("time-strict", egaroucid),
            ("soft-pct", engine == EngineKind.Stello),
            ("mpc", engine == EngineKind.Stello),
            ("selective", engine == EngineKind.Stello),
        })
        {
            if (keys.Contains(key) && !allowed)
            {
                throw new ArgumentException($"{key} is only for {(egaroucid ? "engine=stello, stello-v2 and stello-v1" : "engine=egaroucid")} (side \"{text}\").");
            }
        }

        SearchLimits limits = mode switch
        {
            TimeControlMode.TimePerMove => SearchLimits.TimePerMove(TimeSpan.FromMilliseconds(amount), useBook),
            TimeControlMode.TimePerGame => SearchLimits.TimePerGame(TimeSpan.FromSeconds(amount), useBook),
            _ => SearchLimits.FixedDepth(amount, useBook),
        } with { Threads = threads };
        string limitText = mode switch
        {
            TimeControlMode.TimePerMove => $"{amount} ms/move",
            TimeControlMode.TimePerGame => $"{amount} s/game",
            _ => egaroucid ? $"level {amount}" : $"depth {amount}",
        } + (threads > 1 ? $", {threads} threads" : "");

        if (!egaroucid)
        {
            int bits = hashBits ?? SearchLimits.DefaultHashBits;
            // Stello 1 plays from the book it was made with; Stello 2's book was recalculated by Stello 2.
            OpeningBook? book = useBook
                ? OpeningBook.Load(bookFile ?? Program.DataFile(engine == EngineKind.StelloV1 ? "opening-book-stello1.txt" : "opening-book.txt"))
                : null;
            string description = $"{limitText}, hash bits {bits}, {(useBook ? "book" : "no book")}";
            return engine switch
            {
                EngineKind.StelloV1 => new MatchSide(name ?? text, limits, bits, CreatePlayer: () => new V1MatchPlayer(bits, book), Description: $"Stello 1, {description}"),
                EngineKind.StelloV2 => new MatchSide(name ?? text, limits, bits, CreatePlayer: () => new Stello.Engine.V2.V2MatchPlayer(bits, book), Description: $"Stello 2.0 (C#), {description}"),
                EngineKind.StelloV3 => StelloV3Side(name ?? text, limits, bits, description),
                EngineKind.StelloLibrary => StelloLibrarySide(name ?? text, limits, bits, description, libraryFile!, librarySha256!),
                _ => StelloSide(name ?? text, limits, bits, book, description, evalFile, softTimePercent, probCutPercent, selective, ybwc),
            };
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
            CreatePlayer: () => new EgaroucidPlayer(tableBits, egaroucidBook, bookAccuracy, seed, timeStrict),
            Description: $"Egaroucid port, {limitText}, table bits {tableBits}, " +
                (useBook ? $"book (accuracy {bookAccuracy}, seed {seed})" : "no book") + $", strict time {(timeStrict ? "on" : "off")}");

        void SetLimit(TimeControlMode value, int number)
        {
            if (mode is not null)
            {
                throw new ArgumentException($"Give only one of time-ms, game-s and depth/level in the side \"{text}\".");
            }

            mode = value;
            amount = number;
        }

        void SetSearchOption(string key, string value)
        {
            if (!searchOptionKeys.Add(key))
            {
                throw new ArgumentException($"Search option {key} is given more than once in the side \"{text}\".");
            }

            switch (key)
            {
                case "eval":
                    evalFile = value;
                    break;
                case "soft-pct":
                    softTimePercent = Number(key, value, 1, 100);
                    break;
                case "mpc":
                    probCutPercent = Number(key, value, 1, 1000);
                    break;
                case "selective":
                    selective = OnOff(key, value, text);
                    break;
                case "ybwc":
                    ybwc = OnOff(key, value, text);
                    break;
                default:
                    throw new ArgumentException($"Unknown Stello search option in the side \"{text}\": {key}");
            }
        }
    }

    private static bool OnOff(string key, string value, string side) => value switch
    {
        "on" => true,
        "off" => false,
        _ => throw new ArgumentException($"{key} must be on or off in the side \"{side}\"."),
    };

    private static MatchSide StelloV3Side(string name, SearchLimits limits, int bits, string description)
    {
        (string fullPath, string hash, string versionText) = StelloLibrary.ResolveV3();
        return new MatchSide(name, limits, bits,
            CreatePlayer: () => new StelloLibraryMatchPlayer(fullPath, hash, bits),
            Description: $"Stello {versionText} (Rust), {description}, SHA-256 {hash}");
    }

    private static MatchSide StelloLibrarySide(string name, SearchLimits limits, int bits, string description, string path, string sha256)
    {
        using StelloLibrary library = StelloLibrary.Open(path, sha256);
        string fullPath = library.Path;
        string hash = library.Sha256;
        return new MatchSide(name, limits, bits,
            CreatePlayer: () => new StelloLibraryMatchPlayer(fullPath, hash, bits),
            Description: $"Stello library {library.VersionText}, {description}, SHA-256 {hash}");
    }

    /// <summary>A Stello side with its evaluation file and search switches.</summary>
    private static MatchSide StelloSide(string name, SearchLimits limits, int bits, OpeningBook? book, string description,
        string? evalFile, int? softTimePercent, int? probCutPercent, bool selective, bool ybwc)
    {
        SearchOptions options = (evalFile is null or "default" ? SearchOptions.Default : PatternOptions(evalFile)) with
        {
            SoftTimePercent = softTimePercent,
            ProbCutPercent = probCutPercent,
            Selective = selective,
            Ybwc = ybwc,
        };
        if (options == SearchOptions.Default)
        {
            return new MatchSide(name, limits, bits, book, Description: $"Stello, {description}");
        }

        string extra = (evalFile is null or "default" ? "" : $", evaluation {Path.GetFileName(evalFile)}")
            + (softTimePercent is null ? "" : $", soft limit {softTimePercent} %")
            + (probCutPercent is null ? "" : $", Multi-ProbCut t = {probCutPercent / 100.0:0.00}")
            + (selective ? "" : ", Multi-ProbCut off")
            + (ybwc ? "" : ", YBWC off (Lazy SMP alone)");
        return new MatchSide(name, limits, bits, book, CreatePlayer: () => new StelloMatchPlayer(bits, book, options),
            Description: $"Stello, {description}{extra}", Options: options);
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

    /// <summary>The transposition table size of an Egaroucid side: as much memory as Stello 2.0's default hash size (48 MB; 3.0's slots of 64 bytes take 64 MB).</summary>
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
    /// <summary>Stello.Engine: the current C++ 4.0 engine.</summary>
    Stello,

    /// <summary>Stello.Engine.V1: Stello 1, frozen as the baseline.</summary>
    StelloV1,

    /// <summary>Stello.Engine.V2: Stello 2.0's C# engine, frozen as a baseline.</summary>
    StelloV2,

    /// <summary>Stello.Rust 3.1 (<c>stello_engine_v3</c>), preserved as the 3.1 baseline.</summary>
    StelloV3,

    /// <summary>A separately loaded Stello native library.</summary>
    StelloLibrary,

    /// <summary>Egaroucid.Engine: the port of Egaroucid 7.9.</summary>
    Egaroucid,
}
