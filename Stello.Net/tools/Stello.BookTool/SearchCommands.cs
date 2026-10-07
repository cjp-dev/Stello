using System.Diagnostics;
using Stello.Engine;

namespace Stello.BookTool;

/// <summary>The commands that run the engine for a long time: recalc and match. Ctrl+C stops them cleanly.</summary>
internal static class SearchCommands
{
    private static readonly int DefaultWorkers = Math.Max(1, Environment.ProcessorCount / 2);

    public static int Recalc(Options options)
    {
        options.Allow("time-s", "depth", "workers", "save-every", "hash-bits");
        string path = options[0];
        SearchLimits limits = options.Limits(SearchLimits.TimePerMove(TimeSpan.FromSeconds(60)));
        int workers = options.Number("workers", DefaultWorkers, 1, 256);
        int saveEvery = options.Number("save-every", 10, 1, 100_000);
        int hashBits = options.Number("hash-bits", 19, 10, 26);

        OpeningBook before = OpeningBook.Load(path);
        OpeningBook book = OpeningBook.Load(path);
        var recalculator = new BookRecalculator(book, limits, workers, hashBits);
        Console.WriteLine(
            $"{recalculator.LeafPositions:N0} leaf positions; {recalculator.ToSearch:N0} to search, " +
            $"{recalculator.LeafPositions - recalculator.ToSearch:N0} already good enough. " +
            $"{LimitText(limits)} per position, {workers} workers. Ctrl+C stops and saves; run again to continue.");

        var clock = Stopwatch.StartNew();
        TimeSpan searching = TimeSpan.Zero;
        bool completed = Cancellable(token => recalculator.Run(
            progress =>
            {
                searching += progress.Elapsed;
                TimeSpan left = searching / progress.Done * (progress.Total - progress.Done) / Math.Min(workers, progress.Total);
                Console.WriteLine(
                    $"[{progress.Done,6:N0}/{progress.Total:N0}] ply {progress.Ply,2} {progress.Line} {progress.Value,6} " +
                    $"{progress.Origin} d{progress.Effort.DepthReached} ({progress.Elapsed.TotalSeconds:0.0} s), left {Duration(left)}");
            },
            () => Program.WriteText(book, path),
            saveEvery,
            token));

        BookMinimax.Run(book);
        Program.WriteText(book, path);

        BookComparison comparison = BookComparison.Of(before, book);
        string reportPath = Path.ChangeExtension(path, ".report.txt");
        using (var writer = new StreamWriter(reportPath))
        {
            comparison.Write(writer, $"Recalculated {path} with {LimitText(limits)} per position ({DateTime.Now:yyyy-MM-dd HH:mm})");
        }

        Console.WriteLine(
            $"{(completed ? "Finished" : "Stopped")} after {Duration(clock.Elapsed)}. Values changed: {comparison.ValuesChanged:N0}; " +
            $"positions with another first move: {comparison.BestMoveChanges.Count:N0}. Report: {reportPath}.");
        Console.WriteLine("The values are backed up and sorted. Run build to update the binary book.");
        return completed ? 0 : 2;
    }

    public static int Match(Options options)
    {
        options.Allow("starts", "starts-ply", "max-starts", "time-s", "depth", "workers", "hash-bits");
        OpeningBook bookA = OpeningBook.Load(options[0]);
        OpeningBook bookB = OpeningBook.Load(options[1]);
        SearchLimits limits = options.Limits(SearchLimits.FixedDepth(10));
        int workers = options.Number("workers", DefaultWorkers, 1, 256);
        int hashBits = options.Number("hash-bits", 19, 10, 26);

        List<string> starts = (options.Text("starts"), options.Text("starts-ply")) switch
        {
            (string file, null) => ReadStarts(file),
            (null, string) => BookMatch.StartsAtPly(bookB, options.Number("starts-ply", 1, 1, 60)),
            _ => throw new ArgumentException("Give either --starts <report.txt> or --starts-ply <ply>."),
        };
        starts = starts.Take(options.Number("max-starts", starts.Count, 1, int.MaxValue)).ToList();

        Console.WriteLine($"A: {options[0]}");
        Console.WriteLine($"B: {options[1]}");
        Console.WriteLine($"{starts.Count:N0} start positions, 2 games each; {LimitText(limits)} per move, {workers} workers.");

        var pairs = new List<MatchPair>();
        var clock = Stopwatch.StartNew();
        bool completed = Cancellable(token => new BookMatch(bookA, bookB, limits, workers, hashBits).Play(
            starts,
            pair =>
            {
                pairs.Add(pair);
                MatchSummary sofar = MatchSummary.Of(pairs);
                Console.WriteLine(
                    $"[{pairs.Count,5:N0}/{starts.Count:N0}] {pair.Start} B {pair.PointsB:0.0} " +
                    $"(discs for B: {pair.BookAFirst.DiscsB:+0;-0;0} and {pair.BookBFirst.DiscsB:+0;-0;0}), " +
                    $"B so far {sofar.Score:P1}");
            },
            token));

        MatchSummary summary = MatchSummary.Of(pairs);
        Console.WriteLine();
        Console.WriteLine(
            $"{(completed ? "Finished" : "Stopped")} after {Duration(clock.Elapsed)}: {summary.Pairs:N0} pairs. " +
            $"Book B scored {summary.PointsB:0.#} of {2 * summary.Pairs:N0} ({summary.Score:P1}, 95 % interval " +
            $"{summary.Low:P1} to {summary.High:P1}), {summary.AverageDiscsB:+0.0;-0.0;0.0} discs per game.");
        Console.WriteLine(summary.Low > 0.5 ? "Book B is stronger."
            : summary.High < 0.5 ? "Book A is stronger."
            : "No significant difference.");

        List<MatchPair> lost = pairs.Where(pair => pair.PointsB < 1).ToList();
        if (lost.Count > 0)
        {
            Console.WriteLine();
            Console.WriteLine($"Pairs where book B lost points ({lost.Count:N0}):");
            foreach (MatchPair pair in lost)
            {
                Console.WriteLine($"  {pair.Start} B {pair.PointsB:0.0}");
            }
        }

        return completed ? 0 : 2;
    }

    // Returns false if Ctrl+C stopped the work.
    private static bool Cancellable(Action<CancellationToken> work)
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

    private static List<string> ReadStarts(string path)
    {
        using StreamReader reader = File.OpenText(path);
        return BookMatch.ReadStarts(reader);
    }

    private static string LimitText(SearchLimits limits) => limits.Mode switch
    {
        TimeControlMode.FixedDepth => $"depth {limits.Depth}",
        _ => $"{limits.Time.TotalSeconds:0.##} s",
    };

    private static string Duration(TimeSpan time) =>
        time.TotalHours >= 1 ? $"{(int)time.TotalHours}h {time.Minutes:00}m" : $"{time.Minutes}m {time.Seconds:00}s";
}
