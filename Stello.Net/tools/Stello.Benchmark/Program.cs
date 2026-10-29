using System.Globalization;
using Stello.BookTool;

namespace Stello.Benchmark;

/// <summary>Command line tool that measures the engine: matches between engines and speed benchmarks (docs/brain chapter 18).</summary>
internal static class Program
{
    private const string Usage = """
        Usage: Stello.Benchmark <command> [options]

          match --a <side> --b <side> [--starts <file>] [--max-starts N] [--workers N] [--name <name>]
                                    Play side A against side B, two games per start position with the colours swapped.
                                    A side is a list such as "time-ms=100", "depth=8", "game-s=60,hash-bits=20,name=new":
                                    time-ms (per move), game-s (per game), depth, hash-bits (19), name.
          solve-bench [--positions <file>] [--from 40] [--to 44] [--passes 1] [--hash-bits 19] [--out <file.json>] [--compare <file.json>]
                                    Solve the FFO positions exactly: score check, nodes, time to solve.
          mid-bench [--positions <file>] [--depth 10] [--passes 3] [--hash-bits 19] [--out <file.json>] [--compare <file.json>]
                                    Search the midgame positions to a fixed depth: nodes, move and score of each search, speed.
          make-starts [--book <book.txt>] [--plies 10-12] [--max-value 40] [--out <file>]
                                    Write balanced start positions for match from the opening book.
          make-positions [--starts <file>] [--games 20] [--depth 6] [--empties 48,42,36,30,26] [--out <file>]
                                    Write the midgame positions for mid-bench from games the engine plays against itself.

        The default files are in the Data folder next to the tool. match writes its results to Plans/benchmark-results.
        --compare reports whether every search visited the same nodes and found the same move and score as in the saved
        file (the check that a change kept the search unchanged), and the time against the saved time.
        Ctrl+C stops match cleanly and reports the games played so far.
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
            if (options.Positional.Count != 0)
            {
                return Fail(Usage);
            }

            return args[0] switch
            {
                "match" => MatchCommands.Match(options),
                "make-starts" => MatchCommands.MakeStarts(options),
                "solve-bench" => BenchCommands.SolveBench(options),
                "mid-bench" => BenchCommands.MidBench(options),
                "make-positions" => BenchCommands.MakePositions(options),
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

    /// <summary>A file in the Data folder next to the tool.</summary>
    public static string DataFile(string name) => Path.Combine(AppContext.BaseDirectory, "Data", name);

    /// <summary>The Stello repository folder: the first folder above the current folder or the tool with Stello.Net/Stello.Net.slnx.</summary>
    public static string RepositoryFolder()
    {
        foreach (string start in new[] { Environment.CurrentDirectory, AppContext.BaseDirectory })
        {
            for (DirectoryInfo? folder = new(start); folder is not null; folder = folder.Parent)
            {
                if (File.Exists(Path.Combine(folder.FullName, "Stello.Net", "Stello.Net.slnx")))
                {
                    return folder.FullName;
                }
            }
        }

        return Environment.CurrentDirectory;
    }

    /// <summary>Runs <paramref name="work"/>; Ctrl+C cancels its token. Returns false if Ctrl+C stopped the work.</summary>
    public static bool Cancellable(Action<CancellationToken> work)
    {
        using var cancel = new CancellationTokenSource();
        ConsoleCancelEventHandler handler = (_, e) =>
        {
            e.Cancel = true;
            Console.WriteLine("Stopping...");
            cancel.Cancel();
        };

        Console.CancelKeyPress += handler;
        try
        {
            work(cancel.Token);
            return true;
        }
        catch (OperationCanceledException) when (cancel.IsCancellationRequested)
        {
            return false;
        }
        finally
        {
            Console.CancelKeyPress -= handler;
        }
    }

    public static string Duration(TimeSpan time) =>
        time.TotalHours >= 1 ? $"{(int)time.TotalHours}h {time.Minutes:00}m" : $"{time.Minutes}m {time.Seconds:00}s";

    private static int Fail(string message)
    {
        Console.Error.WriteLine(message);
        return 1;
    }
}
